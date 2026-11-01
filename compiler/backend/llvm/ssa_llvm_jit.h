#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "ir/ssa/ssa_ir.h"
#include "profile/runtime_profile.h"

namespace nanopass::llvm_backend {

struct JITExecutionResult {
  std::int64_t returnValue = 0;
  std::string stdoutText;
  std::string error;
};

// Minimal ORCv2 execution boundary.  The caller retains ownership of SSA;
// lowering, module verification, and ORC ownership happen inside this call.
JITExecutionResult executeWithLLJIT(const SSAProgram &program);

enum class CompilationTier { Tier0, Tier1 };

inline const char *compilationTierName(CompilationTier tier) {
  return tier == CompilationTier::Tier0 ? "tier0" : "tier1";
}

struct TierState {
  CompilationTier activeTier = CompilationTier::Tier0;
  uint64_t callCount = 0;
  bool recompilationRequested = false;
  bool recompilationComplete = false;
};

enum class AdaptiveJITEventKind { Tier0Compiled, HotThresholdReached,
                                  Tier1CompilationStarted, Tier1Installed,
                                  Tier1Rejected };

struct AdaptiveJITEvent {
  AdaptiveJITEventKind kind;
  std::string function;
  uint64_t calls = 0;
  std::string detail;
};

struct AdaptiveJITConfig {
  uint64_t hotFunctionCallThreshold = 1000;
  bool forceTier0 = false;
  bool forceTier1 = false;
};

struct AdaptiveJITExecutionResult {
  JITExecutionResult execution;
  profile::ProgramRuntimeProfile runtimeProfile;
  std::map<std::string, TierState> tiers;
  std::vector<AdaptiveJITEvent> events;
};

// Uses ORC indirect stubs as stable public function entries. Tier-0 code can
// finish on its existing stack frame; a Tier-1 installation redirects only
// future direct Nano calls through the matching stub.
AdaptiveJITExecutionResult executeAdaptivelyWithLLJIT(
    const SSAProgram &program, const AdaptiveJITConfig &config = {});

} // namespace nanopass::llvm_backend
