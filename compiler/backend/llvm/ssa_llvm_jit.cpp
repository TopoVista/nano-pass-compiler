#include "backend/llvm/ssa_llvm_jit.h"

#include <algorithm>

#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/TargetSelect.h>

#include "backend/llvm/ssa_llvm_lowering.h"

namespace nanopass::llvm_backend {
namespace {
std::string errorText(llvm::Error error) { return llvm::toString(std::move(error)); }
}

JITExecutionResult executeWithLLJIT(const SSAProgram &program) {
  if (llvm::InitializeNativeTarget()) return {0, "could not initialize native LLVM target"};
  if (llvm::InitializeNativeTargetAsmPrinter()) return {0, "could not initialize native LLVM asm printer"};
  if (llvm::InitializeNativeTargetAsmParser()) return {0, "could not initialize native LLVM asm parser"};
  auto jitOrError = llvm::orc::LLJITBuilder().create();
  if (!jitOrError) return {0, errorText(jitOrError.takeError())};
  auto jit = std::move(*jitOrError);
  auto generator = llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
      jit->getDataLayout().getGlobalPrefix());
  if (!generator) return {0, errorText(generator.takeError())};
  jit->getMainJITDylib().addGenerator(std::move(*generator));

  auto context = std::make_unique<llvm::LLVMContext>();
  try {
    auto result = SSALLVMLowering(*context).lower(program, "nano_jit_module");
    if (auto error = jit->addIRModule(llvm::orc::ThreadSafeModule(
            std::move(result.module), std::move(context))))
      return {0, errorText(std::move(error))};
  } catch (const std::exception &error) {
    return {0, error.what()};
  }
  auto symbol = jit->lookup("main");
  if (!symbol) return {0, errorText(symbol.takeError())};
  const auto main = std::find_if(program.functions.begin(), program.functions.end(),
                                 [](const SSAFunction &function) { return function.name == "main"; });
  if (main == program.functions.end()) return {0, "JIT module has no @main"};
  if (main->returnType.kind == LangTypeKind::Void) {
    using Entry = void (*)(); symbol->toPtr<Entry>()(); return {0, {}};
  }
  if (main->returnType.kind != LangTypeKind::Integer) return {0, "JIT @main must return an integer or void"};
  switch (main->returnType.bitWidth) {
  case 8: { using Entry = std::int8_t (*)(); return {symbol->toPtr<Entry>()(), {}}; }
  case 16: { using Entry = std::int16_t (*)(); return {symbol->toPtr<Entry>()(), {}}; }
  case 32: { using Entry = std::int32_t (*)(); return {symbol->toPtr<Entry>()(), {}}; }
  case 64: { using Entry = std::int64_t (*)(); return {symbol->toPtr<Entry>()(), {}}; }
  default: return {0, "JIT @main has unsupported integer width"};
  }
}

} // namespace nanopass::llvm_backend
