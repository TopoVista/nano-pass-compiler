#pragma once

#include "../ast/stmt.h"
#include "../ast/expr.h"

struct DesugarPlusAssignPass {

    /* ========= ENTRY ========= */

    unique_ptr<Stmt> transformStmt(unique_ptr<Stmt> stmt) {

        if (auto b = dynamic_cast<BlockStmt*>(stmt.get())) {
            auto nb = make_unique<BlockStmt>();
            for (auto& s : b->stmts)
                nb->stmts.push_back(transformStmt(move(s)));
            return nb;
        }

        if (auto e = dynamic_cast<ExprStmt*>(stmt.get())) {
            e->e = transformExpr(move(e->e));
            return stmt;
        }

        if (auto p = dynamic_cast<PrintStmt*>(stmt.get())) {
            p->e = transformExpr(move(p->e));
            return stmt;
        }


        if (auto i = dynamic_cast<IfStmt*>(stmt.get())) {
            i->condition = transformExpr(move(i->condition));
            i->thenBranch = transformStmt(move(i->thenBranch));
            if (i->elseBranch)
                i->elseBranch = transformStmt(move(i->elseBranch));
            return stmt;
        }

        if (auto w = dynamic_cast<WhileStmt*>(stmt.get())) {
            w->condition = transformExpr(move(w->condition));
            w->body = transformStmt(move(w->body));
            return stmt;
        }

        if (auto f = dynamic_cast<ForStmt*>(stmt.get())) {
            if (f->init)
                f->init = transformStmt(move(f->init));
            if (f->condition)
                f->condition = transformExpr(move(f->condition));
            if (f->increment)
                f->increment = transformExpr(move(f->increment));
            f->body = transformStmt(move(f->body));
            return stmt;
        }

        if (auto r = dynamic_cast<ReturnStmt*>(stmt.get())) {
            if (r->value)
                r->value = transformExpr(move(r->value));
            return stmt;
        }

        return stmt;
    }

private:

    /* ========= EXPRESSION TRANSFORM ========= */

    unique_ptr<Expr> transformExpr(unique_ptr<Expr> expr) {

        if (auto b = dynamic_cast<BinaryExpr*>(expr.get())) {

            // Recursively transform children first
            b->left  = transformExpr(move(b->left));
            b->right = transformExpr(move(b->right));

            // Desugar +=
            if (b->op == "+=") {
                // lhs must be variable
                auto* var = dynamic_cast<VariableExpr*>(b->left.get());
                if (!var)
                    throw runtime_error("Left side of += must be a variable");

                string name = var->name;

                // a += b  -->  a = a + b
                auto newRight = make_unique<BinaryExpr>(
                    "+",
                    make_unique<VariableExpr>(name),
                    move(b->right)
                );

                return make_unique<BinaryExpr>(
                    "=",
                    make_unique<VariableExpr>(name),
                    move(newRight)
                );
            }

            return expr;
        }

        if (auto u = dynamic_cast<UnaryExpr*>(expr.get())) {
            u->right = transformExpr(move(u->right));
            return expr;
        }

        if (auto c = dynamic_cast<CallExpr*>(expr.get())) {
            for (auto& a : c->args)
                a = transformExpr(move(a));
            return expr;
        }

        // NumberExpr, VariableExpr → unchanged
        return expr;
    }
};
