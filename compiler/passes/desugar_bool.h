#pragma once
#include <memory>
#include "../ast/expr.h"
#include "../ast/stmt.h"

using namespace std;

struct DesugarBoolPass {

    unique_ptr<Expr> transformExpr(unique_ptr<Expr> e) {

        // true → 1
        if (auto v = dynamic_cast<VariableExpr*>(e.get())) {
            if (v->name == "true") {
                return make_unique<NumberExpr>(1);
            }
            if (v->name == "false") {
                return make_unique<NumberExpr>(0);
            }
            return e;
        }

        // binary
        if (auto b = dynamic_cast<BinaryExpr*>(e.get())) {
            b->left = transformExpr(move(b->left));
            b->right = transformExpr(move(b->right));
            return e;
        }

        // unary
        if (auto u = dynamic_cast<UnaryExpr*>(e.get())) {
            u->right = transformExpr(move(u->right));
            return e;
        }

        // call
        if (auto c = dynamic_cast<CallExpr*>(e.get())) {
            for (auto& a : c->args)
                a = transformExpr(move(a));
            return e;
        }

        return e;
    }

    unique_ptr<Stmt> transformStmt(unique_ptr<Stmt> s) {

        if (auto e = dynamic_cast<ExprStmt*>(s.get())) {
            e->e = transformExpr(move(e->e));
            return s;
        }

        if (auto p = dynamic_cast<PrintStmt*>(s.get())) {
            p->e = transformExpr(move(p->e));
            return s;
        }

        if (auto i = dynamic_cast<IfStmt*>(s.get())) {
            i->condition = transformExpr(move(i->condition));
            i->thenBranch = transformStmt(move(i->thenBranch));
            if (i->elseBranch)
                i->elseBranch = transformStmt(move(i->elseBranch));
            return s;
        }

        if (auto w = dynamic_cast<WhileStmt*>(s.get())) {
            w->condition = transformExpr(move(w->condition));
            w->body = transformStmt(move(w->body));
            return s;
        }

        if (auto b = dynamic_cast<BlockStmt*>(s.get())) {
            for (auto& x : b->stmts)
                x = transformStmt(move(x));
            return s;
        }

        return s;
    }
};
