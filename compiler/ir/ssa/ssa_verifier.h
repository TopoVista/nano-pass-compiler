#pragma once

#include <algorithm>
#include <map>
#include <set>
#include <type_traits>

#include "ir/cfg/analysis/dominator_analysis.h"
#include "ir/ssa/ssa_def_use.h"

namespace nanopass {
struct SSAVerifierResult { bool ok=true; std::vector<std::string> errors; void error(std::string s){ok=false;errors.push_back(std::move(s));} };
class SSAVerifier {
public:
  SSAVerifierResult verify(const SSAFunction &fn) {
    SSAVerifierResult r; std::map<int,const SSABlock*> blocks;
    for(const auto &b:fn.blocks) { if(!blocks.emplace(b.id.index,&b).second) r.error("duplicate block "+b.id.str()); }
    if(!blocks.contains(fn.entryBlock.index)) {r.error("missing entry block");return r;}
    CFGFunction view; view.entryBlock=fn.entryBlock; for(const auto &b:fn.blocks){auto bb=std::make_unique<BasicBlock>();bb->id=b.id;bb->predecessors=b.predecessors;bb->successors=b.successors;view.blockIndexMap[b.id.index]=view.blocks.size();view.blocks.push_back(std::move(bb));}
    auto dom=DominatorAnalysis::run(view); SSADefUseIndex index; index.rebuild(fn);
    struct Definition { SSAValueDefinition where; LangType type; };
    std::map<int,Definition> defs; auto addDef=[&](ValueId v,SSAValueDefinition d,const LangType&t){if(!defs.emplace(v.index,Definition{d,t}).second)r.error("multiple definitions of "+v.str());};
    for(size_t i=0;i<fn.parameters.size();++i)addDef(fn.parameters[i].value,{fn.entryBlock,SSADefinitionKind::Parameter,i},fn.parameters[i].type);
    for(const auto &b:fn.blocks){std::set<int> expected;for(auto p:b.predecessors)expected.insert(p.index);if(expected.size()!=b.predecessors.size())r.error("duplicate CFG predecessor in "+b.id.str());for(size_t i=0;i<b.phis.size();++i){const auto&p=b.phis[i];addDef(p.result,{b.id,SSADefinitionKind::Phi,i},p.type);std::set<int> seen;for(auto in:p.incoming){if(!seen.insert(in.predecessor.index).second)r.error("duplicate phi predecessor in "+b.id.str());if(!expected.contains(in.predecessor.index))r.error("extra phi predecessor in "+b.id.str());}if(seen!=expected)r.error("phi predecessor coverage mismatch in "+b.id.str());}for(size_t i=0;i<b.instructions.size();++i)if(b.instructions[i].result)addDef(*b.instructions[i].result,{b.id,SSADefinitionKind::Instruction,i},b.instructions[i].type);}
    auto check=[&](ValueId v,BlockId use,size_t order,bool phiEdge,const LangType* expected=nullptr){auto it=defs.find(v.index);if(it==defs.end()){r.error("undefined use of "+v.str());return;}auto d=it->second.where;const bool truthiness=(expected&&((expected->isBool()&&it->second.type.isInt())||(expected->isInt()&&it->second.type.isBool())));if(expected&&expected->kind!=LangTypeKind::Unknown&&it->second.type.kind!=LangTypeKind::Unknown&&!truthiness&&!sameType(it->second.type,*expected))r.error("type mismatch for use of "+v.str());if(d.kind==SSADefinitionKind::Parameter)return;if(!dom.dominates(d.block,use)){r.error("non-dominating use of "+v.str());return;}if(!phiEdge&&d.block==use&&d.kind==SSADefinitionKind::Instruction&&d.index>=order)r.error("same-block use before definition of "+v.str());};
    auto operand=[&](const SSAOperand&o,BlockId b,size_t n,bool edge,const LangType* expected=nullptr){if(std::holds_alternative<ValueId>(o))check(std::get<ValueId>(o),b,n,edge,expected);};
    auto promoted=[&](const LocalId&local){return std::find(fn.promotedLocals.begin(),fn.promotedLocals.end(),local)!=fn.promotedLocals.end();};
    const LangType boolType = LangType::Bool();
    for(const auto &b:fn.blocks){for(const auto&p:b.phis)for(auto in:p.incoming)check(in.value,in.predecessor,0,true,&p.type);for(size_t i=0;i<b.instructions.size();++i){const auto&x=b.instructions[i];if((x.kind==SSAInstructionKind::LoadLocal||x.kind==SSAInstructionKind::StoreLocal)&&promoted(x.local))r.error("residual local memory operation for promoted "+x.local.str());switch(x.kind){case SSAInstructionKind::Copy:case SSAInstructionKind::Unary:case SSAInstructionKind::LoadIndex:case SSAInstructionKind::StoreLocal:operand(x.operand1,b.id,i,false);break;case SSAInstructionKind::Binary:case SSAInstructionKind::StoreIndex:operand(x.operand1,b.id,i,false);operand(x.operand2,b.id,i,false);break;case SSAInstructionKind::Call:for(const auto&a:x.callArgs)operand(a,b.id,i,false);break;default:break;}}std::visit([&](auto&&t){using T=std::decay_t<decltype(t)>;if constexpr(std::is_same_v<T,SSATerminator::Branch>)operand(t.condition,b.id,b.instructions.size(),false,&boolType);else if constexpr(std::is_same_v<T,SSATerminator::Return>)if(t.value)operand(*t.value,b.id,b.instructions.size(),false,&fn.returnType);},b.terminator.value);}
    return r;
  }
};
inline SSAVerifierResult verifySSA(const SSAFunction &fn){return SSAVerifier{}.verify(fn);}
} // namespace nanopass
