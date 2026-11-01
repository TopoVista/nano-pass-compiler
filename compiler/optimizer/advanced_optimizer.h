#pragma once

#include <map>
#include <set>
#include <any>
#include <tuple>
#include <typeindex>

#include "ir/ssa/call_graph.h"
#include "ir/ssa/loop_analysis.h"
#include "optimizer/ssa_optimizer.h"
#include "passes/AnalysisManager.h"

namespace nanopass::opt {

// SSA analyses share the established AnalysisManager invalidation vocabulary,
// but need identities richer than the AST Program alias can express.  Program
// address is a lifetime token; function name is the stable subject identity.
class SSAAnalysisManager {
  struct Key { std::type_index type; const SSAProgram *program; std::string function; bool operator<(const Key&o)const{return std::tie(type,program,function)<std::tie(o.type,o.program,o.function);} };
  std::map<Key,std::any> cache_;
public:
  template<class Analysis> const typename Analysis::Result &get(const SSAProgram &program) { Key key{typeid(Analysis),&program,{}};auto it=cache_.find(key);if(it==cache_.end())it=cache_.emplace(key,Analysis::compute(program)).first;return std::any_cast<const typename Analysis::Result&>(it->second); }
  template<class Analysis> const typename Analysis::Result &get(const SSAProgram &program,const SSAFunction &function) { Key key{typeid(Analysis),&program,function.name};auto it=cache_.find(key);if(it==cache_.end())it=cache_.emplace(key,Analysis::compute(function)).first;return std::any_cast<const typename Analysis::Result&>(it->second); }
  template<class Analysis> void invalidate(const SSAProgram &program,const SSAFunction &function){cache_.erase(Key{typeid(Analysis),&program,function.name});}
  template<class Analysis> bool isCached(const SSAProgram &program,const SSAFunction &function)const{return cache_.contains(Key{typeid(Analysis),&program,function.name});}
  template<class Analysis> bool isCached(const SSAProgram &program)const{return cache_.contains(Key{typeid(Analysis),&program,{}});}
  void invalidateFunction(const SSAProgram &program,const SSAFunction &function){for(auto it=cache_.begin();it!=cache_.end();)if(it->first.program==&program&&it->first.function==function.name)it=cache_.erase(it);else ++it;}
  void invalidateProgram(const SSAProgram &program){for(auto it=cache_.begin();it!=cache_.end();)if(it->first.program==&program)it=cache_.erase(it);else ++it;}
};
struct CachedLoopInfoAnalysis { using Result=LoopInfo; static Result compute(const SSAFunction&f){return LoopInfoAnalysis::run(f);} };
struct CachedCallGraphAnalysis { using Result=CallGraph; static Result compute(const SSAProgram&p){return CallGraphAnalysis::run(p);} };

enum class OptimizationReasonCode { SideEffects, LoopVariantOperand, UnsafeToSpeculate, NoPreheader, RecursiveCallee, InlineCostTooHigh, BudgetExceeded, InlineDepthLimit, NoReturn, UnsupportedControlFlow, MemoryDependenceUnknown };
inline const char *reasonCodeName(OptimizationReasonCode code) { switch(code){case OptimizationReasonCode::SideEffects:return "SIDE_EFFECTS";case OptimizationReasonCode::LoopVariantOperand:return "LOOP_VARIANT_OPERAND";case OptimizationReasonCode::UnsafeToSpeculate:return "UNSAFE_TO_SPECULATE";case OptimizationReasonCode::NoPreheader:return "NO_PREHEADER";case OptimizationReasonCode::RecursiveCallee:return "INLINE_RECURSIVE";case OptimizationReasonCode::InlineCostTooHigh:return "INLINE_COST_TOO_HIGH";case OptimizationReasonCode::BudgetExceeded:return "INLINE_GROWTH_BUDGET";case OptimizationReasonCode::InlineDepthLimit:return "INLINE_DEPTH_LIMIT";case OptimizationReasonCode::NoReturn:return "INLINE_NORETURN_UNSUPPORTED";case OptimizationReasonCode::UnsupportedControlFlow:return "INLINE_UNSUPPORTED_CFG";case OptimizationReasonCode::MemoryDependenceUnknown:return "MEMORY_DEPENDENCE_UNKNOWN";}return "UNKNOWN"; }
struct OptimizationDecision { std::string passId; bool applied=false; OptimizationReasonCode reason; std::string explanation; std::optional<BlockId> block; std::optional<ValueId> value; SourceOrigin origin; };
struct OptimizationBudget {
  size_t maxInlineCalleeInstructions=12;
  size_t maxCallerGrowth=32;
  size_t maxTotalInlineGrowth=96;
  size_t maxInlineDepth=1;
  // Optional, measured function-call counts from a NanoProfile. These values
  // tune only cost limits; they never override recursion, type, CFG, or SSA
  // verifier gates.
  std::map<std::string,uint64_t> observedFunctionCalls;
  uint64_t hotFunctionThreshold=10;
  size_t hotInlineCalleeInstructions=24;
  // Profile data may increase bounded optimization effort for a demonstrably
  // hot callee, but it is never an escape hatch from a finite growth budget.
  size_t hotMaxCallerGrowth=64;
  size_t hotMaxTotalInlineGrowth=192;
};
inline bool isSafeToSpeculate(const SSAInstruction &instruction) { return instruction.kind==SSAInstructionKind::Unary || (instruction.kind==SSAInstructionKind::Binary && instruction.binaryOp!=BinaryOpKind::Div && instruction.binaryOp!=BinaryOpKind::Mod); }

class LoopSimplifyPass final : public SSAFunctionPass {
public:
 std::string_view id()const override{return "loop-simplify";} std::string_view name()const override{return "Loop Canonicalization";}
 SSAOptimizationResult run(SSAFunction &function,SSAOptimizationContext&)override { SSAOptimizationResult result; auto info=LoopInfoAnalysis::run(function);int nextBlock=0,nextValue=0;for(const auto&block:function.blocks){nextBlock=std::max(nextBlock,block.id.index+1);for(const auto&phi:block.phis)nextValue=std::max(nextValue,phi.result.index+1);for(const auto&i:block.instructions)if(i.result)nextValue=std::max(nextValue,i.result->index+1);}for(const auto&p:function.parameters)nextValue=std::max(nextValue,p.value.index+1);
  auto redirect=[](SSABlock&block,BlockId from,BlockId to){for(auto&s:block.successors)if(s==from)s=to;if(auto*j=std::get_if<SSATerminator::Jump>(&block.terminator.value)){if(j->target==from)j->target=to;}else if(auto*b=std::get_if<SSATerminator::Branch>(&block.terminator.value)){if(b->thenTarget==from)b->thenTarget=to;if(b->elseTarget==from)b->elseTarget=to;}};
  auto repairEdges=[&]{for(auto&block:function.blocks)block.predecessors.clear();for(auto&block:function.blocks)for(BlockId successor:block.successors)for(auto&destination:function.blocks)if(destination.id==successor)destination.predecessors.push_back(block.id);};
  for(const auto&loop:info.loops){result.statistics.add("loopsSeen");if(loop.preheader){result.statistics.add("preheadersReused");continue;}auto headerIt=std::find_if(function.blocks.begin(),function.blocks.end(),[&](const SSABlock&b){return b.id==loop.header;});if(headerIt==function.blocks.end())continue;std::set<int> members;for(BlockId id:loop.blocks)members.insert(id.index);std::vector<BlockId> outside;for(BlockId pred:headerIt->predecessors)if(!members.contains(pred.index))outside.push_back(pred);if(outside.empty())continue;std::sort(outside.begin(),outside.end());SSABlock preheader;preheader.id={nextBlock++};preheader.label="preheader."+std::to_string(loop.header.index);preheader.predecessors=outside;preheader.successors={headerIt->id};preheader.terminator.value=SSATerminator::Jump{headerIt->id};
    for(auto&phi:headerIt->phis){std::vector<PhiIncoming> external;std::vector<PhiIncoming> retained;for(const auto&incoming:phi.incoming){if(std::find(outside.begin(),outside.end(),incoming.predecessor)!=outside.end())external.push_back(incoming);else retained.push_back(incoming);}if(external.empty())continue;std::sort(external.begin(),external.end(),[](const auto&a,const auto&b){return a.predecessor<b.predecessor;});ValueId merged=external.front().value;bool same=std::all_of(external.begin(),external.end(),[&](const auto&i){return i.value==merged;});if(!same){PhiInstruction mergedPhi;mergedPhi.result={nextValue++};mergedPhi.originalLocal=phi.originalLocal;mergedPhi.type=phi.type;mergedPhi.incoming=external;merged=mergedPhi.result;preheader.phis.push_back(std::move(mergedPhi));result.statistics.add("preheaderPhisCreated");}retained.push_back({preheader.id,merged});phi.incoming=std::move(retained);}
    for(BlockId predecessor:outside)for(auto&block:function.blocks)if(block.id==predecessor)redirect(block,headerIt->id,preheader.id);function.blocks.push_back(std::move(preheader));repairEdges();result.changed=result.cfgChanged=true;result.statistics.add("preheadersCreated");result.remarks.push_back({OptimizationRemarkKind::Applied,"loop-simplify","created canonical preheader",loop.header,{},function.origin});}
  return result; }
};

class LICMPass final : public SSAFunctionPass {
public:
 std::string_view id()const override{return "licm";} std::string_view name()const override{return "Loop Invariant Code Motion";}
 SSAOptimizationResult run(SSAFunction &function,SSAOptimizationContext&)override {
  SSAOptimizationResult result; const auto info=LoopInfoAnalysis::run(function); std::map<int,SSABlock*> blocks;std::map<int,std::optional<BlockId>> definitions;
  for(auto &parameter:function.parameters)definitions[parameter.value.index]=std::nullopt;
  for(auto &block:function.blocks){blocks[block.id.index]=&block;for(const auto&phi:block.phis)definitions[phi.result.index]=block.id;for(const auto&instruction:block.instructions)if(instruction.result)definitions[instruction.result->index]=block.id;}
  for(const auto&loop:info.loops){result.statistics.add("loopsConsidered");if(!loop.preheader){result.remarks.push_back({OptimizationRemarkKind::Missed,"licm","NO_PREHEADER: loop lacks canonical preheader",loop.header,{},function.origin});continue;}auto pre=blocks.find(loop.preheader->index);if(pre==blocks.end())continue;std::set<int> inLoop;for(BlockId id:loop.blocks)inLoop.insert(id.index);std::set<int> invariant;
    bool changed=true;while(changed){changed=false;for(BlockId id:loop.blocks){auto found=blocks.find(id.index);if(found==blocks.end())continue;auto &block=*found->second;for(auto it=block.instructions.begin();it!=block.instructions.end();){if(!it->result||invariant.contains(it->result->index)){++it;continue;}result.statistics.add("licmCandidates");if(!isPureInstruction(*it)){result.remarks.push_back({OptimizationRemarkKind::Missed,"licm","SIDE_EFFECTS: instruction may read or write memory",block.id,*it->result,it->origin});++it;continue;}if(!isSafeToSpeculate(*it)){result.remarks.push_back({OptimizationRemarkKind::Missed,"licm","UNSAFE_TO_SPECULATE: division or modulo may trap",block.id,*it->result,it->origin});++it;continue;}auto invariantOperand=[&](const SSAOperand&o){if(std::holds_alternative<CoreConstantVal>(o))return true;auto def=definitions.find(std::get<ValueId>(o).index);return def!=definitions.end()&&(!def->second||!inLoop.contains(def->second->index)||invariant.contains(std::get<ValueId>(o).index));};if(!invariantOperand(it->operand1)||(it->kind==SSAInstructionKind::Binary&&!invariantOperand(it->operand2))){result.remarks.push_back({OptimizationRemarkKind::Missed,"licm","LOOP_VARIANT_OPERAND: operand is defined in this loop",block.id,*it->result,it->origin});++it;continue;}auto moved=*it;it=block.instructions.erase(it);pre->second->instructions.push_back(std::move(moved));invariant.insert(pre->second->instructions.back().result->index);definitions[pre->second->instructions.back().result->index]=pre->second->id;result.changed=true;changed=true;result.statistics.add("instructionsHoisted");result.remarks.push_back({OptimizationRemarkKind::Applied,"licm","hoisted loop-invariant "+pre->second->instructions.back().result->str(),block.id,*pre->second->instructions.back().result,pre->second->instructions.back().origin});}}}
  } return result;
 }
};

class StrengthReducePass final : public SSAFunctionPass { public: std::string_view id()const override{return "strength-reduce";}std::string_view name()const override{return "Strength Reduction";}SSAOptimizationResult run(SSAFunction&function,SSAOptimizationContext&)override{SSAOptimizationResult result;for(auto&block:function.blocks)for(auto&instruction:block.instructions){if(instruction.kind!=SSAInstructionKind::Binary||instruction.binaryOp!=BinaryOpKind::Mul||!instruction.type.isInt())continue;auto two=[](const SSAOperand&o){return std::holds_alternative<CoreConstantVal>(o)&&std::get_if<int64_t>(&std::get<CoreConstantVal>(o))&&*std::get_if<int64_t>(&std::get<CoreConstantVal>(o))==2;};if(two(instruction.operand1)||two(instruction.operand2)){auto value=two(instruction.operand1)?instruction.operand2:instruction.operand1;instruction.binaryOp=BinaryOpKind::Add;instruction.operand1=value;instruction.operand2=value;result.changed=true;result.statistics.add("multipliesReduced");if(instruction.result)result.remarks.push_back({OptimizationRemarkKind::Applied,"strength-reduce","reduced multiply-by-two to add",block.id,*instruction.result,instruction.origin});}}return result;} };

inline std::unique_ptr<SSAFunctionPass> makeAdvancedPass(std::string_view id){if(id=="loop-simplify")return std::make_unique<LoopSimplifyPass>();if(id=="licm")return std::make_unique<LICMPass>();if(id=="strength-reduce")return std::make_unique<StrengthReducePass>();return nullptr;}

struct InlineDecision { bool inlineCall=false; size_t estimatedCost=0; OptimizationReasonCode reason=OptimizationReasonCode::UnsupportedControlFlow; std::string explanation; };

class InlineCostModel {
public:
  size_t estimate(const SSAFunction &callee) const {
    size_t returns = 0;
    for (const auto &block : callee.blocks)
      returns += std::holds_alternative<SSATerminator::Return>(block.terminator.value);
    return instructionCount(callee) + 2 * callee.blocks.size() +
           (returns > 0 ? returns - 1 : 0);
  }

  InlineDecision decide(const SSAFunction &callee, bool recursive,
                        const OptimizationBudget &budget,
                        size_t callerGrowth, size_t totalGrowth,
                        size_t inlineDepth) const {
    const size_t cost = estimate(callee);
    if (recursive)
      return {false, cost, OptimizationReasonCode::RecursiveCallee, "callee is in a recursive SCC"};
    if (inlineDepth > budget.maxInlineDepth)
      return {false, cost, OptimizationReasonCode::InlineDepthLimit, "inline depth limit reached"};
    if (cost > budget.maxInlineCalleeInstructions)
      return {false, cost, OptimizationReasonCode::InlineCostTooHigh, "callee cost exceeds threshold"};
    if (callerGrowth + cost > budget.maxCallerGrowth ||
        totalGrowth + cost > budget.maxTotalInlineGrowth)
      return {false, cost, OptimizationReasonCode::BudgetExceeded, "growth budget exhausted"};
    return {true, cost, OptimizationReasonCode::UnsupportedControlFlow, "direct CFG callee"};
  }
};

inline void rebuildSSAEdges(SSAFunction &function) {
  std::map<int, SSABlock *> blocks;
  for (auto &block : function.blocks) {
    block.predecessors.clear();
    block.successors.clear();
    blocks.emplace(block.id.index, &block);
  }
  for (auto &block : function.blocks) {
    std::visit([&](const auto &term) {
      using T = std::decay_t<decltype(term)>;
      if constexpr (std::is_same_v<T, SSATerminator::Jump>) {
        block.successors.push_back(term.target);
      } else if constexpr (std::is_same_v<T, SSATerminator::Branch>) {
        block.successors = {term.thenTarget, term.elseTarget};
      }
    }, block.terminator.value);
    std::sort(block.successors.begin(), block.successors.end());
    block.successors.erase(std::unique(block.successors.begin(), block.successors.end()), block.successors.end());
    for (BlockId successor : block.successors)
      if (auto found = blocks.find(successor.index); found != blocks.end())
        found->second->predecessors.push_back(block.id);
  }
  for (auto &block : function.blocks) {
    std::sort(block.predecessors.begin(), block.predecessors.end());
    block.predecessors.erase(std::unique(block.predecessors.begin(), block.predecessors.end()), block.predecessors.end());
  }
}

class SSAProgramInliner {
  struct ReturnSite { BlockId block; std::optional<ValueId> value; };

  static void addMissed(SSAOptimizationResult &result, OptimizationReasonCode code,
                        std::string explanation, BlockId block,
                        std::optional<ValueId> value, SourceOrigin origin) {
    result.remarks.push_back({OptimizationRemarkKind::Missed, "inline",
                              std::string(reasonCodeName(code)) + ": " + explanation,
                              block, value, origin});
  }

public:
  SSAOptimizationResult run(SSAProgram &program,
                            const OptimizationBudget &budget = {},
                            SSAAnalysisManager *analyses = nullptr) const {
    SSAOptimizationResult result;
    const CallGraph graphSnapshot = analyses
        ? analyses->get<CachedCallGraphAnalysis>(program)
        : CallGraphAnalysis::run(program);

    InlineCostModel model;
    std::map<std::string, const SSAFunction *> functions;
    std::map<std::string, size_t> callerGrowth;
    for (const auto &function : program.functions)
      functions.emplace(function.name, &function);
    size_t totalGrowth = 0;

    for (auto &caller : program.functions) {
      bool changedThisCaller = true;
      while (changedThisCaller) {
        changedThisCaller = false;
        for (size_t blockIndex = 0; blockIndex < caller.blocks.size() && !changedThisCaller; ++blockIndex) {
          if (caller.blocks[blockIndex].label.rfind("inline.", 0) == 0)
            continue; // Cloned nested calls are considered only by a later pass run.
          for (size_t instructionIndex = 0;
               instructionIndex < caller.blocks[blockIndex].instructions.size() && !changedThisCaller;
               ++instructionIndex) {
            const SSAInstruction call = caller.blocks[blockIndex].instructions[instructionIndex];
            if (call.kind != SSAInstructionKind::Call)
              continue;
            result.statistics.add("callsConsidered");
            const auto calleeIt = functions.find(call.callee);
            if (calleeIt == functions.end()) {
              addMissed(result, OptimizationReasonCode::UnsupportedControlFlow,
                        "callee has no SSA body", caller.blocks[blockIndex].id,
                        call.result, call.origin);
              result.statistics.add("callsRejectedUnsupported");
              continue;
            }
            const SSAFunction &callee = *calleeIt->second;
            OptimizationBudget decisionBudget = budget;
            const auto observedCalls = budget.observedFunctionCalls.find(callee.name);
            const bool hotByProfile = observedCalls != budget.observedFunctionCalls.end() &&
                observedCalls->second >= budget.hotFunctionThreshold;
            if (hotByProfile) {
              decisionBudget.maxInlineCalleeInstructions = std::max(
                  decisionBudget.maxInlineCalleeInstructions,
                  decisionBudget.hotInlineCalleeInstructions);
              decisionBudget.maxCallerGrowth = std::max(
                  decisionBudget.maxCallerGrowth,
                  decisionBudget.hotMaxCallerGrowth);
              decisionBudget.maxTotalInlineGrowth = std::max(
                  decisionBudget.maxTotalInlineGrowth,
                  decisionBudget.hotMaxTotalInlineGrowth);
              result.remarks.push_back({OptimizationRemarkKind::Analysis, "inline",
                  "PROFILE_HOT_CALLEE: @" + callee.name + " observed " +
                      std::to_string(observedCalls->second) + " calls; inline threshold " +
                      std::to_string(budget.maxInlineCalleeInstructions) + " -> " +
                      std::to_string(decisionBudget.maxInlineCalleeInstructions) +
                      ", caller growth " + std::to_string(budget.maxCallerGrowth) +
                      " -> " + std::to_string(decisionBudget.maxCallerGrowth) +
                      ", total growth " + std::to_string(budget.maxTotalInlineGrowth) +
                      " -> " + std::to_string(decisionBudget.maxTotalInlineGrowth),
                  caller.blocks[blockIndex].id, call.result, call.origin});
              result.statistics.add("profileHotCallees");
            } else if (observedCalls != budget.observedFunctionCalls.end()) {
              result.remarks.push_back({OptimizationRemarkKind::Analysis, "inline",
                  "PROFILE_COLD_CALLEE: @" + callee.name + " observed " +
                      std::to_string(observedCalls->second) + " calls below hot threshold " +
                      std::to_string(budget.hotFunctionThreshold) +
                      "; retaining static inline budget",
                  caller.blocks[blockIndex].id, call.result, call.origin});
              result.statistics.add("profileColdCallees");
            }
            const InlineDecision decision = model.decide(
                callee, graphSnapshot.recursive(callee.name), decisionBudget,
                callerGrowth[caller.name], totalGrowth, 1);
            if (!decision.inlineCall) {
              addMissed(result, decision.reason, decision.explanation,
                        caller.blocks[blockIndex].id, call.result, call.origin);
              if (decision.reason == OptimizationReasonCode::RecursiveCallee) result.statistics.add("callsRejectedRecursive");
              if (decision.reason == OptimizationReasonCode::InlineCostTooHigh) result.statistics.add("callsRejectedCost");
              if (decision.reason == OptimizationReasonCode::BudgetExceeded) result.statistics.add("callsRejectedBudget");
              if (decision.reason == OptimizationReasonCode::InlineDepthLimit) result.statistics.add("callsRejectedDepth");
              if (decision.reason == OptimizationReasonCode::RecursiveCallee) result.statistics.add("rejectedRecursive");
              if (decision.reason == OptimizationReasonCode::InlineCostTooHigh) result.statistics.add("rejectedCost");
              if (decision.reason == OptimizationReasonCode::BudgetExceeded) result.statistics.add("rejectedBudget");
              if (decision.reason == OptimizationReasonCode::InlineDepthLimit) result.statistics.add("rejectedDepth");
              continue;
            }
            if (callee.parameters.size() != call.callArgs.size()) {
              addMissed(result, OptimizationReasonCode::UnsupportedControlFlow,
                        "argument count mismatch", caller.blocks[blockIndex].id,
                        call.result, call.origin);
              result.statistics.add("callsRejectedUnsupported");
              continue;
            }

            size_t returnCount = 0;
            bool malformedReturn = false;
            for (const auto &block : callee.blocks) {
              if (const auto *returned = std::get_if<SSATerminator::Return>(&block.terminator.value)) {
                ++returnCount;
                malformedReturn |= (callee.returnType.kind == LangTypeKind::Void) != !returned->value.has_value();
              }
            }
            if (returnCount == 0) {
              addMissed(result, OptimizationReasonCode::NoReturn,
                        "callee has no reachable return", caller.blocks[blockIndex].id,
                        call.result, call.origin);
              result.statistics.add("callsRejectedNoReturn");
              continue;
            }
            if (malformedReturn || (callee.returnType.kind == LangTypeKind::Void) == call.result.has_value() ||
                !sameType(callee.returnType, call.type)) {
              addMissed(result, OptimizationReasonCode::UnsupportedControlFlow,
                        "call or return type is malformed", caller.blocks[blockIndex].id,
                        call.result, call.origin);
              result.statistics.add("callsRejectedUnsupported");
              continue;
            }
            const auto entryIt = std::find_if(callee.blocks.begin(), callee.blocks.end(), [&](const SSABlock &block) {
              return block.id == callee.entryBlock;
            });
            if (entryIt == callee.blocks.end() || !entryIt->phis.empty()) {
              addMissed(result, OptimizationReasonCode::UnsupportedControlFlow,
                        "callee entry has unsupported phi state", caller.blocks[blockIndex].id,
                        call.result, call.origin);
              result.statistics.add("callsRejectedUnsupported");
              continue;
            }

            int nextBlock = 0;
            int nextValue = 0;
            for (const auto &parameter : caller.parameters)
              nextValue = std::max(nextValue, parameter.value.index + 1);
            for (const auto &block : caller.blocks) {
              nextBlock = std::max(nextBlock, block.id.index + 1);
              for (const auto &phi : block.phis)
                nextValue = std::max(nextValue, phi.result.index + 1);
              for (const auto &instruction : block.instructions)
                if (instruction.result)
                  nextValue = std::max(nextValue, instruction.result->index + 1);
            }

            std::map<int, BlockId> blockMap;
            for (const auto &block : callee.blocks)
              blockMap.emplace(block.id.index, BlockId{nextBlock++});
            const BlockId continuationId{nextBlock++};
            std::map<int, SSAOperand> valueMap;
            for (size_t index = 0; index < callee.parameters.size(); ++index)
              valueMap.emplace(callee.parameters[index].value.index, call.callArgs[index]);
            for (const auto &block : callee.blocks) {
              for (const auto &phi : block.phis)
                valueMap.emplace(phi.result.index, ValueId{nextValue++});
              for (const auto &instruction : block.instructions)
                if (instruction.result)
                  valueMap.emplace(instruction.result->index, ValueId{nextValue++});
            }
            const auto remapOperand = [&](const SSAOperand &operand) -> SSAOperand {
              if (std::holds_alternative<CoreConstantVal>(operand)) return operand;
              return valueMap.at(std::get<ValueId>(operand).index);
            };

            const SSABlock original = caller.blocks[blockIndex];
            SSABlock continuation;
            continuation.id = continuationId;
            continuation.label = "inline.cont." + std::to_string(continuationId.index);
            continuation.instructions.assign(original.instructions.begin() + static_cast<std::ptrdiff_t>(instructionIndex + 1), original.instructions.end());
            continuation.terminator = original.terminator;

            std::vector<SSABlock> clones;
            std::vector<ReturnSite> returnSites;
            clones.reserve(callee.blocks.size());
            for (const auto &oldBlock : callee.blocks) {
              SSABlock clone;
              clone.id = blockMap.at(oldBlock.id.index);
              clone.label = "inline." + callee.name + "." + oldBlock.label;
              for (const auto &oldPhi : oldBlock.phis) {
                PhiInstruction phi = oldPhi;
                phi.result = std::get<ValueId>(valueMap.at(oldPhi.result.index));
                for (auto &incoming : phi.incoming) {
                  incoming.predecessor = blockMap.at(incoming.predecessor.index);
                  incoming.value = std::get<ValueId>(valueMap.at(incoming.value.index));
                }
                std::sort(phi.incoming.begin(), phi.incoming.end(), [](const PhiIncoming &left, const PhiIncoming &right) {
                  return left.predecessor < right.predecessor;
                });
                clone.phis.push_back(std::move(phi));
              }
              for (SSAInstruction instruction : oldBlock.instructions) {
                instruction.operand1 = remapOperand(instruction.operand1);
                instruction.operand2 = remapOperand(instruction.operand2);
                for (auto &argument : instruction.callArgs)
                  argument = remapOperand(argument);
                if (instruction.result)
                  instruction.result = std::get<ValueId>(valueMap.at(instruction.result->index));
                clone.instructions.push_back(std::move(instruction));
              }
              if (const auto *returned = std::get_if<SSATerminator::Return>(&oldBlock.terminator.value)) {
                ReturnSite site{clone.id, std::nullopt};
                if (returned->value) {
                  SSAOperand value = remapOperand(*returned->value);
                  if (std::holds_alternative<CoreConstantVal>(value)) {
                    const ValueId materialized{nextValue++};
                    SSAInstruction constant;
                    constant.kind = SSAInstructionKind::Constant;
                    constant.result = materialized;
                    constant.type = callee.returnType;
                    constant.constantValue = std::get<CoreConstantVal>(value);
                    constant.origin = call.origin;
                    clone.instructions.push_back(std::move(constant));
                    value = materialized;
                  }
                  site.value = std::get<ValueId>(value);
                }
                returnSites.push_back(site);
                clone.terminator.value = SSATerminator::Jump{continuationId};
              } else if (const auto *jump = std::get_if<SSATerminator::Jump>(&oldBlock.terminator.value)) {
                clone.terminator.value = SSATerminator::Jump{blockMap.at(jump->target.index)};
              } else if (const auto *branch = std::get_if<SSATerminator::Branch>(&oldBlock.terminator.value)) {
                clone.terminator.value = SSATerminator::Branch{remapOperand(branch->condition), blockMap.at(branch->thenTarget.index), blockMap.at(branch->elseTarget.index)};
              } else {
                clone.terminator = oldBlock.terminator;
              }
              clones.push_back(std::move(clone));
            }

            // All rejection checks above precede mutation.  From here on the
            // caller has a complete, verifier-checkable replacement CFG.
            for (BlockId successor : original.successors)
              for (auto &block : caller.blocks)
                if (block.id == successor)
                  for (auto &phi : block.phis)
                    for (auto &incoming : phi.incoming)
                      if (incoming.predecessor == original.id)
                        incoming.predecessor = continuationId;
            caller.blocks[blockIndex].instructions.assign(original.instructions.begin(), original.instructions.begin() + static_cast<std::ptrdiff_t>(instructionIndex));
            caller.blocks[blockIndex].terminator.value = SSATerminator::Jump{blockMap.at(callee.entryBlock.index)};
            caller.blocks.push_back(std::move(continuation));
            caller.blocks.insert(caller.blocks.end(), std::make_move_iterator(clones.begin()), std::make_move_iterator(clones.end()));
            SSABlock &continuationBlock = caller.blocks[caller.blocks.size() - clones.size() - 1];

            if (call.result) {
              if (returnSites.size() == 1) {
                replaceAllUses(caller, *call.result, *returnSites.front().value);
              } else {
                PhiInstruction phi;
                phi.result = *call.result;
                phi.type = call.type;
                phi.origin = call.origin;
                for (const ReturnSite &site : returnSites)
                  phi.incoming.push_back({site.block, *site.value});
                std::sort(phi.incoming.begin(), phi.incoming.end(), [](const PhiIncoming &left, const PhiIncoming &right) {
                  return left.predecessor < right.predecessor;
                });
                continuationBlock.phis.push_back(std::move(phi));
                result.statistics.add("continuationPhisCreated");
                result.statistics.add("returnSitesMerged", returnSites.size());
              }
            }
            rebuildSSAEdges(caller);
            // Def-use is not a persistent inliner-owned analysis, but rebuild
            // it here so the transformed representation is checked through
            // the same use-indexing path consumed by later optimizer passes.
            SSADefUseIndex defUse;
            defUse.rebuild(caller);
            const auto verified = verifySSA(caller);
            if (!verified.ok) {
              std::string message = "internal compiler error: invalid SSA after pass `inline` in @" + caller.name +
                                    " while inlining @" + callee.name + " from " + original.id.str();
              for (const auto &error : verified.errors) message += "\n  " + error;
              throw std::runtime_error(message);
            }
            result.changed = result.cfgChanged = true;
            result.statistics.add("callsInlined");
            result.statistics.add("blocksCloned", clones.size());
            result.statistics.add("instructionsCloned", instructionCount(callee));
            for (const auto &block : callee.blocks) result.statistics.add("phisCloned", block.phis.size());
            callerGrowth[caller.name] += decision.estimatedCost;
            totalGrowth += decision.estimatedCost;
            const std::string inlineMessage = hotByProfile
                ? "inlined hot CFG " + callee.name + " (profile calls=" + std::to_string(observedCalls->second) + ")"
                : "inlined CFG " + callee.name;
            result.remarks.push_back({OptimizationRemarkKind::Applied, "inline", inlineMessage, original.id, {}, call.origin});
            if (hotByProfile) result.statistics.add("hotProfileInlines");
            if (analyses) {
              analyses->invalidateFunction(program, caller);
              analyses->invalidateProgram(program);
            }
            changedThisCaller = true;
          }
        }
      }
    }
    return result;
  }
};

} // namespace nanopass::opt
