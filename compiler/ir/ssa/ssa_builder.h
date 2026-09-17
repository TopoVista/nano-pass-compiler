#pragma once

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

#include "ir/cfg/analysis/dominance_frontier_analysis.h"
#include "ir/ssa/ssa_ir.h"

namespace nanopass {

class SSABuilder {
  struct LocalInfo { LangType type; std::vector<BlockId> definitions; };
  using LocalMap = std::map<LocalId, LocalInfo>;
  using PhiPlan = std::map<int, std::vector<LocalId>>;

public:
  SSAProgram build(const CFGProgram &program, SSAStatistics *statistics = nullptr) {
    SSAProgram result;
    SSAStatistics totals;
    for (const auto &function : program.functions) {
      SSAStatistics one;
      result.functions.push_back(buildFunction(function, one));
      totals.functions += one.functions; totals.blocks += one.blocks;
      totals.values += one.values; totals.phiNodes += one.phiNodes;
      totals.promotableLocals += one.promotableLocals; totals.promotedLocals += one.promotedLocals;
      totals.loadsRemoved += one.loadsRemoved; totals.storesRemoved += one.storesRemoved;
      totals.instructions += one.instructions;
    }
    if (statistics) *statistics = totals;
    return result;
  }

private:
  static bool isScalar(const LangType &type) { return type.kind != LangTypeKind::Array; }

  SSAFunction buildFunction(const CFGFunction &cfg, SSAStatistics &stats) {
    SSAFunction out; out.name = cfg.name; out.returnType = cfg.returnType;
    out.entryBlock = cfg.entryBlock; out.origin = cfg.origin;
    const auto locals = collectPromotableLocals(cfg);
    for (const auto &[local, _] : locals) out.promotedLocals.push_back(local);
    stats.functions = 1; stats.blocks = cfg.blocks.size();
    stats.promotableLocals = locals.size(); stats.promotedLocals = locals.size();

    int nextValue = 0;
    std::map<LocalId, std::vector<ValueId>> stacks;
    for (const auto &param : cfg.params) {
      ValueId value{nextValue++};
      out.parameters.push_back({value, param.id, param.type});
      if (locals.contains(param.id)) stacks[param.id].push_back(value);
    }
    for (const auto &source : cfg.blocks) {
      SSABlock block; block.id = source->id; block.label = source->label;
      block.predecessors = source->predecessors; block.successors = source->successors;
      out.blocks.push_back(std::move(block));
    }

    const DominatorTree dom = DominatorAnalysis::run(cfg);
    const DominanceFrontier frontier = DominanceFrontierAnalysis::run(cfg, dom);
    const PhiPlan plan = placePhis(locals, frontier);
    std::map<int, SSABlock *> blocks;
    for (auto &block : out.blocks) blocks.emplace(block.id.index, &block);
    for (const auto &[blockIndex, phiLocals] : plan) {
      SSABlock &block = *blocks.at(blockIndex);
      for (const auto &local : phiLocals) {
        const auto &info = locals.at(local);
        block.phis.push_back({ValueId{nextValue++}, local, info.type, {}, {}});
      }
    }

    std::map<int, ValueId> valueMap;
    rename(cfg, out, dom, cfg.entryBlock, locals, stacks, valueMap, nextValue, stats);
    stats.values = static_cast<size_t>(nextValue);
    for (const auto &block : out.blocks) stats.phiNodes += block.phis.size();
    return out;
  }

  static LocalMap collectPromotableLocals(const CFGFunction &cfg) {
    LocalMap locals;
    for (const auto &param : cfg.params) if (isScalar(param.type)) locals.emplace(param.id, LocalInfo{param.type, {}});
    // StoreLocal intentionally carries no result type in CFG.  Learn each
    // local's declared type from loads before collecting store definitions.
    for (const auto &block : cfg.blocks) for (const auto &inst : block->instructions)
      if (inst.kind == CFGInstructionKind::LoadLocal && isScalar(inst.type))
        locals[inst.local].type = inst.type;
    for (const auto &block : cfg.blocks) for (const auto &inst : block->instructions) {
      if ((inst.kind == CFGInstructionKind::LoadLocal && isScalar(inst.type)) ||
          (inst.kind == CFGInstructionKind::StoreLocal && locals.contains(inst.local) && isScalar(locals.at(inst.local).type))) {
        auto &info = locals[inst.local]; if (info.type.kind == LangTypeKind::Unknown && inst.kind == CFGInstructionKind::LoadLocal) info.type = inst.type;
        if (inst.kind == CFGInstructionKind::StoreLocal) info.definitions.push_back(block->id);
      }
    }
    for (auto &[_, info] : locals) { std::sort(info.definitions.begin(), info.definitions.end()); info.definitions.erase(std::unique(info.definitions.begin(), info.definitions.end()), info.definitions.end()); }
    // CFG does not yet carry lexical lifetime ranges. A local first defined in
    // an inner region must remain memory-backed: otherwise IDF could place a
    // phi outside its source scope. Parameters and entry-defined locals are
    // safe scalar promotion candidates in the current IR.
    for (auto it=locals.begin(); it!=locals.end();) {
      const bool entryDefined=std::find(it->second.definitions.begin(),it->second.definitions.end(),cfg.entryBlock)!=it->second.definitions.end();
      const bool parameter=std::any_of(cfg.params.begin(),cfg.params.end(),[&](const CoreParam&p){return p.id==it->first;});
      if (!entryDefined && !parameter) it=locals.erase(it); else ++it;
    }
    return locals;
  }

  static PhiPlan placePhis(const LocalMap &locals, const DominanceFrontier &frontier) {
    PhiPlan result;
    for (const auto &[local, info] : locals) {
      std::set<int> definitions, placed; std::vector<BlockId> work;
      for (BlockId block : info.definitions) { definitions.insert(block.index); work.push_back(block); }
      for (size_t i = 0; i < work.size(); ++i) for (BlockId join : frontier.frontier(work[i])) {
        if (!placed.insert(join.index).second) continue;
        result[join.index].push_back(local);
        if (!definitions.contains(join.index)) work.push_back(join);
      }
    }
    return result;
  }

  static SSAOperand operand(const CFGOperand &source, const std::map<int, ValueId> &values) {
    if (source.kind == CFGOperandKind::Constant) return source.constantValue;
    if (source.kind == CFGOperandKind::Value) return values.at(source.valueId.index);
    throw std::runtime_error("SSA builder encountered an unpromoted local operand");
  }

  static SSATerminator terminator(const Terminator &source, const std::map<int, ValueId> &values) {
    return std::visit([&](auto &&term) -> SSATerminator {
      using T = std::decay_t<decltype(term)>;
      if constexpr (std::is_same_v<T, JumpTerminator>) return {SSATerminator::Jump{term.target}};
      else if constexpr (std::is_same_v<T, BranchTerminator>) return {SSATerminator::Branch{operand(term.condition, values), term.thenTarget, term.elseTarget}};
      else if constexpr (std::is_same_v<T, ReturnTerminator>) { std::optional<SSAOperand> value; if (term.value) value = operand(*term.value, values); return {SSATerminator::Return{value}}; }
      else return {SSATerminator::Unreachable{}};
    }, source);
  }

  static void rename(const CFGFunction &cfg, SSAFunction &out, const DominatorTree &dom, BlockId id,
                     const LocalMap &locals, std::map<LocalId, std::vector<ValueId>> &stacks,
                     std::map<int, ValueId> &values, int &next, SSAStatistics &stats) {
    const BasicBlock *source = cfg.getBlock(id); SSABlock &target = out.blocks.at(static_cast<size_t>(id.index));
    std::map<LocalId, size_t> pushes;
    for (const auto &phi : target.phis) { stacks[phi.originalLocal].push_back(phi.result); ++pushes[phi.originalLocal]; }
    auto current = [&](const LocalId &local) -> ValueId { auto it = stacks.find(local); if (it == stacks.end() || it->second.empty()) throw std::runtime_error("SSA use before definition: " + local.str()); return it->second.back(); };
    for (const auto &inst : source->instructions) {
      if (inst.kind == CFGInstructionKind::LoadLocal && locals.contains(inst.local)) { values[inst.dst->index] = current(inst.local); ++stats.loadsRemoved; continue; }
      if (inst.kind == CFGInstructionKind::StoreLocal && locals.contains(inst.local)) {
        const SSAOperand stored = operand(inst.op1, values);
        ValueId definition;
        if (std::holds_alternative<ValueId>(stored)) {
          definition = std::get<ValueId>(stored);
        } else {
          definition = ValueId{next++};
          SSAInstruction constant;
          constant.kind = SSAInstructionKind::Constant;
          constant.result = definition;
          constant.type = locals.at(inst.local).type;
          constant.constantValue = std::get<CoreConstantVal>(stored);
          constant.origin = inst.origin;
          target.instructions.push_back(std::move(constant));
        }
        stacks[inst.local].push_back(definition);
        ++pushes[inst.local]; ++stats.storesRemoved; continue;
      }
      SSAInstruction lowered; lowered.type = inst.type; lowered.origin = inst.origin; lowered.callee = inst.callee; lowered.local = inst.local;
      if (inst.dst) { lowered.result = ValueId{next++}; values[inst.dst->index] = *lowered.result; }
      switch (inst.kind) {
      case CFGInstructionKind::Constant: lowered.kind = SSAInstructionKind::Constant; lowered.constantValue = inst.constantVal; break;
      case CFGInstructionKind::Copy: lowered.kind = SSAInstructionKind::Copy; lowered.operand1 = operand(inst.op1, values); break;
      case CFGInstructionKind::Unary: lowered.kind = SSAInstructionKind::Unary; lowered.unaryOp = inst.unOp; lowered.operand1 = operand(inst.op1, values); break;
      case CFGInstructionKind::Binary: lowered.kind = SSAInstructionKind::Binary; lowered.binaryOp = inst.binOp; lowered.operand1 = operand(inst.op1, values); lowered.operand2 = operand(inst.op2, values); break;
      case CFGInstructionKind::Call: lowered.kind = SSAInstructionKind::Call; for (const auto &arg : inst.callArgs) lowered.callArgs.push_back(operand(arg, values)); break;
      case CFGInstructionKind::LoadLocal: lowered.kind = SSAInstructionKind::LoadLocal; break;
      case CFGInstructionKind::StoreLocal: lowered.kind = SSAInstructionKind::StoreLocal; lowered.operand1 = operand(inst.op1, values); break;
      case CFGInstructionKind::LoadIndex: lowered.kind = SSAInstructionKind::LoadIndex; lowered.operand1 = operand(inst.op1, values); break;
      case CFGInstructionKind::StoreIndex: lowered.kind = SSAInstructionKind::StoreIndex; lowered.operand1 = operand(inst.op1, values); lowered.operand2 = operand(inst.op2, values); break;
      default: throw std::runtime_error("unexpected local operation during SSA renaming"); }
      target.instructions.push_back(std::move(lowered));
      ++stats.instructions;
    }
    target.terminator = terminator(*source->terminator, values);
    for (BlockId successor : source->successors) for (auto &phi : out.blocks.at(static_cast<size_t>(successor.index)).phis) phi.incoming.push_back({id, current(phi.originalLocal)});
    for (BlockId child : dom.children(id)) rename(cfg, out, dom, child, locals, stacks, values, next, stats);
    for (const auto &[local, count] : pushes)
      for (size_t remaining = count; remaining > 0; --remaining) stacks[local].pop_back();
  }
};

} // namespace nanopass
