#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "common/source_location.h"
#include "sema/type.h"

namespace nanopass {

// ============================================================
// Source Provenance
// ============================================================
struct SourceOrigin {
  SourceLocation loc;
  std::string transformation;

  SourceOrigin() = default;
  SourceOrigin(SourceLocation l, std::string t = "")
      : loc(l), transformation(std::move(t)) {}
};

// ============================================================
// Stable Identifiers
// ============================================================
struct LocalId {
  std::string name;
  int index = 0; // Uniquely distinguishes shadowed instances

  bool operator==(const LocalId &other) const {
    return name == other.name && index == other.index;
  }

  bool operator!=(const LocalId &other) const { return !(*this == other); }

  bool operator<(const LocalId &other) const {
    if (name != other.name)
      return name < other.name;
    return index < other.index;
  }

  std::string str() const {
    return name + "#" + std::to_string(index);
  }
};

// ============================================================
// Operator Enums
// ============================================================
enum class BinaryOpKind {
  Add,
  Sub,
  Mul,
  Div,
  Mod,
  Eq,
  Ne,
  Lt,
  Le,
  Gt,
  Ge,
  LogicalAnd,
  LogicalOr
};

inline const char *binaryOpToString(BinaryOpKind op) {
  switch (op) {
  case BinaryOpKind::Add: return "add";
  case BinaryOpKind::Sub: return "sub";
  case BinaryOpKind::Mul: return "mul";
  case BinaryOpKind::Div: return "div";
  case BinaryOpKind::Mod: return "mod";
  case BinaryOpKind::Eq: return "eq";
  case BinaryOpKind::Ne: return "ne";
  case BinaryOpKind::Lt: return "lt";
  case BinaryOpKind::Le: return "le";
  case BinaryOpKind::Gt: return "gt";
  case BinaryOpKind::Ge: return "ge";
  case BinaryOpKind::LogicalAnd: return "and";
  case BinaryOpKind::LogicalOr: return "or";
  }
  return "unknown";
}

enum class UnaryOpKind {
  Negate,
  LogicalNot
};

inline const char *unaryOpToString(UnaryOpKind op) {
  switch (op) {
  case UnaryOpKind::Negate: return "neg";
  case UnaryOpKind::LogicalNot: return "not";
  }
  return "unknown";
}

// ============================================================
// Constant Literal Representation
// ============================================================
using CoreConstantVal = std::variant<int64_t, double, bool, std::string>;

// ============================================================
// Core Expressions
// ============================================================
struct CoreExpr {
  LangType type = LangType::Unknown();
  SourceOrigin origin;

  virtual ~CoreExpr() = default;
};

struct CoreConstantExpr : CoreExpr {
  CoreConstantVal value;

  explicit CoreConstantExpr(CoreConstantVal val, LangType t, SourceOrigin orig = {})
      : value(std::move(val)) {
    type = std::move(t);
    origin = std::move(orig);
  }
};

struct CoreVariableRefExpr : CoreExpr {
  LocalId id;

  explicit CoreVariableRefExpr(LocalId i, LangType t, SourceOrigin orig = {})
      : id(std::move(i)) {
    type = std::move(t);
    origin = std::move(orig);
  }
};

struct CoreBinaryExpr : CoreExpr {
  BinaryOpKind op;
  std::unique_ptr<CoreExpr> left;
  std::unique_ptr<CoreExpr> right;

  CoreBinaryExpr(BinaryOpKind o, std::unique_ptr<CoreExpr> l,
                 std::unique_ptr<CoreExpr> r, LangType t, SourceOrigin orig = {})
      : op(o), left(std::move(l)), right(std::move(r)) {
    type = std::move(t);
    origin = std::move(orig);
  }
};

struct CoreUnaryExpr : CoreExpr {
  UnaryOpKind op;
  std::unique_ptr<CoreExpr> operand;

  CoreUnaryExpr(UnaryOpKind o, std::unique_ptr<CoreExpr> operand_, LangType t,
                SourceOrigin orig = {})
      : op(o), operand(std::move(operand_)) {
    type = std::move(t);
    origin = std::move(orig);
  }
};

struct CoreCallExpr : CoreExpr {
  std::string callee;
  std::vector<std::unique_ptr<CoreExpr>> args;

  CoreCallExpr(std::string c, std::vector<std::unique_ptr<CoreExpr>> a,
               LangType t, SourceOrigin orig = {})
      : callee(std::move(c)), args(std::move(a)) {
    type = std::move(t);
    origin = std::move(orig);
  }
};

struct CoreIndexExpr : CoreExpr {
  std::unique_ptr<CoreExpr> array;
  std::unique_ptr<CoreExpr> index;

  CoreIndexExpr(std::unique_ptr<CoreExpr> arr, std::unique_ptr<CoreExpr> idx,
                LangType t, SourceOrigin orig = {})
      : array(std::move(arr)), index(std::move(idx)) {
    type = std::move(t);
    origin = std::move(orig);
  }
};

// ============================================================
// Core Statements
// ============================================================
struct CoreStmt {
  SourceOrigin origin;
  virtual ~CoreStmt() = default;
};

struct CoreVarDeclStmt : CoreStmt {
  LocalId id;
  LangType type;
  std::unique_ptr<CoreExpr> initializer;

  CoreVarDeclStmt(LocalId i, LangType t, std::unique_ptr<CoreExpr> init,
                  SourceOrigin orig = {})
      : id(std::move(i)), type(std::move(t)), initializer(std::move(init)) {
    origin = std::move(orig);
  }
};

struct CoreAssignStmt : CoreStmt {
  LocalId target;
  std::unique_ptr<CoreExpr> value;

  CoreAssignStmt(LocalId tgt, std::unique_ptr<CoreExpr> val, SourceOrigin orig = {})
      : target(std::move(tgt)), value(std::move(val)) {
    origin = std::move(orig);
  }
};

struct CoreIndexAssignStmt : CoreStmt {
  LocalId targetArray;
  std::unique_ptr<CoreExpr> index;
  std::unique_ptr<CoreExpr> value;

  CoreIndexAssignStmt(LocalId arr, std::unique_ptr<CoreExpr> idx,
                      std::unique_ptr<CoreExpr> val, SourceOrigin orig = {})
      : targetArray(std::move(arr)), index(std::move(idx)), value(std::move(val)) {
    origin = std::move(orig);
  }
};

struct CoreExprStmt : CoreStmt {
  std::unique_ptr<CoreExpr> expr;

  explicit CoreExprStmt(std::unique_ptr<CoreExpr> e, SourceOrigin orig = {})
      : expr(std::move(e)) {
    origin = std::move(orig);
  }
};

struct CoreIfStmt : CoreStmt {
  std::unique_ptr<CoreExpr> condition;
  std::vector<std::unique_ptr<CoreStmt>> thenBranch;
  std::vector<std::unique_ptr<CoreStmt>> elseBranch;

  CoreIfStmt(std::unique_ptr<CoreExpr> c,
             std::vector<std::unique_ptr<CoreStmt>> t,
             std::vector<std::unique_ptr<CoreStmt>> e = {},
             SourceOrigin orig = {})
      : condition(std::move(c)), thenBranch(std::move(t)),
        elseBranch(std::move(e)) {
    origin = std::move(orig);
  }
};

struct CoreWhileStmt : CoreStmt {
  std::unique_ptr<CoreExpr> condition;
  std::vector<std::unique_ptr<CoreStmt>> body;

  CoreWhileStmt(std::unique_ptr<CoreExpr> c,
                std::vector<std::unique_ptr<CoreStmt>> b,
                SourceOrigin orig = {})
      : condition(std::move(c)), body(std::move(b)) {
    origin = std::move(orig);
  }
};

struct CoreReturnStmt : CoreStmt {
  std::unique_ptr<CoreExpr> value; // nullptr for void return

  explicit CoreReturnStmt(std::unique_ptr<CoreExpr> v = nullptr,
                          SourceOrigin orig = {})
      : value(std::move(v)) {
    origin = std::move(orig);
  }
};

struct CoreBreakStmt : CoreStmt {
  explicit CoreBreakStmt(SourceOrigin orig = {}) { origin = std::move(orig); }
};

struct CoreContinueStmt : CoreStmt {
  explicit CoreContinueStmt(SourceOrigin orig = {}) { origin = std::move(orig); }
};

// ============================================================
// Core Function & Program
// ============================================================
struct CoreParam {
  LocalId id;
  LangType type;
};

struct CoreFunction {
  std::string name;
  LangType returnType;
  std::vector<CoreParam> params;
  std::vector<std::unique_ptr<CoreStmt>> body;
  SourceOrigin origin;
};

struct CoreProgram {
  std::vector<CoreFunction> functions;
  std::vector<std::unique_ptr<CoreStmt>> topLevelStmts;
};

} // namespace nanopass
