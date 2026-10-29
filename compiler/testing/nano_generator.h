#pragma once

#include <cstdint>
#include <random>
#include <string>

namespace nanopass {

struct GeneratorOptions { uint64_t seed=1; size_t maxDepth=4; size_t maxStatements=8; };
struct GeneratedProgram { uint64_t seed=0; size_t caseIndex=0; std::string source; };

// A deliberately conservative typed grammar subset.  Every produced source
// has an int main, initialized locals, and bounded loops; it is reparsed by
// DifferentialRunner before it can count as a valid fuzz comparison.
class NanoProgramGenerator {
  std::mt19937_64 rng_; GeneratorOptions options_;
  int number(int limit=20) { return static_cast<int>(rng_()%static_cast<uint64_t>(limit)); }
  std::string expression(size_t depth, const std::string &name) {
    if(depth==0) return number()%2 ? name : std::to_string(number());
    static constexpr const char* ops[]={"+","-","*"};
    return "("+expression(depth-1,name)+" "+ops[number(3)]+" "+expression(depth-1,name)+")";
  }
public:
  explicit NanoProgramGenerator(GeneratorOptions options={}) : rng_(options.seed), options_(options) {}
  GeneratedProgram generate(size_t caseIndex) {
    const int bound=1+number(8); const int increments=1+number(4); std::string source="int helper(int x) { return x + "+std::to_string(increments)+"; }\nint main() { int i = 0; int total = "+std::to_string(number())+"; while (i < "+std::to_string(bound)+") { if ((i % 2) == 0) { total = total + helper("+expression(options_.maxDepth,"i")+"); } else { total = total + "+expression(options_.maxDepth,"i")+"; } i = i + 1; } print(total); return total; }\n"; return {options_.seed,caseIndex,std::move(source)};
  }
};

} // namespace nanopass
