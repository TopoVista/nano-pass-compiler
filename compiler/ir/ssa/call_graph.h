#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "ir/ssa/ssa_ir.h"

namespace nanopass {

struct CallSite { std::string caller; std::string callee; BlockId block; size_t instructionIndex; };
struct CallGraph {
  std::map<std::string,std::vector<std::string>> outgoing;
  std::map<std::string,std::vector<std::string>> incoming;
  std::vector<CallSite> sites;
  std::set<std::string> recursiveFunctions;
  bool recursive(const std::string &function) const { return recursiveFunctions.contains(function); }
  const std::vector<std::string> &callees(const std::string &function) const { static const std::vector<std::string> none;auto it=outgoing.find(function);return it==outgoing.end()?none:it->second; }
  const std::vector<std::string> &callers(const std::string &function) const { static const std::vector<std::string> none;auto it=incoming.find(function);return it==incoming.end()?none:it->second; }
};
class CallGraphAnalysis { public: static CallGraph run(const SSAProgram &program) {
  CallGraph graph; std::set<std::string> names;for(const auto&function:program.functions){names.insert(function.name);graph.outgoing[function.name];for(const auto&block:function.blocks)for(size_t i=0;i<block.instructions.size();++i){const auto&instruction=block.instructions[i];if(instruction.kind!=SSAInstructionKind::Call)continue;graph.outgoing[function.name].push_back(instruction.callee);graph.incoming[instruction.callee].push_back(function.name);graph.sites.push_back({function.name,instruction.callee,block.id,i});}}
  for(auto&[_,list]:graph.outgoing){std::sort(list.begin(),list.end());list.erase(std::unique(list.begin(),list.end()),list.end());}for(auto&[_,list]:graph.incoming){std::sort(list.begin(),list.end());list.erase(std::unique(list.begin(),list.end()),list.end());}
  std::map<std::string,int> index,low;std::set<std::string> active;std::vector<std::string> stack;int next=0;std::function<void(const std::string&)> visit=[&](const std::string&node){index[node]=low[node]=next++;stack.push_back(node);active.insert(node);for(const auto&nextNode:graph.callees(node)){if(!names.contains(nextNode))continue;if(!index.contains(nextNode)){visit(nextNode);low[node]=std::min(low[node],low[nextNode]);}else if(active.contains(nextNode))low[node]=std::min(low[node],index[nextNode]);}if(low[node]!=index[node])return;std::vector<std::string> component;while(true){auto member=stack.back();stack.pop_back();active.erase(member);component.push_back(member);if(member==node)break;}if(component.size()>1)graph.recursiveFunctions.insert(component.begin(),component.end());else if(std::find(graph.callees(node).begin(),graph.callees(node).end(),node)!=graph.callees(node).end())graph.recursiveFunctions.insert(node);};
  for(const auto&name:names)if(!index.contains(name))visit(name);return graph;
} };

} // namespace nanopass
