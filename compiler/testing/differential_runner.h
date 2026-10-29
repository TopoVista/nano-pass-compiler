#pragma once

#include <sstream>

#include "interpreter/core_interpreter.h"
#include "interpreter/ssa_interpreter.h"
#include "ir/pipeline.h"
#include "lexer/lexer.h"
#include "optimizer/ssa_optimizer.h"
#include "parser/parser.h"
#include "passes/PassContext.h"
#include "passes/PassManager.h"
#include "passes/PipelineBuilder.h"
#include "sema/resolve_scopes.h"
#include "sema/type_check.h"

namespace nanopass {

struct DifferentialLevelResult { int level = 0; ExecutionResult execution; bool equivalent = false; };
struct DifferentialResult {
  bool compiled = false;
  std::string diagnostic;
  ExecutionResult reference;
  std::vector<DifferentialLevelResult> levels;
  bool equivalent = false;
};

inline IRPipelineResult buildIRForDifferential(const std::string &source) {
  PipelineBuilder::initialise();
  Lexer lexer(source); Parser parser(lexer.scanTokens()); auto program=parser.parseProgram();
  ResolveScopesPass{}.resolve(program); TypeCheckPass{}.check(program);
  AnalysisManager analyses; PipelineBuilder::registerAnalysesWithManager(analyses);
  CompilerPassOptions options; PassContext context(analyses, options, std::cerr);
  auto pipeline=PipelineBuilder::buildDesugarPipeline(); pipeline.run(program,context);
  return IRPipeline::runAll(program);
}

class DifferentialRunner {
public:
  DifferentialResult run(const std::string &source, const std::vector<int> &levels={0,1,2,3}, ExecutionLimits limits={}) const {
    DifferentialResult result;
    try {
      auto ir=buildIRForDifferential(source); result.compiled=true;
      CoreInterpreter core; SSAInterpreter ssa; result.reference=core.run(ir.core,"main",limits);
      for(int level:levels) { SSAProgram candidate=ir.ssa; for(auto &function:candidate.functions){ opt::SSAOptimizationContext context; opt::buildPipeline(level).run(function,context); const auto verified=verifySSA(function); if(!verified.ok) throw std::runtime_error("optimized SSA verifier failed"); } auto execution=ssa.run(candidate,"main",limits); result.levels.push_back({level,execution,sameObservableResult(result.reference,execution)}); }
      result.equivalent=std::all_of(result.levels.begin(),result.levels.end(),[](const auto &item){return item.equivalent;});
    } catch(const std::exception &error) { result.diagnostic=error.what(); }
    return result;
  }
};

} // namespace nanopass
