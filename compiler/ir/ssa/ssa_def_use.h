#pragma once

#include <algorithm>
#include <optional>
#include <unordered_map>

#include "ir/ssa/ssa_ir.h"

namespace nanopass {
enum class SSADefinitionKind { Parameter, Phi, Instruction };
enum class SSAUseKind { Instruction, Phi, Branch, Return };
struct SSAValueDefinition { BlockId block; SSADefinitionKind kind; size_t index; };
struct SSAValueUse { BlockId block; SSAUseKind kind; size_t index; size_t operand; };

class SSADefUseIndex {
  std::unordered_map<int, SSAValueDefinition> definitions_;
  std::unordered_map<int, std::vector<SSAValueUse>> uses_;
public:
  void rebuild(const SSAFunction &fn) {
    definitions_.clear(); uses_.clear();
    for (size_t i=0;i<fn.parameters.size();++i) definitions_.emplace(fn.parameters[i].value.index, SSAValueDefinition{fn.entryBlock,SSADefinitionKind::Parameter,i});
    for (const auto &block:fn.blocks) {
      for(size_t i=0;i<block.phis.size();++i) { definitions_.emplace(block.phis[i].result.index,SSAValueDefinition{block.id,SSADefinitionKind::Phi,i}); for(size_t j=0;j<block.phis[i].incoming.size();++j) uses_[block.phis[i].incoming[j].value.index].push_back({block.id,SSAUseKind::Phi,i,j}); }
      for(size_t i=0;i<block.instructions.size();++i) { const auto &in=block.instructions[i]; if(in.result) definitions_.emplace(in.result->index,SSAValueDefinition{block.id,SSADefinitionKind::Instruction,i}); addInstructionUses(block.id,in,i); }
      std::visit([&](auto &&t){using T=std::decay_t<decltype(t)>; if constexpr(std::is_same_v<T,SSATerminator::Branch>) add(t.condition, {block.id,SSAUseKind::Branch,0,0}); else if constexpr(std::is_same_v<T,SSATerminator::Return>) if(t.value) add(*t.value,{block.id,SSAUseKind::Return,0,0});},block.terminator.value);
    }
  }
  std::optional<SSAValueDefinition> definitionOf(ValueId v) const { auto it=definitions_.find(v.index); return it==definitions_.end()?std::nullopt:std::optional<SSAValueDefinition>(it->second); }
  const std::vector<SSAValueUse>& usesOf(ValueId v) const { static const std::vector<SSAValueUse> empty; auto it=uses_.find(v.index); return it==uses_.end()?empty:it->second; }
  size_t numberOfUses(ValueId v) const { return usesOf(v).size(); }
private:
  void add(const SSAOperand &op, SSAValueUse use) { if(std::holds_alternative<ValueId>(op)) uses_[std::get<ValueId>(op).index].push_back(use); }
  void addInstructionUses(BlockId b,const SSAInstruction &in,size_t i) { if(in.kind==SSAInstructionKind::Copy||in.kind==SSAInstructionKind::Unary||in.kind==SSAInstructionKind::LoadIndex||in.kind==SSAInstructionKind::StoreLocal) add(in.operand1,{b,SSAUseKind::Instruction,i,0}); else if(in.kind==SSAInstructionKind::Binary||in.kind==SSAInstructionKind::StoreIndex){add(in.operand1,{b,SSAUseKind::Instruction,i,0});add(in.operand2,{b,SSAUseKind::Instruction,i,1});} else if(in.kind==SSAInstructionKind::Call) for(size_t j=0;j<in.callArgs.size();++j)add(in.callArgs[j],{b,SSAUseKind::Instruction,i,j}); }
};

inline void replaceAllUses(SSAFunction &fn, ValueId oldValue, ValueId newValue) {
  auto replace=[&](SSAOperand &op){if(std::holds_alternative<ValueId>(op)&&std::get<ValueId>(op)==oldValue)op=newValue;};
  for(auto &b:fn.blocks){for(auto &p:b.phis)for(auto &in:p.incoming)if(in.value==oldValue)in.value=newValue; for(auto &i:b.instructions){replace(i.operand1);replace(i.operand2);for(auto &a:i.callArgs)replace(a);} std::visit([&](auto &t){using T=std::decay_t<decltype(t)>;if constexpr(std::is_same_v<T,SSATerminator::Branch>)replace(t.condition);else if constexpr(std::is_same_v<T,SSATerminator::Return>)if(t.value)replace(*t.value);},b.terminator.value);}
}
} // namespace nanopass
