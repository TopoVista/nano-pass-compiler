#pragma once

#include <fstream>
#include <string>
#include "../ast/stmt.h"
#include "../ast/expr.h"

// Emit a Graphviz DOT file representing the AST of a program.
// Usage:
//   ASTGraphviz g("ast.dot");
//   g.draw(program);
struct ASTGraphviz {

    int nodeId = 0;
    std::ofstream out;

    ASTGraphviz(const std::string& filename) {
        out.open(filename);
        out << "digraph AST {\n";
        out << "  node [shape=box fontname=\"Monospace\"];\n";
    }

    ~ASTGraphviz() {
        out << "}\n";
        out.close();
    }

    void draw(const std::vector<std::unique_ptr<Stmt>>& program) {
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
        else if (auto f = dynamic_cast<const ForStmt*>(s)) {
            if (f->init)      drawStmt(f->init.get(), id);
            if (f->condition) drawExpr(f->condition.get(), id);
            if (f->increment) drawExpr(f->increment.get(), id);
            drawStmt(f->body.get(), id);
        }
        else if (auto r = dynamic_cast<const ReturnStmt*>(s)) {
            if (r->value)
                drawExpr(r->value.get(), id);
        }
        else if (auto v = dynamic_cast<const VarDeclStmt*>(s)) {
            if (v->initializer)
                drawExpr(v->initializer.get(), id);
        }
        else if (auto fn = dynamic_cast<const FunctionStmt*>(s)) {
            for (auto& p : fn->params) {
                int pid = newNode("Param " + p.first);
                link(id, pid);
            }
            drawStmt(fn->body.get(), id);
        }
        // BreakStmt, ContinueStmt: leaf nodes — no children.
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
        else if (auto i = dynamic_cast<const IndexExpr*>(e)) {
            drawExpr(i->array.get(), id);
            drawExpr(i->index.get(), id);
        }
        // Leaf nodes (Number, Bool, String, Variable): no children.
    }

    /* ===== HELPERS ===== */

    int newNode(const std::string& label) {
        int id = nodeId++;
        // Escape backslashes and double-quotes inside labels.
        std::string escaped;
        for (char c : label) {
            if (c == '"' || c == '\\') escaped += '\\';
            escaped += c;
        }
        out << "  node" << id << " [label=\"" << escaped << "\"];\n";
        return id;
    }

    void link(int from, int to) {
        out << "  node" << from << " -> node" << to << ";\n";
    }

    std::string stmtLabel(const Stmt* s) {
        if (dynamic_cast<const ExprStmt*>(s))     return "ExprStmt";
        if (dynamic_cast<const PrintStmt*>(s))    return "PrintStmt";
        if (dynamic_cast<const BlockStmt*>(s))    return "Block";
        if (dynamic_cast<const IfStmt*>(s))       return "If";
        if (dynamic_cast<const WhileStmt*>(s))    return "While";
        if (dynamic_cast<const ForStmt*>(s))      return "For";
        if (dynamic_cast<const ReturnStmt*>(s))   return "Return";
        if (auto v = dynamic_cast<const VarDeclStmt*>(s))
            return "VarDecl(" + v->name + ")";
        if (auto f = dynamic_cast<const FunctionStmt*>(s))
            return "Function(" + f->name + ")";
        if (dynamic_cast<const BreakStmt*>(s))    return "Break";
        if (dynamic_cast<const ContinueStmt*>(s)) return "Continue";
        return "Stmt";
    }

    std::string exprLabel(const Expr* e) {
        if (auto n = dynamic_cast<const NumberExpr*>(e)) {
            if (n->isFloat)
                return "Float(" + std::to_string(n->floatValue) + ")";
            return "Int(" + std::to_string(n->intValue) + ")";
        }
        if (auto b = dynamic_cast<const BoolExpr*>(e))
            return std::string("Bool(") + (b->value ? "true" : "false") + ")";
        if (auto s = dynamic_cast<const StringExpr*>(e))
            return "Str(\\\"" + s->value + "\\\")";
        if (auto v = dynamic_cast<const VariableExpr*>(e))
            return "Var(" + v->name + ")";
        if (auto u = dynamic_cast<const UnaryExpr*>(e))
            return "Unary(" + u->op + ")";
        if (auto b = dynamic_cast<const BinaryExpr*>(e))
            return "Binary(" + b->op + ")";
        if (auto c = dynamic_cast<const CallExpr*>(e))
            return "Call(" + c->callee + ")";
        if (dynamic_cast<const IndexExpr*>(e))
            return "Index";
        return "Expr";
    }
};
