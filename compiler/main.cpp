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


// ================== TEST DRIVER ==================
#include "passes/anf_pass.h"

int main() {
    auto stmt = make_unique<PrintStmt>(
        make_unique<BinaryExpr>(
            "+",
            make_unique<VariableExpr>("a"),
            make_unique<BinaryExpr>(
                "*",
                make_unique<VariableExpr>("b"),
                make_unique<VariableExpr>("c")
            )
        )
    );

    cout << "=== BEFORE ANF ===\n";
    stmt->print(0);

    ANFPass pass;
    auto lowered = pass.transformStmt(move(stmt));

    cout << "\n=== AFTER ANF ===\n";
    for (auto& s : lowered)
        s->print(0);
}



