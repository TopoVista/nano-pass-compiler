#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include "ir/ssa/ssa_ir.h"

namespace nanopass::llvm_backend {

// A structured backend failure.  Keeping the failing SSA identity here makes
// lowering errors actionable without leaking LLVM implementation details into
// frontend and middle-end diagnostics.
struct LoweringDiagnostic {
  std::string message;
  std::string function;
  std::optional<BlockId> block;
  std::optional<ValueId> value;
};

class LoweringError final : public std::runtime_error {
public:
  explicit LoweringError(LoweringDiagnostic diagnostic);
  const LoweringDiagnostic &diagnostic() const noexcept { return diagnostic_; }
private:
  LoweringDiagnostic diagnostic_;
};

struct LoweringStatistics {
  size_t functions = 0;
  size_t blocks = 0;
  size_t phis = 0;
  size_t instructions = 0;
};

struct LoweringResult {
  std::unique_ptr<llvm::Module> module;
  LoweringStatistics statistics;
};

// Lowers verified Nano SSA to a new module owned by the caller.  The supplied
// context must outlive the returned module.  This API never mutates SSA.
class SSALLVMLowering {
public:
  explicit SSALLVMLowering(llvm::LLVMContext &context) : context_(context) {}
  LoweringResult lower(const SSAProgram &program,
                       std::string moduleName = "nano_ssa_module") const;
  // Emit exactly one function body while retaining declarations for every
  // Nano direct callee.  The body may have a private implementation symbol;
  // this lets ORC dispatch stubs keep the public Nano symbol stable across
  // tier replacement without rewriting callers.
  LoweringResult lowerFunction(const SSAProgram &program,
                               std::string_view functionName,
                               std::string implementationName,
                               std::string moduleName = "nano_ssa_function") const;
private:
  llvm::LLVMContext &context_;
};

} // namespace nanopass::llvm_backend
