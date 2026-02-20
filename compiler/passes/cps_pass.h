#pragma once
#include <memory>
#include <string>

#include "../ast/expr.h"
#include "../ast/stmt.h"
#include "../ir/cps.h"

using namespace std;

struct CPSPass {

  int tempCounter = 0;

  string freshTemp() { return "_k" + to_string(tempCounter++); }

  // ENTRY POINT
  unique_ptr<CPSExpr> transformStmt(Stmt *stmt, const string &k) {

    if (auto s = dynamic_cast<ExprStmt *>(stmt)) {
      return transformExpr(s->e.get(), k);
    }

    if (auto s = dynamic_cast<PrintStmt *>(stmt)) {
      vector<string> args = {"_print", "_dummy"};
      return transformExpr(s->e.get(), "_print");
    }

    if (auto s = dynamic_cast<BlockStmt *>(stmt)) {
      unique_ptr<CPSExpr> cur = nullptr;
      for (auto &st : s->stmts)
        cur = transformStmt(st.get(), k);
      return cur;
    }

    if (auto s = dynamic_cast<IfStmt *>(stmt)) {

      string cond = getName(s->condition.get());

      auto thenCPS = transformStmt(s->thenBranch.get(), k);

      unique_ptr<CPSExpr> elseCPS = nullptr;
      if (s->elseBranch)
        elseCPS = transformStmt(s->elseBranch.get(), k);
      else
        elseCPS = make_unique<CPSCall>(k, vector<string>{"0"});

      return make_unique<CPSIf>(cond, std::move(thenCPS), std::move(elseCPS));
    }

    throw runtime_error("Unsupported stmt in CPS");
  }

  // ================= EXPRESSIONS =================
  unique_ptr<CPSExpr> transformExpr(Expr *expr, const string &k) {

    // ----- NUMBER -----
    if (auto e = dynamic_cast<NumberExpr *>(expr)) {
      return make_unique<CPSCall>(k, vector<string>{to_string((int)e->value)});
    }

    // ----- VARIABLE -----
    if (auto e = dynamic_cast<VariableExpr *>(expr)) {
      return make_unique<CPSCall>(k, vector<string>{e->name});
    }

    // ----- BINARY -----
    if (auto e = dynamic_cast<BinaryExpr *>(expr)) {

      // 🔥 FIX: skip assignment
      if (e->op == "=") {
        return transformExpr(e->right.get(), k);
      }

      string t = freshTemp();

      // k(x op y)
      auto body = make_unique<CPSCall>(k, vector<string>{t});

      // let t = x op y
      auto rhs =
          make_unique<CPSCall>(e->op, vector<string>{getName(e->left.get()),
                                                     getName(e->right.get())});

      return make_unique<CPSLet>(t, std::move(rhs), std::move(body));
    }

    //------UNARY------
    else if (auto u = dynamic_cast<UnaryExpr *>(expr)) {
      // convert -a  →  call neg(a)
      string tmp = freshTemp();
      return make_unique<CPSLet>(
          tmp,
          make_unique<CPSCall>(u->op == "-" ? "neg" : "not",
                               vector<string>{getName(u->right.get())}),
          make_unique<CPSCall>(k, vector<string>{tmp}));
    }

    throw runtime_error("Unsupported expr in CPS");
  }

  string getName(Expr *e) {

    if (auto v = dynamic_cast<VariableExpr *>(e))
      return v->name;

    if (auto n = dynamic_cast<NumberExpr *>(e))
      return to_string((int)n->value);

    // TEMP bridge: unary expressions must have been ANF'd
    if (auto u = dynamic_cast<UnaryExpr *>(e)) {
      if (u->op == "-")
        return "-" + getName(u->right.get());
      if (u->op == "!")
        return "!" + getName(u->right.get());
    }

    throw runtime_error(
        "CPS error: expected variable or literal (ANF violation)");
  }
};
