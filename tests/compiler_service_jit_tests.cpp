#include <algorithm>
#include <iostream>
#include <string>

#include "service/compiler_service.h"

namespace {
int assertions = 0;
#define ASSERT(condition) do { ++assertions; if (!(condition)) { std::cerr << "assertion failed: " #condition " at " << __FILE__ << ':' << __LINE__ << '\n'; return 1; } } while (false)

nanopass::service::CompileRequestV1 request(std::string source) {
  nanopass::service::CompileRequestV1 result;
  result.source = std::move(source);
  result.optimizationLevel = 1;
  result.include.ssa = true;
  result.include.optimizedSsa = true;
  result.include.llvm = true;
  result.include.graphs = true;
  result.include.provenance = true;
  return result;
}
}

int main() {
  using namespace nanopass::service;
  CompilerService service;
  const auto source = R"(int step(int x) { return x + 1; }
int main() { int i = 0; int total = 0; while (i < 12) { total = step(total); i = i + 1; } print(total); return total; })";
  const auto compiled = service.compile(request(source));
  ASSERT(compiled.success);
  bool llvm = false; for (const auto &snapshot : compiled.snapshots) llvm |= snapshot.stageId == "llvm" && !snapshot.text.empty();
  ASSERT(llvm);
  ASSERT(!compiled.values.empty());
  ASSERT(!compiled.values.front().uses.empty());

  RunRequestV1 interpreted; interpreted.compile = request(source); interpreted.mode = ExecutionMode::SSAInterpreter;
  const auto ssa = service.run(interpreted);
  ASSERT(ssa.success); ASSERT(ssa.stdoutText == "12\n"); ASSERT(ssa.returnValue == "12"); ASSERT(!ssa.runtimeProfileJson.empty());

  RunRequestV1 jit; jit.compile = request(source); jit.mode = ExecutionMode::JIT;
  const auto native = service.run(jit);
  ASSERT(native.success); ASSERT(native.stdoutText == "12\n"); ASSERT(native.returnValue == "12");

  RunRequestV1 adaptive; adaptive.compile = request(source); adaptive.mode = ExecutionMode::AdaptiveJIT;
  const auto tiered = service.run(adaptive);
  ASSERT(tiered.success); ASSERT(tiered.stdoutText == "12\n"); ASSERT(!tiered.runtimeProfileJson.empty()); ASSERT(!tiered.adaptiveEvents.empty());
  const auto json = runResponseJson(tiered);
  ASSERT(json.find("adaptiveEvents") != std::string::npos); ASSERT(json.find("runtimeProfile") != std::string::npos);
  const auto correctness = service.checkCorrectness(request(source));
  ASSERT(correctness.success); ASSERT(correctness.equivalent); ASSERT(correctness.checks.size() == 7);
  ASSERT(std::all_of(correctness.checks.begin(), correctness.checks.end(), [](const auto &check) { return check.passed; }));
  ASSERT(correctnessResponseJson(correctness).find("Adaptive JIT") != std::string::npos);
  std::cout << "compiler service JIT: " << assertions << "/" << assertions << " assertions passed\n";
}
