#include <iostream>
#include <vector>
#include <memory>

#include "lexer/lexer.h"
#include "parser/parser.h"

#include "passes/desugar_for.h"
#include "passes/desugar_plus_assign.h"
#include "passes/desugar_inc_dec.h"
#include "passes/desugar_if_else.h"
#include "passes/anf_pass.h"

#include "visualize/ast_graphviz.h"

#include "sema/symbol_table.h"
#include "sema/resolve_scopes.h"


using namespace std;


//================= TEST DRIVER ===================//
int main() {

    // ================= SOURCE PROGRAM =================
    string src =
        "for (i = 0; i < 3; i = i + 1) {"
        "   if (i % 2 == 0) {"
        "       print a + b * c;"
        "   } else {"
        "       print i;"
        "   }"
        "}";

    // ================= LEX + PARSE =================
    Lexer lx(src);
    Parser parser(lx.scanTokens());
    auto program = parser.parseProgram();

    // ================= DAY 23: SCOPE RESOLUTION =================
    ResolveScopesPass resolver;
    resolver.resolve(program);

    {
        ASTGraphviz gv("01_parse.dot");
        gv.draw(program);
    }

    // ================= PASS 1: for → while =================
    DesugarForPass pass1;
    vector<unique_ptr<Stmt>> p1;
    for (auto& s : program)
        p1.push_back(pass1.transform(move(s)));

    {
        ASTGraphviz gv("02_for.dot");
        gv.draw(p1);
    }

    // ================= PASS 2: += / ++ / -- =================
    DesugarPlusAssignPass pass2;
    DesugarIncDecPass pass2b;
    vector<unique_ptr<Stmt>> p2;

    for (auto& s : p1) {
        auto t = pass2.transformStmt(move(s));
        t = pass2b.transformStmt(move(t));
        p2.push_back(move(t));
    }

    {
        ASTGraphviz gv("03_assign.dot");
        gv.draw(p2);
    }

    // ================= PASS 3: if / else =================
    DesugarIfElsePass pass3;
    vector<unique_ptr<Stmt>> p3;
    for (auto& s : p2)
        p3.push_back(pass3.transform(move(s)));

    {
        ASTGraphviz gv("04_if.dot");
        gv.draw(p3);
    }

    // ================= PASS 4: ANF =================
    ANFPass pass4;
    vector<unique_ptr<Stmt>> anf;

    for (auto& s : p3) {
        auto lowered = pass4.transformStmt(move(s));
        for (auto& x : lowered)
            anf.push_back(move(x));
    }

    {
        ASTGraphviz gv("05_anf.dot");
        gv.draw(anf);
    }

    cout << "Day 23 scope resolution complete.\n";
    return 0;
}
