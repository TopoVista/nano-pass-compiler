#pragma once

#include <algorithm>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ir/cfg/analysis/reverse_postorder_analysis.h"
#include "ir/cfg/cfg_ir.h"

namespace nanopass {

// Dominators are defined only for blocks reachable from a function entry.
// The implementation is the Cooper-Harvey-Kennedy iterative algorithm over
// reverse postorder. IDs rather than BasicBlock pointers keep the result safe
// if CFG storage changes later.
class DominatorTree {
  BlockId entry_{};
  std::unordered_set<int> reachable_;
  std::unordered_map<int, BlockId> idom_;
  std::unordered_map<int, std::vector<BlockId>> children_;

public:
  BlockId entry() const { return entry_; }

  bool isReachable(BlockId block) const {
    return reachable_.contains(block.index);
  }

  std::optional<BlockId> immediateDominator(BlockId block) const {
    if (!isReachable(block) || block == entry_)
      return std::nullopt;
    auto it = idom_.find(block.index);
    return it == idom_.end() ? std::nullopt
                             : std::optional<BlockId>(it->second);
  }

  bool dominates(BlockId dominator, BlockId block) const {
    if (!isReachable(dominator) || !isReachable(block))
      return false;
    for (BlockId current = block;;) {
      if (current == dominator)
        return true;
      if (current == entry_)
        return false;
      auto idom = immediateDominator(current);
      if (!idom)
        return false;
      current = *idom;
    }
  }

  std::vector<BlockId> dominatorsOf(BlockId block) const {
    std::vector<BlockId> result;
    if (!isReachable(block))
      return result;
    for (BlockId current = block;;) {
      result.push_back(current);
      if (current == entry_)
        break;
      current = *immediateDominator(current);
    }
    std::reverse(result.begin(), result.end());
    return result;
  }

  const std::vector<BlockId> &children(BlockId block) const {
    static const std::vector<BlockId> empty;
    auto it = children_.find(block.index);
    return it == children_.end() ? empty : it->second;
  }

private:
  friend class DominatorAnalysis;
};

class DominatorAnalysis {
public:
  static DominatorTree run(const CFGFunction &function) {
    DominatorTree tree;
    tree.entry_ = function.entryBlock;

    const auto rpo = ReversePostOrderAnalysis::run(function).order;
    if (rpo.empty())
      return tree;
    for (BlockId block : rpo)
      tree.reachable_.insert(block.index);

    std::unordered_map<int, int> rpoNumber;
    for (size_t i = 0; i < rpo.size(); ++i)
      rpoNumber.emplace(rpo[i].index, static_cast<int>(i));

    tree.idom_.emplace(tree.entry_.index, tree.entry_);
    bool changed = true;
    while (changed) {
      changed = false;
      for (size_t i = 1; i < rpo.size(); ++i) {
        const BlockId block = rpo[i];
        const BasicBlock *basicBlock = function.getBlock(block);
        if (!basicBlock)
          continue;

        std::optional<BlockId> newIdom;
        for (BlockId predecessor : basicBlock->predecessors) {
          if (!tree.idom_.contains(predecessor.index))
            continue;
          newIdom = newIdom ? intersect(*newIdom, predecessor, tree.idom_,
                                         rpoNumber)
                            : predecessor;
        }
        if (!newIdom)
          continue;
        auto found = tree.idom_.find(block.index);
        if (found == tree.idom_.end() || found->second != *newIdom) {
          tree.idom_[block.index] = *newIdom;
          changed = true;
        }
      }
    }

    // Entry has no immediate dominator; remove the iteration sentinel.
    tree.idom_.erase(tree.entry_.index);
    for (const auto &[blockIndex, idom] : tree.idom_)
      tree.children_[idom.index].push_back(BlockId{blockIndex});
    for (auto &[_, children] : tree.children_)
      std::sort(children.begin(), children.end());
    return tree;
  }

private:
  static BlockId intersect(BlockId first, BlockId second,
                           const std::unordered_map<int, BlockId> &idom,
                           const std::unordered_map<int, int> &rpoNumber) {
    BlockId finger1 = first;
    BlockId finger2 = second;
    while (finger1 != finger2) {
      while (rpoNumber.at(finger1.index) > rpoNumber.at(finger2.index))
        finger1 = idom.at(finger1.index);
      while (rpoNumber.at(finger2.index) > rpoNumber.at(finger1.index))
        finger2 = idom.at(finger2.index);
    }
    return finger1;
  }
};

struct DominatorVerifierResult {
  bool ok = true;
  std::vector<std::string> errors;

  void addError(std::string error) {
    ok = false;
    errors.push_back(std::move(error));
  }
};

inline DominatorVerifierResult verifyDominatorTree(const CFGFunction &function,
                                                    const DominatorTree &tree) {
  DominatorVerifierResult result;
  const auto rpo = ReversePostOrderAnalysis::run(function).order;
  if (rpo.empty())
    return result;
  if (tree.entry() != function.entryBlock)
    result.addError("Dominator tree entry does not match CFG entry");
  for (BlockId block : rpo) {
    if (block == function.entryBlock)
      continue;
    const auto idom = tree.immediateDominator(block);
    if (!idom) {
      result.addError("Reachable block " + block.str() + " has no immediate dominator");
      continue;
    }
    if (*idom == block || !tree.dominates(*idom, block))
      result.addError("Invalid immediate dominator for " + block.str());
  }
  return result;
}

} // namespace nanopass
