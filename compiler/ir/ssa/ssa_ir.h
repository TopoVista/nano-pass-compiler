#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "ir/cfg/cfg_ir.h"

namespace nanopass {

// SSA operands intentionally reuse CFG constants and stable ValueId identity.
// Locals never occur as scalar SSA operands after promotion.
using SSAOperand = std::variant<CoreConstantVal, ValueId>;

struct SSAParameter {
  ValueId value;
  LocalId sourceLocal;
  LangType type;
};

struct PhiIncoming {
  BlockId predecessor;
  ValueId value;
};

struct PhiInstruction {
  ValueId result;
  LocalId originalLocal;
  LangType type;
  std::vector<PhiIncoming> incoming;
  SourceOrigin origin;
};

enum class SSAInstructionKind { Constant, Copy, Unary, Binary, Call, LoadLocal, StoreLocal, LoadIndex, StoreIndex };

struct SSAInstruction {
  SSAInstructionKind kind;
  std::optional<ValueId> result;
  LangType type = LangType::Unknown();
  CoreConstantVal constantValue;
  UnaryOpKind unaryOp = UnaryOpKind::Negate;
  BinaryOpKind binaryOp = BinaryOpKind::Add;
  SSAOperand operand1;
  SSAOperand operand2;
  std::string callee;
  std::vector<SSAOperand> callArgs;
  LocalId local;
  SourceOrigin origin;
};

struct SSATerminator {
  struct Jump { BlockId target; };
  struct Branch { SSAOperand condition; BlockId thenTarget; BlockId elseTarget; };
  struct Return { std::optional<SSAOperand> value; };
  struct Unreachable {};
  std::variant<Jump, Branch, Return, Unreachable> value;
};

struct SSABlock {
  BlockId id;
  std::string label;
  std::vector<PhiInstruction> phis;
  std::vector<SSAInstruction> instructions;
  SSATerminator terminator;
  std::vector<BlockId> predecessors;
  std::vector<BlockId> successors;
};

struct SSAFunction {
  std::string name;
  LangType returnType;
  std::vector<SSAParameter> parameters;
  BlockId entryBlock;
  std::vector<SSABlock> blocks;
  // Source locals eliminated by mem2reg.  Kept as metadata so verifiers and
  // later passes can distinguish deliberately memory-backed locals from a
  // broken promotion.
  std::vector<LocalId> promotedLocals;
  SourceOrigin origin;
};

struct SSAProgram { std::vector<SSAFunction> functions; };

struct SSAStatistics {
  size_t functions = 0;
  size_t blocks = 0;
  size_t values = 0;
  size_t phiNodes = 0;
  size_t promotableLocals = 0;
  size_t promotedLocals = 0;
  size_t loadsRemoved = 0;
  size_t storesRemoved = 0;
  size_t instructions = 0;
};

} // namespace nanopass
