#pragma once

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "ast/stmt.h"
#include "ir/anf/anf_ir.h"
#include "ir/anf/anf_printer.h"
#include "ir/anf/anf_verifier.h"
#include "ir/anf/core_to_anf.h"
#include "ir/cfg/analysis/cfg_stats_analysis.h"
#include "ir/cfg/analysis/reachability_analysis.h"
#include "ir/cfg/analysis/reverse_postorder_analysis.h"
#include "ir/cfg/anf_to_cfg.h"
#include "ir/cfg/cfg_graphviz.h"
#include "ir/cfg/cfg_ir.h"
#include "ir/cfg/cfg_printer.h"
#include "ir/cfg/cfg_verifier.h"
#include "ir/core/ast_to_core.h"
#include "ir/core/core_ir.h"
#include "ir/core/core_printer.h"
#include "ir/core/core_verifier.h"

namespace nanopass {

struct IRPipelineResult {
  CoreProgram core;
  ANFProgram anf;
  CFGProgram cfg;
  CoreVerifierResult coreVerResult;
  ANFVerifierResult anfVerResult;
  CFGVerifierResult cfgVerResult;
  CFGStats cfgStats;
};

class IRPipeline {
public:
  static IRPipelineResult runAll(const std::vector<std::unique_ptr<Stmt>> &desugaredAST) {
    IRPipelineResult res;

    // 1. AST -> CoreIR
    ASTToCoreLowering astToCore;
    res.core = astToCore.lower(desugaredAST);
    res.coreVerResult = verifyCoreIR(res.core);
    if (!res.coreVerResult.ok) {
      std::string msg = "internal compiler error: CoreIR verification failed:\n";
      for (const auto &err : res.coreVerResult.errors)
        msg += "  " + err + "\n";
      throw std::runtime_error(msg);
    }

    // 2. CoreIR -> ANF
    CoreToANFLowering coreToANF;
    res.anf = coreToANF.lower(res.core);
    res.anfVerResult = verifyANF(res.anf);
    if (!res.anfVerResult.ok) {
      std::string msg = "internal compiler error: ANF verification failed:\n";
      for (const auto &err : res.anfVerResult.errors)
        msg += "  " + err + "\n";
      throw std::runtime_error(msg);
    }

    // 3. ANF -> CFG
    ANFToCFGLowering anfToCFG;
    res.cfg = anfToCFG.lower(res.anf);
    res.cfgVerResult = verifyCFG(res.cfg);
    if (!res.cfgVerResult.ok) {
      std::string msg = "internal compiler error: CFG verification failed:\n";
      for (const auto &err : res.cfgVerResult.errors)
        msg += "  " + err + "\n";
      throw std::runtime_error(msg);
    }

    // 4. CFG Stats
    res.cfgStats = CFGStatsAnalysis::run(res.cfg);

    return res;
  }
};

} // namespace nanopass
