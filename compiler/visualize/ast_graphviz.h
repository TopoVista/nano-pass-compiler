#pragma once

#include <fstream>
#include <string>
#include "../ast/stmt.h"
#include "../ast/expr.h"

struct ASTGraphviz {

    int nodeId = 0;
    ofstream out;

    ASTGraphviz(const string& filename) {
        out.open(filename);
        out << "digraph AST {\n";
        out << "node [shape=box];\n";
    }

    ~ASTGraphviz() {
        out << "}\n";
        out.close();
    }

    /* ===== ENTRY ===== */

    void draw(const vector<unique_ptr<Stmt>>& program) {
        for (auto& s : program)
            drawStmt(s.get(), -1);
    }

private:

    /* ===== STATEMENTS ===== */

    void drawStmt(const Stmt* s, int parent) {
        int id = newNode(stmtLabel(s));
        if (parent != -1)
            link(parent, id);

        if (auto e = dynamic_cast<const ExprStmt*>(s))
            drawExpr(e->e.get(), id);

        else if (auto p = dynamic_cast<const PrintStmt*>(s))
            drawExpr(p->e.get(), id);

        else if (auto b = dynamic_cast<const BlockStmt*>(s)) {
            for (auto& x : b->stmts)
                drawStmt(x.get(), id);
        }

        else if (auto i = dynamic_cast<const IfStmt*>(s)) {
            drawExpr(i->condition.get(), id);
            drawStmt(i->thenBranch.get(), id);
            if (i->elseBranch)
                drawStmt(i->elseBranch.get(), id);
        }

        else if (auto w = dynamic_cast<const WhileStmt*>(s)) {
            drawExpr(w->condition.get(), id);
            drawStmt(w->body.get(), id);
        }

        else if (auto r = dynamic_cast<const ReturnStmt*>(s)) {
            if (r->value)
                drawExpr(r->value.get(), id);
        }

        else if (auto f = dynamic_cast<const FunctionStmt*>(s)) {
            for (auto& p : f->params) {
                int pid = newNode("Param " + p);
                link(id, pid);
            }
            drawStmt(f->body.get(), id);
        }
    }

    /* ===== EXPRESSIONS ===== */

    void drawExpr(const Expr* e, int parent) {
        int id = newNode(exprLabel(e));
        link(parent, id);

        if (auto b = dynamic_cast<const BinaryExpr*>(e)) {
            drawExpr(b->left.get(), id);
            drawExpr(b->right.get(), id);
        }

        else if (auto u = dynamic_cast<const UnaryExpr*>(e)) {
            drawExpr(u->right.get(), id);
        }

        else if (auto c = dynamic_cast<const CallExpr*>(e)) {
            for (auto& a : c->args)
                drawExpr(a.get(), id);
        }
    }

    /* ===== HELPERS ===== */

    int newNode(const string& label) {
        int id = nodeId++;
        out << "node" << id << " [label=\"" << label << "\"];\n";
        return id;
    }

    void link(int from, int to) {
        out << "node" << from << " -> node" << to << ";\n";
    }

    string stmtLabel(const Stmt* s) {
        if (dynamic_cast<const ExprStmt*>(s))   return "ExprStmt";
        if (dynamic_cast<const PrintStmt*>(s))  return "PrintStmt";
        if (dynamic_cast<const BlockStmt*>(s))  return "Block";
        if (dynamic_cast<const IfStmt*>(s))     return "If";
        if (dynamic_cast<const WhileStmt*>(s))  return "While";
        if (dynamic_cast<const ReturnStmt*>(s)) return "Return";
        if (dynamic_cast<const FunctionStmt*>(s)) return "Function";
        return "Stmt";
    }

    string exprLabel(const Expr* e) {
        if (auto n = dynamic_cast<const NumberExpr*>(e))
            return "Number(" + to_string(n->value) + ")";
        if (auto v = dynamic_cast<const VariableExpr*>(e))
            return "Var(" + v->name + ")";
        if (auto u = dynamic_cast<const UnaryExpr*>(e))
            return "Unary(" + u->op + ")";
        if (auto b = dynamic_cast<const BinaryExpr*>(e))
            return "Binary(" + b->op + ")";
        if (auto c = dynamic_cast<const CallExpr*>(e))
            return "Call(" + c->callee + ")";
        return "Expr";
    }
};
