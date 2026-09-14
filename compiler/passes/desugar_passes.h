#pragma once
// compiler/passes/desugar_passes.h
//
// Phase 2 desugaring passes implementing the nanopass::Pass base class.
//
// All three passes:
//   1. Implement Pass::run(Program &, PassContext &)
//   2. Execute once over the FULL program (not once per top-level statement)
//   3. Recurse correctly into FunctionStmt bodies (Phase 1 bug fixed here)
//   4. Count actual rewrites so changed=false when nothing matched
//   5. Preserve source locations on synthesised nodes
//   6. Return PreservedAnalyses::none() when changed (structural rewrite)
//      or PreservedAnalyses::all() when unchanged (read-only when no match)
//
// Pass IDs (stable):
//   "desugar-for"          DesugarForPass
//   "desugar-inc-dec"      DesugarIncDecPass
//   "desugar-plus-assign"  DesugarPlusAssignPass
//
// The old struct interfaces (transform / transformStmt) are kept as private
// implementation helpers; they are still exercised through run() so nothing
// breaks.  PipelineBuilder and the test suite use the Pass interface.

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <vector>

#include "../ast/expr.h"
#include "../ast/stmt.h"
#include "Pass.h"
#include "PassResult.h"
#include "PassContext.h"   // used transitively; kept for completeness

namespace nanopass {

// ═══════════════════════════════════════════════════════════════════════════
// DesugarForPass
//
// Lowers every ForStmt in the program (including inside function bodies) to:
//
//   for (init; cond; inc) body
//   →  { init; while (cond /* or true */) { body; inc; } }
//
// Source location of the synthesised BlockStmt and WhileStmt is copied from
// the original ForStmt so diagnostics remain useful.
// ═══════════════════════════════════════════════════════════════════════════

class DesugarForPass : public Pass {
public:
    PassInfo info() const noexcept override {
        return {"desugar-for", "DesugarFor",
                "Lower for-loops to equivalent while-loops.",
                "desugaring", IRUnitKind::Program};
    }

    PassResult run(Program &program, PassContext & /*ctx*/) override {
        rewrites_ = 0;
        for (auto &stmt : program)
            stmt = transformStmt(std::move(stmt));
        PassStatistics stats;
        stats.record("rewrites", static_cast<int64_t>(rewrites_));
        if (rewrites_ == 0)
            return PassResult::noChange();
        return PassResult::didChange(PreservedAnalyses::none(), std::move(stats));
    }

    // Kept public so tests can call the pass directly on individual statements.
    std::unique_ptr<Stmt> transformStmt(std::unique_ptr<Stmt> stmt) {
        if (!stmt) return stmt;

        // ── ForStmt → {init; while(cond){body;inc}} ──────────────────────
        if (auto *f = dynamic_cast<ForStmt *>(stmt.get()))
            return desugarFor(f, std::move(stmt));

        // ── FunctionStmt: recurse into body ──────────────────────────────
        // Phase 1 bug: this case was missing.
        if (auto *fn = dynamic_cast<FunctionStmt *>(stmt.get())) {
            fn->body = transformBlock(std::move(fn->body));
            return stmt;
        }

        // ── BlockStmt ────────────────────────────────────────────────────
        if (auto *blk = dynamic_cast<BlockStmt *>(stmt.get())) {
            for (auto &s : blk->stmts)
                s = transformStmt(std::move(s));
            return stmt;
        }

        // ── IfStmt ───────────────────────────────────────────────────────
        if (auto *ifs = dynamic_cast<IfStmt *>(stmt.get())) {
            ifs->thenBranch = transformStmt(std::move(ifs->thenBranch));
            if (ifs->elseBranch)
                ifs->elseBranch = transformStmt(std::move(ifs->elseBranch));
            return stmt;
        }

        // ── WhileStmt ────────────────────────────────────────────────────
        if (auto *ws = dynamic_cast<WhileStmt *>(stmt.get())) {
            ws->body = transformStmt(std::move(ws->body));
            return stmt;
        }

        // All other statements (Return, Expr, Print, VarDecl, Break,
        // Continue) contain no sub-statements needing desugar.
        return stmt;
    }

private:
    std::size_t rewrites_ = 0;

    std::unique_ptr<BlockStmt>
    transformBlock(std::unique_ptr<BlockStmt> blk) {
        if (!blk) return blk;
        for (auto &s : blk->stmts)
            s = transformStmt(std::move(s));
        return blk;
    }

    std::unique_ptr<Stmt>
    desugarFor(ForStmt *f, std::unique_ptr<Stmt> orig) {
        ++rewrites_;
        const SourceLocation loc = orig->loc; // preserve origin

        // Build the new body: original body + increment expression
        std::unique_ptr<Stmt> newBody;
        if (f->increment) {
            auto bodyBlock = std::make_unique<BlockStmt>();
            bodyBlock->loc = loc;
            bodyBlock->stmts.push_back(transformStmt(std::move(f->body)));
            auto incStmt = std::make_unique<ExprStmt>(std::move(f->increment));
            incStmt->loc = loc;
            bodyBlock->stmts.push_back(std::move(incStmt));
            newBody = std::move(bodyBlock);
        } else {
            newBody = transformStmt(std::move(f->body));
        }

        // while (cond) newBody
        auto cond = f->condition
            ? std::move(f->condition)
            : std::unique_ptr<Expr>(std::make_unique<NumberExpr>(1LL));

        auto whileStmt = std::make_unique<WhileStmt>(
            std::move(cond), std::move(newBody));
        whileStmt->loc = loc;

        // { init; while(...) ... }
        auto block = std::make_unique<BlockStmt>();
        block->loc = loc;
        if (f->init)
            block->stmts.push_back(transformStmt(std::move(f->init)));
        block->stmts.push_back(std::move(whileStmt));
        return block;
        // orig (ForStmt) is destroyed here — its remaining fields have been
        // moved out; destroying the partially-empty ForStmt is safe because
        // unique_ptr fields that were moved from are null.
    }
};


// ═══════════════════════════════════════════════════════════════════════════
// DesugarIncDecPass
//
// Lowers x++ and x-- at statement level:
//   x++  →  x = x + 1
//   x--  →  x = x - 1
//
// Note: only statement-level ++ / -- (ExprStmt wrapping UnaryExpr) are
// lowered.  Expression-level ++ (e.g. as an argument) are not supported by
// the Nano language and are rejected by the parser; no case needed.
// ═══════════════════════════════════════════════════════════════════════════

class DesugarIncDecPass : public Pass {
public:
    PassInfo info() const noexcept override {
        return {"desugar-inc-dec", "DesugarIncDec",
                "Lower ++ and -- operators to explicit additions/subtractions.",
                "desugaring", IRUnitKind::Program};
    }

    PassResult run(Program &program, PassContext & /*ctx*/) override {
        rewrites_ = 0;
        for (auto &stmt : program)
            stmt = transformStmt(std::move(stmt));
        PassStatistics stats;
        stats.record("rewrites", static_cast<int64_t>(rewrites_));
        if (rewrites_ == 0)
            return PassResult::noChange();
        return PassResult::didChange(PreservedAnalyses::none(), std::move(stats));
    }

    std::unique_ptr<Stmt> transformStmt(std::unique_ptr<Stmt> stmt) {
        if (!stmt) return stmt;

        // ── FunctionStmt: recurse into body ──────────────────────────────
        if (auto *fn = dynamic_cast<FunctionStmt *>(stmt.get())) {
            for (auto &s : fn->body->stmts)
                s = transformStmt(std::move(s));
            return stmt;
        }

        // ── BlockStmt ────────────────────────────────────────────────────
        if (auto *blk = dynamic_cast<BlockStmt *>(stmt.get())) {
            for (auto &s : blk->stmts)
                s = transformStmt(std::move(s));
            return stmt;
        }

        // ── ExprStmt: check for top-level ++ / -- ─────────────────────
        if (auto *es = dynamic_cast<ExprStmt *>(stmt.get())) {
            if (auto *u = dynamic_cast<UnaryExpr *>(es->e.get())) {
                if (u->op == "++" || u->op == "--") {
                    auto *var = dynamic_cast<VariableExpr *>(u->right.get());
                    if (!var)
                        throw std::runtime_error("++/-- requires a variable");
                    const std::string name = var->name;
                    const std::string op = (u->op == "++") ? "+" : "-";
                    const SourceLocation loc = stmt->loc;
                    ++rewrites_;
                    // x++  →  x = x + 1
                    auto assign = std::make_unique<ExprStmt>(
                        std::make_unique<BinaryExpr>(
                            "=",
                            std::make_unique<VariableExpr>(name),
                            std::make_unique<BinaryExpr>(
                                op,
                                std::make_unique<VariableExpr>(name),
                                std::make_unique<NumberExpr>(1LL))));
                    assign->loc = loc;
                    return assign;
                }
            }
            // No ++ / -- at top level; recurse into expression for nested cases.
            es->e = transformExpr(std::move(es->e));
            return stmt;
        }

        // ── IfStmt ───────────────────────────────────────────────────────
        if (auto *ifs = dynamic_cast<IfStmt *>(stmt.get())) {
            ifs->condition = transformExpr(std::move(ifs->condition));
            ifs->thenBranch = transformStmt(std::move(ifs->thenBranch));
            if (ifs->elseBranch)
                ifs->elseBranch = transformStmt(std::move(ifs->elseBranch));
            return stmt;
        }

        // ── WhileStmt ────────────────────────────────────────────────────
        if (auto *ws = dynamic_cast<WhileStmt *>(stmt.get())) {
            ws->condition = transformExpr(std::move(ws->condition));
            ws->body = transformStmt(std::move(ws->body));
            return stmt;
        }

        // ── PrintStmt ────────────────────────────────────────────────────
        if (auto *ps = dynamic_cast<PrintStmt *>(stmt.get())) {
            ps->e = transformExpr(std::move(ps->e));
            return stmt;
        }

        // ── ReturnStmt ───────────────────────────────────────────────────
        if (auto *rs = dynamic_cast<ReturnStmt *>(stmt.get())) {
            if (rs->value)
                rs->value = transformExpr(std::move(rs->value));
            return stmt;
        }

        // ── VarDeclStmt ──────────────────────────────────────────────────
        if (auto *vd = dynamic_cast<VarDeclStmt *>(stmt.get())) {
            if (vd->initializer)
                vd->initializer = transformExpr(std::move(vd->initializer));
            return stmt;
        }

        return stmt;
    }

private:
    std::size_t rewrites_ = 0;

    std::unique_ptr<Expr> transformExpr(std::unique_ptr<Expr> expr) {
        if (!expr) return expr;
        if (auto *b = dynamic_cast<BinaryExpr *>(expr.get())) {
            b->left  = transformExpr(std::move(b->left));
            b->right = transformExpr(std::move(b->right));
            return expr;
        }
        if (auto *u = dynamic_cast<UnaryExpr *>(expr.get())) {
            u->right = transformExpr(std::move(u->right));
            return expr;
        }
        if (auto *c = dynamic_cast<CallExpr *>(expr.get())) {
            for (auto &a : c->args)
                a = transformExpr(std::move(a));
            return expr;
        }
        return expr;
    }
};


// ═══════════════════════════════════════════════════════════════════════════
// DesugarPlusAssignPass
//
// Lowers compound assignment:
//   a += b   →   a = a + b
//
// Only += is currently supported (the lexer/parser emit it; -=, *=, /= are
// not yet in the grammar).  The pass is written to be easily extended.
// ═══════════════════════════════════════════════════════════════════════════

class DesugarPlusAssignPass : public Pass {
public:
    PassInfo info() const noexcept override {
        return {"desugar-plus-assign", "DesugarPlusAssign",
                "Lower += compound assignment to simple assignment.",
                "desugaring", IRUnitKind::Program};
    }

    PassResult run(Program &program, PassContext & /*ctx*/) override {
        rewrites_ = 0;
        for (auto &stmt : program)
            stmt = transformStmt(std::move(stmt));
        PassStatistics stats;
        stats.record("rewrites", static_cast<int64_t>(rewrites_));
        if (rewrites_ == 0)
            return PassResult::noChange();
        return PassResult::didChange(PreservedAnalyses::none(), std::move(stats));
    }

    std::unique_ptr<Stmt> transformStmt(std::unique_ptr<Stmt> stmt) {
        if (!stmt) return stmt;

        // ── FunctionStmt: recurse into body ──────────────────────────────
        if (auto *fn = dynamic_cast<FunctionStmt *>(stmt.get())) {
            for (auto &s : fn->body->stmts)
                s = transformStmt(std::move(s));
            return stmt;
        }

        // ── BlockStmt ────────────────────────────────────────────────────
        if (auto *blk = dynamic_cast<BlockStmt *>(stmt.get())) {
            for (auto &s : blk->stmts)
                s = transformStmt(std::move(s));
            return stmt;
        }

        // ── ExprStmt ─────────────────────────────────────────────────────
        if (auto *es = dynamic_cast<ExprStmt *>(stmt.get())) {
            es->e = transformExpr(std::move(es->e));
            return stmt;
        }

        // ── PrintStmt ────────────────────────────────────────────────────
        if (auto *ps = dynamic_cast<PrintStmt *>(stmt.get())) {
            ps->e = transformExpr(std::move(ps->e));
            return stmt;
        }

        // ── IfStmt ───────────────────────────────────────────────────────
        if (auto *ifs = dynamic_cast<IfStmt *>(stmt.get())) {
            ifs->condition = transformExpr(std::move(ifs->condition));
            ifs->thenBranch = transformStmt(std::move(ifs->thenBranch));
            if (ifs->elseBranch)
                ifs->elseBranch = transformStmt(std::move(ifs->elseBranch));
            return stmt;
        }

        // ── WhileStmt ────────────────────────────────────────────────────
        if (auto *ws = dynamic_cast<WhileStmt *>(stmt.get())) {
            ws->condition = transformExpr(std::move(ws->condition));
            ws->body = transformStmt(std::move(ws->body));
            return stmt;
        }

        // ── ReturnStmt ───────────────────────────────────────────────────
        if (auto *rs = dynamic_cast<ReturnStmt *>(stmt.get())) {
            if (rs->value)
                rs->value = transformExpr(std::move(rs->value));
            return stmt;
        }

        // ── VarDeclStmt ──────────────────────────────────────────────────
        if (auto *vd = dynamic_cast<VarDeclStmt *>(stmt.get())) {
            if (vd->initializer)
                vd->initializer = transformExpr(std::move(vd->initializer));
            return stmt;
        }

        return stmt;
    }

private:
    std::size_t rewrites_ = 0;

    std::unique_ptr<Expr> transformExpr(std::unique_ptr<Expr> expr) {
        if (!expr) return expr;

        if (auto *b = dynamic_cast<BinaryExpr *>(expr.get())) {
            // Recurse children first (depth-first).
            b->left  = transformExpr(std::move(b->left));
            b->right = transformExpr(std::move(b->right));

            if (b->op == "+=") {
                auto *var = dynamic_cast<VariableExpr *>(b->left.get());
                if (!var)
                    throw std::runtime_error(
                        "Left side of += must be a variable");
                const std::string name = var->name;
                const SourceLocation loc = expr->loc;
                ++rewrites_;
                // a += b  →  a = a + b
                auto rhs = std::make_unique<BinaryExpr>(
                    "+",
                    std::make_unique<VariableExpr>(name),
                    std::move(b->right));
                rhs->loc = loc;
                auto assign = std::make_unique<BinaryExpr>(
                    "=",
                    std::make_unique<VariableExpr>(name),
                    std::move(rhs));
                assign->loc = loc;
                return assign;
            }
            return expr;
        }

        if (auto *u = dynamic_cast<UnaryExpr *>(expr.get())) {
            u->right = transformExpr(std::move(u->right));
            return expr;
        }
        if (auto *c = dynamic_cast<CallExpr *>(expr.get())) {
            for (auto &a : c->args)
                a = transformExpr(std::move(a));
            return expr;
        }
        return expr;
    }
};

} // namespace nanopass
