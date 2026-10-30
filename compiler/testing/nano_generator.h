#pragma once

#include <cstdint>
#include <algorithm>
#include <random>
#include <string>

namespace nanopass {

struct GeneratorOptions {
  uint64_t seed=1;
  size_t maxDepth=4;
  size_t maxStatements=8;
  size_t maxFunctions=3;
  size_t maxCallDepth=2;
  size_t maxLoopNesting=1;
  size_t maxSourceBytes=8192;
};
struct GeneratedFeatures {
  bool arithmetic=false, comparisons=false, conditionals=false, loops=false;
  bool functions=false, nestedCalls=false, mutableLocals=false, returns=false;
};
struct GeneratedProgram { uint64_t seed=0; size_t caseIndex=0; std::string source; GeneratedFeatures features; };

// A deliberately conservative typed grammar subset.  Every produced source
// has an int main, initialized locals, and bounded loops; it is reparsed by
// DifferentialRunner before it can count as a valid fuzz comparison.
class NanoProgramGenerator {
  std::mt19937_64 rng_; GeneratorOptions options_;
  static uint64_t caseSeed(uint64_t seed, size_t index) {
    uint64_t value=seed+0x9e3779b97f4a7c15ull+static_cast<uint64_t>(index);
    value=(value^(value>>30))*0xbf58476d1ce4e5b9ull;
    value=(value^(value>>27))*0x94d049bb133111ebull;
    return value^(value>>31);
  }
  int number(int limit=20) { return static_cast<int>(rng_()%static_cast<uint64_t>(limit)); }
  std::string expression(size_t depth, const std::string &name) {
    if(depth==0) return number()%2 ? name : std::to_string(number());
    static constexpr const char* ops[]={"+","-","*"};
    return "("+expression(depth-1,name)+" "+ops[number(3)]+" "+expression(depth-1,name)+")";
  }
public:
  explicit NanoProgramGenerator(GeneratorOptions options={}) : rng_(options.seed), options_(options) {
    options_.maxSourceBytes=std::max<size_t>(options_.maxSourceBytes,96);
  }
  GeneratedProgram generate(size_t caseIndex) {
    // Case index is part of the PRNG identity, so a persisted (seed, case)
    // reproduces the exact source without replaying preceding cases.
    rng_.seed(caseSeed(options_.seed,caseIndex));
    const int bound=1+number(8);
    const size_t depth=std::min<size_t>(options_.maxDepth,5);
    const bool directCalls=options_.maxFunctions>=2&&options_.maxCallDepth>=1;
    const bool nestedCalls=options_.maxFunctions>=3&&options_.maxCallDepth>=2;
    const bool loop=options_.maxLoopNesting>=1&&options_.maxStatements>=6;
    const std::string direct=directCalls?"increment(i)":"(i + 1)";
    const std::string nested=nestedCalls?"relay(i)":direct;
    std::string functions;
    if(directCalls) functions+="int increment(int value) { return value + 1; }\n";
    if(nestedCalls) functions+="int relay(int value) { return increment(value); }\n";
    std::string body="  int i = 0;\n  int total = "+std::to_string(number())+";\n";
    if(loop) body+="  while (i < "+std::to_string(bound)+") {\n    if ((i % 2) == 0) { total = total + "+nested+" + "+expression(depth,"i")+"; }\n    else { total = total + "+direct+" + "+expression(depth,"i")+"; }\n    i = i + 1;\n  }\n";
    else body+="  total = total + "+expression(std::min<size_t>(depth,1),"i")+";\n";
    std::string source=functions+"int main() {\n"+body+"  print(total);\n  return total;\n}\n";
    // Produce a valid reduced form for deliberately tiny source-size budgets.
    if(source.size()>options_.maxSourceBytes)
      source="int main() { int value = "+std::to_string(number())+"; print(value); return value; }\n";
    GeneratedFeatures features;
    features.arithmetic=true; features.comparisons=source.find('%')!=std::string::npos;
    features.conditionals=source.find("if (")!=std::string::npos;
    features.loops=source.find("while (")!=std::string::npos;
    features.functions=source.find("int increment(")!=std::string::npos;
    features.nestedCalls=source.find("relay(")!=std::string::npos;
    features.mutableLocals=source.find("i = i + 1")!=std::string::npos; features.returns=true;
    return {options_.seed,caseIndex,std::move(source),features};
  }
};

} // namespace nanopass
