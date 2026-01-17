#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <cctype>      // for isdigit, isalpha, isalnum
#include <stdexcept>   // for runtime_error
#include <memory>
#include "ast\stmt.h"
#include "lexer\lexer.h"
#include "ast\expr.h"
#include "passes\desugar_for.h"
#include "passes\desugar_plus_assign.h"
#include "passes\desugar_if_else.h"
#include "parser\parser.h"
#include "passes\desugar_inc_dec.h"
#include "passes\anf_pass.h"
#include "visualize\ast_graphviz.h"



// ================== TEST DRIVER ==================

int main() {

    string src =
        "if (x < 5) {"
        "   print x + b * c;"
        "} else {"
        "   print x;"
        "}";

    // ===== 1. PARSE =====
    Lexer lx(src);
    Parser parser(lx.scanTokens());
    auto program = parser.parseProgram();

    // ---- dump after parsing ----
    {
        ASTGraphviz gv("01_parse.dot");
        gv.draw(program);
    }

    // ===== 2. DESUGAR FOR =====
    DesugarForPass pass1;
    vector<unique_ptr<Stmt>> afterFor;
    for (auto& s : program)
        afterFor.push_back(pass1.transform(move(s)));

    {
        ASTGraphviz gv("02_for_desugar.dot");
        gv.draw(afterFor);
    }

    // ===== 3. DESUGAR += =====
    DesugarPlusAssignPass pass2;
    vector<unique_ptr<Stmt>> afterPlus;
    for (auto& s : afterFor)
        afterPlus.push_back(pass2.transformStmt(move(s)));

    {
        ASTGraphviz gv("03_plus_assign.dot");
        gv.draw(afterPlus);
    }

    // ===== 4. DESUGAR IF/ELSE =====
    DesugarIfElsePass pass3;
    vector<unique_ptr<Stmt>> afterIf;
    for (auto& s : afterPlus)
        afterIf.push_back(pass3.transform(move(s)));

    {
        ASTGraphviz gv("04_if_else.dot");
        gv.draw(afterIf);
    }

    // ===== 5. ANF =====
    ANFPass pass4;
    vector<unique_ptr<Stmt>> anfProgram;

    for (auto& s : afterIf) {
        auto lowered = pass4.transformStmt(move(s));
        for (auto& x : lowered)
            anfProgram.push_back(move(x));
    }

    {
        ASTGraphviz gv("05_anf.dot");
        gv.draw(anfProgram);
    }
}


