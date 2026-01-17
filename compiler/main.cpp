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
#include "visualize\ast_graphviz.h"
#include "ir/ir_printer.h"


// ================== TEST DRIVER ==================


int main() {

    vector<IRInstr> ir = {
        { IROp::Mul,    "_t0", "b", "c" },
        { IROp::Add,    "_t1", "a", "_t0" },
        { IROp::Print, "",    "_t1", "" }
    };

    IRPrinter::print(ir);
}



