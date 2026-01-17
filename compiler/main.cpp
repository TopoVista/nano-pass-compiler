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

using namespace std;

// ================== TEST DRIVER ==================
int main(){
    auto forStmt = make_unique<ForStmt>(
        make_unique<ExprStmt>(
            make_unique<BinaryExpr>("=",
                make_unique<VariableExpr>("i"),
                make_unique<NumberExpr>(0)
            )
        ),
        make_unique<BinaryExpr>("<",
            make_unique<VariableExpr>("i"),
            make_unique<NumberExpr>(3)
        ),
        make_unique<BinaryExpr>("=",
            make_unique<VariableExpr>("i"),
            make_unique<BinaryExpr>("+",
                make_unique<VariableExpr>("i"),
                make_unique<NumberExpr>(1)
            )
        ),
        make_unique<PrintStmt>(
            make_unique<VariableExpr>("i")
        )
    );

    cout<<"=== BEFORE DESUGARING ===\n";
    forStmt->print(0);

    DesugarForPass pass;
    auto lowered = pass.transform(move(forStmt));

    cout<<"\n=== AFTER DESUGARING ===\n";
    lowered->print(0);
}




