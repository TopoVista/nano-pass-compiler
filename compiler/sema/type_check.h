#pragma once

#include <stdexcept>
#include <memory>
#include <vector>

#include "type.h"
#include "symbol.h"

#include "../ast/expr.h"
#include "../ast/stmt.h"

using namespace std;

struct TypeCheckPass {

    void check(const vector<unique_ptr<Stmt>>& program) {
        for (auto& s : program)
            checkStmt(s.get());
    }

    void checkStmt(Stmt* stmt) {

        if (auto s = dynamic_cast<ExprStmt*>(stmt)) {
            checkExpr(s->e.get());
        }

        else if (auto s = dynamic_cast<PrintStmt*>(stmt)) {
            checkExpr(s->e.get());
        }

        else if (auto s = dynamic_cast<BlockStmt*>(stmt)) {
            for (auto& x : s->stmts)
                checkStmt(x.get());
        }

        else if (auto s = dynamic_cast<IfStmt*>(stmt)) {
            Type cond = checkExpr(s->condition.get());
            if (cond.kind != TypeKind::Bool)
                throw runtime_error("if condition must be bool");
            checkStmt(s->thenBranch.get());
            if (s->elseBranch)
                checkStmt(s->elseBranch.get());
        }

        else if (auto s = dynamic_cast<WhileStmt*>(stmt)) {
            Type cond = checkExpr(s->condition.get());
            if (cond.kind != TypeKind::Bool)
                throw runtime_error("while condition must be bool");
            checkStmt(s->body.get());
        }

        else if (auto s = dynamic_cast<ReturnStmt*>(stmt)) {
            if (s->value)
                checkExpr(s->value.get());
        }

        else if (auto s = dynamic_cast<FunctionStmt*>(stmt)) {
            for (auto& b : s->body->stmts)
                checkStmt(b.get());
        }
    }

    Type checkExpr(Expr* expr) {

        if (auto e = dynamic_cast<NumberExpr*>(expr)) {
            e->type = Type::Int();
        }

        else if (auto e = dynamic_cast<VariableExpr*>(expr)) {
            e->type = e->symbol->type;
        }

        else if (auto e = dynamic_cast<UnaryExpr*>(expr)) {
            Type rt = checkExpr(e->right.get());

            if (e->op == "!") {
                if (rt.kind != TypeKind::Bool)
                    throw runtime_error("! expects bool");
                e->type = Type::Bool();
            }
            else if (e->op == "-") {
                if (!isNumeric(rt))
                    throw runtime_error("unary - expects numeric");
                e->type = rt;
            }
        }

        else if (auto e = dynamic_cast<BinaryExpr*>(expr)) {
            Type lt = checkExpr(e->left.get());
            Type rt = checkExpr(e->right.get());

            if (e->op == "+" || e->op == "-" || e->op == "*" ||
                e->op == "/" || e->op == "%") {

                if (!isNumeric(lt) || !isNumeric(rt))
                    throw runtime_error("arithmetic expects numeric types");

                e->type = (lt.kind == TypeKind::Float || rt.kind == TypeKind::Float)
                          ? Type::Float()
                          : Type::Int();
            }

            else if (e->op == "<" || e->op == "<=" ||
                     e->op == ">" || e->op == ">=") {

                if (!isNumeric(lt) || !isNumeric(rt))
                    throw runtime_error("comparison expects numeric types");

                e->type = Type::Bool();
            }

            else if (e->op == "==" || e->op == "!=") {
                if (!sameType(lt, rt))
                    throw runtime_error("equality operands must match");
                e->type = Type::Bool();
            }

            else if (e->op == "=") {
                auto v = dynamic_cast<VariableExpr*>(e->left.get());
                if (!v)
                    throw runtime_error("invalid assignment target");

                if (v->symbol->type.kind == TypeKind::Unknown)
                    v->symbol->type = rt;
                else if (!sameType(v->symbol->type, rt))
                    throw runtime_error("assignment type mismatch");

                e->type = rt;
            }
        }

        else if (auto e = dynamic_cast<CallExpr*>(expr)) {
            for (auto& a : e->args)
                checkExpr(a.get());

            e->type = Type::Int(); // temporary until Day 25
        }

        return expr->type;
    }
};
