#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "ir/core/core_ir.h"
#include "sema/type.h"

namespace nanopass {

// ============================================================
// Stable Temporary Identifier
// ============================================================
struct TemporaryId {
  int index = 0;

  bool operator==(const TemporaryId &o) const { return index == o.index; }
  bool operator!=(const TemporaryId &o) const { return index != o.index; }
  bool operator<(const TemporaryId &o) const { return index < o.index; }

  std::string str() const { return "%t" + std::to_string(index); }
};

// ============================================================
// ANF Atomic Value
// ============================================================
enum class ANFAtomKind { Constant, Variable, Temporary };

struct ANFAtom {
  ANFAtomKind kind;
  CoreConstantVal constantValue;
  LocalId variableId;
  TemporaryId temporaryId;
  LangType type = LangType::Unknown();

  static ANFAtom fromConstant(CoreConstantVal val, LangType t) {
    ANFAtom a;
    a.kind = ANFAtomKind::Constant;
    a.constantValue = std::move(val);
    a.type = std::move(t);
    return a;
  }

  static ANFAtom fromVariable(LocalId id, LangType t) {
    ANFAtom a;
    a.kind = ANFAtomKind::Variable;
    a.variableId = std::move(id);
    a.type = std::move(t);
    return a;
  }

  static ANFAtom fromTemporary(TemporaryId id, LangType t) {
    ANFAtom a;
    a.kind = ANFAtomKind::Temporary;
    a.temporaryId = id;
    a.type = std::move(t);
    return a;
  }

  std::string str() const {
    switch (kind) {
    case ANFAtomKind::Constant: {
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
    case ANFAtomKind::Variable:
      return variableId.str();
    case ANFAtomKind::Temporary:
      return temporaryId.str();
    }
    return "<bad-atom>";
  }
};

// ============================================================
// ANF Non-Trivial Computations
// ============================================================
enum class ANFComputationKind { Binary, Unary, Call, IndexLoad };

struct ANFComputation {
  ANFComputationKind kind;
  std::optional<TemporaryId> dst; // None for void call
  LangType type = LangType::Unknown();

  // Binary
  BinaryOpKind binOp = BinaryOpKind::Add;
  ANFAtom left;
  ANFAtom right;

  // Unary
  UnaryOpKind unOp = UnaryOpKind::Negate;
  ANFAtom operand;

  // Call
  std::string callee;
  std::vector<ANFAtom> args;

  // IndexLoad
  LocalId array;
  ANFAtom index;
  SourceOrigin origin;

  static ANFComputation makeBinary(TemporaryId dst, BinaryOpKind op,
                                   ANFAtom left, ANFAtom right, LangType t,
                                   SourceOrigin orig = {}) {
    ANFComputation c;
    c.kind = ANFComputationKind::Binary;
    c.dst = dst;
    c.binOp = op;
    c.left = std::move(left);
    c.right = std::move(right);
    c.type = std::move(t);
    c.origin = std::move(orig);
    return c;
  }

  static ANFComputation makeUnary(TemporaryId dst, UnaryOpKind op,
                                  ANFAtom operand, LangType t,
                                  SourceOrigin orig = {}) {
    ANFComputation c;
    c.kind = ANFComputationKind::Unary;
    c.dst = dst;
    c.unOp = op;
    c.operand = std::move(operand);
    c.type = std::move(t);
    c.origin = std::move(orig);
    return c;
  }

  static ANFComputation makeCall(std::optional<TemporaryId> dst,
                                 std::string callee, std::vector<ANFAtom> args,
                                 LangType t, SourceOrigin orig = {}) {
    ANFComputation c;
    c.kind = ANFComputationKind::Call;
    c.dst = dst;
    c.callee = std::move(callee);
    c.args = std::move(args);
    c.type = std::move(t);
    c.origin = std::move(orig);
    return c;
  }

  static ANFComputation makeIndexLoad(TemporaryId dst, LocalId arr,
                                      ANFAtom idx, LangType t,
                                      SourceOrigin orig = {}) {
    ANFComputation c;
    c.kind = ANFComputationKind::IndexLoad;
    c.dst = dst;
    c.array = std::move(arr);
    c.index = std::move(idx);
    c.type = std::move(t);
    c.origin = std::move(orig);
    return c;
  }
};

// ============================================================
// ANF Statements
// ============================================================
struct ANFStmt {
  SourceOrigin origin;
  virtual ~ANFStmt() = default;
};

struct ANFComputeStmt : ANFStmt {
  ANFComputation comp;

  explicit ANFComputeStmt(ANFComputation c) : comp(std::move(c)) {
    origin = comp.origin;
  }
};

struct ANFVarDeclStmt : ANFStmt {
  LocalId id;
  LangType type;
  std::optional<ANFAtom> init;

  ANFVarDeclStmt(LocalId i, LangType t, std::optional<ANFAtom> in = std::nullopt,
                 SourceOrigin orig = {})
      : id(std::move(i)), type(std::move(t)), init(std::move(in)) {
    origin = std::move(orig);
  }
};

struct ANFAssignStmt : ANFStmt {
  LocalId target;
  ANFAtom value;

  ANFAssignStmt(LocalId tgt, ANFAtom val, SourceOrigin orig = {})
      : target(std::move(tgt)), value(std::move(val)) {
    origin = std::move(orig);
  }
};

struct ANFIndexAssignStmt : ANFStmt {
  LocalId targetArray;
  ANFAtom index;
  ANFAtom value;

  ANFIndexAssignStmt(LocalId arr, ANFAtom idx, ANFAtom val,
                     SourceOrigin orig = {})
      : targetArray(std::move(arr)), index(std::move(idx)),
        value(std::move(val)) {
    origin = std::move(orig);
  }
};

struct ANFIfStmt : ANFStmt {
  ANFAtom condition;
  std::vector<std::unique_ptr<ANFStmt>> thenBranch;
  std::vector<std::unique_ptr<ANFStmt>> elseBranch;

  ANFIfStmt(ANFAtom c, std::vector<std::unique_ptr<ANFStmt>> t,
            std::vector<std::unique_ptr<ANFStmt>> e = {},
            SourceOrigin orig = {})
      : condition(std::move(c)), thenBranch(std::move(t)),
        elseBranch(std::move(e)) {
    origin = std::move(orig);
  }
};

struct ANFWhileStmt : ANFStmt {
  std::vector<std::unique_ptr<ANFStmt>> condHeader;
  ANFAtom condition;
  std::vector<std::unique_ptr<ANFStmt>> body;

  ANFWhileStmt(std::vector<std::unique_ptr<ANFStmt>> ch, ANFAtom c,
               std::vector<std::unique_ptr<ANFStmt>> b,
               SourceOrigin orig = {})
      : condHeader(std::move(ch)), condition(std::move(c)), body(std::move(b)) {
    origin = std::move(orig);
  }
};

struct ANFReturnStmt : ANFStmt {
  std::optional<ANFAtom> value;

  explicit ANFReturnStmt(std::optional<ANFAtom> v = std::nullopt,
                         SourceOrigin orig = {})
      : value(std::move(v)) {
    origin = std::move(orig);
  }
};

struct ANFBreakStmt : ANFStmt {
  explicit ANFBreakStmt(SourceOrigin orig = {}) { origin = std::move(orig); }
};

struct ANFContinueStmt : ANFStmt {
  explicit ANFContinueStmt(SourceOrigin orig = {}) { origin = std::move(orig); }
};

// ============================================================
// ANF Function & Program
// ============================================================
struct ANFFunction {
  std::string name;
  LangType returnType;
  std::vector<CoreParam> params;
  std::vector<std::unique_ptr<ANFStmt>> body;
  SourceOrigin origin;
};

struct ANFProgram {
  std::vector<ANFFunction> functions;
  std::vector<std::unique_ptr<ANFStmt>> topLevelStmts;
};

} // namespace nanopass
