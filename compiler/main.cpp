#include <iostream>
#include <vector>
#include <memory>

using namespace std;

// ===== FRONTEND =====
#include "lexer/lexer.h"
#include "parser/parser.h"

// ===== DESUGARING PASSES =====
#include "passes/desugar_for.h"
#include "passes/desugar_plus_assign.h"
#include "passes/desugar_inc_dec.h"
#include "passes/desugar_if_else.h"
#include "passes/desugar_bool.h"
#include "passes/anf_pass.h"

// ===== SEMANTIC ANALYSIS =====
#include "sema/resolve_scopes.h"
#include "sema/type_check.h"

// ===== CPS =====
#include "passes/cps_pass.h"
#include "ir/cps.h"

// ============================================================
// DEBUG PRINTER FOR CPS IR (TEMP, FOR DAY 30 ONLY)
// ============================================================
void printCPS(CPSExpr* e, int d = 0) {
    string pad(d, ' ');

    if (auto x = dynamic_cast<CPSCall*>(e)) {
        cout << pad << "Call " << x->func << "(";
        for (auto& a : x->args) cout << a << " ";
        cout << ")\n";
    }
    else if (auto x = dynamic_cast<CPSLet*>(e)) {
        cout << pad << "Let " << x->var << " =\n";
        printCPS(x->rhs.get(), d + 2);
        cout << pad << "In\n";
        printCPS(x->body.get(), d + 2);
    }
    else if (auto x = dynamic_cast<CPSIf*>(e)) {
        cout << pad << "If " << x->cond << "\n";
        printCPS(x->thenE.get(), d + 2);
        printCPS(x->elseE.get(), d + 2);
    }
}

// ============================================================
// MAIN
// ============================================================
int main() {

    // ===== SOURCE PROGRAM (SIMPLE & VALID) =====
    string src =
        "a = 1;"
        "b = 2;"
        "print a + b;";

    try {
        // ===== LEX + PARSE =====
        Lexer lx(src);
        Parser parser(lx.scanTokens());
        auto program = parser.parseProgram();

        // ===== SCOPE RESOLUTION =====
        ResolveScopesPass resolver;
        resolver.resolve(program);

        // ===== TYPE CHECK =====
        TypeCheckPass typecheck;
        typecheck.check(program);

        // ===== DESUGAR PASSES =====
        DesugarForPass pass1;
        DesugarPlusAssignPass pass2;
        DesugarIncDecPass pass2b;
        DesugarIfElsePass pass3;
        DesugarBoolPass passBool;

        vector<unique_ptr<Stmt>> lowered;

        for (auto& s : program) {
            auto x = pass1.transform(move(s));
            x = pass2.transformStmt(move(x));
            x = pass2b.transformStmt(move(x));
            x = pass3.transform(move(x));
            x = passBool.transformStmt(move(x));
            lowered.push_back(move(x));
        }

        // ===== ANF =====
        ANFPass anfPass;
        vector<unique_ptr<Stmt>> anf;

        for (auto& s : lowered) {
            auto xs = anfPass.transformStmt(move(s));
            for (auto& t : xs)
                anf.push_back(move(t));
        }

        // ===== CPS (DAY 30) =====
        cout << "\n=== CPS IR ===\n";

        CPSPass cps;

        for (auto& s : anf) {
            auto cpsIR = cps.transformStmt(s.get(), "_halt");
            printCPS(cpsIR.get());
        }

        cout << "\nDay 30 CPS test complete.\n";
    }
    catch (const runtime_error& e) {
        cout << "ERROR: " << e.what() << "\n";
    }

    return 0;
}
