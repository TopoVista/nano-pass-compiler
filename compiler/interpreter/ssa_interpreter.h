#pragma once

#include <algorithm>
#include <map>

#include "interpreter/core_interpreter.h"
#include "ir/ssa/ssa_ir.h"

namespace nanopass {

// A deliberately independent execution engine for SSA. Aggregate locals stay
// memory-backed, while promoted scalars live exclusively in `Registers`.
class SSAInterpreter {
  const SSAProgram *program_ = nullptr;
  ExecutionLimits limits_;
  ExecutionResult result_;
  size_t callDepth_ = 0;
  using Registers = std::map<int, RuntimeValue>;
  using Locals = std::map<LocalId, RuntimeValue>;

  [[noreturn]] void fail(ExecutionStatus status, std::string category,
                         std::string message) {
    result_.status = status;
    result_.error = RuntimeError{std::move(category), std::move(message)};
    throw std::runtime_error("runtime stop");
  }
  void step() {
    if (++result_.stepsExecuted > limits_.maxSteps)
      fail(ExecutionStatus::StepLimitExceeded, "step-limit", "execution step limit exceeded");
  }
  bool truth(const RuntimeValue &value) {
    if (auto boolean = std::get_if<bool>(&value)) return *boolean;
    if (auto integer = std::get_if<int64_t>(&value)) return *integer != 0;
    fail(ExecutionStatus::RuntimeError, "type-error", "condition is not boolean/integer");
  }
  RuntimeValue operand(const SSAOperand &source, const Registers &registers) {
    if (auto constant = std::get_if<CoreConstantVal>(&source))
      return std::visit([](const auto &value) -> RuntimeValue { return value; }, *constant);
    const auto it = registers.find(std::get<ValueId>(source).index);
    if (it == registers.end())
      fail(ExecutionStatus::RuntimeError, "undefined-value", "read of undefined SSA value");
    return it->second;
  }
  static CoreConstantVal scalarConstant(const RuntimeValue &value) {
    return std::visit([](const auto &item) -> CoreConstantVal {
      using T = std::decay_t<decltype(item)>;
      if constexpr (std::is_same_v<T, RuntimeArray>)
        throw std::runtime_error("array value cannot be stored in scalar slot");
      else return item;
    }, value);
  }
  RuntimeValue unary(UnaryOpKind op, const RuntimeValue &value) {
    if (op == UnaryOpKind::LogicalNot) return !truth(value);
    if (auto integer = std::get_if<int64_t>(&value)) return -*integer;
    if (auto floating = std::get_if<double>(&value)) return -*floating;
    fail(ExecutionStatus::RuntimeError, "type-error", "invalid unary operand");
  }
  RuntimeValue binary(BinaryOpKind op, const RuntimeValue &left, const RuntimeValue &right) {
    if (op == BinaryOpKind::LogicalAnd) return truth(left) && truth(right);
    if (op == BinaryOpKind::LogicalOr) return truth(left) || truth(right);
    if (auto x = std::get_if<int64_t>(&left)) if (auto y = std::get_if<int64_t>(&right)) {
      switch (op) {
      case BinaryOpKind::Add: return *x + *y; case BinaryOpKind::Sub: return *x - *y;
      case BinaryOpKind::Mul: return *x * *y;
      case BinaryOpKind::Div: if (!*y) fail(ExecutionStatus::RuntimeError, "division-by-zero", "integer division by zero"); return *x / *y;
      case BinaryOpKind::Mod: if (!*y) fail(ExecutionStatus::RuntimeError, "division-by-zero", "integer remainder by zero"); return *x % *y;
      case BinaryOpKind::Eq: return *x == *y; case BinaryOpKind::Ne: return *x != *y;
      case BinaryOpKind::Lt: return *x < *y; case BinaryOpKind::Le: return *x <= *y;
      case BinaryOpKind::Gt: return *x > *y; case BinaryOpKind::Ge: return *x >= *y;
      default: break;
      }
    }
    if (auto x = std::get_if<double>(&left)) if (auto y = std::get_if<double>(&right)) {
      switch (op) {
      case BinaryOpKind::Add: return *x + *y; case BinaryOpKind::Sub: return *x - *y;
      case BinaryOpKind::Mul: return *x * *y; case BinaryOpKind::Div: return *x / *y;
      case BinaryOpKind::Eq: return *x == *y; case BinaryOpKind::Ne: return *x != *y;
      case BinaryOpKind::Lt: return *x < *y; case BinaryOpKind::Le: return *x <= *y;
      case BinaryOpKind::Gt: return *x > *y; case BinaryOpKind::Ge: return *x >= *y;
      default: break;
      }
    }
    if (op == BinaryOpKind::Eq) return runtimeValueText(left) == runtimeValueText(right);
    if (op == BinaryOpKind::Ne) return runtimeValueText(left) != runtimeValueText(right);
    fail(ExecutionStatus::RuntimeError, "type-error", "invalid binary operands");
  }
  int64_t indexOf(const RuntimeValue &value) {
    if (auto index = std::get_if<int64_t>(&value)) return *index;
    fail(ExecutionStatus::RuntimeError, "type-error", "array index is not an integer");
  }
  RuntimeValue loadIndex(Locals &locals, const LocalId &local, int64_t index) {
    const auto it = locals.find(local);
    if (it == locals.end()) fail(ExecutionStatus::RuntimeError, "uninitialized-local", "array is not initialized");
    const auto pointer = std::get_if<RuntimeArray>(&it->second);
    if (!pointer || !*pointer) fail(ExecutionStatus::RuntimeError, "type-error", "indexed value is not an array");
    if (index < 0 || static_cast<size_t>(index) >= (*pointer)->size())
      fail(ExecutionStatus::RuntimeError, "array-bounds", "array index out of bounds");
    return std::visit([](const auto &value) -> RuntimeValue { return value; },
                      (**pointer)[static_cast<size_t>(index)]);
  }
  void storeIndex(Locals &locals, const LocalId &local, int64_t index, const RuntimeValue &value) {
    const auto it = locals.find(local);
    if (it == locals.end()) fail(ExecutionStatus::RuntimeError, "uninitialized-local", "array is not initialized");
    const auto pointer = std::get_if<RuntimeArray>(&it->second);
    if (!pointer || !*pointer) fail(ExecutionStatus::RuntimeError, "type-error", "indexed value is not an array");
    if (index < 0 || static_cast<size_t>(index) >= (*pointer)->size())
      fail(ExecutionStatus::RuntimeError, "array-bounds", "array index out of bounds");
    try { (**pointer)[static_cast<size_t>(index)] = scalarConstant(value); }
    catch (const std::runtime_error &) { fail(ExecutionStatus::UnsupportedFeature, "array-of-array", "array values are not storable"); }
  }
  void initializeMemory(const SSAFunction &function, Locals &locals) {
    for (const auto &[local, type] : function.memoryLocals) {
      if (type.kind != LangTypeKind::Array) continue;
      if (type.arraySize < 0 || static_cast<size_t>(type.arraySize) > limits_.maxArrayElements)
        fail(ExecutionStatus::RuntimeError, "allocation-limit", "array allocation exceeds execution limit");
      CoreConstantVal zero = int64_t(0);
      if (type.element && type.element->kind == LangTypeKind::Bool) zero = false;
      else if (type.element && type.element->kind == LangTypeKind::Floating) zero = 0.0;
      locals.emplace(local, std::make_shared<std::vector<CoreConstantVal>>(
          static_cast<size_t>(type.arraySize), zero));
    }
  }
  RuntimeValue call(const std::string &name, const std::vector<RuntimeValue> &args) {
    if (name == "print") {
      if (args.size() != 1) fail(ExecutionStatus::RuntimeError, "arity", "print expects one argument");
      const auto text = runtimeValueText(args[0]) + "\n";
      if (result_.stdoutText.size() + text.size() > limits_.maxOutputBytes)
        fail(ExecutionStatus::OutputLimitExceeded, "output-limit", "output limit exceeded");
      result_.stdoutText += text; return int64_t(0);
    }
    const SSAFunction *function = nullptr;
    for (const auto &candidate : program_->functions) if (candidate.name == name) { function = &candidate; break; }
    if (!function) fail(ExecutionStatus::RuntimeError, "unknown-function", "unknown function @" + name);
    if (args.size() != function->parameters.size()) fail(ExecutionStatus::RuntimeError, "arity", "wrong argument count");
    if (++callDepth_ > limits_.maxCallDepth)
      fail(ExecutionStatus::RecursionLimitExceeded, "recursion-limit", "call depth limit exceeded");
    Registers registers; Locals locals;
    for (size_t index = 0; index < args.size(); ++index) registers[function->parameters[index].value.index] = args[index];
    initializeMemory(*function, locals);
    const auto value = execute(*function, std::move(registers), std::move(locals));
    --callDepth_;
    return value.value_or(RuntimeValue{int64_t(0)});
  }
  std::optional<RuntimeValue> execute(const SSAFunction &function, Registers registers, Locals locals) {
    std::map<int, const SSABlock *> blocks;
    for (const auto &block : function.blocks) blocks.emplace(block.id.index, &block);
    auto current = blocks.find(function.entryBlock.index);
    if (current == blocks.end()) fail(ExecutionStatus::InternalError, "missing-entry", "missing entry block");
    std::optional<BlockId> predecessor;
    while (true) {
      step(); const SSABlock &block = *current->second;
      if (predecessor) {
        // Read every phi input before assigning any phi result: parallel phi semantics.
        std::vector<std::pair<int, RuntimeValue>> pending;
        for (const auto &phi : block.phis) {
          const auto incoming = std::find_if(phi.incoming.begin(), phi.incoming.end(),
              [&](const PhiIncoming &edge) { return edge.predecessor == *predecessor; });
          if (incoming == phi.incoming.end()) fail(ExecutionStatus::InternalError, "phi-edge", "missing phi edge");
          pending.emplace_back(phi.result.index, operand(SSAOperand{incoming->value}, registers));
        }
        for (auto &[id, value] : pending) registers[id] = std::move(value);
      }
      for (const auto &instruction : block.instructions) {
        step(); RuntimeValue value = int64_t(0);
        switch (instruction.kind) {
        case SSAInstructionKind::Constant: value = std::visit([](const auto &item) -> RuntimeValue { return item; }, instruction.constantValue); break;
        case SSAInstructionKind::Copy: value = operand(instruction.operand1, registers); break;
        case SSAInstructionKind::Unary: value = unary(instruction.unaryOp, operand(instruction.operand1, registers)); break;
        case SSAInstructionKind::Binary: value = binary(instruction.binaryOp, operand(instruction.operand1, registers), operand(instruction.operand2, registers)); break;
        case SSAInstructionKind::Call: { std::vector<RuntimeValue> args; for (const auto &argument : instruction.callArgs) args.push_back(operand(argument, registers)); value = call(instruction.callee, args); break; }
        case SSAInstructionKind::LoadLocal: { const auto found = locals.find(instruction.local); if (found == locals.end()) fail(ExecutionStatus::RuntimeError, "uninitialized-local", "read of " + instruction.local.str()); value = found->second; break; }
        case SSAInstructionKind::StoreLocal: locals[instruction.local] = operand(instruction.operand1, registers); break;
        case SSAInstructionKind::LoadIndex: value = loadIndex(locals, instruction.local, indexOf(operand(instruction.operand1, registers))); break;
        case SSAInstructionKind::StoreIndex: storeIndex(locals, instruction.local, indexOf(operand(instruction.operand1, registers)), operand(instruction.operand2, registers)); break;
        }
        if (instruction.result) registers[instruction.result->index] = std::move(value);
      }
      if (const auto jump = std::get_if<SSATerminator::Jump>(&block.terminator.value)) {
        predecessor = block.id; current = blocks.find(jump->target.index);
        if (current == blocks.end()) fail(ExecutionStatus::InternalError, "bad-jump", "unknown jump target"); continue;
      }
      if (const auto branch = std::get_if<SSATerminator::Branch>(&block.terminator.value)) {
        predecessor = block.id;
        current = blocks.find((truth(operand(branch->condition, registers)) ? branch->thenTarget : branch->elseTarget).index);
        if (current == blocks.end()) fail(ExecutionStatus::InternalError, "bad-branch", "unknown branch target"); continue;
      }
      if (const auto returned = std::get_if<SSATerminator::Return>(&block.terminator.value))
        return returned->value ? std::optional<RuntimeValue>(operand(*returned->value, registers)) : std::nullopt;
      fail(ExecutionStatus::UnsupportedFeature, "unreachable", "unreachable terminator executed");
    }
  }
public:
  ExecutionResult run(const SSAProgram &program, std::string entry = "main", ExecutionLimits limits = {}) {
    program_ = &program; limits_ = limits; result_ = {}; callDepth_ = 0;
    try {
      const auto value = call(entry, {});
      const SSAFunction *function = nullptr;
      for (const auto &candidate : program.functions) if (candidate.name == entry) { function = &candidate; break; }
      if (function && function->returnType.kind != LangTypeKind::Void) result_.returnValue = value;
    } catch (const std::runtime_error &) {
      if (result_.status == ExecutionStatus::Success) { result_.status = ExecutionStatus::InternalError; result_.error = RuntimeError{"internal", "interpreter failure"}; }
    }
    return result_;
  }
};

} // namespace nanopass
