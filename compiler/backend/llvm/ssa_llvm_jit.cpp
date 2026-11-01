#include "backend/llvm/ssa_llvm_jit.h"

#include <algorithm>
#include <memory>
#include <optional>

#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/IndirectionUtils.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/TargetSelect.h>

#include "backend/llvm/ssa_llvm_lowering.h"
#include "backend/llvm/llvm_runtime.h"
#include "ir/ssa/call_graph.h"
#include "ir/ssa/ssa_verifier.h"
#include "optimizer/advanced_optimizer.h"
#include "optimizer/ssa_optimizer.h"

namespace nanopass::llvm_backend {
namespace {
std::string errorText(llvm::Error error) { return llvm::toString(std::move(error)); }

class AdaptiveSession;
thread_local AdaptiveSession *activeAdaptiveSession = nullptr;

extern "C" void nano_adaptive_function_entry(const char *name);

void insertFunctionEntryHook(llvm::Module &module, llvm::Function &function,
                             const std::string &nanoName) {
  auto &context = module.getContext();
  auto *i8Pointer = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(context));
  auto hook = module.getOrInsertFunction("nano_adaptive_function_entry",
      llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i8Pointer}, false));
  llvm::IRBuilder<> builder(&*function.getEntryBlock().getFirstInsertionPt());
  builder.CreateCall(hook, {builder.CreateGlobalStringPtr(nanoName)});
}

class AdaptiveSession {
  const SSAProgram &source_;
  AdaptiveJITConfig config_;
  std::unique_ptr<llvm::orc::LLJIT> jit_;
  std::unique_ptr<llvm::orc::IndirectStubsManager> stubs_;
  CallGraph callGraph_;
  AdaptiveJITExecutionResult result_;
  std::map<std::string, size_t> generation_;
  std::optional<std::string> fatalError_;

  void event(AdaptiveJITEventKind kind, const std::string &function,
             uint64_t calls = 0, std::string detail = {}) {
    result_.events.push_back({kind, function, calls, std::move(detail)});
  }

  const SSAFunction *function(std::string_view name) const {
    const auto found = std::find_if(source_.functions.begin(), source_.functions.end(),
        [&](const SSAFunction &candidate) { return candidate.name == name; });
    return found == source_.functions.end() ? nullptr : &*found;
  }

  bool createPublicStubs(std::string &error) {
    auto factory = llvm::orc::createLocalIndirectStubsManagerBuilder(jit_->getTargetTriple());
    stubs_ = factory();
    llvm::orc::SymbolMap symbols;
    for (const auto &sourceFunction : source_.functions) {
      if (auto failure = stubs_->createStub(sourceFunction.name, llvm::orc::ExecutorAddr(),
                                            llvm::JITSymbolFlags::Exported)) {
        error = errorText(std::move(failure)); return false;
      }
      const auto stub = stubs_->findStub(sourceFunction.name, true);
      if (stub.getAddress().isNull()) { error = "ORC failed to create dispatch stub for @" + sourceFunction.name; return false; }
      symbols[jit_->getExecutionSession().intern(sourceFunction.name)] = stub;
      result_.tiers.emplace(sourceFunction.name, TierState{});
    }
    if (auto failure = jit_->getMainJITDylib().define(llvm::orc::absoluteSymbols(std::move(symbols)))) {
      error = errorText(std::move(failure)); return false;
    }
    return true;
  }

  SSAProgram tier1Program(std::string_view name) const {
    SSAProgram tier1 = source_;
    const auto found = std::find_if(tier1.functions.begin(), tier1.functions.end(),
        [&](const SSAFunction &candidate) { return candidate.name == name; });
    if (found == tier1.functions.end()) return tier1;
    opt::SSAOptimizationContext context;
    auto pipeline = opt::buildPipeline(3);
    pipeline.add(opt::makeAdvancedPass("loop-simplify"));
    pipeline.add(opt::makeAdvancedPass("licm"));
    pipeline.add(opt::makeAdvancedPass("strength-reduce"));
    pipeline.run(*found, context);
    return tier1;
  }

  bool compileAndInstall(const std::string &name, CompilationTier tier,
                         std::string &error) {
    const SSAFunction *sourceFunction = function(name);
    if (!sourceFunction) { error = "unknown adaptive function @" + name; return false; }
    SSAProgram candidate = tier == CompilationTier::Tier0 ? source_ : tier1Program(name);
    const auto selected = std::find_if(candidate.functions.begin(), candidate.functions.end(),
        [&](const SSAFunction &item) { return item.name == name; });
    if (selected == candidate.functions.end() || !verifySSA(*selected).ok) {
      error = "SSA verifier rejected " + std::string(compilationTierName(tier)) + " candidate for @" + name;
      return false;
    }
    const std::string implementation = "__nano_adaptive_" + name + "_" +
        compilationTierName(tier) + "_" + std::to_string(generation_[name]++);
    auto context = std::make_unique<llvm::LLVMContext>();
    try {
      auto lowered = SSALLVMLowering(*context).lowerFunction(
          candidate, name, implementation, "nano_adaptive_" + name);
      llvm::Function *body = lowered.module->getFunction(implementation);
      if (!body) { error = "adaptive lowering did not produce implementation for @" + name; return false; }
      insertFunctionEntryHook(*lowered.module, *body, name);
      if (auto failure = jit_->addIRModule(llvm::orc::ThreadSafeModule(
              std::move(lowered.module), std::move(context)))) {
        error = errorText(std::move(failure)); return false;
      }
      auto address = jit_->lookup(implementation);
      if (!address) { error = errorText(address.takeError()); return false; }
      if (auto failure = stubs_->updatePointer(name, *address)) {
        error = errorText(std::move(failure)); return false;
      }
    } catch (const std::exception &failure) {
      error = failure.what(); return false;
    }
    result_.tiers[name].activeTier = tier;
    return true;
  }

public:
  AdaptiveSession(const SSAProgram &source, AdaptiveJITConfig config)
      : source_(source), config_(config), callGraph_(CallGraphAnalysis::run(source)) {
    result_.runtimeProfile.programFingerprint = profile::stableProgramFingerprint(source);
    // Native adaptive instrumentation currently observes function entries in
    // generated code. The complete SSA block/edge profile remains available
    // through `nano profile`; marking this partial prevents accidental PGO use.
    result_.runtimeProfile.status = profile::ProfileStatus::Partial;
  }

  AdaptiveJITExecutionResult run() {
    if (config_.forceTier0 && config_.forceTier1) {
      result_.execution.error = "--force-tier0 and --force-tier1 are mutually exclusive";
      return std::move(result_);
    }
    if (llvm::InitializeNativeTarget() || llvm::InitializeNativeTargetAsmPrinter() ||
        llvm::InitializeNativeTargetAsmParser()) {
      result_.execution.error = "could not initialize native LLVM target";
      return std::move(result_);
    }
    auto jitOrError = llvm::orc::LLJITBuilder().create();
    if (!jitOrError) { result_.execution.error = errorText(jitOrError.takeError()); return std::move(result_); }
    jit_ = std::move(*jitOrError);
    auto generator = llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
        jit_->getDataLayout().getGlobalPrefix());
    if (!generator) { result_.execution.error = errorText(generator.takeError()); return std::move(result_); }
    jit_->getMainJITDylib().addGenerator(std::move(*generator));
    std::string error;
    if (!createPublicStubs(error)) { result_.execution.error = error; return std::move(result_); }
    for (const auto &sourceFunction : source_.functions) {
      if (!compileAndInstall(sourceFunction.name, CompilationTier::Tier0, error)) {
        result_.execution.error = "tier0 compile for @" + sourceFunction.name + " failed: " + error;
        return std::move(result_);
      }
      event(AdaptiveJITEventKind::Tier0Compiled, sourceFunction.name);
    }
    if (config_.forceTier1) {
      for (const auto &sourceFunction : source_.functions) {
        if (callGraph_.recursive(sourceFunction.name)) continue;
        event(AdaptiveJITEventKind::Tier1CompilationStarted, sourceFunction.name);
        if (!compileAndInstall(sourceFunction.name, CompilationTier::Tier1, error)) {
          event(AdaptiveJITEventKind::Tier1Rejected, sourceFunction.name, 0, error);
          result_.execution.error = "forced tier1 compile for @" + sourceFunction.name + " failed: " + error;
          return std::move(result_);
        }
        result_.tiers[sourceFunction.name].recompilationRequested = true;
        result_.tiers[sourceFunction.name].recompilationComplete = true;
        event(AdaptiveJITEventKind::Tier1Installed, sourceFunction.name);
      }
    }
    auto entry = jit_->lookup("main");
    if (!entry) { result_.execution.error = errorText(entry.takeError()); return std::move(result_); }
    const SSAFunction *main = function("main");
    if (!main) { result_.execution.error = "JIT module has no @main"; return std::move(result_); }
    std::string output;
    struct SinkReset { std::string *previous; ~SinkReset() { setRuntimeOutputSink(previous); } } reset{setRuntimeOutputSink(&output)};
    activeAdaptiveSession = this;
    struct SessionReset { ~SessionReset() { activeAdaptiveSession = nullptr; } } sessionReset;
    if (main->returnType.kind == LangTypeKind::Void) {
      using Entry = void (*)(); entry->toPtr<Entry>()();
    } else if (main->returnType.kind == LangTypeKind::Integer) {
      switch (main->returnType.bitWidth) {
      case 8: { using Entry = std::int8_t (*)(); result_.execution.returnValue = entry->toPtr<Entry>()(); break; }
      case 16: { using Entry = std::int16_t (*)(); result_.execution.returnValue = entry->toPtr<Entry>()(); break; }
      case 32: { using Entry = std::int32_t (*)(); result_.execution.returnValue = entry->toPtr<Entry>()(); break; }
      case 64: { using Entry = std::int64_t (*)(); result_.execution.returnValue = entry->toPtr<Entry>()(); break; }
      default: result_.execution.error = "JIT @main has unsupported integer width"; return std::move(result_);
      }
    } else { result_.execution.error = "JIT @main must return an integer or void"; return std::move(result_); }
    result_.execution.stdoutText = std::move(output);
    if (fatalError_) result_.execution.error = *fatalError_;
    return std::move(result_);
  }

  void onFunctionEntry(const char *rawName) {
    if (!rawName || fatalError_) return;
    const std::string name(rawName);
    auto state = result_.tiers.find(name);
    if (state == result_.tiers.end()) return;
    ++state->second.callCount;
    result_.runtimeProfile.functionEntry(name);
    if (config_.forceTier0 || config_.forceTier1 ||
        state->second.activeTier != CompilationTier::Tier0 ||
        state->second.recompilationRequested ||
        state->second.callCount < config_.hotFunctionCallThreshold ||
        callGraph_.recursive(name)) return;
    state->second.recompilationRequested = true;
    event(AdaptiveJITEventKind::HotThresholdReached, name, state->second.callCount);
    event(AdaptiveJITEventKind::Tier1CompilationStarted, name, state->second.callCount);
    std::string error;
    if (!compileAndInstall(name, CompilationTier::Tier1, error)) {
      event(AdaptiveJITEventKind::Tier1Rejected, name, state->second.callCount, error);
      fatalError_ = "tier1 compile for @" + name + " failed: " + error;
      return;
    }
    state->second.recompilationComplete = true;
    event(AdaptiveJITEventKind::Tier1Installed, name, state->second.callCount);
  }
};
}

extern "C" void nano_adaptive_function_entry(const char *name) {
  if (activeAdaptiveSession) activeAdaptiveSession->onFunctionEntry(name);
}

JITExecutionResult executeWithLLJIT(const SSAProgram &program) {
  if (llvm::InitializeNativeTarget()) return {0, {}, "could not initialize native LLVM target"};
  if (llvm::InitializeNativeTargetAsmPrinter()) return {0, {}, "could not initialize native LLVM asm printer"};
  if (llvm::InitializeNativeTargetAsmParser()) return {0, {}, "could not initialize native LLVM asm parser"};
  auto jitOrError = llvm::orc::LLJITBuilder().create();
  if (!jitOrError) return {0, {}, errorText(jitOrError.takeError())};
  auto jit = std::move(*jitOrError);
  auto generator = llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
      jit->getDataLayout().getGlobalPrefix());
  if (!generator) return {0, {}, errorText(generator.takeError())};
  jit->getMainJITDylib().addGenerator(std::move(*generator));

  auto context = std::make_unique<llvm::LLVMContext>();
  try {
    auto result = SSALLVMLowering(*context).lower(program, "nano_jit_module");
    if (auto error = jit->addIRModule(llvm::orc::ThreadSafeModule(
            std::move(result.module), std::move(context))))
      return {0, {}, errorText(std::move(error))};
  } catch (const std::exception &error) {
    return {0, {}, error.what()};
  }
  auto symbol = jit->lookup("main");
  if (!symbol) return {0, {}, errorText(symbol.takeError())};
  const auto main = std::find_if(program.functions.begin(), program.functions.end(),
                                 [](const SSAFunction &function) { return function.name == "main"; });
  if (main == program.functions.end()) return {0, {}, "JIT module has no @main"};
  std::string output;
  struct SinkReset { std::string *previous; ~SinkReset() { setRuntimeOutputSink(previous); } } reset{setRuntimeOutputSink(&output)};
  if (main->returnType.kind == LangTypeKind::Void) {
    using Entry = void (*)(); symbol->toPtr<Entry>()(); return {0, std::move(output), {}};
  }
  if (main->returnType.kind != LangTypeKind::Integer) return {0, {}, "JIT @main must return an integer or void"};
  switch (main->returnType.bitWidth) {
  case 8: { using Entry = std::int8_t (*)(); return {symbol->toPtr<Entry>()(), std::move(output), {}}; }
  case 16: { using Entry = std::int16_t (*)(); return {symbol->toPtr<Entry>()(), std::move(output), {}}; }
  case 32: { using Entry = std::int32_t (*)(); return {symbol->toPtr<Entry>()(), std::move(output), {}}; }
  case 64: { using Entry = std::int64_t (*)(); return {symbol->toPtr<Entry>()(), std::move(output), {}}; }
  default: return {0, {}, "JIT @main has unsupported integer width"};
  }
}

AdaptiveJITExecutionResult executeAdaptivelyWithLLJIT(
    const SSAProgram &program, const AdaptiveJITConfig &config) {
  return AdaptiveSession(program, config).run();
}

} // namespace nanopass::llvm_backend
