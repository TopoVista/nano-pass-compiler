#pragma once

#include <iostream>
#include <string>

#include "cps.h"

using namespace std;

struct CPSPrinter {

    void print(CPSExpr* e, int indent = 0) {
        string pad(indent, ' ');

        // -------- CALL --------
        if (auto x = dynamic_cast<CPSCall*>(e)) {
            cout << pad << "call " << x->func << "(";
            for (size_t i = 0; i < x->args.size(); i++) {
                cout << x->args[i];
                if (i + 1 < x->args.size()) cout << ", ";
            }
            cout << ")\n";
            return;
        }

        // -------- LET --------
        if (auto x = dynamic_cast<CPSLet*>(e)) {
            cout << pad << "let " << x->var << " =\n";
            print(x->rhs.get(), indent + 2);
            cout << pad << "in\n";
            print(x->body.get(), indent + 2);
            return;
        }

        // -------- IF --------
        if (auto x = dynamic_cast<CPSIf*>(e)) {
            cout << pad << "if " << x->cond << " then\n";
            print(x->thenE.get(), indent + 2);
            cout << pad << "else\n";
            print(x->elseE.get(), indent + 2);
            return;
        }

        cout << pad << "<unknown cps expr>\n";
    }
};
