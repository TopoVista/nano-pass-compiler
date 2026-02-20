#pragma once
#include <memory>
#include <stdexcept>
#include <vector>

#include "../ast/expr.h"
#include "../ast/stmt.h"
#include "../common/error.h"
#include "symbol_table.h"

using namespace std;

class ResolveScopesPass {
  SymbolTable table;

public:
  void resolve(const vector<unique_ptr<Stmt>> &program) {
    for (auto &s : program)
      resolveStmt(s.get());
  }

private:
  // ---------------- Statements ----------------

  void resolveStmt(Stmt *stmt) {

    // ---------------- BLOCK ----------------
    if (auto s = dynamic_cast<BlockStmt *>(stmt)) {
      table.enterScope();
      for (auto &st : s->stmts)
        resolveStmt(st.get());
      table.exitScope();
      return;
    }

    // ---------------- VARIABLE DECLARATION ----------------
    if (auto s = dynamic_cast<VarDeclStmt *>(stmt)) {

      if (table.isDeclaredInCurrentScope(s->name)) {
        throw CompileError("Redeclaration of variable '" + s->name + "'",
                           s->loc.line, s->loc.col);
      }

      table.declare(s->name, SymbolKind::Variable);

      auto sym = table.lookup(s->name);
      sym->type = s->type;

      if (s->initializer)
        resolveExpr(s->initializer.get());

      return;
    }

    // ---------------- FUNCTION ----------------
    if (auto s = dynamic_cast<FunctionStmt *>(stmt)) {

      if (table.isDeclaredInCurrentScope(s->name)) {
        throw CompileError("Redeclaration of function '" + s->name + "'",
                           s->loc.line, s->loc.col);
      }

      table.declare(s->name, SymbolKind::Function);

      auto fnSymbol = table.lookup(s->name);
      fnSymbol->type = s->returnType;
      fnSymbol->paramTypes.clear();

      for (auto &p : s->params)
        fnSymbol->paramTypes.push_back(p.second);

      table.enterScope();

      for (auto &p : s->params) {
        table.declare(p.first, SymbolKind::Variable);
        auto sym = table.lookup(p.first);
        sym->type = p.second;
      }

      resolveStmt(s->body.get());
      table.exitScope();
      return;
    }

    // ---------------- IF ----------------
    if (auto s = dynamic_cast<IfStmt *>(stmt)) {
      resolveExpr(s->condition.get());
      resolveStmt(s->thenBranch.get());
      if (s->elseBranch)
        resolveStmt(s->elseBranch.get());
      return;
    }

    // ---------------- WHILE ----------------
    if (auto s = dynamic_cast<WhileStmt *>(stmt)) {
      resolveExpr(s->condition.get());
      resolveStmt(s->body.get());
      return;
    }

    // ---------------- FOR ----------------
    if (auto s = dynamic_cast<ForStmt *>(stmt)) {
      table.enterScope();

      if (s->init)
        resolveStmt(s->init.get());
      if (s->condition)
        resolveExpr(s->condition.get());
      if (s->increment)
        resolveExpr(s->increment.get());

      resolveStmt(s->body.get());
      table.exitScope();
      return;
    }

    // ---------------- RETURN ----------------
    if (auto s = dynamic_cast<ReturnStmt *>(stmt)) {
      if (s->value)
        resolveExpr(s->value.get());
      return;
    }

    // ---------------- PRINT ----------------
    if (auto s = dynamic_cast<PrintStmt *>(stmt)) {
      resolveExpr(s->e.get());
      return;
    }

    // ---------------- EXPRESSION STATEMENT ----------------
    if (auto s = dynamic_cast<ExprStmt *>(stmt)) {
      resolveExpr(s->e.get());
      return;
    }
  }

  // ---------------- Expressions ----------------

  void resolveExpr(Expr *expr) {

    // ---------------- VARIABLE ----------------
    if (auto e = dynamic_cast<VariableExpr *>(expr)) {
      auto sym = table.lookup(e->name);
      if (!sym)
        throw CompileError("Use of undeclared variable '" + e->name + "'",
                           e->loc.line, e->loc.col);
      e->symbol = sym;
      return;
    }

    // ---------------- INDEX (ARRAY ACCESS) ----------------
    if (auto e = dynamic_cast<IndexExpr *>(expr)) {
      resolveExpr(e->array.get());
      resolveExpr(e->index.get());
      return;
    }

    // ---------------- BINARY ----------------
    if (auto e = dynamic_cast<BinaryExpr *>(expr)) {

      if (e->op == "=") {

        // Allow variable OR array[index]
        if (auto var = dynamic_cast<VariableExpr *>(e->left.get())) {

          auto sym = table.lookup(var->name);
          if (!sym)
            throw CompileError("Assignment to undeclared variable '" +
                                   var->name + "'",
                               e->loc.line, e->loc.col);

          var->symbol = sym;
        } else if (auto idx = dynamic_cast<IndexExpr *>(e->left.get())) {
          resolveExpr(idx->array.get());
          resolveExpr(idx->index.get());
        } else {
          throw CompileError("Invalid assignment target", e->loc.line,
                             e->loc.col);
        }

        resolveExpr(e->right.get());
        return;
      }

      resolveExpr(e->left.get());
      resolveExpr(e->right.get());
      return;
    }

    // ---------------- UNARY ----------------
    if (auto e = dynamic_cast<UnaryExpr *>(expr)) {
      resolveExpr(e->right.get());
      return;
    }

    // ---------------- CALL ----------------
    if (auto e = dynamic_cast<CallExpr *>(expr)) {

      auto sym = table.lookup(e->callee);
      if (!sym)
        throw CompileError("Call to undeclared function '" + e->callee + "'",
                           e->loc.line, e->loc.col);

      e->symbol = sym;

      for (auto &a : e->args)
        resolveExpr(a.get());

      return;
    }

    // ---------------- LITERALS ----------------
    if (dynamic_cast<NumberExpr *>(expr) || dynamic_cast<StringExpr *>(expr) ||
        dynamic_cast<BoolExpr *>(expr)) {
      return;
    }
  }
};
