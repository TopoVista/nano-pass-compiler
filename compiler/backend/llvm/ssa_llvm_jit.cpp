#include "backend/llvm/ssa_llvm_jit.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>

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
extern "C" void nano_adaptive_block_entry(const char *name, std::int64_t block);
extern "C" void nano_adaptive_edge(const char *name, std::int64_t from,
                                    std::int64_t to);

void insertRuntimeProfileHooks(llvm::Module &module, llvm::Function &function,
                               const SSAFunction &source) {
  auto &context = module.getContext();
  auto *i8Pointer = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(context));
  auto *i64 = llvm::Type::getInt64Ty(context);
  auto functionEntry = module.getOrInsertFunction("nano_adaptive_function_entry",
      llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i8Pointer}, false));
  auto blockEntry = module.getOrInsertFunction("nano_adaptive_block_entry",
      llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i8Pointer, i64}, false));
  auto edge = module.getOrInsertFunction("nano_adaptive_edge",
      llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i8Pointer, i64, i64}, false));
  auto stringFor = [&] {
    return llvm::IRBuilder<>(context).CreateGlobalStringPtr(
        source.name, "nano.profile.function", 0, &module);
  };
  std::map<int, llvm::BasicBlock *> blocks;
  for (auto &block : function)
    if (block.getName().starts_with("bb")) {
      int id = -1;
      const auto suffix = block.getName().drop_front(2);
      if (!suffix.getAsInteger(10, id)) blocks.emplace(id, &block);
    }
  for (const auto &sourceBlock : source.blocks) {
    const auto found = blocks.find(sourceBlock.id.index);
    if (found == blocks.end()) continue;
    llvm::BasicBlock *block = found->second;
    llvm::IRBuilder<> builder(&*block->getFirstInsertionPt());
    if (sourceBlock.id == source.entryBlock)
      builder.CreateCall(functionEntry, {stringFor()});
    builder.CreateCall(blockEntry, {stringFor(), llvm::ConstantInt::get(i64, sourceBlock.id.index)});
    auto *branch = llvm::dyn_cast<llvm::BranchInst>(block->getTerminator());
    if (!branch) continue;
    auto emitDirectEdge = [&](int target) {
      llvm::IRBuilder<> edgeBuilder(branch);
      edgeBuilder.CreateCall(edge, {stringFor(), llvm::ConstantInt::get(i64, sourceBlock.id.index),
                                    llvm::ConstantInt::get(i64, target)});
    };
    if (const auto *jump = std::get_if<SSATerminator::Jump>(&sourceBlock.terminator.value)) {
      emitDirectEdge(jump->target.index);
      continue;
    }
    const auto *sourceBranch =
        std::get_if<SSATerminator::Branch>(&sourceBlock.terminator.value);
    if (!sourceBranch) continue;
    // A collapsed conditional has one CFG edge. Preserve phi structure and
    // record that edge directly rather than creating duplicate predecessors.
    if (sourceBranch->thenTarget == sourceBranch->elseTarget ||
        branch->getSuccessor(0) == branch->getSuccessor(1)) {
      emitDirectEdge(sourceBranch->thenTarget.index);
      continue;
    }
    auto makeEdgeBlock = [&](unsigned successorIndex, BlockId targetId) {
      llvm::BasicBlock *target = branch->getSuccessor(successorIndex);
      auto *hookBlock = llvm::BasicBlock::Create(
          context, "nano.edge." + std::to_string(sourceBlock.id.index) + "." + std::to_string(targetId.index),
          &function, target);
      llvm::IRBuilder<> hookBuilder(hookBlock);
      hookBuilder.CreateCall(edge, {stringFor(), llvm::ConstantInt::get(i64, sourceBlock.id.index),
                                    llvm::ConstantInt::get(i64, targetId.index)});
      hookBuilder.CreateBr(target);
      for (auto &instruction : *target)
        if (auto *phi = llvm::dyn_cast<llvm::PHINode>(&instruction)) {
          const int incoming = phi->getBasicBlockIndex(block);
          if (incoming >= 0) phi->setIncomingBlock(static_cast<unsigned>(incoming), hookBlock);
        } else break;
      branch->setSuccessor(successorIndex, hookBlock);
    };
    makeEdgeBlock(0, sourceBranch->thenTarget);
    makeEdgeBlock(1, sourceBranch->elseTarget);
  }
}

class AdaptiveSession {
  const SSAProgram &source_;
  AdaptiveJITConfig config_;
  std::unique_ptr<llvm::orc::LLJIT> jit_;
  std::unique_ptr<llvm::orc::IndirectStubsManager> stubs_;
  CallGraph callGraph_;
  AdaptiveJITExecutionResult result_;
  std::map<std::string, size_t> generation_;

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

  SSAProgram tier1Program(std::string_view name) {
    SSAProgram tier1 = source_;
    // Reuse the ordinary, verifier-gated inliner rather than inventing a JIT
    // variant. Counters observed before this recompilation can widen only its
    // documented hot-callee cost limit; recursion, growth, CFG and SSA checks
    // stay mandatory.
    opt::OptimizationBudget inlineBudget;
    inlineBudget.hotFunctionThreshold = config_.hotFunctionCallThreshold;
    for (const auto &[functionName, record] : result_.runtimeProfile.functions)
      inlineBudget.observedFunctionCalls.emplace(functionName, record.calls);
    opt::SSAProgramInliner{}.run(tier1, inlineBudget);
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
      insertRuntimeProfileHooks(*lowered.module, *body, *selected);
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
    result_.runtimeProfile.optimizationLevel = 0;
    result_.runtimeProfile.executionMode = "orc-adaptive";
    result_.runtimeProfile.status = profile::ProfileStatus::Complete;
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
    result_.runtimeProfile.finalizeLoops(source_);
    return std::move(result_);
  }

  void onFunctionEntry(const char *rawName) {
    if (!rawName) return;
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
      // A recompilation failure must never invalidate working Tier-0 code.
      // Keep the request latched so this deterministic one-promotion policy
      // does not retry on every entry, and report the concrete failure event.
      return;
    }
    state->second.recompilationComplete = true;
    event(AdaptiveJITEventKind::Tier1Installed, name, state->second.callCount);
  }

  void onBlockEntry(const char *rawName, std::int64_t block) {
    if (rawName && block >= 0)
      result_.runtimeProfile.blockEntry(rawName, BlockId{static_cast<int>(block)});
  }

  void onEdge(const char *rawName, std::int64_t from, std::int64_t to) {
    if (rawName && from >= 0 && to >= 0)
      result_.runtimeProfile.edge(rawName, BlockId{static_cast<int>(from)},
                                  BlockId{static_cast<int>(to)});
  }
};
}

extern "C" void nano_adaptive_function_entry(const char *name) {
  if (activeAdaptiveSession) activeAdaptiveSession->onFunctionEntry(name);
}
extern "C" void nano_adaptive_block_entry(const char *name, std::int64_t block) {
  if (activeAdaptiveSession) activeAdaptiveSession->onBlockEntry(name, block);
}
extern "C" void nano_adaptive_edge(const char *name, std::int64_t from,
                                    std::int64_t to) {
  if (activeAdaptiveSession) activeAdaptiveSession->onEdge(name, from, to);
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
