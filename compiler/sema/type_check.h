#pragma once

#include <memory>
#include <stdexcept>
#include <vector>


#include "../ast/expr.h"
#include "../ast/stmt.h"
#include "../common/error.h"
#include "symbol.h"
#include "type.h"


using namespace std;

struct TypeCheckPass {

  LangType currentFunctionReturnType;
  bool hasReturn = false;

  /* ================= PROGRAM ================= */

  void check(const vector<unique_ptr<Stmt>> &program) {

    for (auto &s : program)
      checkStmt(s.get());

    bool foundMain = false;

    for (auto &s : program) {
      if (auto fn = dynamic_cast<FunctionStmt *>(s.get())) {
        if (fn->name == "main") {
          foundMain = true;
          if (fn->returnType.kind != LangTypeKind::Integer)
            throw CompileError("main must return int", fn->loc.line,
                               fn->loc.col);
        }
      }
    }

    if (!foundMain)
      throw CompileError("Program must define main function", 0, 0);
  }

  /* ================= STATEMENTS ================= */

  void checkStmt(Stmt *stmt) {

    if (auto s = dynamic_cast<ExprStmt *>(stmt))
      checkExpr(s->e.get());

    else if (auto s = dynamic_cast<PrintStmt *>(stmt))
      checkExpr(s->e.get());

    else if (auto s = dynamic_cast<BlockStmt *>(stmt))
      for (auto &x : s->stmts)
        checkStmt(x.get());

    else if (auto s = dynamic_cast<VarDeclStmt *>(stmt)) {

      if (s->initializer) {
        LangType initType = checkExpr(s->initializer.get());

        if (!isAssignable(s->type, initType))
          throw CompileError("Type mismatch in variable declaration",
                             s->loc.line, s->loc.col);
      }
    }

    else if (auto s = dynamic_cast<IfStmt *>(stmt)) {

      LangType cond = checkExpr(s->condition.get());

      if (cond.kind != LangTypeKind::Bool && cond.kind != LangTypeKind::Integer)
        throw CompileError("If condition must be bool or int",
                           s->condition->loc.line, s->condition->loc.col);

      checkStmt(s->thenBranch.get());
      if (s->elseBranch)
        checkStmt(s->elseBranch.get());
    }

    else if (auto s = dynamic_cast<WhileStmt *>(stmt)) {

      LangType cond = checkExpr(s->condition.get());

      if (cond.kind != LangTypeKind::Bool && cond.kind != LangTypeKind::Integer)
        throw CompileError("While condition must be bool or int",
                           s->condition->loc.line, s->condition->loc.col);

      checkStmt(s->body.get());
    }

    else if (auto s = dynamic_cast<ReturnStmt *>(stmt)) {

      hasReturn = true;

      if (!s->value && currentFunctionReturnType.kind != LangTypeKind::Void)
        throw CompileError("Return value required", s->loc.line, s->loc.col);

      if (s->value) {

        LangType rt = checkExpr(s->value.get());

        if (!isAssignable(currentFunctionReturnType, rt))
          throw CompileError("Return type mismatch", s->loc.line, s->loc.col);
      }
    }

    else if (auto s = dynamic_cast<FunctionStmt *>(stmt)) {

      currentFunctionReturnType = s->returnType;
      hasReturn = false;

      for (auto &b : s->body->stmts)
        checkStmt(b.get());

      if (s->returnType.kind != LangTypeKind::Void && !hasReturn)
        throw CompileError("Non-void function must return a value", s->loc.line,
                           s->loc.col);
    }
  }

  /* ================= EXPRESSIONS ================= */

  LangType checkExpr(Expr *expr) {

    /* ===== NUMBER ===== */
    if (auto *n = dynamic_cast<NumberExpr *>(expr)) {
      if (n->isFloat)
        return expr->type = LangType::Float(64);
      return expr->type = LangType::Int(32);
    }

    /* ===== BOOL ===== */
    if (auto *b = dynamic_cast<BoolExpr *>(expr))
      return expr->type = LangType::Bool();

    /* ===== STRING ===== */
    if (auto *s = dynamic_cast<StringExpr *>(expr))
      return expr->type = LangType::String();

    /* ===== VARIABLE ===== */
    if (auto *v = dynamic_cast<VariableExpr *>(expr)) {

      if (!v->symbol)
        throw CompileError("Use of undeclared variable '" + v->name + "'",
                           v->loc.line, v->loc.col);

      return expr->type = v->symbol->type;
    }

    /* ===== ARRAY ACCESS (IndexExpr in YOUR AST) ===== */
    if (auto *idx = dynamic_cast<IndexExpr *>(expr)) {

      LangType arrType = checkExpr(idx->array.get());
      LangType indexType = checkExpr(idx->index.get());

      if (arrType.kind != LangTypeKind::Array)
        throw CompileError("Subscripted value is not an array", idx->loc.line,
                           idx->loc.col);

      if (indexType.kind != LangTypeKind::Integer)
        throw CompileError("Array index must be integer", idx->loc.line,
                           idx->loc.col);

      return expr->type = *arrType.element;
    }

    /* ===== UNARY ===== */
    if (auto *u = dynamic_cast<UnaryExpr *>(expr)) {

      LangType rt = checkExpr(u->right.get());

      if (u->op == "!") {
        if (rt.kind != LangTypeKind::Bool && rt.kind != LangTypeKind::Integer)
          throw CompileError("'!' expects bool or int", u->loc.line,
                             u->loc.col);

        return expr->type = LangType::Bool();
      }

      if (u->op == "-") {
        if (rt.kind != LangTypeKind::Integer &&
            rt.kind != LangTypeKind::Floating)
          throw CompileError("Unary '-' expects numeric", u->loc.line,
                             u->loc.col);

        return expr->type = rt;
      }
    }

    /* ===== BINARY ===== */
    if (auto *b = dynamic_cast<BinaryExpr *>(expr)) {

      LangType L = checkExpr(b->left.get());
      LangType R = checkExpr(b->right.get());

      if (b->op == "=") {

        // Left must be variable or index
        if (!dynamic_cast<VariableExpr *>(b->left.get()) &&
            !dynamic_cast<IndexExpr *>(b->left.get())) {
          throw CompileError("Invalid assignment target", b->loc.line,
                             b->loc.col);
        }

        LangType L = checkExpr(b->left.get());
        LangType R = checkExpr(b->right.get());

        if (!isAssignable(L, R))
          throw CompileError("Assignment type mismatch", b->loc.line,
                             b->loc.col);

        return expr->type = L;
      }

      if (b->op == "+" || b->op == "-" || b->op == "*" || b->op == "/") {

        if ((L.kind != LangTypeKind::Integer &&
             L.kind != LangTypeKind::Floating) ||
            (R.kind != LangTypeKind::Integer &&
             R.kind != LangTypeKind::Floating))
          throw CompileError("Arithmetic requires numeric operands",
                             b->loc.line, b->loc.col);

        if (L.kind == LangTypeKind::Floating ||
            R.kind == LangTypeKind::Floating)
          return expr->type = LangType::Float(64);

        return expr->type = LangType::Int(32);
      }

      if (b->op == "<" || b->op == "<=" || b->op == ">" || b->op == ">=" ||
          b->op == "==" || b->op == "!=")
        return expr->type = LangType::Bool();

      if (b->op == "&&" || b->op == "||")
        return expr->type = LangType::Bool();
    }

    /* ===== CALL ===== */
    if (auto *c = dynamic_cast<CallExpr *>(expr)) {

      if (!c->symbol || c->symbol->kind != SymbolKind::Function)
        throw CompileError("Attempt to call non-function '" + c->callee + "'",
                           c->loc.line, c->loc.col);

      if (c->args.size() != c->symbol->paramTypes.size())
        throw CompileError("Incorrect number of arguments", c->loc.line,
                           c->loc.col);

      for (size_t i = 0; i < c->args.size(); i++) {

        LangType argType = checkExpr(c->args[i].get());

        if (!isAssignable(c->symbol->paramTypes[i], argType))
          throw CompileError("Argument type mismatch", c->loc.line, c->loc.col);
      }

      return expr->type =
                 c->symbol->type.ret ? *c->symbol->type.ret : LangType::Void();
    }

    return LangType::Unknown();
  }

  /* ================= ASSIGNMENT RULES ================= */

  bool isAssignable(const LangType &target, const LangType &value) {

    if (sameType(target, value))
      return true;

    if (target.kind == LangTypeKind::Floating &&
        value.kind == LangTypeKind::Integer)
      return true;

    return false;
  }
};
