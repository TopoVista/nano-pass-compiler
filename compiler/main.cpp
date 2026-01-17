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
#include "sema/type.h"
#include "sema/type_check.h"



using namespace std;


//================= TEST DRIVER ===================//
int main() {

    // ================= SOURCE PROGRAM =================
    // Change THIS string to test different cases
    string src =
        "a = 10;"
        "b = 20;"
        "print a + b;";

    // ================= LEX + PARSE =================
    Lexer lx(src);
    Parser parser(lx.scanTokens());
    auto program = parser.parseProgram();

    // ================= DAY 23: SCOPE RESOLUTION =================
    ResolveScopesPass resolver;
    resolver.resolve(program);

    // ================= DAY 24: TYPE CHECKING =================
    TypeCheckPass typecheck;
    typecheck.check(program);

    // ================= OPTIONAL: AST DUMP =================
    {
        ASTGraphviz gv("typecheck.dot");
        gv.draw(program);
    }

    cout << "Day 24 type checking PASSED.\n";
    return 0;
}
