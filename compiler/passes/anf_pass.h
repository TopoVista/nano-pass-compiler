#pragma once

#include "../ast/stmt.h"
#include "../ast/expr.h"

struct ANFPass {

    int tempCounter = 0;

    /* ===== ENTRY ===== */

    vector<unique_ptr<Stmt>> transformStmt(unique_ptr<Stmt> stmt) {
        vector<unique_ptr<Stmt>> out;

        if (auto b = dynamic_cast<BlockStmt*>(stmt.get())) {
            auto nb = make_unique<BlockStmt>();
            for (auto& s : b->stmts) {
                auto lowered = transformStmt(move(s));
                for (auto& x : lowered)
                    nb->stmts.push_back(move(x));
            }
            out.push_back(move(nb));
            return out;
        }

        if (auto e = dynamic_cast<ExprStmt*>(stmt.get())) {
            auto r = transformExpr(move(e->e), out);
            out.push_back(make_unique<ExprStmt>(move(r)));
            return out;
        }

        if (auto p = dynamic_cast<PrintStmt*>(stmt.get())) {
            auto r = transformExpr(move(p->e), out);
            out.push_back(make_unique<PrintStmt>(move(r)));
            return out;
        }

        if (auto i = dynamic_cast<IfStmt*>(stmt.get())) {
            auto cond = transformExpr(move(i->condition), out);
            auto thenB = transformStmt(move(i->thenBranch));
            vector<unique_ptr<Stmt>> elseB;
            if (i->elseBranch)
                elseB = transformStmt(move(i->elseBranch));

            auto nb = make_unique<BlockStmt>();
            for (auto& s : out) nb->stmts.push_back(move(s));
            out.clear();

            nb->stmts.push_back(
                make_unique<IfStmt>(
                    move(cond),
                    thenB.size()==1 ? move(thenB[0]) : wrapBlock(move(thenB)),
                    elseB.empty() ? nullptr :
                        (elseB.size()==1 ? move(elseB[0]) : wrapBlock(move(elseB)))
                )
            );

            out.push_back(move(nb));
            return out;
        }

        if (auto w = dynamic_cast<WhileStmt*>(stmt.get())) {
            auto cond = transformExpr(move(w->condition), out);
            auto body = transformStmt(move(w->body));

            auto nb = make_unique<BlockStmt>();
            for (auto& s : out) nb->stmts.push_back(move(s));
            out.clear();

            nb->stmts.push_back(
                make_unique<WhileStmt>(
                    move(cond),
                    body.size()==1 ? move(body[0]) : wrapBlock(move(body))
                )
            );

            out.push_back(move(nb));
            return out;
        }

        if (auto r = dynamic_cast<ReturnStmt*>(stmt.get())) {
            if (r->value) {
                auto v = transformExpr(move(r->value), out);
                out.push_back(make_unique<ReturnStmt>(move(v)));
            } else {
                out.push_back(move(stmt));
            }
            return out;
        }

        out.push_back(move(stmt));
        return out;
    }

private:

    /* ===== EXPRESSION LOWERING ===== */

    unique_ptr<Expr> transformExpr(unique_ptr<Expr> expr,
                                   vector<unique_ptr<Stmt>>& out) {

        // Atomic expressions
        if (dynamic_cast<NumberExpr*>(expr.get()) ||
            dynamic_cast<VariableExpr*>(expr.get())) {
            return expr;
        }

        // Binary expression
        if (auto b = dynamic_cast<BinaryExpr*>(expr.get())) {
            auto l = transformExpr(move(b->left), out);
            auto r = transformExpr(move(b->right), out);

            auto tmp = newTemp();
            out.push_back(
                make_unique<ExprStmt>(
                    make_unique<BinaryExpr>(
                        "=",
                        make_unique<VariableExpr>(tmp),
                        make_unique<BinaryExpr>(b->op, move(l), move(r))
                    )
                )
            );

            return make_unique<VariableExpr>(tmp);
        }

        // Unary expression
        if (auto u = dynamic_cast<UnaryExpr*>(expr.get())) {
            auto r = transformExpr(move(u->right), out);
            auto tmp = newTemp();
            out.push_back(
                make_unique<ExprStmt>(
                    make_unique<BinaryExpr>(
                        "=",
                        make_unique<VariableExpr>(tmp),
                        make_unique<UnaryExpr>(u->op, move(r))
                    )
                )
            );
            return make_unique<VariableExpr>(tmp);
        }

        // Function call
        if (auto c = dynamic_cast<CallExpr*>(expr.get())) {
            vector<unique_ptr<Expr>> args;
            for (auto& a : c->args)
                args.push_back(transformExpr(move(a), out));

            auto tmp = newTemp();
            out.push_back(
                make_unique<ExprStmt>(
                    make_unique<BinaryExpr>(
                        "=",
                        make_unique<VariableExpr>(tmp),
                        make_unique<CallExpr>(c->callee, move(args))
                    )
                )
            );
            return make_unique<VariableExpr>(tmp);
        }

        throw runtime_error("Unknown expr in ANF");
    }

    /* ===== HELPERS ===== */

    string newTemp() {
        return "_t" + to_string(tempCounter++);
    }

    unique_ptr<Stmt> wrapBlock(vector<unique_ptr<Stmt>> stmts) {
        auto b = make_unique<BlockStmt>();
        for (auto& s : stmts)
            b->stmts.push_back(move(s));
        return b;
    }
};
