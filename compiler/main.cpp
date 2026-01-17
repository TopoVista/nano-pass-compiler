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

using namespace std;

// ================== TEST DRIVER ==================
int main() {
    string src =
        "if (x < 5) {"
        "   print x;"
        "} else {"
        "   print x + 1;"
        "}";

    // 1. Lex + parse
    Lexer lx(src);
    Parser parser(lx.scanTokens());
    auto program = parser.parseProgram();

    cout << "===== BEFORE IF/ELSE DESUGARING =====\n";
    for (auto& s : program)
        s->print(0);

    // 2. Run Day 17 pass
    DesugarIfElsePass pass;
    vector<unique_ptr<Stmt>> lowered;

    for (auto& s : program)
        lowered.push_back(pass.transform(move(s)));

    cout << "\n===== AFTER IF/ELSE DESUGARING =====\n";
    for (auto& s : lowered)
        s->print(0);
}





