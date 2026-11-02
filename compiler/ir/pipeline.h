#pragma once

#include <chrono>
#include <cstdint>
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
#include "ir/cfg/analysis/dominance_frontier_analysis.h"
#include "ir/cfg/analysis/dominator_analysis.h"
#include "ir/cfg/analysis/reachability_analysis.h"
#include "ir/cfg/analysis/reverse_postorder_analysis.h"
#include "ir/cfg/anf_to_cfg.h"
#include "ir/cfg/cfg_graphviz.h"
#include "ir/cfg/dominator_graphviz.h"
#include "ir/cfg/cfg_ir.h"
#include "ir/cfg/cfg_printer.h"
#include "ir/cfg/cfg_verifier.h"
#include "ir/core/ast_to_core.h"
#include "ir/core/core_ir.h"
#include "ir/core/core_printer.h"
#include "ir/core/core_verifier.h"
#include "ir/ssa/ssa_builder.h"
#include "ir/ssa/ssa_printer.h"
#include "ir/ssa/ssa_verifier.h"

namespace nanopass {

struct IRPipelineResult {
  CoreProgram core;
  ANFProgram anf;
  CFGProgram cfg;
  CoreVerifierResult coreVerResult;
  ANFVerifierResult anfVerResult;
  CFGVerifierResult cfgVerResult;
  CFGStats cfgStats;
  SSAProgram ssa;
  SSAStatistics ssaStats;
  SSAVerifierResult ssaVerResult;
  // The service exposes this measured boundary independently from the
  // aggregate Core→ANF→CFG→SSA lowering event.  It includes construction and
  // mandatory SSA verification, which are inseparable correctness work.
  uint64_t ssaConstructionUs = 0;
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
    const auto ssaStarted = std::chrono::steady_clock::now();
    SSABuilder ssaBuilder;
    res.ssa = ssaBuilder.build(res.cfg, &res.ssaStats);
    for (const auto &function : res.ssa.functions) {
      const auto verified = verifySSA(function);
      if (!verified.ok) {
        res.ssaVerResult = verified;
        std::string message = "internal compiler error: SSA verification failed in @" + function.name + ":\n";
        for (const auto &error : verified.errors) message += "  " + error + "\n";
        throw std::runtime_error(message);
      }
    }
    res.ssaVerResult.ok = true;
    res.ssaConstructionUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - ssaStarted).count());

    return res;
  }
};

} // namespace nanopass
