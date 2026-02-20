#pragma once

#include "../ast/expr.h"
#include "../ast/stmt.h"


struct DesugarPlusAssignPass {

  /* ========= ENTRY ========= */

  unique_ptr<Stmt> transformStmt(unique_ptr<Stmt> stmt) {

    if (auto b = dynamic_cast<BlockStmt *>(stmt.get())) {
      auto nb = make_unique<BlockStmt>();
      for (auto &s : b->stmts)
        nb->stmts.push_back(transformStmt(std::move(s)));
      return nb;
    }

    if (auto e = dynamic_cast<ExprStmt *>(stmt.get())) {
      e->e = transformExpr(std::move(e->e));
      return stmt;
    }

    if (auto p = dynamic_cast<PrintStmt *>(stmt.get())) {
      p->e = transformExpr(std::move(p->e));
      return stmt;
    }

    if (auto i = dynamic_cast<IfStmt *>(stmt.get())) {
      i->condition = transformExpr(std::move(i->condition));
      i->thenBranch = transformStmt(std::move(i->thenBranch));
      if (i->elseBranch)
        i->elseBranch = transformStmt(std::move(i->elseBranch));
      return stmt;
    }

    if (auto w = dynamic_cast<WhileStmt *>(stmt.get())) {
      w->condition = transformExpr(std::move(w->condition));
      w->body = transformStmt(std::move(w->body));
      return stmt;
    }

    if (auto f = dynamic_cast<ForStmt *>(stmt.get())) {
      if (f->init)
        f->init = transformStmt(std::move(f->init));
      if (f->condition)
        f->condition = transformExpr(std::move(f->condition));
      if (f->increment)
        f->increment = transformExpr(std::move(f->increment));
      f->body = transformStmt(std::move(f->body));
      return stmt;
    }

    if (auto r = dynamic_cast<ReturnStmt *>(stmt.get())) {
      if (r->value)
        r->value = transformExpr(std::move(r->value));
      return stmt;
    }

    return stmt;
  }

private:
  /* ========= EXPRESSION TRANSFORM ========= */

  unique_ptr<Expr> transformExpr(unique_ptr<Expr> expr) {

    if (auto b = dynamic_cast<BinaryExpr *>(expr.get())) {

      // Recursively transform children first
      b->left = transformExpr(std::move(b->left));
      b->right = transformExpr(std::move(b->right));

      // Desugar +=
      if (b->op == "+=") {
        // lhs must be variable
        auto *var = dynamic_cast<VariableExpr *>(b->left.get());
        if (!var)
          throw runtime_error("Left side of += must be a variable");

        string name = var->name;

        // a += b  -->  a = a + b
        auto newRight = make_unique<BinaryExpr>(
            "+", make_unique<VariableExpr>(name), std::move(b->right));

        return make_unique<BinaryExpr>("=", make_unique<VariableExpr>(name),
                                       std::move(newRight));
      }

      return expr;
    }

    if (auto u = dynamic_cast<UnaryExpr *>(expr.get())) {
      u->right = transformExpr(std::move(u->right));
      return expr;
    }

    if (auto c = dynamic_cast<CallExpr *>(expr.get())) {
      for (auto &a : c->args)
        a = transformExpr(std::move(a));
      return expr;
    }

    // NumberExpr, VariableExpr → unchanged
    return expr;
  }
};
