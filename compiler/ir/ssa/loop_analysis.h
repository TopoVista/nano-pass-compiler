#pragma once

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "ir/cfg/analysis/dominator_analysis.h"
#include "ir/ssa/ssa_ir.h"

namespace nanopass {

struct LoopId { int index = -1; auto operator<=>(const LoopId &) const = default; };
struct LoopExitEdge { BlockId from; BlockId to; };
struct NaturalLoop {
  LoopId id;
  BlockId header;
  std::vector<BlockId> blocks;
  std::vector<BlockId> backedgeSources;
  std::vector<BlockId> exitBlocks;
  std::vector<LoopExitEdge> exitEdges;
  std::optional<BlockId> preheader;
  std::optional<LoopId> parent;
  std::vector<LoopId> children;
  size_t depth = 1;
};

struct LoopInfo {
  std::vector<NaturalLoop> loops;
  std::optional<LoopId> innermostLoopContaining(BlockId block) const {
    std::optional<LoopId> result;
    for (const auto &loop : loops) if (std::find(loop.blocks.begin(), loop.blocks.end(), block) != loop.blocks.end())
      if (!result || loops[result->index].blocks.size() > loop.blocks.size()) result = loop.id;
    return result;
  }
};

class LoopInfoAnalysis {
public:
  static LoopInfo run(const SSAFunction &function) {
    CFGFunction view; view.entryBlock = function.entryBlock;
    std::map<int, const SSABlock *> blocks;
    for (const auto &block : function.blocks) {
      blocks[block.id.index] = &block;
      auto cfg = std::make_unique<BasicBlock>(); cfg->id=block.id;
      cfg->predecessors=block.predecessors; cfg->successors=block.successors;
      view.blockIndexMap[block.id.index]=view.blocks.size(); view.blocks.push_back(std::move(cfg));
    }
    const auto dominators = DominatorAnalysis::run(view);
    std::map<int, std::set<int>> members;
    std::map<int, std::set<int>> latches;
    for (const auto &[fromIndex, block] : blocks) for (BlockId header : block->successors) {
      if (!dominators.dominates(header, block->id)) continue;
      auto &loopMembers=members[header.index]; auto &loopLatches=latches[header.index];
      loopMembers.insert(header.index); loopMembers.insert(fromIndex); loopLatches.insert(fromIndex);
      std::vector<BlockId> work{block->id};
      while (!work.empty()) {
        BlockId current=work.back(); work.pop_back();
        auto found=blocks.find(current.index); if(found==blocks.end()) continue;
        for(BlockId predecessor:found->second->predecessors)
          if(loopMembers.insert(predecessor.index).second && predecessor!=header) work.push_back(predecessor);
      }
    }
    LoopInfo info;
    for (const auto &[headerIndex, memberSet] : members) {
      NaturalLoop loop; loop.id={static_cast<int>(info.loops.size())}; loop.header={headerIndex};
      for(int id:memberSet)loop.blocks.push_back({id}); for(int id:latches[headerIndex])loop.backedgeSources.push_back({id});
      for(BlockId from:loop.blocks) if(auto found=blocks.find(from.index);found!=blocks.end()) for(BlockId to:found->second->successors)
        if(!memberSet.contains(to.index)){loop.exitEdges.push_back({from,to});loop.exitBlocks.push_back(from);}
      std::sort(loop.blocks.begin(),loop.blocks.end()); std::sort(loop.backedgeSources.begin(),loop.backedgeSources.end());
      std::sort(loop.exitBlocks.begin(),loop.exitBlocks.end()); loop.exitBlocks.erase(std::unique(loop.exitBlocks.begin(),loop.exitBlocks.end()),loop.exitBlocks.end());
      auto headerBlock=blocks.find(headerIndex); if(headerBlock!=blocks.end()) { std::vector<BlockId> outside;
        for(BlockId predecessor:headerBlock->second->predecessors)if(!memberSet.contains(predecessor.index))outside.push_back(predecessor);
        if(outside.size()==1){auto pre=blocks.find(outside.front().index);if(pre!=blocks.end()&&pre->second->successors.size()==1&&pre->second->successors.front()==loop.header)loop.preheader=outside.front();}
      }
      info.loops.push_back(std::move(loop));
    }
    for(size_t child=0;child<info.loops.size();++child){size_t best=info.loops.size();for(size_t candidate=0;candidate<info.loops.size();++candidate){if(child==candidate)continue;const auto&a=info.loops[child];const auto&b=info.loops[candidate];if(b.blocks.size()<=a.blocks.size())continue;bool contains=std::all_of(a.blocks.begin(),a.blocks.end(),[&](BlockId id){return std::find(b.blocks.begin(),b.blocks.end(),id)!=b.blocks.end();});if(contains&&(best==info.loops.size()||b.blocks.size()<info.loops[best].blocks.size()))best=candidate;}if(best!=info.loops.size()){info.loops[child].parent=info.loops[best].id;info.loops[best].children.push_back(info.loops[child].id);}}
    for(auto &loop:info.loops){size_t depth=1;for(auto parent=loop.parent;parent;parent=info.loops[parent->index].parent)++depth;loop.depth=depth;std::sort(loop.children.begin(),loop.children.end());}
    return info;
  }
};

struct LoopInfoVerifierResult { bool ok=true; std::vector<std::string> errors; void error(std::string message){ok=false;errors.push_back(std::move(message));} };
inline LoopInfoVerifierResult verifyLoopInfo(const SSAFunction &function,const LoopInfo &info){
  LoopInfoVerifierResult result; CFGFunction view;view.entryBlock=function.entryBlock;for(const auto&block:function.blocks){auto cfg=std::make_unique<BasicBlock>();cfg->id=block.id;cfg->predecessors=block.predecessors;cfg->successors=block.successors;view.blockIndexMap[block.id.index]=view.blocks.size();view.blocks.push_back(std::move(cfg));}auto dom=DominatorAnalysis::run(view);
  for(const auto&loop:info.loops){auto contains=[&](BlockId id){return std::find(loop.blocks.begin(),loop.blocks.end(),id)!=loop.blocks.end();};if(!contains(loop.header))result.error("loop header is absent from its loop");for(BlockId latch:loop.backedgeSources)if(!contains(latch)||!dom.dominates(loop.header,latch))result.error("invalid loop backedge");for(const auto&edge:loop.exitEdges)if(!contains(edge.from)||contains(edge.to))result.error("invalid loop exit edge");if(loop.parent){const auto&parent=info.loops[loop.parent->index];for(BlockId id:loop.blocks)if(std::find(parent.blocks.begin(),parent.blocks.end(),id)==parent.blocks.end())result.error("parent does not contain child loop");if(loop.depth!=parent.depth+1)result.error("inconsistent loop depth");}}
  return result;
}

} // namespace nanopass
