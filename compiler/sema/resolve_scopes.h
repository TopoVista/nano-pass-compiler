#pragma once
#include <memory>
#include <vector>
#include <stdexcept>

#include "../ast/stmt.h"
#include "../ast/expr.h"
#include "symbol_table.h"

using namespace std;

class ResolveScopesPass {
    SymbolTable table;

public:
    void resolve(const vector<unique_ptr<Stmt>>& program) {
        for (auto& s : program)
            resolveStmt(s.get());
    }

private:
    // ---------------- Statements ----------------

    void resolveStmt(Stmt* stmt) {
        if (auto s = dynamic_cast<BlockStmt*>(stmt)) {
            table.enterScope();
            for (auto& st : s->stmts)
                resolveStmt(st.get());
            table.exitScope();
        }
        else if (auto s = dynamic_cast<ExprStmt*>(stmt)) {
            resolveExpr(s->e.get());
        }
        else if (auto s = dynamic_cast<PrintStmt*>(stmt)) {
            resolveExpr(s->e.get());
        }
        else if (auto s = dynamic_cast<IfStmt*>(stmt)) {
            resolveExpr(s->condition.get());
            resolveStmt(s->thenBranch.get());
            if (s->elseBranch)
                resolveStmt(s->elseBranch.get());
        }
        else if (auto s = dynamic_cast<WhileStmt*>(stmt)) {
            resolveExpr(s->condition.get());
            resolveStmt(s->body.get());
        }
        else if (auto s = dynamic_cast<ForStmt*>(stmt)) {
            table.enterScope();
            if (s->init) resolveStmt(s->init.get());
            if (s->condition) resolveExpr(s->condition.get());
            if (s->increment) resolveExpr(s->increment.get());
            resolveStmt(s->body.get());
            table.exitScope();
        }
        else if (auto s = dynamic_cast<FunctionStmt*>(stmt)) {
            table.declare(s->name, SymbolKind::Function);
            table.enterScope();
            for (auto& p : s->params)
                table.declare(p, SymbolKind::Variable);
            resolveStmt(s->body.get());
            table.exitScope();
        }
        else if (auto s = dynamic_cast<ReturnStmt*>(stmt)) {
            if (s->value)
                resolveExpr(s->value.get());
        }
    }

    // ---------------- Expressions ----------------

    void resolveExpr(Expr* expr) {
        if (auto e = dynamic_cast<VariableExpr*>(expr)) {
            // 🔥 FIX: boolean literals are NOT variables
            if (e->name == "true" || e->name == "false") {
                  return;  // skip scope resolution for bool literals
            }
            auto sym = table.lookup(e->name);
            if (!sym)
                throw runtime_error("Use of undeclared variable: " + e->name);
            e->symbol = sym;
            return;
        }    
        else if (auto e = dynamic_cast<BinaryExpr*>(expr)) {
            // Handle assignment: lhs = rhs
            if (e->op == "=") {
                auto var = dynamic_cast<VariableExpr*>(e->left.get());
                if (!var)
                throw runtime_error("Invalid assignment target");

                // Declare variable if not already declared
                if (!table.lookup(var->name)) {
                    table.declare(var->name, SymbolKind::Variable);
                }

                resolveExpr(e->right.get());
                var->symbol = table.lookup(var->name);
                return;
            }

            // Normal binary expression
            resolveExpr(e->left.get());
            resolveExpr(e->right.get());
        }
        else if (auto e = dynamic_cast<UnaryExpr*>(expr)) {
            resolveExpr(e->right.get());
        }
        else if (auto e = dynamic_cast<CallExpr*>(expr)) {
            auto sym = table.lookup(e->callee);
            if (!sym)
                throw runtime_error("Call to undeclared function: " + e->callee);

            for (auto& a : e->args)
                resolveExpr(a.get());
        }
        else if (auto e = dynamic_cast<NumberExpr*>(expr)) {
            // nothing
        }
    }
};
