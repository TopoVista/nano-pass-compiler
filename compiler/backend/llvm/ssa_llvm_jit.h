#pragma once

#include <cstdint>
#include <string>

#include "ir/ssa/ssa_ir.h"

namespace nanopass::llvm_backend {

struct JITExecutionResult {
  std::int64_t returnValue = 0;
  std::string error;
};

// Minimal ORCv2 execution boundary.  The caller retains ownership of SSA;
// lowering, module verification, and ORC ownership happen inside this call.
JITExecutionResult executeWithLLJIT(const SSAProgram &program);

} // namespace nanopass::llvm_backend
