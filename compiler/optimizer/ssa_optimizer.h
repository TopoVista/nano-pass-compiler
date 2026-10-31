#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "ir/cfg/analysis/dominator_analysis.h"
#include "ir/cfg/analysis/reachability_analysis.h"
#include "ir/ssa/ssa_def_use.h"
#include "ir/ssa/ssa_verifier.h"

namespace nanopass::opt {

enum class EffectKind { Pure, ReadsMemory, WritesMemory, HasUnknownEffects };
inline EffectKind effectsOf(const SSAInstruction &i) {
  switch (i.kind) {
  case SSAInstructionKind::Constant: case SSAInstructionKind::Copy:
  case SSAInstructionKind::Unary: case SSAInstructionKind::Binary: return EffectKind::Pure;
  case SSAInstructionKind::LoadIndex: return EffectKind::ReadsMemory;
  case SSAInstructionKind::StoreIndex: case SSAInstructionKind::StoreLocal: return EffectKind::WritesMemory;
  case SSAInstructionKind::LoadLocal: case SSAInstructionKind::Call: return EffectKind::HasUnknownEffects;
  }
  return EffectKind::HasUnknownEffects;
}
inline bool isPureInstruction(const SSAInstruction &i) { return effectsOf(i) == EffectKind::Pure; }
inline bool mayHaveSideEffects(const SSAInstruction &i) { return !isPureInstruction(i); }

enum class OptimizationRemarkKind { Applied, Missed, Analysis };
struct OptimizationRemark { OptimizationRemarkKind kind; std::string passId; std::string message; std::optional<BlockId> block; std::optional<ValueId> value; SourceOrigin origin; };
struct SSAOptimizationStatistics { std::map<std::string, size_t> counters; void add(std::string key, size_t value=1) { counters[std::move(key)] += value; } size_t get(const std::string &key) const { auto it=counters.find(key); return it==counters.end()?0:it->second; } };
struct SSAOptimizationPassTelemetry {
  std::string passId;
  bool changed = false;
  size_t instructionsBefore = 0;
  size_t instructionsAfter = 0;
  size_t blocksBefore = 0;
  size_t blocksAfter = 0;
  size_t valuesBefore = 0;
  size_t valuesAfter = 0;
  long long durationUs = 0;
  SSAOptimizationStatistics statistics;
};
struct SSAOptimizationResult { bool changed=false; bool cfgChanged=false; SSAOptimizationStatistics statistics; std::vector<OptimizationRemark> remarks; std::vector<SSAOptimizationPassTelemetry> telemetry; };
struct SSAOptimizationOptions { bool verifyAfterPass=true; bool verboseRemarks=false; };
struct SSAOptimizationContext { SSAOptimizationOptions options; std::vector<OptimizationRemark> remarks; };

// CSE keys are intentionally structural rather than printer strings: formatting
// must never affect optimization identity.  Binary CSE currently applies only
// to scalar SSA expressions, for which these type fields are sufficient.
struct CSEOperandKey {
  bool isConstant = false;
  CoreConstantVal constant = int64_t(0);
  ValueId value{};
  bool operator<(const CSEOperandKey &other) const {
    if (isConstant != other.isConstant) return isConstant < other.isConstant;
    return isConstant ? constant < other.constant : value < other.value;
  }
};
inline CSEOperandKey cseOperandKey(const SSAOperand &operand) {
  if (std::holds_alternative<CoreConstantVal>(operand))
    return {true, std::get<CoreConstantVal>(operand), {}};
  return {false, int64_t(0), std::get<ValueId>(operand)};
}
struct CSEExpressionKey {
  BinaryOpKind operation;
  CSEOperandKey left;
  CSEOperandKey right;
  LangTypeKind typeKind;
  int bitWidth;
  bool isUnsigned;
  bool operator<(const CSEExpressionKey &other) const {
    return std::tie(operation, left, right, typeKind, bitWidth, isUnsigned) <
           std::tie(other.operation, other.left, other.right, other.typeKind,
                    other.bitWidth, other.isUnsigned);
  }
};

inline size_t instructionCount(const SSAFunction &f) { size_t n=0; for(const auto &b:f.blocks)n+=b.instructions.size()+b.phis.size(); return n; }
inline size_t valueCount(const SSAFunction &f) { size_t n=f.parameters.size(); for(const auto &b:f.blocks){n+=b.phis.size();for(const auto&i:b.instructions)if(i.result)++n;}return n; }
inline std::optional<CoreConstantVal> constantOf(const SSAOperand &operand, const std::map<int,CoreConstantVal> &constants) { if(std::holds_alternative<CoreConstantVal>(operand)) return std::get<CoreConstantVal>(operand); auto it=constants.find(std::get<ValueId>(operand).index); return it==constants.end()?std::nullopt:std::optional<CoreConstantVal>(it->second); }
inline bool constantEqual(const CoreConstantVal&a,const CoreConstantVal&b){ return a==b; }
inline std::optional<bool> truth(const CoreConstantVal &v) { if(auto p=std::get_if<bool>(&v))return *p; if(auto p=std::get_if<int64_t>(&v))return *p!=0; return std::nullopt; }
inline std::optional<CoreConstantVal> foldUnary(UnaryOpKind op,const CoreConstantVal &v) { if(op==UnaryOpKind::Negate){if(auto p=std::get_if<int64_t>(&v))return CoreConstantVal{-*p};if(auto p=std::get_if<double>(&v))return CoreConstantVal{-*p};} if(op==UnaryOpKind::LogicalNot)if(auto b=truth(v))return CoreConstantVal{!*b}; return std::nullopt; }
inline std::optional<CoreConstantVal> foldBinary(BinaryOpKind op,const CoreConstantVal&a,const CoreConstantVal&b) {
  if(auto x=std::get_if<int64_t>(&a)){ if(auto y=std::get_if<int64_t>(&b)){ switch(op){case BinaryOpKind::Add:return *x+*y;case BinaryOpKind::Sub:return *x-*y;case BinaryOpKind::Mul:return *x**y;case BinaryOpKind::Div:if(*y)return *x / *y;break;case BinaryOpKind::Mod:if(*y)return *x % *y;break;case BinaryOpKind::Eq:return *x==*y;case BinaryOpKind::Ne:return *x!=*y;case BinaryOpKind::Lt:return *x<*y;case BinaryOpKind::Le:return *x<=*y;case BinaryOpKind::Gt:return *x>*y;case BinaryOpKind::Ge:return *x>=*y;default:break;} } }
  if(auto x=std::get_if<double>(&a)){if(auto y=std::get_if<double>(&b)){switch(op){case BinaryOpKind::Add:return *x+*y;case BinaryOpKind::Sub:return *x-*y;case BinaryOpKind::Mul:return *x**y;case BinaryOpKind::Div:if(*y)return *x / *y;break;case BinaryOpKind::Eq:return *x==*y;case BinaryOpKind::Ne:return *x!=*y;case BinaryOpKind::Lt:return *x<*y;case BinaryOpKind::Le:return *x<=*y;case BinaryOpKind::Gt:return *x>*y;case BinaryOpKind::Ge:return *x>=*y;default:break;}}}
  if(auto x=truth(a)){if(auto y=truth(b)){if(op==BinaryOpKind::LogicalAnd)return *x&&*y;if(op==BinaryOpKind::LogicalOr)return *x||*y;if(op==BinaryOpKind::Eq)return *x==*y;if(op==BinaryOpKind::Ne)return *x!=*y;}}
  return std::nullopt;
}

class SSAFunctionPass { public: virtual ~SSAFunctionPass()=default; virtual std::string_view id()const=0; virtual std::string_view name()const=0; virtual SSAOptimizationResult run(SSAFunction&,SSAOptimizationContext&)=0; };
inline void verifyAfter(const SSAFunctionPass &pass,const SSAFunction &f,const SSAOptimizationContext &ctx) { if(!ctx.options.verifyAfterPass)return; auto r=verifySSA(f); if(!r.ok){std::string msg="internal compiler error: SSA verification failed after pass `"+std::string(pass.id())+"` in @"+f.name+":";for(auto&e:r.errors)msg+="\n  "+e;throw std::runtime_error(msg);} }

class ConstantFoldPass final: public SSAFunctionPass { public: std::string_view id()const override{return "constant-fold";} std::string_view name()const override{return "Constant Folding";} SSAOptimizationResult run(SSAFunction&f,SSAOptimizationContext&)override{SSAOptimizationResult r;std::map<int,CoreConstantVal> c;for(auto &b:f.blocks)for(auto&i:b.instructions){r.statistics.add("instructionsVisited");if(i.kind==SSAInstructionKind::Constant&&i.result)c[i.result->index]=i.constantValue;else if(i.result&&(i.kind==SSAInstructionKind::Unary||i.kind==SSAInstructionKind::Binary)){auto a=constantOf(i.operand1,c);std::optional<CoreConstantVal> folded;if(a){if(i.kind==SSAInstructionKind::Unary)folded=foldUnary(i.unaryOp,*a);else{auto z=constantOf(i.operand2,c);if(z)folded=foldBinary(i.binaryOp,*a,*z);}}if(folded){i.kind=SSAInstructionKind::Constant;i.constantValue=*folded;c[i.result->index]=*folded;r.changed=true;r.statistics.add("instructionsFolded");r.remarks.push_back({OptimizationRemarkKind::Applied,"constant-fold","folded "+i.result->str(),b.id,*i.result,i.origin});}else c.erase(i.result->index);}}return r;} };

class AlgebraicSimplifyPass final: public SSAFunctionPass { public: std::string_view id()const override{return "algebraic-simplify";}std::string_view name()const override{return "Algebraic Simplification";}SSAOptimizationResult run(SSAFunction&f,SSAOptimizationContext&)override{SSAOptimizationResult r;for(auto&b:f.blocks)for(auto&i:b.instructions){if(i.kind!=SSAInstructionKind::Binary||!i.result)continue;auto zero=[](const SSAOperand&o){return std::holds_alternative<CoreConstantVal>(o)&&std::get_if<int64_t>(&std::get<CoreConstantVal>(o))&&*std::get_if<int64_t>(&std::get<CoreConstantVal>(o))==0;};auto one=[](const SSAOperand&o){return std::holds_alternative<CoreConstantVal>(o)&&std::get_if<int64_t>(&std::get<CoreConstantVal>(o))&&*std::get_if<int64_t>(&std::get<CoreConstantVal>(o))==1;};std::optional<SSAOperand> repl;if((i.binaryOp==BinaryOpKind::Add&&zero(i.operand1))||(i.binaryOp==BinaryOpKind::Mul&&one(i.operand1)))repl=i.operand2;else if((i.binaryOp==BinaryOpKind::Add&&zero(i.operand2))||(i.binaryOp==BinaryOpKind::Sub&&zero(i.operand2))||(i.binaryOp==BinaryOpKind::Mul&&one(i.operand2))||(i.binaryOp==BinaryOpKind::Div&&one(i.operand2)))repl=i.operand1;if(repl&&std::holds_alternative<ValueId>(*repl)){replaceAllUses(f,*i.result,std::get<ValueId>(*repl));i.kind=SSAInstructionKind::Constant;i.constantValue=int64_t(0);i.result.reset();r.changed=true;r.statistics.add("simplified");}}return r;} };

class TrivialPhiPass final: public SSAFunctionPass { public: std::string_view id()const override{return "trivial-phi";}std::string_view name()const override{return "Trivial Phi Elimination";}SSAOptimizationResult run(SSAFunction&f,SSAOptimizationContext&)override{SSAOptimizationResult r;bool again=true;while(again){again=false;for(auto&b:f.blocks)for(auto it=b.phis.begin();it!=b.phis.end();){std::optional<ValueId> same;bool bad=false;for(auto in:it->incoming){if(in.value==it->result)continue;if(same&&*same!=in.value){bad=true;break;}same=in.value;}if(!bad&&same){replaceAllUses(f,it->result,*same);r.changed=again=true;r.statistics.add("phisRemoved");it=b.phis.erase(it);}else ++it;}}return r;} };

class CopyPropagationPass final: public SSAFunctionPass { public:std::string_view id()const override{return "copy-prop";}std::string_view name()const override{return "Copy Propagation";}SSAOptimizationResult run(SSAFunction&f,SSAOptimizationContext&)override{SSAOptimizationResult r;for(auto&b:f.blocks)for(auto it=b.instructions.begin();it!=b.instructions.end();){if(it->kind==SSAInstructionKind::Copy&&it->result&&std::holds_alternative<ValueId>(it->operand1)){replaceAllUses(f,*it->result,std::get<ValueId>(it->operand1));r.changed=true;r.statistics.add("copiesRemoved");it=b.instructions.erase(it);}else ++it;}return r;} };

class DeadCodeEliminationPass final: public SSAFunctionPass { public:std::string_view id()const override{return "dce";}std::string_view name()const override{return "Dead Code Elimination";}SSAOptimizationResult run(SSAFunction&f,SSAOptimizationContext&)override{SSAOptimizationResult r;bool again=true;while(again){again=false;SSADefUseIndex du;du.rebuild(f);for(auto&b:f.blocks){for(auto it=b.phis.begin();it!=b.phis.end();){if(du.numberOfUses(it->result)==0){r.remarks.push_back({OptimizationRemarkKind::Applied,"dce","removed unused phi "+it->result.str(),b.id,it->result,it->origin});it=b.phis.erase(it);r.changed=again=true;r.statistics.add("phisRemoved");}else ++it;}for(auto it=b.instructions.begin();it!=b.instructions.end();){if(it->result&&du.numberOfUses(*it->result)==0&&isPureInstruction(*it)){r.remarks.push_back({OptimizationRemarkKind::Applied,"dce","removed unused pure value "+it->result->str(),b.id,*it->result,it->origin});it=b.instructions.erase(it);r.changed=again=true;r.statistics.add("instructionsRemoved");}else ++it;}}}return r;} };

class CSEPass final : public SSAFunctionPass {
public:
  std::string_view id() const override { return "cse"; }
  std::string_view name() const override { return "Common Subexpression Elimination"; }

  SSAOptimizationResult run(SSAFunction &function, SSAOptimizationContext &) override {
    SSAOptimizationResult result;
    CFGFunction view;
    view.entryBlock = function.entryBlock;
    std::map<int, SSABlock *> blocks;
    for (auto &block : function.blocks) {
      blocks[block.id.index] = &block;
      auto cfgBlock = std::make_unique<BasicBlock>();
      cfgBlock->id = block.id;
      cfgBlock->predecessors = block.predecessors;
      cfgBlock->successors = block.successors;
      view.blockIndexMap[block.id.index] = view.blocks.size();
      view.blocks.push_back(std::move(cfgBlock));
    }
    const auto dominators = DominatorAnalysis::run(view);
    using AvailableExpressions = std::map<CSEExpressionKey, ValueId>;
    const auto commutative = [](BinaryOpKind operation) {
      return operation == BinaryOpKind::Add || operation == BinaryOpKind::Mul ||
             operation == BinaryOpKind::Eq || operation == BinaryOpKind::Ne;
    };
    std::function<void(BlockId, AvailableExpressions)> visit =
        [&](BlockId id, AvailableExpressions available) {
          auto foundBlock = blocks.find(id.index);
          if (foundBlock == blocks.end()) return;
          auto &block = *foundBlock->second;
          for (auto instruction = block.instructions.begin();
               instruction != block.instructions.end();) {
            if (instruction->kind != SSAInstructionKind::Binary || !instruction->result) {
              ++instruction;
              continue;
            }
            CSEOperandKey left = cseOperandKey(instruction->operand1);
            CSEOperandKey right = cseOperandKey(instruction->operand2);
            if (commutative(instruction->binaryOp) && right < left) std::swap(left, right);
            const CSEExpressionKey key{instruction->binaryOp, left, right,
                                       instruction->type.kind, instruction->type.bitWidth,
                                       instruction->type.isUnsigned};
            auto availableValue = available.find(key);
            if (availableValue != available.end()) {
              const ValueId eliminated = *instruction->result;
              replaceAllUses(function, eliminated, availableValue->second);
              result.remarks.push_back({OptimizationRemarkKind::Applied, "cse",
                                        "reused " + availableValue->second.str() +
                                            " for " + eliminated.str(),
                                        block.id, eliminated, instruction->origin});
              instruction = block.instructions.erase(instruction);
              result.changed = true;
              result.statistics.add("expressionsEliminated");
              continue;
            }
            available.emplace(key, *instruction->result);
            ++instruction;
          }
          for (BlockId child : dominators.children(id)) visit(child, available);
        };
    visit(function.entryBlock, {});
    return result;
  }
};

class SCCPPass final: public SSAFunctionPass { public:std::string_view id()const override{return "sccp";}std::string_view name()const override{return "Sparse Conditional Constant Propagation";}SSAOptimizationResult run(SSAFunction&f,SSAOptimizationContext&)override{SSAOptimizationResult r;std::map<int,CoreConstantVal> constants;std::set<int> executable{f.entryBlock.index};bool progress=true;while(progress){progress=false;for(auto&b:f.blocks){if(!executable.contains(b.id.index))continue;for(auto&i:b.instructions){if(i.kind==SSAInstructionKind::Constant&&i.result)constants[i.result->index]=i.constantValue;else if(i.result&&(i.kind==SSAInstructionKind::Unary||i.kind==SSAInstructionKind::Binary)){auto a=constantOf(i.operand1,constants);std::optional<CoreConstantVal>x;if(a){if(i.kind==SSAInstructionKind::Unary)x=foldUnary(i.unaryOp,*a);else{auto z=constantOf(i.operand2,constants);if(z)x=foldBinary(i.binaryOp,*a,*z);}}if(x&&(!constants.contains(i.result->index)||!constantEqual(constants[i.result->index],*x))){constants[i.result->index]=*x;progress=true;}}}for(auto&p:b.phis){std::optional<CoreConstantVal>x;bool same=true;for(auto in:p.incoming){auto z=constantOf(SSAOperand{in.value},constants);if(!z){same=false;break;}if(x&&!constantEqual(*x,*z)){same=false;break;}x=*z;}if(same&&x&&(!constants.contains(p.result.index)||!constantEqual(constants[p.result.index],*x))){constants[p.result.index]=*x;progress=true;}}if(auto branch=std::get_if<SSATerminator::Branch>(&b.terminator.value)){auto x=constantOf(branch->condition,constants);if(x&&truth(*x)){auto target=*truth(*x)?branch->thenTarget:branch->elseTarget;if(executable.insert(target.index).second)progress=true;}else{if(executable.insert(branch->thenTarget.index).second)progress=true;if(executable.insert(branch->elseTarget.index).second)progress=true;}}else if(auto jump=std::get_if<SSATerminator::Jump>(&b.terminator.value))if(executable.insert(jump->target.index).second)progress=true;}}for(auto&b:f.blocks)if(executable.contains(b.id.index)){for(auto&i:b.instructions)if(i.result&&constants.contains(i.result->index)&&i.kind!=SSAInstructionKind::Constant){i.kind=SSAInstructionKind::Constant;i.constantValue=constants[i.result->index];r.changed=true;r.statistics.add("constantsPropagated");}if(auto branch=std::get_if<SSATerminator::Branch>(&b.terminator.value)){auto x=constantOf(branch->condition,constants);if(x&&truth(*x)){BlockId target=*truth(*x)?branch->thenTarget:branch->elseTarget;b.terminator.value=SSATerminator::Jump{target};b.successors={target};r.changed=r.cfgChanged=true;r.statistics.add("branchesFolded");}}}r.statistics.add("executableBlocks",executable.size());return r;} };

enum class SCCPLatticeKind { Unknown, Constant, Overdefined };
struct SCCPLatticeValue { SCCPLatticeKind kind=SCCPLatticeKind::Unknown; std::optional<CoreConstantVal> constant; };
inline SCCPLatticeValue meetSCCP(SCCPLatticeValue a, SCCPLatticeValue b) { if(a.kind==SCCPLatticeKind::Overdefined||b.kind==SCCPLatticeKind::Overdefined)return {SCCPLatticeKind::Overdefined,{}};if(a.kind==SCCPLatticeKind::Unknown)return b;if(b.kind==SCCPLatticeKind::Unknown)return a;if(constantEqual(*a.constant,*b.constant))return a;return {SCCPLatticeKind::Overdefined,{}}; }

class WorklistSCCPPass final: public SSAFunctionPass {
public:
  std::string_view id() const override { return "sccp"; }
  std::string_view name() const override { return "Sparse Conditional Constant Propagation"; }
  SSAOptimizationResult run(SSAFunction &function, SSAOptimizationContext &) override {
    SSAOptimizationResult result;
    std::map<int, SSABlock *> blocks;
    for (auto &block : function.blocks) blocks[block.id.index] = &block;
    std::map<int, SCCPLatticeValue> values;
    std::set<std::pair<int,int>> executableEdges;
    std::set<int> executableBlocks;
    std::deque<BlockId> blockWorklist;
    auto operandValue = [&](const SSAOperand &operand) -> SCCPLatticeValue {
      if (std::holds_alternative<CoreConstantVal>(operand)) return {SCCPLatticeKind::Constant, std::get<CoreConstantVal>(operand)};
      auto it = values.find(std::get<ValueId>(operand).index);
      return it == values.end() ? SCCPLatticeValue{} : it->second;
    };
    auto markBlock = [&](BlockId id) { if (executableBlocks.insert(id.index).second) blockWorklist.push_back(id); };
    auto markEdge = [&](BlockId from, BlockId to) { if (executableEdges.insert({from.index,to.index}).second) markBlock(to); };
    auto update = [&](ValueId value, SCCPLatticeValue incoming) {
      const auto next = meetSCCP(values[value.index], incoming);
      const auto &old = values[value.index];
      if (next.kind != old.kind || (next.constant && (!old.constant || !constantEqual(*next.constant,*old.constant)))) {
        values[value.index] = next;
        // Def-use information is stable during analysis. Revisit only blocks
        // containing users rather than restarting a whole-function scan.
        SSADefUseIndex uses; uses.rebuild(function);
        for (const auto &use : uses.usesOf(value)) if (executableBlocks.contains(use.block.index)) blockWorklist.push_back(use.block);
      }
    };
    markBlock(function.entryBlock);
    while (!blockWorklist.empty()) {
      const BlockId id = blockWorklist.front(); blockWorklist.pop_front();
      auto found = blocks.find(id.index); if (found == blocks.end()) continue;
      auto &block = *found->second;
      for (const auto &phi : block.phis) {
        SCCPLatticeValue merged; bool sawEdge = false;
        for (const auto &incoming : phi.incoming) if (executableEdges.contains({incoming.predecessor.index, block.id.index})) {
          merged = meetSCCP(merged, operandValue(SSAOperand{incoming.value})); sawEdge = true;
        }
        if (sawEdge) update(phi.result, merged);
      }
      for (const auto &instruction : block.instructions) if (instruction.result) {
        SCCPLatticeValue evaluated{SCCPLatticeKind::Overdefined,{}};
        if (instruction.kind == SSAInstructionKind::Constant) evaluated = {SCCPLatticeKind::Constant,instruction.constantValue};
        else if (instruction.kind == SSAInstructionKind::Copy) evaluated = operandValue(instruction.operand1);
        else if (instruction.kind == SSAInstructionKind::Unary || instruction.kind == SSAInstructionKind::Binary) {
          auto left = operandValue(instruction.operand1);
          if (left.kind == SCCPLatticeKind::Unknown) evaluated = left;
          else if (left.kind == SCCPLatticeKind::Overdefined) evaluated = left;
          else if (instruction.kind == SSAInstructionKind::Unary) { auto folded=foldUnary(instruction.unaryOp,*left.constant); evaluated=folded?SCCPLatticeValue{SCCPLatticeKind::Constant,*folded}:SCCPLatticeValue{SCCPLatticeKind::Overdefined,{}}; }
          else { auto right=operandValue(instruction.operand2); if(right.kind==SCCPLatticeKind::Constant){auto folded=foldBinary(instruction.binaryOp,*left.constant,*right.constant);evaluated=folded?SCCPLatticeValue{SCCPLatticeKind::Constant,*folded}:SCCPLatticeValue{SCCPLatticeKind::Overdefined,{}};} else evaluated=right; }
        }
        update(*instruction.result, evaluated);
      }
      if (auto jump = std::get_if<SSATerminator::Jump>(&block.terminator.value)) markEdge(block.id,jump->target);
      else if (auto branch = std::get_if<SSATerminator::Branch>(&block.terminator.value)) {
        auto condition = operandValue(branch->condition);
        if (condition.kind == SCCPLatticeKind::Constant && truth(*condition.constant)) markEdge(block.id,*truth(*condition.constant) ? branch->thenTarget : branch->elseTarget);
        else { markEdge(block.id,branch->thenTarget); markEdge(block.id,branch->elseTarget); }
      }
    }
    for (auto &block : function.blocks) if (executableBlocks.contains(block.id.index)) {
      for (auto &instruction : block.instructions) if (instruction.result) {
        auto state = values[instruction.result->index].kind == SCCPLatticeKind::Constant ? values[instruction.result->index].constant : std::nullopt;
        if (state && instruction.kind != SSAInstructionKind::Constant) { instruction.kind=SSAInstructionKind::Constant; instruction.constantValue=*state; result.changed=true; result.statistics.add("constantsPropagated"); }
      }
      if (auto branch = std::get_if<SSATerminator::Branch>(&block.terminator.value)) {
        auto state=operandValue(branch->condition);
        if(state.kind==SCCPLatticeKind::Constant && truth(*state.constant)) { branch->condition=*state.constant; result.changed=true; result.statistics.add("branchConditionsProven"); }
      }
    }
    result.statistics.add("executableBlocks", executableBlocks.size());
    result.statistics.add("executableEdges", executableEdges.size());
    return result;
  }
};

class SimplifyCFGPass final : public SSAFunctionPass {
public:
  std::string_view id() const override { return "simplify-cfg"; }
  std::string_view name() const override { return "CFG Simplification"; }
  SSAOptimizationResult run(SSAFunction &function, SSAOptimizationContext &) override {
    SSAOptimizationResult result;
    for (auto &block : function.blocks) {
      auto branch = std::get_if<SSATerminator::Branch>(&block.terminator.value);
      if (!branch || !std::holds_alternative<CoreConstantVal>(branch->condition)) continue;
      const auto known = truth(std::get<CoreConstantVal>(branch->condition));
      if (!known) continue;
      const BlockId target = *known ? branch->thenTarget : branch->elseTarget;
      block.terminator.value = SSATerminator::Jump{target};
      block.successors = {target};
      result.changed = result.cfgChanged = true;
      result.statistics.add("branchesSimplified");
      result.remarks.push_back({OptimizationRemarkKind::Applied, "simplify-cfg",
                                "replaced constant branch with jump to " + target.str(),
                                block.id, {}, {}});
    }
    auto recomputePredecessors = [&] {
      for (auto &block : function.blocks) block.predecessors.clear();
      for (auto &block : function.blocks) {
        std::sort(block.successors.begin(), block.successors.end());
        block.successors.erase(std::unique(block.successors.begin(), block.successors.end()), block.successors.end());
      }
      for (auto &block : function.blocks) for (BlockId successor : block.successors)
        for (auto &destination : function.blocks) if (destination.id == successor)
          destination.predecessors.push_back(block.id);
      for (auto &block : function.blocks) {
        std::sort(block.predecessors.begin(), block.predecessors.end());
        block.predecessors.erase(std::unique(block.predecessors.begin(), block.predecessors.end()), block.predecessors.end());
      }
    };
    auto removeUnreachable = [&] {
      std::set<int> reachable; std::deque<BlockId> worklist{function.entryBlock};
      while (!worklist.empty()) {
        const BlockId id = worklist.front(); worklist.pop_front();
        if (!reachable.insert(id.index).second) continue;
        for (const auto &block : function.blocks) if (block.id == id)
          for (BlockId successor : block.successors) worklist.push_back(successor);
      }
      if (reachable.size() == function.blocks.size()) return;
      function.blocks.erase(std::remove_if(function.blocks.begin(), function.blocks.end(),
          [&](const SSABlock &block) { return !reachable.contains(block.id.index); }),
          function.blocks.end());
      result.changed = result.cfgChanged = true;
      result.statistics.add("blocksRemoved");
    };
    removeUnreachable(); recomputePredecessors();
    // Forward only an empty, non-entry, single-predecessor jump block. This
    // avoids inventing phi values or changing control-flow multiplicity.
    bool forwarded = true;
    while (forwarded) {
      forwarded = false;
      for (size_t index = 0; index < function.blocks.size(); ++index) {
        auto &middle = function.blocks[index];
        auto jump = std::get_if<SSATerminator::Jump>(&middle.terminator.value);
        if (middle.id == function.entryBlock || !middle.phis.empty() ||
            !middle.instructions.empty() || middle.predecessors.size() != 1 || !jump ||
            jump->target == middle.id) continue;
        const BlockId predecessor = middle.predecessors.front();
        const BlockId destination = jump->target;
        auto destinationIt = std::find_if(function.blocks.begin(), function.blocks.end(),
            [&](const SSABlock &block) { return block.id == destination; });
        if (destinationIt == function.blocks.end()) continue;
        bool wouldDuplicatePhiPredecessor = false;
        for (const auto &phi : destinationIt->phis) {
          bool fromMiddle = false, fromPredecessor = false;
          for (const auto &incoming : phi.incoming) {
            fromMiddle |= incoming.predecessor == middle.id;
            fromPredecessor |= incoming.predecessor == predecessor;
          }
          wouldDuplicatePhiPredecessor |= fromMiddle && fromPredecessor;
        }
        if (wouldDuplicatePhiPredecessor) continue;
        auto predecessorIt = std::find_if(function.blocks.begin(), function.blocks.end(),
            [&](const SSABlock &block) { return block.id == predecessor; });
        if (predecessorIt == function.blocks.end()) continue;
        if (auto predJump = std::get_if<SSATerminator::Jump>(&predecessorIt->terminator.value)) {
          if (predJump->target != middle.id) continue;
          predJump->target = destination;
        } else if (auto predBranch = std::get_if<SSATerminator::Branch>(&predecessorIt->terminator.value)) {
          if (predBranch->thenTarget == middle.id) predBranch->thenTarget = destination;
          if (predBranch->elseTarget == middle.id) predBranch->elseTarget = destination;
        } else continue;
        for (auto &successor : predecessorIt->successors) if (successor == middle.id) successor = destination;
        for (auto &phi : destinationIt->phis) for (auto &incoming : phi.incoming)
          if (incoming.predecessor == middle.id) incoming.predecessor = predecessor;
        function.blocks.erase(function.blocks.begin() + static_cast<std::ptrdiff_t>(index));
        result.changed = result.cfgChanged = forwarded = true;
        result.statistics.add("blocksForwarded");
        break;
      }
      if (forwarded) recomputePredecessors();
    }
    recomputePredecessors();
    for (auto &block : function.blocks) for (auto &phi : block.phis)
      phi.incoming.erase(std::remove_if(phi.incoming.begin(), phi.incoming.end(),
          [&](const PhiIncoming &incoming) {
            return std::find(block.predecessors.begin(), block.predecessors.end(),
                             incoming.predecessor) == block.predecessors.end();
          }), phi.incoming.end());
    return result;
  }
};

class SSAOptimizationPipeline {
  std::vector<std::unique_ptr<SSAFunctionPass>> passes_;
  size_t maxIterations_ = 1;
public:
  void add(std::unique_ptr<SSAFunctionPass> pass) { passes_.push_back(std::move(pass)); }
  void repeatUntilStable(size_t maxIterations) { maxIterations_ = std::max<size_t>(1, maxIterations); }
  SSAOptimizationResult run(SSAFunction &function, SSAOptimizationContext &context) {
    SSAOptimizationResult total;
    for (size_t iteration = 0; iteration < maxIterations_; ++iteration) {
      bool changedThisIteration = false;
      total.statistics.add("pipelineIterations");
      for (auto &pass : passes_) {
        SSAOptimizationPassTelemetry telemetry;
        telemetry.passId = std::string(pass->id());
        telemetry.instructionsBefore = instructionCount(function);
        telemetry.blocksBefore = function.blocks.size();
        telemetry.valuesBefore = valueCount(function);
        const auto start = std::chrono::steady_clock::now();
        auto result = pass->run(function, context);
        telemetry.durationUs = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start).count();
        verifyAfter(*pass, function, context);
        telemetry.changed = result.changed;
        telemetry.instructionsAfter = instructionCount(function);
        telemetry.blocksAfter = function.blocks.size();
        telemetry.valuesAfter = valueCount(function);
        telemetry.statistics = result.statistics;
        total.telemetry.push_back(std::move(telemetry));
        total.changed |= result.changed;
        total.cfgChanged |= result.cfgChanged;
        changedThisIteration |= result.changed;
        for (auto &[key, value] : result.statistics.counters) total.statistics.add(key, value);
        total.remarks.insert(total.remarks.end(), result.remarks.begin(), result.remarks.end());
        context.remarks.insert(context.remarks.end(), result.remarks.begin(), result.remarks.end());
      }
      if (!changedThisIteration) break;
    }
    return total;
  }
};
inline std::unique_ptr<SSAFunctionPass> makePass(std::string_view id){if(id=="constant-fold")return std::make_unique<ConstantFoldPass>();if(id=="algebraic-simplify")return std::make_unique<AlgebraicSimplifyPass>();if(id=="sccp")return std::make_unique<WorklistSCCPPass>();if(id=="copy-prop")return std::make_unique<CopyPropagationPass>();if(id=="trivial-phi")return std::make_unique<TrivialPhiPass>();if(id=="cse")return std::make_unique<CSEPass>();if(id=="simplify-cfg")return std::make_unique<SimplifyCFGPass>();if(id=="dce")return std::make_unique<DeadCodeEliminationPass>();return nullptr;}
struct SSAOptimizationPassInfo {
  std::string_view id;
  std::string_view name;
  std::string_view description;
  bool preservesCFG;
};
inline const std::array<SSAOptimizationPassInfo, 8> &availablePasses() {
  static const std::array<SSAOptimizationPassInfo, 8> passes{{
      {"constant-fold", "Constant Folding", "Fold literal unary and binary expressions.", true},
      {"algebraic-simplify", "Algebraic Simplification", "Apply conservative scalar identities.", true},
      {"sccp", "Sparse Conditional Constant Propagation", "Propagate constants over executable edges.", true},
      {"copy-prop", "Copy Propagation", "Replace scalar copies with their sources.", true},
      {"trivial-phi", "Trivial Phi Elimination", "Remove phis with one effective incoming value.", true},
      {"cse", "Common Subexpression Elimination", "Reuse dominating pure binary expressions.", true},
      {"simplify-cfg", "CFG Simplification", "Fold constant branches and remove unreachable blocks.", false},
      {"dce", "Dead Code Elimination", "Remove unused pure instructions and phis.", true},
  }};
  return passes;
}
inline SSAOptimizationPipeline buildPipeline(int level){SSAOptimizationPipeline p;if(level<=0)return p;auto add=[&](std::initializer_list<const char*> ids){for(auto id:ids)p.add(makePass(id));};if(level==1)add({"constant-fold","algebraic-simplify","copy-prop","trivial-phi","dce"});else{add({"sccp","simplify-cfg","constant-fold","algebraic-simplify","copy-prop","trivial-phi","cse","dce","simplify-cfg","trivial-phi","dce"});if(level>=3)p.repeatUntilStable(4);}return p;}
} // namespace nanopass::opt
