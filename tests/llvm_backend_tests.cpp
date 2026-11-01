#include <cstdlib>
#include <iostream>

#include <llvm/IR/Verifier.h>

#include "backend/llvm/ssa_llvm_jit.h"
#include "backend/llvm/ssa_llvm_lowering.h"
#include "interpreter/core_interpreter.h"
#include "interpreter/ssa_interpreter.h"
#include "ir/pipeline.h"
#include "lexer/lexer.h"
#include "optimizer/advanced_optimizer.h"
#include "optimizer/ssa_optimizer.h"
#include "parser/parser.h"
#include "sema/resolve_scopes.h"
#include "sema/type_check.h"
#include "testing/nano_generator.h"

using namespace nanopass;
using namespace nanopass::llvm_backend;

namespace {
int failures = 0;
int assertions = 0;
#define CHECK(EXPR) do { ++assertions; if (!(EXPR)) { std::cerr << "FAIL " #EXPR "\n"; ++failures; } } while (false)

SSAInstruction constant(ValueId id, int64_t value) {
  SSAInstruction instruction; instruction.kind = SSAInstructionKind::Constant;
  instruction.result = id; instruction.type = LangType::Int(64); instruction.constantValue = value;
  return instruction;
}

SSAFunction choose() {
  SSAFunction fn; fn.name = "choose"; fn.returnType = LangType::Int(64); fn.entryBlock = {0};
  fn.parameters.push_back({{0}, {"condition", 0}, LangType::Bool()});
  SSABlock entry; entry.id = {0}; entry.successors = {{1}, {2}}; entry.terminator.value = SSATerminator::Branch{SSAOperand{ValueId{0}}, {1}, {2}};
  SSABlock yes; yes.id = {1}; yes.predecessors = {{0}}; yes.successors = {{3}}; yes.instructions.push_back(constant({1}, 7)); yes.terminator.value = SSATerminator::Jump{{3}};
  SSABlock no; no.id = {2}; no.predecessors = {{0}}; no.successors = {{3}}; no.instructions.push_back(constant({2}, 11)); no.terminator.value = SSATerminator::Jump{{3}};
  SSABlock merge; merge.id = {3}; merge.phis.push_back({{3}, {"selected", 0}, LangType::Int(64), {{{1}, {1}}, {{2}, {2}}}, {}});
  merge.predecessors = {{1}, {2}}; merge.terminator.value = SSATerminator::Return{SSAOperand{ValueId{3}}};
  fn.blocks = {std::move(entry), std::move(yes), std::move(no), std::move(merge)};
  return fn;
}

SSAFunction mainFunction() {
  SSAFunction fn; fn.name = "main"; fn.returnType = LangType::Int(64); fn.entryBlock = {0};
  SSABlock block; block.id = {0};
  SSAInstruction call; call.kind = SSAInstructionKind::Call; call.result = ValueId{0}; call.type = LangType::Int(64);
  call.callee = "choose"; call.callArgs.push_back(CoreConstantVal{true}); block.instructions.push_back(std::move(call));
  SSAInstruction print; print.kind = SSAInstructionKind::Call; print.type = LangType::Void();
  print.callee = "print"; print.callArgs.push_back(SSAOperand{ValueId{0}}); block.instructions.push_back(std::move(print));
  block.terminator.value = SSATerminator::Return{SSAOperand{ValueId{0}}}; fn.blocks.push_back(std::move(block)); return fn;
}

SSAFunction loopFunction() {
  SSAFunction fn; fn.name = "main"; fn.returnType = LangType::Int(64); fn.entryBlock = {0};
  SSABlock entry; entry.id = {0}; entry.successors = {{1}}; entry.instructions.push_back(constant({0}, 0)); entry.terminator.value = SSATerminator::Jump{{1}};
  SSABlock header; header.id = {1}; header.predecessors = {{0}, {2}}; header.successors = {{2}, {3}};
  header.phis.push_back({{1}, {"i", 0}, LangType::Int(64), {{{0}, {0}}, {{2}, {3}}}, {}});
  SSAInstruction compare; compare.kind = SSAInstructionKind::Binary; compare.result = ValueId{2}; compare.type = LangType::Bool();
  compare.binaryOp = BinaryOpKind::Lt; compare.operand1 = ValueId{1}; compare.operand2 = CoreConstantVal{int64_t(3)}; header.instructions.push_back(compare);
  header.terminator.value = SSATerminator::Branch{SSAOperand{ValueId{2}}, {2}, {3}};
  SSABlock latch; latch.id = {2}; latch.predecessors = {{1}}; latch.successors = {{1}};
  SSAInstruction increment; increment.kind = SSAInstructionKind::Binary; increment.result = ValueId{3}; increment.type = LangType::Int(64);
  increment.binaryOp = BinaryOpKind::Add; increment.operand1 = ValueId{1}; increment.operand2 = CoreConstantVal{int64_t(1)}; latch.instructions.push_back(increment);
  latch.terminator.value = SSATerminator::Jump{{1}};
  SSABlock exit; exit.id = {3}; exit.predecessors = {{1}}; exit.terminator.value = SSATerminator::Return{SSAOperand{ValueId{1}}};
  fn.blocks = {std::move(entry), std::move(header), std::move(latch), std::move(exit)};
  return fn;
}

SSAFunction factorial() {
  SSAFunction fn; fn.name = "fact"; fn.returnType = LangType::Int(64); fn.entryBlock = {0};
  fn.parameters.push_back({{0}, {"n", 0}, LangType::Int(64)});
  SSABlock entry; entry.id = {0}; entry.successors = {{1}, {2}};
  SSAInstruction compare; compare.kind = SSAInstructionKind::Binary; compare.result = ValueId{1}; compare.type = LangType::Bool();
  compare.binaryOp = BinaryOpKind::Le; compare.operand1 = ValueId{0}; compare.operand2 = CoreConstantVal{int64_t(1)}; entry.instructions.push_back(compare);
  entry.terminator.value = SSATerminator::Branch{SSAOperand{ValueId{1}}, {1}, {2}};
  SSABlock base; base.id = {1}; base.predecessors = {{0}}; base.terminator.value = SSATerminator::Return{SSAOperand{CoreConstantVal{int64_t(1)}}};
  SSABlock recur; recur.id = {2}; recur.predecessors = {{0}};
  SSAInstruction decrement; decrement.kind = SSAInstructionKind::Binary; decrement.result = ValueId{2}; decrement.type = LangType::Int(64);
  decrement.binaryOp = BinaryOpKind::Sub; decrement.operand1 = ValueId{0}; decrement.operand2 = CoreConstantVal{int64_t(1)}; recur.instructions.push_back(decrement);
  SSAInstruction call; call.kind = SSAInstructionKind::Call; call.result = ValueId{3}; call.type = LangType::Int(64); call.callee = "fact"; call.callArgs = {SSAOperand{ValueId{2}}}; recur.instructions.push_back(call);
  SSAInstruction multiply; multiply.kind = SSAInstructionKind::Binary; multiply.result = ValueId{4}; multiply.type = LangType::Int(64);
  multiply.binaryOp = BinaryOpKind::Mul; multiply.operand1 = ValueId{0}; multiply.operand2 = ValueId{3}; recur.instructions.push_back(multiply);
  recur.terminator.value = SSATerminator::Return{SSAOperand{ValueId{4}}};
  fn.blocks = {std::move(entry), std::move(base), std::move(recur)}; return fn;
}

SSAFunction factorialMain() {
  SSAFunction fn; fn.name = "main"; fn.returnType = LangType::Int(64); fn.entryBlock = {0}; SSABlock block; block.id = {0};
  SSAInstruction call; call.kind = SSAInstructionKind::Call; call.result = ValueId{0}; call.type = LangType::Int(64); call.callee = "fact"; call.callArgs = {SSAOperand{CoreConstantVal{int64_t(5)}}}; block.instructions.push_back(call);
  block.terminator.value = SSATerminator::Return{SSAOperand{ValueId{0}}}; fn.blocks.push_back(std::move(block)); return fn;
}

SSAFunction arrayMain() {
  SSAFunction fn; fn.name = "main"; fn.returnType = LangType::Int(64); fn.entryBlock = {0};
  const LocalId array{"values", 0}; fn.memoryLocals.emplace(array, LangType::Array(LangType::Int(64), 2));
  SSABlock block; block.id = {0};
  SSAInstruction store; store.kind = SSAInstructionKind::StoreIndex; store.type = LangType::Int(64); store.local = array;
  store.operand1 = CoreConstantVal{int64_t(1)}; store.operand2 = CoreConstantVal{int64_t(17)}; block.instructions.push_back(store);
  SSAInstruction load; load.kind = SSAInstructionKind::LoadIndex; load.result = ValueId{0}; load.type = LangType::Int(64); load.local = array;
  load.operand1 = CoreConstantVal{int64_t(1)}; block.instructions.push_back(load);
  block.terminator.value = SSATerminator::Return{SSAOperand{ValueId{0}}}; fn.blocks.push_back(std::move(block)); return fn;
}

nanopass::IRPipelineResult compilePipeline(const std::string &source) {
  Lexer lexer(source); Parser parser(lexer.scanTokens()); auto program = parser.parseProgram();
  ResolveScopesPass resolver; resolver.resolve(program); TypeCheckPass checker; checker.check(program);
  return nanopass::IRPipeline::runAll(program);
}
}

int main() {
  SSAProgram program; program.functions.push_back(choose()); program.functions.push_back(mainFunction());
  llvm::LLVMContext context;
  auto lowered = SSALLVMLowering(context).lower(program, "backend-test");
  CHECK(lowered.statistics.functions == 2);
  CHECK(lowered.statistics.phis == 1);
  CHECK(!llvm::verifyModule(*lowered.module, &llvm::errs()));
  CHECK(lowered.module->getFunction("choose") != nullptr);
  const auto execution = executeWithLLJIT(program);
  CHECK(execution.error.empty());
  CHECK(execution.returnValue == 7);
  CHECK(execution.stdoutText == "7\n");

  SSAProgram loop; loop.functions.push_back(loopFunction());
  const auto loopExecution = executeWithLLJIT(loop);
  CHECK(loopExecution.error.empty());
  CHECK(loopExecution.returnValue == 3);

  SSAProgram recursive; recursive.functions.push_back(factorial()); recursive.functions.push_back(factorialMain());
  const auto recursiveExecution = executeWithLLJIT(recursive);
  CHECK(recursiveExecution.error.empty());
  CHECK(recursiveExecution.returnValue == 120);

  SSAProgram arrays; arrays.functions.push_back(arrayMain());
  llvm::LLVMContext arrayContext;
  auto loweredArrays = SSALLVMLowering(arrayContext).lower(arrays, "array-bounds-test");
  CHECK(loweredArrays.module->getFunction("nano_check_array_bounds_i64") != nullptr);
  const auto arrayExecution = executeWithLLJIT(arrays);
  CHECK(arrayExecution.error.empty());
  CHECK(arrayExecution.returnValue == 17);
  AdaptiveJITConfig adaptiveArrays; adaptiveArrays.hotFunctionCallThreshold = 100;
  const auto adaptiveArrayExecution = executeAdaptivelyWithLLJIT(arrays, adaptiveArrays);
  CHECK(adaptiveArrayExecution.execution.error.empty());
  CHECK(adaptiveArrayExecution.execution.returnValue == 17);
  CHECK(adaptiveArrayExecution.runtimeProfile.status == profile::ProfileStatus::Complete);

  // This uses the real parser → CoreIR → SSA pipeline and compares the JIT
  // result to the independent Core interpreter at each Nano O level.
  auto pipeline = compilePipeline(R"(
    int choose(int x) { if (x > 0) return 7; return 9; }
    int loopfn(int n) { int i = 0; while (i < n) { i = i + 1; } return i; }
    int main() { int[3] values; values[0] = choose(1); values[1] = loopfn(values[0]); values[2] = values[0] + values[1]; print(values[2]); return values[2]; }
  )");
  const auto expected = CoreInterpreter{}.run(pipeline.core);
  CHECK(expected.status == ExecutionStatus::Success);
  for (int level = 0; level <= 3; ++level) {
    SSAProgram optimized = pipeline.ssa;
    for (auto &function : optimized.functions) {
      nanopass::opt::SSAOptimizationContext context; nanopass::opt::buildPipeline(level).run(function, context);
      CHECK(verifySSA(function).ok);
    }
    const auto executionAtLevel = executeWithLLJIT(optimized);
    if (!executionAtLevel.error.empty()) std::cerr << "O" << level << " JIT error: " << executionAtLevel.error << '\n';
    CHECK(executionAtLevel.error.empty());
    CHECK(executionAtLevel.stdoutText == expected.stdoutText);
    CHECK(executionAtLevel.returnValue == std::stoll(runtimeValueText(*expected.returnValue)));
  }
  SSAProgram inlined = pipeline.ssa;
  CHECK(nanopass::opt::SSAProgramInliner{}.run(inlined).changed);
  const auto inlinedReference = SSAInterpreter{}.run(inlined);
  CHECK(sameObservableResult(expected, inlinedReference));
  const auto inlinedExecution = executeWithLLJIT(inlined);
  if (!inlinedExecution.error.empty()) std::cerr << "inlined JIT error: " << inlinedExecution.error << '\n';
  CHECK(inlinedExecution.error.empty());
  CHECK(inlinedExecution.stdoutText == expected.stdoutText);
  CHECK(inlinedExecution.returnValue == std::stoll(runtimeValueText(*expected.returnValue)));
  // The general SSA inliner leaves a multi-return continuation phi in @main.
  // Adaptive Tier-0 lowering must execute that real cloned CFG unchanged.
  AdaptiveJITConfig inlinedAdaptiveConfig; inlinedAdaptiveConfig.forceTier0 = true;
  const auto inlinedAdaptive = executeAdaptivelyWithLLJIT(inlined, inlinedAdaptiveConfig);
  CHECK(inlinedAdaptive.execution.error.empty());
  CHECK(inlinedAdaptive.execution.stdoutText == expected.stdoutText);
  CHECK(inlinedAdaptive.execution.returnValue == std::stoll(runtimeValueText(*expected.returnValue)));
  CHECK(inlinedAdaptive.telemetry.at("main").tier0Instructions > 0u);
  CHECK(inlinedAdaptive.telemetry.at("main").tier0Blocks > 0u);

  // CFG simplification may retain blocks in a storage order that is not a
  // dominance order after general inlining. LLVM lowering must use a stable
  // reverse-postorder, not vector order, for value-producing instructions.
  auto reorderedCFG = compilePipeline(R"(
    int increment(int value) { return value + 1; }
    int main() { int i = 0; int total = 0; while (i < 8) { total = total + increment(i); i = i + 1; } print(total); return total; }
  )");
  const auto reorderedOracle = CoreInterpreter{}.run(reorderedCFG.core);
  CHECK(nanopass::opt::SSAProgramInliner{}.run(reorderedCFG.ssa).changed);
  for (auto &function : reorderedCFG.ssa.functions) {
    nanopass::opt::SSAOptimizationContext context;
    nanopass::opt::buildPipeline(2).run(function, context);
    auto loopSimplify = nanopass::opt::makeAdvancedPass("loop-simplify");
    auto licm = nanopass::opt::makeAdvancedPass("licm");
    loopSimplify->run(function, context);
    licm->run(function, context);
    CHECK(verifySSA(function).ok);
  }
  const auto reorderedExecution = executeWithLLJIT(reorderedCFG.ssa);
  CHECK(reorderedExecution.error.empty());
  CHECK(reorderedExecution.stdoutText == reorderedOracle.stdoutText);
  CHECK(reorderedExecution.returnValue == std::stoll(runtimeValueText(*reorderedOracle.returnValue)));

  // Adaptive ORC uses public indirect stubs, so this exercises a real
  // Tier-0 → Tier-1 pointer replacement while callers keep their original
  // direct Nano call instruction. The Tier-0 frame that triggers promotion
  // finishes normally; only later calls observe Tier-1.
  auto adaptiveProgram = compilePipeline(R"(
    int add(int value) { return value + 1; }
    int main() { int i = 0; int total = 0; while (i < 5) { total = total + add(i); i = i + 1; } print(total); return total; }
  )");
  const auto adaptiveOracle = CoreInterpreter{}.run(adaptiveProgram.core);
  AdaptiveJITConfig tier0Only; tier0Only.forceTier0 = true;
  const auto adaptiveTier0 = executeAdaptivelyWithLLJIT(adaptiveProgram.ssa, tier0Only);
  CHECK(adaptiveTier0.execution.error.empty());
  CHECK(adaptiveTier0.execution.stdoutText == adaptiveOracle.stdoutText);
  CHECK(adaptiveTier0.execution.returnValue == std::stoll(runtimeValueText(*adaptiveOracle.returnValue)));
  CHECK(adaptiveTier0.tiers.at("add").activeTier == CompilationTier::Tier0);
  AdaptiveJITConfig naturallyCold; naturallyCold.hotFunctionCallThreshold = 100;
  const auto adaptiveCold = executeAdaptivelyWithLLJIT(adaptiveProgram.ssa, naturallyCold);
  CHECK(adaptiveCold.execution.error.empty());
  CHECK(adaptiveCold.execution.returnValue == std::stoll(runtimeValueText(*adaptiveOracle.returnValue)));
  CHECK(adaptiveCold.tiers.at("add").activeTier == CompilationTier::Tier0);
  CHECK(std::none_of(adaptiveCold.events.begin(), adaptiveCold.events.end(),
      [](const AdaptiveJITEvent &event) { return event.kind == AdaptiveJITEventKind::Tier1Installed; }));
  AdaptiveJITConfig promote; promote.hotFunctionCallThreshold = 2;
  const auto adaptivePromoted = executeAdaptivelyWithLLJIT(adaptiveProgram.ssa, promote);
  CHECK(adaptivePromoted.execution.error.empty());
  CHECK(adaptivePromoted.execution.stdoutText == adaptiveOracle.stdoutText);
  CHECK(adaptivePromoted.execution.returnValue == std::stoll(runtimeValueText(*adaptiveOracle.returnValue)));
  CHECK(adaptivePromoted.runtimeProfile.status == profile::ProfileStatus::Complete);
  CHECK(adaptivePromoted.runtimeProfile.optimizationLevel == 0u);
  CHECK(adaptivePromoted.runtimeProfile.executionMode == "orc-adaptive");
  CHECK(adaptivePromoted.runtimeProfile.functions.at("add").calls == 5u);
  CHECK(adaptivePromoted.runtimeProfile.functions.at("main").blocksExecuted > 0u);
  CHECK(!adaptivePromoted.runtimeProfile.functions.at("main").edgeCounts.empty());
  CHECK(!adaptivePromoted.runtimeProfile.functions.at("main").loops.empty());
  CHECK(adaptivePromoted.tiers.at("add").activeTier == CompilationTier::Tier1);
  CHECK(adaptivePromoted.tiers.at("add").recompilationComplete);
  CHECK(adaptivePromoted.telemetry.at("add").tier0Instructions > 0u);
  CHECK(adaptivePromoted.telemetry.at("add").tier1Instructions > 0u);
  CHECK(adaptivePromoted.telemetry.at("add").tier1Blocks > 0u);
  CHECK(std::any_of(adaptivePromoted.events.begin(), adaptivePromoted.events.end(),
      [](const AdaptiveJITEvent &event) { return event.kind == AdaptiveJITEventKind::Tier1Installed && event.function == "add"; }));
  AdaptiveJITConfig forced; forced.forceTier1 = true;
  const auto adaptiveForced = executeAdaptivelyWithLLJIT(adaptiveProgram.ssa, forced);
  CHECK(adaptiveForced.execution.error.empty());
  CHECK(adaptiveForced.execution.returnValue == adaptivePromoted.execution.returnValue);
  CHECK(adaptiveForced.tiers.at("add").activeTier == CompilationTier::Tier1);
  AdaptiveJITConfig recursiveAdaptive; recursiveAdaptive.hotFunctionCallThreshold = 1;
  const auto adaptiveRecursive = executeAdaptivelyWithLLJIT(recursive, recursiveAdaptive);
  CHECK(adaptiveRecursive.execution.error.empty());
  CHECK(adaptiveRecursive.execution.returnValue == 120);
  CHECK(adaptiveRecursive.tiers.at("fact").activeTier == CompilationTier::Tier0);
  CHECK(adaptiveRecursive.runtimeProfile.functions.at("fact").calls == 5u);

  // Independent stubs and counters must let more than one function promote
  // during the same run; identifiers and updates cannot collide by call order.
  auto multipleHot = compilePipeline(R"(
    int addOne(int value) { return value + 1; }
    int addTwo(int value) { return value + 2; }
    int main() { int i = 0; int total = 0; while (i < 5) { total = addOne(total); total = addTwo(total); i = i + 1; } print(total); return total; }
  )");
  const auto multipleHotOracle = CoreInterpreter{}.run(multipleHot.core);
  AdaptiveJITConfig multipleHotConfig; multipleHotConfig.hotFunctionCallThreshold = 2;
  const auto multipleHotAdaptive = executeAdaptivelyWithLLJIT(multipleHot.ssa, multipleHotConfig);
  CHECK(multipleHotAdaptive.execution.error.empty());
  CHECK(multipleHotAdaptive.execution.stdoutText == multipleHotOracle.stdoutText);
  CHECK(multipleHotAdaptive.execution.returnValue == std::stoll(runtimeValueText(*multipleHotOracle.returnValue)));
  CHECK(multipleHotAdaptive.tiers.at("addOne").activeTier == CompilationTier::Tier1);
  CHECK(multipleHotAdaptive.tiers.at("addTwo").activeTier == CompilationTier::Tier1);
  CHECK(multipleHotAdaptive.runtimeProfile.functions.at("addOne").calls == 5u);
  CHECK(multipleHotAdaptive.runtimeProfile.functions.at("addTwo").calls == 5u);

  // A promoted function may itself contain a natural loop. Runtime loop facts
  // remain attached to the original stable SSA BlockIds and Tier-1 still uses
  // normal loop legality/verification gates.
  auto hotLoopCallee = compilePipeline(R"(
    int spin(int n) { int i = 0; while (i < n) { i = i + 1; } return i; }
    int main() { int i = 0; int total = 0; while (i < 3) { total = total + spin(5); i = i + 1; } print(total); return total; }
  )");
  const auto hotLoopOracle = CoreInterpreter{}.run(hotLoopCallee.core);
  AdaptiveJITConfig hotLoopConfig; hotLoopConfig.hotFunctionCallThreshold = 2;
  const auto hotLoopAdaptive = executeAdaptivelyWithLLJIT(hotLoopCallee.ssa, hotLoopConfig);
  CHECK(hotLoopAdaptive.execution.error.empty());
  CHECK(hotLoopAdaptive.execution.stdoutText == hotLoopOracle.stdoutText);
  CHECK(hotLoopAdaptive.execution.returnValue == std::stoll(runtimeValueText(*hotLoopOracle.returnValue)));
  CHECK(hotLoopAdaptive.tiers.at("spin").activeTier == CompilationTier::Tier1);
  CHECK(hotLoopAdaptive.telemetry.at("spin").tier1ProfileLoopEffortEnabled);
  CHECK(!hotLoopAdaptive.runtimeProfile.functions.at("spin").loops.empty());
  CHECK(hotLoopAdaptive.runtimeProfile.functions.at("spin").loops.begin()->second.backedges == 15u);

  // Short-circuit lowering must keep the dead division off the executed path.
  auto shortCircuit = compilePipeline(R"(
    int main() { int x = 0; if (false && ((1 / 0) > 0)) { x = 1; } else { x = 2; } print(x); return x; }
  )");
  const auto shortCircuitOracle = CoreInterpreter{}.run(shortCircuit.core);
  CHECK(shortCircuitOracle.status == ExecutionStatus::Success);
  const auto shortCircuitNative = executeWithLLJIT(shortCircuit.ssa);
  CHECK(shortCircuitNative.error.empty());
  CHECK(shortCircuitNative.stdoutText == shortCircuitOracle.stdoutText);
  CHECK(shortCircuitNative.returnValue == std::stoll(runtimeValueText(*shortCircuitOracle.returnValue)));

  // An inner-scope scalar must remain typed memory in SSA; this exercises the
  // residual LoadLocal/StoreLocal lowering path after O2 transforms.
  auto nestedMemory = compilePipeline(R"(
    int countPairs(int n, int target) {
      int count = 0;
      int i = 1;
      while (i <= n) {
        int j = 1;
        while (j <= n) { if ((i + j) == target) { count = count + 1; } j = j + 1; }
        i = i + 1;
      }
      return count;
    }
    int main() { int result = countPairs(5, 6); print(result); return result; }
  )");
  const auto nestedMemoryOracle = CoreInterpreter{}.run(nestedMemory.core);
  CHECK(nestedMemoryOracle.status == ExecutionStatus::Success);
  for (auto &function : nestedMemory.ssa.functions) {
    nanopass::opt::SSAOptimizationContext context;
    nanopass::opt::buildPipeline(2).run(function, context);
    CHECK(verifySSA(function).ok);
  }
  const auto nestedMemoryNative = executeWithLLJIT(nestedMemory.ssa);
  CHECK(nestedMemoryNative.error.empty());
  CHECK(nestedMemoryNative.stdoutText == nestedMemoryOracle.stdoutText);
  CHECK(nestedMemoryNative.returnValue == std::stoll(runtimeValueText(*nestedMemoryOracle.returnValue)));

  // A bounded deterministic campaign supplements the structural cases above.
  // The Phase 7 generator produces the scalar subset whose reference semantics
  // are already established independently of LLVM.
  NanoProgramGenerator generator({0xC0FFEEu, 3, 8});
  size_t nativeCompared = 0;
  for (size_t index = 0; index < 64; ++index) {
    auto generated = compilePipeline(generator.generate(index).source);
    const auto oracle = CoreInterpreter{}.run(generated.core);
    CHECK(oracle.status == ExecutionStatus::Success);
    SSAProgram optimized = generated.ssa;
    for (auto &function : optimized.functions) {
      nanopass::opt::SSAOptimizationContext context;
      nanopass::opt::buildPipeline(3).run(function, context);
      CHECK(verifySSA(function).ok);
    }
    const auto native = executeWithLLJIT(optimized);
    if (!native.error.empty()) std::cerr << "generated case " << index << " JIT error: " << native.error << '\n';
    CHECK(native.error.empty());
    CHECK(native.stdoutText == oracle.stdoutText);
    CHECK(native.returnValue == std::stoll(runtimeValueText(*oracle.returnValue)));
    ++nativeCompared;
  }
  CHECK(nativeCompared == 64u);
  std::cout << "llvm backend tests: " << (failures == 0 ? "passed" : "failed")
            << " (" << assertions << " assertions)\n";
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
