#pragma once

#include "../ast/expr.h"
#include "../ast/stmt.h"


struct DesugarIncDecPass {

  /* ===== ENTRY ===== */

  unique_ptr<Stmt> transformStmt(unique_ptr<Stmt> stmt) {

    if (auto b = dynamic_cast<BlockStmt *>(stmt.get())) {
      auto nb = make_unique<BlockStmt>();
      for (auto &s : b->stmts)
        nb->stmts.push_back(transformStmt(std::move(s)));
      return nb;
    }

    if (auto e = dynamic_cast<ExprStmt *>(stmt.get())) {
      return desugarExprStmt(std::move(stmt));
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

    if (auto r = dynamic_cast<ReturnStmt *>(stmt.get())) {
      if (r->value)
        r->value = transformExpr(std::move(r->value));
      return stmt;
    }

    return stmt;
  }

private:
  /* ===== STATEMENT-LEVEL DESUGARING ===== */

  unique_ptr<Stmt> desugarExprStmt(unique_ptr<Stmt> stmt) {
    auto *es = static_cast<ExprStmt *>(stmt.get());

    // Only desugar top-level ++ / --
    if (auto u = dynamic_cast<UnaryExpr *>(es->e.get())) {

      if (u->op == "++" || u->op == "--") {

        auto *var = dynamic_cast<VariableExpr *>(u->right.get());
        if (!var)
          throw runtime_error("++/-- requires variable");

        string name = var->name;
        string op = (u->op == "++") ? "+" : "-";

        // x++  ->  x = x + 1
        return make_unique<ExprStmt>(make_unique<BinaryExpr>(
            "=", make_unique<VariableExpr>(name),
            make_unique<BinaryExpr>(op, make_unique<VariableExpr>(name),
                                    make_unique<NumberExpr>(1))));
      }
    }

    // Otherwise just recurse
    es->e = transformExpr(std::move(es->e));
    return stmt;
  }

  /* ===== EXPRESSION TRANSFORM ===== */

  unique_ptr<Expr> transformExpr(unique_ptr<Expr> expr) {

    if (auto b = dynamic_cast<BinaryExpr *>(expr.get())) {
      b->left = transformExpr(std::move(b->left));
      b->right = transformExpr(std::move(b->right));
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

    return expr;
  }
};
