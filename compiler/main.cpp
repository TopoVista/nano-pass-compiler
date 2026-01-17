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

using namespace std;

// ================== TEST DRIVER ==================

int main() {
    vector<unique_ptr<Stmt>> program;

    program.push_back(make_unique<ExprStmt>(
        make_unique<UnaryExpr>("++", make_unique<VariableExpr>("x"))
    ));

    program.push_back(make_unique<ExprStmt>(
        make_unique<UnaryExpr>("--", make_unique<VariableExpr>("y"))
    ));

    program.push_back(make_unique<PrintStmt>(
        make_unique<VariableExpr>("x")
    ));

    cout << "=== BEFORE ++/-- DESUGARING ===\n";
    for (auto& s : program) s->print(0);

    DesugarIncDecPass pass;
    vector<unique_ptr<Stmt>> lowered;

    for (auto& s : program)
        lowered.push_back(pass.transformStmt(move(s)));

    cout << "\n=== AFTER ++/-- DESUGARING ===\n";
    for (auto& s : lowered) s->print(0);
}




