#pragma once

#include "../ast/stmt.h"
#include "../ast/expr.h"

struct DesugarIfElsePass {

    unique_ptr<Stmt> transform(unique_ptr<Stmt> stmt) {

        if (auto b = dynamic_cast<BlockStmt*>(stmt.get())) {
            auto nb = make_unique<BlockStmt>();
            for (auto& s : b->stmts)
                nb->stmts.push_back(transform(move(s)));
            return nb;
        }

        if (auto i = dynamic_cast<IfStmt*>(stmt.get())) {
            return desugarIf(move(stmt));
        }

        if (auto w = dynamic_cast<WhileStmt*>(stmt.get())) {
            w->condition = transformExpr(move(w->condition));
            w->body = transform(move(w->body));
            return stmt;
        }

        if (auto e = dynamic_cast<ExprStmt*>(stmt.get())) {
            e->e = transformExpr(move(e->e));
            return stmt;
        }

        if (auto p = dynamic_cast<PrintStmt*>(stmt.get())) {
            p->e = transformExpr(move(p->e));
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

    unique_ptr<Stmt> desugarIf(unique_ptr<Stmt> stmt) {
        auto* ifs = static_cast<IfStmt*>(stmt.get());

        // First recursively transform children
        auto cond = transformExpr(move(ifs->condition));
        auto thenB = transform(move(ifs->thenBranch));

        // If there is NO else → just normalize children
        if (!ifs->elseBranch) {
            return make_unique<IfStmt>(
                move(cond),
                move(thenB),
                nullptr
            );
        }

        // else exists → desugar
        auto elseB = transform(move(ifs->elseBranch));

        /*
            if (c) T else E
            =>
            {
                if (c) T
                if (!c) E
            }
        */

        auto block = make_unique<BlockStmt>();

        // if (c) T
        block->stmts.push_back(
            make_unique<IfStmt>(
                make_unique<BinaryExpr>("", nullptr, nullptr), // placeholder
                nullptr,
                nullptr
            )
        );

        block->stmts.back() = make_unique<IfStmt>(
            cloneExpr(cond.get()),
            move(thenB),
            nullptr
        );

        // if (!c) E
        block->stmts.push_back(
            make_unique<IfStmt>(
                make_unique<UnaryExpr>("!", cloneExpr(cond.get())),
                move(elseB),
                nullptr
            )
        );

        return block;
    }

    /* -------- Expression utilities -------- */

    unique_ptr<Expr> transformExpr(unique_ptr<Expr> expr) {
        if (auto b = dynamic_cast<BinaryExpr*>(expr.get())) {
            b->left = transformExpr(move(b->left));
            b->right = transformExpr(move(b->right));
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

        return expr;
    }

    // Needed because we reuse condition twice
    unique_ptr<Expr> cloneExpr(const Expr* e) {
        if (auto n = dynamic_cast<const NumberExpr*>(e))
            return make_unique<NumberExpr>(n->value);

        if (auto v = dynamic_cast<const VariableExpr*>(e))
            return make_unique<VariableExpr>(v->name);

        if (auto u = dynamic_cast<const UnaryExpr*>(e))
            return make_unique<UnaryExpr>(u->op, cloneExpr(u->right.get()));

        if (auto b = dynamic_cast<const BinaryExpr*>(e))
            return make_unique<BinaryExpr>(
                b->op,
                cloneExpr(b->left.get()),
                cloneExpr(b->right.get())
            );

        if (auto c = dynamic_cast<const CallExpr*>(e)) {
            vector<unique_ptr<Expr>> args;
            for (auto& a : c->args)
                args.push_back(cloneExpr(a.get()));
            return make_unique<CallExpr>(c->callee, move(args));
        }

        throw runtime_error("Unsupported expr clone");
    }
};
