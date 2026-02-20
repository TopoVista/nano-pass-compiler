#pragma once

#include "../ast/expr.h"
#include "../ast/stmt.h"


struct ANFPass {

  int tempCounter = 0;

  /* ===== ENTRY ===== */

  vector<unique_ptr<Stmt>> transformStmt(unique_ptr<Stmt> stmt) {
    vector<unique_ptr<Stmt>> out;

    if (auto b = dynamic_cast<BlockStmt *>(stmt.get())) {
      auto nb = make_unique<BlockStmt>();
      for (auto &s : b->stmts) {
        auto lowered = transformStmt(std::move(s));
        for (auto &x : lowered)
          nb->stmts.push_back(std::move(x));
      }
      out.push_back(std::move(nb));
      return out;
    }

    if (auto e = dynamic_cast<ExprStmt *>(stmt.get())) {
      auto r = transformExpr(std::move(e->e), out);
      out.push_back(make_unique<ExprStmt>(std::move(r)));
      return out;
    }

    if (auto p = dynamic_cast<PrintStmt *>(stmt.get())) {
      auto r = transformExpr(std::move(p->e), out);
      out.push_back(make_unique<PrintStmt>(std::move(r)));
      return out;
    }

    if (auto i = dynamic_cast<IfStmt *>(stmt.get())) {
      auto cond = transformExpr(std::move(i->condition), out);
      auto thenB = transformStmt(std::move(i->thenBranch));
      vector<unique_ptr<Stmt>> elseB;
      if (i->elseBranch)
        elseB = transformStmt(std::move(i->elseBranch));

      auto nb = make_unique<BlockStmt>();
      for (auto &s : out)
        nb->stmts.push_back(std::move(s));
      out.clear();

      nb->stmts.push_back(make_unique<IfStmt>(
          std::move(cond),
          thenB.size() == 1 ? std::move(thenB[0]) : wrapBlock(std::move(thenB)),
          elseB.empty() ? nullptr
                        : (elseB.size() == 1 ? std::move(elseB[0])
                                             : wrapBlock(std::move(elseB)))));

      out.push_back(std::move(nb));
      return out;
    }

    if (auto w = dynamic_cast<WhileStmt *>(stmt.get())) {
      auto cond = transformExpr(std::move(w->condition), out);
      auto body = transformStmt(std::move(w->body));

      auto nb = make_unique<BlockStmt>();
      for (auto &s : out)
        nb->stmts.push_back(std::move(s));
      out.clear();

      nb->stmts.push_back(make_unique<WhileStmt>(
          std::move(cond),
          body.size() == 1 ? std::move(body[0]) : wrapBlock(std::move(body))));

      out.push_back(std::move(nb));
      return out;
    }

    if (auto r = dynamic_cast<ReturnStmt *>(stmt.get())) {
      if (r->value) {
        auto v = transformExpr(std::move(r->value), out);
        out.push_back(make_unique<ReturnStmt>(std::move(v)));
      } else {
        out.push_back(std::move(stmt));
      }
      return out;
    }

    out.push_back(std::move(stmt));
    return out;
  }

private:
  /* ===== EXPRESSION LOWERING ===== */

  unique_ptr<Expr> transformExpr(unique_ptr<Expr> expr,
                                 vector<unique_ptr<Stmt>> &out) {

    // Atomic expressions
    if (dynamic_cast<NumberExpr *>(expr.get()) ||
        dynamic_cast<VariableExpr *>(expr.get())) {
      return expr;
    }

    // Binary expression
    if (auto b = dynamic_cast<BinaryExpr *>(expr.get())) {
      auto l = transformExpr(std::move(b->left), out);
      auto r = transformExpr(std::move(b->right), out);

      auto tmp = newTemp();
      out.push_back(make_unique<ExprStmt>(make_unique<BinaryExpr>(
          "=", make_unique<VariableExpr>(tmp),
          make_unique<BinaryExpr>(b->op, std::move(l), std::move(r)))));

      return make_unique<VariableExpr>(tmp);
    }

    // Unary expression
    if (auto u = dynamic_cast<UnaryExpr *>(expr.get())) {
      auto r = transformExpr(std::move(u->right), out);
      auto tmp = newTemp();
      out.push_back(make_unique<ExprStmt>(make_unique<BinaryExpr>(
          "=", make_unique<VariableExpr>(tmp),
          make_unique<UnaryExpr>(u->op, std::move(r)))));
      return make_unique<VariableExpr>(tmp);
    }

    // Function call
    if (auto c = dynamic_cast<CallExpr *>(expr.get())) {
      vector<unique_ptr<Expr>> args;
      for (auto &a : c->args)
        args.push_back(transformExpr(std::move(a), out));

      auto tmp = newTemp();
      out.push_back(make_unique<ExprStmt>(make_unique<BinaryExpr>(
          "=", make_unique<VariableExpr>(tmp),
          make_unique<CallExpr>(c->callee, std::move(args)))));
      return make_unique<VariableExpr>(tmp);
    }

    throw runtime_error("Unknown expr in ANF");
  }

  /* ===== HELPERS ===== */

  string newTemp() { return "_t" + to_string(tempCounter++); }

  unique_ptr<Stmt> wrapBlock(vector<unique_ptr<Stmt>> stmts) {
    auto b = make_unique<BlockStmt>();
    for (auto &s : stmts)
      b->stmts.push_back(std::move(s));
    return b;
  }
};
