#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "ir/core/core_ir.h"
#include "sema/type.h"

namespace nanopass {

// ============================================================
// Block and Value IDs
// ============================================================
struct BlockId {
  int index = 0;

  bool operator==(const BlockId &o) const { return index == o.index; }
  bool operator!=(const BlockId &o) const { return index != o.index; }
  bool operator<(const BlockId &o) const { return index < o.index; }

  std::string str() const { return "bb" + std::to_string(index); }
};

struct ValueId {
  int index = 0;

  bool operator==(const ValueId &o) const { return index == o.index; }
  bool operator!=(const ValueId &o) const { return index != o.index; }
  bool operator<(const ValueId &o) const { return index < o.index; }

  std::string str() const { return "%" + std::to_string(index); }
};

// ============================================================
// CFG Operands
// ============================================================
enum class CFGOperandKind { Constant, Value, Local };

struct CFGOperand {
  CFGOperandKind kind;
  CoreConstantVal constantValue;
  ValueId valueId;
  LocalId localId;
  LangType type = LangType::Unknown();

  static CFGOperand fromConstant(CoreConstantVal val, LangType t) {
    CFGOperand op;
    op.kind = CFGOperandKind::Constant;
    op.constantValue = std::move(val);
    op.type = std::move(t);
    return op;
  }

  static CFGOperand fromValue(ValueId id, LangType t) {
    CFGOperand op;
    op.kind = CFGOperandKind::Value;
    op.valueId = id;
    op.type = std::move(t);
    return op;
  }

  static CFGOperand fromLocal(LocalId id, LangType t) {
    CFGOperand op;
    op.kind = CFGOperandKind::Local;
    op.localId = std::move(id);
    op.type = std::move(t);
    return op;
  }

  std::string str() const {
    switch (kind) {
    case CFGOperandKind::Constant: {
      std::string out;
      std::visit([&out](auto &&val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, bool>) {
          out = (val ? "true" : "false");
        } else if constexpr (std::is_same_v<T, std::string>) {
          out = "\"" + val + "\"";
        } else {
          out = std::to_string(val);
        }
      }, constantValue);
      return out;
    }
    case CFGOperandKind::Value:
      return valueId.str();
    case CFGOperandKind::Local:
      return localId.str();
    }
    return "<bad-operand>";
  }
};

// ============================================================
// CFG Instructions
// ============================================================
enum class CFGInstructionKind {
  Constant,
  Copy,
  Unary,
  Binary,
  Call,
  LoadLocal,
  StoreLocal,
  LoadIndex,
  StoreIndex
};

struct CFGInstruction {
  CFGInstructionKind kind;
  std::optional<ValueId> dst;
  LangType type = LangType::Unknown();

  // Operands
  CoreConstantVal constantVal;
  UnaryOpKind unOp = UnaryOpKind::Negate;
  BinaryOpKind binOp = BinaryOpKind::Add;
  CFGOperand op1;
  CFGOperand op2;

  // Local / Call metadata
  LocalId local;
  std::string callee;
  std::vector<CFGOperand> callArgs;

  SourceOrigin origin;

  static CFGInstruction makeConstant(ValueId dst, CoreConstantVal val,
                                     LangType t, SourceOrigin orig = {}) {
    CFGInstruction i;
    i.kind = CFGInstructionKind::Constant;
    i.dst = dst;
    i.constantVal = std::move(val);
    i.type = std::move(t);
    i.origin = std::move(orig);
    return i;
  }

  static CFGInstruction makeCopy(ValueId dst, CFGOperand src, LangType t,
                                 SourceOrigin orig = {}) {
    CFGInstruction i;
    i.kind = CFGInstructionKind::Copy;
    i.dst = dst;
    i.op1 = std::move(src);
    i.type = std::move(t);
    i.origin = std::move(orig);
    return i;
  }

  static CFGInstruction makeUnary(ValueId dst, UnaryOpKind op, CFGOperand src,
                                  LangType t, SourceOrigin orig = {}) {
    CFGInstruction i;
    i.kind = CFGInstructionKind::Unary;
    i.dst = dst;
    i.unOp = op;
    i.op1 = std::move(src);
    i.type = std::move(t);
    i.origin = std::move(orig);
    return i;
  }

  static CFGInstruction makeBinary(ValueId dst, BinaryOpKind op, CFGOperand left,
                                   CFGOperand right, LangType t,
                                   SourceOrigin orig = {}) {
    CFGInstruction i;
    i.kind = CFGInstructionKind::Binary;
    i.dst = dst;
    i.binOp = op;
    i.op1 = std::move(left);
    i.op2 = std::move(right);
    i.type = std::move(t);
    i.origin = std::move(orig);
    return i;
  }

  static CFGInstruction makeCall(std::optional<ValueId> dst, std::string callee,
                                 std::vector<CFGOperand> args, LangType t,
                                 SourceOrigin orig = {}) {
    CFGInstruction i;
    i.kind = CFGInstructionKind::Call;
    i.dst = dst;
    i.callee = std::move(callee);
    i.callArgs = std::move(args);
    i.type = std::move(t);
    i.origin = std::move(orig);
    return i;
  }

  static CFGInstruction makeLoadLocal(ValueId dst, LocalId loc, LangType t,
                                      SourceOrigin orig = {}) {
    CFGInstruction i;
    i.kind = CFGInstructionKind::LoadLocal;
    i.dst = dst;
    i.local = std::move(loc);
    i.type = std::move(t);
    i.origin = std::move(orig);
    return i;
  }

  static CFGInstruction makeStoreLocal(LocalId loc, CFGOperand val,
                                       SourceOrigin orig = {}) {
    CFGInstruction i;
    i.kind = CFGInstructionKind::StoreLocal;
    i.local = std::move(loc);
    i.op1 = std::move(val);
    i.origin = std::move(orig);
    return i;
  }

  static CFGInstruction makeLoadIndex(ValueId dst, LocalId arr, CFGOperand idx,
                                      LangType t, SourceOrigin orig = {}) {
    CFGInstruction i;
    i.kind = CFGInstructionKind::LoadIndex;
    i.dst = dst;
    i.local = std::move(arr);
    i.op1 = std::move(idx);
    i.type = std::move(t);
    i.origin = std::move(orig);
    return i;
  }

  static CFGInstruction makeStoreIndex(LocalId arr, CFGOperand idx,
                                       CFGOperand val, SourceOrigin orig = {}) {
    CFGInstruction i;
    i.kind = CFGInstructionKind::StoreIndex;
    i.local = std::move(arr);
    i.op1 = std::move(idx);
    i.op2 = std::move(val);
    i.origin = std::move(orig);
    return i;
  }
};

// ============================================================
// Terminators
// ============================================================
struct JumpTerminator {
  BlockId target;
};

struct BranchTerminator {
  CFGOperand condition;
  BlockId thenTarget;
  BlockId elseTarget;
};

struct ReturnTerminator {
  std::optional<CFGOperand> value;
};

struct UnreachableTerminator {};

using Terminator = std::variant<JumpTerminator, BranchTerminator,
                                ReturnTerminator, UnreachableTerminator>;

// ============================================================
// Basic Block
// ============================================================
struct BasicBlock {
  BlockId id;
  std::string label;
  std::vector<CFGInstruction> instructions;
  std::optional<Terminator> terminator;
  std::vector<BlockId> predecessors;
  std::vector<BlockId> successors;

  bool hasTerminator() const { return terminator.has_value(); }
};

// ============================================================
// CFG Function & Program
// ============================================================
struct CFGFunction {
  std::string name;
  LangType returnType;
  std::vector<CoreParam> params;
  BlockId entryBlock;
  std::vector<std::unique_ptr<BasicBlock>> blocks;
  std::unordered_map<int, size_t> blockIndexMap;
  SourceOrigin origin;

  BasicBlock *getBlock(BlockId id) {
    auto it = blockIndexMap.find(id.index);
    if (it != blockIndexMap.end() && it->second < blocks.size())
      return blocks[it->second].get();
    return nullptr;
  }

  const BasicBlock *getBlock(BlockId id) const {
    auto it = blockIndexMap.find(id.index);
    if (it != blockIndexMap.end() && it->second < blocks.size())
      return blocks[it->second].get();
    return nullptr;
  }
};

struct CFGProgram {
  std::vector<CFGFunction> functions;
};

} // namespace nanopass
