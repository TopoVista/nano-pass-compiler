#pragma once
// compiler/passes/ASTAnalyses.h
//
// Phase 2 AST-level analyses.  These exist primarily to validate the
// AnalysisManager's cache/invalidation machinery before CFG/SSA analyses
// arrive in later phases.
//
// NodeCountAnalysis
//   Returns total AST node counts.  Invalidated by any structural rewrite.
//
// ASTDepthAnalysis
//   Returns maximum AST nesting depth.  Invalidated by any structural rewrite.
//
// Usage:
//   AnalysisManager am;
//   am.registerAnalysis<NodeCountAnalysis>();
//   am.registerAnalysis<ASTDepthAnalysis>();
//   const auto &counts = am.get<NodeCountAnalysis>(program);
//   std::cout << counts.statements << " statements\n";

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

#include "../ast/stmt.h"
#include "../ast/expr.h"
#include "Pass.h"   // for Program alias

namespace nanopass {

// ─────────────────────────────────────────────────────────────────────────────
// NodeCountAnalysis
// ─────────────────────────────────────────────────────────────────────────────

struct NodeCountAnalysis {
    struct Result {
        std::size_t total      = 0; // stmts + exprs
        std::size_t statements = 0;
        std::size_t expressions = 0;
        std::size_t functions   = 0;
    };

    static std::string_view id()   noexcept { return "ast-node-count"; }
    static std::string_view name() noexcept { return "NodeCount"; }

    Result compute(const Program &program) const {
        Result r;
        for (const auto &stmt : program)
            countStmt(stmt.get(), r);
        return r;
    }

private:
    static void countStmt(const Stmt *s, Result &r) {
        if (!s) return;
        ++r.statements;
        ++r.total;

        if (auto *fn = dynamic_cast<const FunctionStmt *>(s)) {
            ++r.functions;
            countStmt(fn->body.get(), r);
            return;
        }
        if (auto *blk = dynamic_cast<const BlockStmt *>(s)) {
            for (const auto &c : blk->stmts)
                countStmt(c.get(), r);
            return;
        }
        if (auto *ifs = dynamic_cast<const IfStmt *>(s)) {
            countExpr(ifs->condition.get(), r);
            countStmt(ifs->thenBranch.get(), r);
            if (ifs->elseBranch) countStmt(ifs->elseBranch.get(), r);
            return;
        }
        if (auto *ws = dynamic_cast<const WhileStmt *>(s)) {
            countExpr(ws->condition.get(), r);
            countStmt(ws->body.get(), r);
            return;
        }
        if (auto *fs = dynamic_cast<const ForStmt *>(s)) {
            if (fs->init)      countStmt(fs->init.get(), r);
            if (fs->condition) countExpr(fs->condition.get(), r);
            if (fs->increment) countExpr(fs->increment.get(), r);
            countStmt(fs->body.get(), r);
            return;
        }
        if (auto *rs = dynamic_cast<const ReturnStmt *>(s)) {
            if (rs->value) countExpr(rs->value.get(), r);
            return;
        }
        if (auto *es = dynamic_cast<const ExprStmt *>(s)) {
            countExpr(es->e.get(), r);
            return;
        }
        if (auto *ps = dynamic_cast<const PrintStmt *>(s)) {
            countExpr(ps->e.get(), r);
            return;
        }
        if (auto *vd = dynamic_cast<const VarDeclStmt *>(s)) {
            if (vd->initializer) countExpr(vd->initializer.get(), r);
            return;
        }
        // BreakStmt, ContinueStmt — leaf nodes, already counted above.
    }

    static void countExpr(const Expr *e, Result &r) {
        if (!e) return;
        ++r.expressions;
        ++r.total;

        if (auto *b = dynamic_cast<const BinaryExpr *>(e)) {
            countExpr(b->left.get(), r);
            countExpr(b->right.get(), r);
            return;
        }
        if (auto *u = dynamic_cast<const UnaryExpr *>(e)) {
            countExpr(u->right.get(), r);
            return;
        }
        if (auto *c = dynamic_cast<const CallExpr *>(e)) {
            for (const auto &a : c->args)
                countExpr(a.get(), r);
            return;
        }
        if (auto *i = dynamic_cast<const IndexExpr *>(e)) {
            countExpr(i->array.get(), r);
            countExpr(i->index.get(), r);
            return;
        }
        // Leaf: NumberExpr, BoolExpr, StringExpr, VariableExpr — no children.
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// ASTDepthAnalysis
// ─────────────────────────────────────────────────────────────────────────────

struct ASTDepthAnalysis {
    struct Result {
        std::size_t maxDepth = 0;
    };

    static std::string_view id()   noexcept { return "ast-depth"; }
    static std::string_view name() noexcept { return "ASTDepth"; }

    Result compute(const Program &program) const {
        std::size_t maxDepth = 0;
        for (const auto &stmt : program)
            maxDepth = std::max(maxDepth, depthStmt(stmt.get(), 0));
        return Result{maxDepth};
    }

private:
    static std::size_t depthStmt(const Stmt *s, std::size_t d) {
        if (!s) return d;
        std::size_t my = d + 1;

        if (auto *fn = dynamic_cast<const FunctionStmt *>(s))
            return depthStmt(fn->body.get(), my);
        if (auto *blk = dynamic_cast<const BlockStmt *>(s)) {
            std::size_t best = my;
            for (const auto &c : blk->stmts)
                best = std::max(best, depthStmt(c.get(), my));
            return best;
        }
        if (auto *ifs = dynamic_cast<const IfStmt *>(s)) {
            std::size_t best = depthExpr(ifs->condition.get(), my);
            best = std::max(best, depthStmt(ifs->thenBranch.get(), my));
            if (ifs->elseBranch)
                best = std::max(best, depthStmt(ifs->elseBranch.get(), my));
            return best;
        }
        if (auto *ws = dynamic_cast<const WhileStmt *>(s)) {
            return std::max(depthExpr(ws->condition.get(), my),
                            depthStmt(ws->body.get(), my));
        }
        if (auto *fs = dynamic_cast<const ForStmt *>(s)) {
            std::size_t best = my;
            if (fs->init)      best = std::max(best, depthStmt(fs->init.get(), my));
            if (fs->condition) best = std::max(best, depthExpr(fs->condition.get(), my));
            if (fs->increment) best = std::max(best, depthExpr(fs->increment.get(), my));
            best = std::max(best, depthStmt(fs->body.get(), my));
            return best;
        }
        if (auto *rs = dynamic_cast<const ReturnStmt *>(s))
            return rs->value ? depthExpr(rs->value.get(), my) : my;
        if (auto *es = dynamic_cast<const ExprStmt *>(s))
            return depthExpr(es->e.get(), my);
        if (auto *ps = dynamic_cast<const PrintStmt *>(s))
            return depthExpr(ps->e.get(), my);
        if (auto *vd = dynamic_cast<const VarDeclStmt *>(s))
            return vd->initializer ? depthExpr(vd->initializer.get(), my) : my;
        return my;
    }

    static std::size_t depthExpr(const Expr *e, std::size_t d) {
        if (!e) return d;
        std::size_t my = d + 1;
        if (auto *b = dynamic_cast<const BinaryExpr *>(e))
            return std::max(depthExpr(b->left.get(), my),
                            depthExpr(b->right.get(), my));
        if (auto *u = dynamic_cast<const UnaryExpr *>(e))
            return depthExpr(u->right.get(), my);
        if (auto *c = dynamic_cast<const CallExpr *>(e)) {
            std::size_t best = my;
            for (const auto &a : c->args)
                best = std::max(best, depthExpr(a.get(), my));
            return best;
        }
        if (auto *i = dynamic_cast<const IndexExpr *>(e))
            return std::max(depthExpr(i->array.get(), my),
                            depthExpr(i->index.get(), my));
        return my; // leaf
    }
};

} // namespace nanopass
