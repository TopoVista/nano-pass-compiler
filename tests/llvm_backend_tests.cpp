#include <cstdlib>
#include <iostream>

#include <llvm/IR/Verifier.h>

#include "backend/llvm/ssa_llvm_jit.h"
#include "backend/llvm/ssa_llvm_lowering.h"

using namespace nanopass;
using namespace nanopass::llvm_backend;

namespace {
int failures = 0;
#define CHECK(EXPR) do { if (!(EXPR)) { std::cerr << "FAIL " #EXPR "\n"; ++failures; } } while (false)

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
  std::cout << "llvm backend tests: " << (failures == 0 ? "passed" : "failed") << '\n';
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
