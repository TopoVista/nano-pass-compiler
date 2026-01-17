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
#include <iostream>
#include <vector>
#include <memory>

#include "lexer/lexer.h"
#include "parser/parser.h"

#include "sema/resolve_scopes.h"
#include "sema/type_check.h"

using namespace std;

int main() {

    // ===== SOURCE PROGRAM (INTENTIONAL ERROR) =====
    string src = R"(
    print x;
)";








    try {
        // ===== LEX + PARSE =====
        Lexer lx(src);
        Parser parser(lx.scanTokens());
        auto program = parser.parseProgram();

        // ===== SCOPE RESOLUTION (Day 23) =====
        ResolveScopesPass resolver;
        resolver.resolve(program);

        // ===== TYPE CHECK (Day 24 / 26) =====
        TypeCheckPass typecheck;
        typecheck.check(program);

        cout << "No semantic error (unexpected)\n";
    }
    catch (const runtime_error& e) {
        cout << e.what() << "\n";
    }

    return 0;
}
