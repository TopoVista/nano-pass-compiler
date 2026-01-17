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
#include "passes/desugar_bool.h"

#include "visualize/ast_graphviz.h"

#include "sema/symbol_table.h"
#include "sema/resolve_scopes.h"
#include "sema/type.h"
#include "sema/type_check.h"



using namespace std;


//================= TEST DRIVER ===================//
int main() {

    // ===== SOURCE PROGRAM =====
    string src =
        "if (true) {"
        "   print false;"
        "}";

    // ===== LEX + PARSE =====
    Lexer lx(src);
    Parser parser(lx.scanTokens());
    auto program = parser.parseProgram();

    {
        ASTGraphviz gv("01_before_bool.dot");
        gv.draw(program);
    }

    // ===== DAY 23: SCOPE RESOLUTION =====
    ResolveScopesPass resolver;
    resolver.resolve(program);

    // ===== DAY 24: TYPE CHECK =====
    TypeCheckPass typecheck;
    typecheck.check(program);

    // ===== DAY 25: BOOL → INT =====
    DesugarBoolPass boolPass;
    vector<unique_ptr<Stmt>> lowered;

    for (auto& s : program)
        lowered.push_back(boolPass.transformStmt(move(s)));

    {
        ASTGraphviz gv("02_after_bool.dot");
        gv.draw(lowered);
    }

    cout << "Day 25 test complete.\n";
    return 0;
}
