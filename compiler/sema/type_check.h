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
                errorAt(s->condition->loc, "Type error: The condition in an 'if' statement must be boolean");
            checkStmt(s->thenBranch.get());
            if (s->elseBranch)
                checkStmt(s->elseBranch.get());
        }

        else if (auto s = dynamic_cast<WhileStmt*>(stmt)) {
            Type cond = checkExpr(s->condition.get());
            if (cond.kind != TypeKind::Bool)
                errorAt(s->condition->loc, "Type error: The condition in a 'while' loop must be boolean");
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

        // ================= NUMBER =================
        if (auto e = dynamic_cast<NumberExpr*>(expr)) {
            e->type = Type::Int();
            return e->type;
        }

        // ================= STRING =================
        else if (auto e = dynamic_cast<StringExpr*>(expr)) {
            e->type = Type::String();
            return e->type;
        }


        // ================= VARIABLE / BOOL LITERAL =================
        else if (auto e = dynamic_cast<VariableExpr*>(expr)) {

            // 🔥 KEY FIX: boolean literals
            if (e->name == "true" || e->name == "false") {
                e->type = Type::Bool();
                return e->type;
            }

            if (!e->symbol)
                errorAt(e->loc, "Use of undeclared variable: " + e->name);

            e->type = e->symbol->type;
            return e->type;
        }

        // ================= UNARY =================
        else if (auto e = dynamic_cast<UnaryExpr*>(expr)) {
            Type rt = checkExpr(e->right.get());

            if (e->op == "!") {
                if (rt.kind != TypeKind::Bool)
                    errorAt(e->loc, "Type error: '!' can only be applied to boolean expressions");
                e->type = Type::Bool();
            }
            else if (e->op == "-") {
                if (!isNumeric(rt))
                    errorAt(e->loc, "Type error: unary '-' expects numeric");
                e->type = rt;
            }

            return e->type;
        }

        // ================= BINARY =================
        else if (auto e = dynamic_cast<BinaryExpr*>(expr)) {
            Type lt = checkExpr(e->left.get());
            Type rt = checkExpr(e->right.get());

            if (e->op == "+" || e->op == "-" || e->op == "*" ||
                e->op == "/" || e->op == "%") {

                if (!isNumeric(lt) || !isNumeric(rt))
                    errorAt(e->loc, "Type error: arithmetic expects numeric types");

                e->type = (lt.kind == TypeKind::Float || rt.kind == TypeKind::Float)
                          ? Type::Float()
                          : Type::Int();
            }

            else if (e->op == "<" || e->op == "<=" ||
                     e->op == ">" || e->op == ">=") {

                if (!isNumeric(lt) || !isNumeric(rt))
                    errorAt(e->loc, "Type error: comparison expects numeric types");

                e->type = Type::Bool();
            }

            else if (e->op == "==" || e->op == "!=") {
                if (!sameType(lt, rt))
                    errorAt(e->loc, "Type error: equality operands must match");
                e->type = Type::Bool();
            }

            else if (e->op == "=") {
                auto v = dynamic_cast<VariableExpr*>(e->left.get());
                if (!v)
                    errorAt(e->loc, "Semantic error: left-hand side of assignment must be a variable");

                if (v->symbol->type.kind == TypeKind::Unknown)
                    v->symbol->type = rt;
                else if (!sameType(v->symbol->type, rt))
                    errorAt(e->loc, "Type error: assignment type mismatch");

                e->type = rt;
            }

            return e->type;
        }

        // ================= CALL =================
        else if (auto e = dynamic_cast<CallExpr*>(expr)) {
            for (auto& a : e->args)
                checkExpr(a.get());

            // temporary until function typing is implemented
            e->type = Type::Int();
            return e->type;
        }

        return Type::Unknown();
    }
};
