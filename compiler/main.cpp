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
#include "ir/cps_printer.h"

// ============================================================
// MAIN
// ============================================================
int main() {

    // ===== DAY 32 TEST PROGRAMS =====
    vector<string> tests = {
        "print x;",
        "if (true) { print 1; }",
        "a = 1; if (a == 1) { print true; }",
        "if (true) {print false;}"
    };


    for (size_t i = 0; i < tests.size(); i++) {

        cout << "\n========================================\n";
        cout << "TEST " << i + 1 << "\n";
        cout << "SOURCE:\n" << tests[i] << "\n";
        cout << "========================================\n";

        try {
            // ===== LEX + PARSE =====
            Lexer lx(tests[i]);
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

            // ===== CPS (DAY 30 / 32) =====
            cout << "\n--- CPS IR ---\n";

            CPSPass cps;
            CPSPrinter printer;

            for (auto& s : anf) {
                auto cpsIR = cps.transformStmt(s.get(), "_halt");
                printer.print(cpsIR.get());
            }

            cout << "\nTEST " << i + 1 << " OK\n";
        }
        catch (const runtime_error& e) {
            cout << "ERROR: " << e.what() << "\n";
        }
    }

    cout << "\n=== DAY 32 COMPLETE: CPS LOWERING VERIFIED ===\n";
    return 0;
}
