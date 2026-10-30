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

struct DifferentialCustomPipeline {
  std::string name = "custom";
  std::vector<std::string> passIds;
};

inline IRPipelineResult buildIRForDifferential(const std::string &source) {
  PipelineBuilder::initialise();
  Lexer lexer(source); Parser parser(lexer.scanTokens()); auto program=parser.parseProgram();
  // AST variable/call nodes retain non-owning Symbol pointers owned by the
  // resolver's table. Keep this resolver alive through type checking,
  // desugaring, and IR lowering; a temporary resolver leaves dangling symbols.
  ResolveScopesPass resolver;
  resolver.resolve(program);
  TypeCheckPass{}.check(program);
  AnalysisManager analyses; PipelineBuilder::registerAnalysesWithManager(analyses);
  CompilerPassOptions options; PassContext context(analyses, options, std::cerr);
  auto pipeline=PipelineBuilder::buildDesugarPipeline(); pipeline.run(program,context);
  return IRPipeline::runAll(program);
}

class DifferentialRunner {
  static void optimize(SSAProgram &program, int level,
                       const std::vector<std::string> *passIds = nullptr) {
    for (auto &function : program.functions) {
      opt::SSAOptimizationContext context;
      if (passIds) {
        opt::SSAOptimizationPipeline pipeline;
        for (const auto &id : *passIds) {
          auto pass = opt::makePass(id);
          if (!pass) throw std::runtime_error("unknown custom SSA pass: " + id);
          pipeline.add(std::move(pass));
        }
        pipeline.run(function, context);
      } else {
        opt::buildPipeline(level).run(function, context);
      }
      const auto verified = verifySSA(function);
      if (!verified.ok) throw std::runtime_error("optimized SSA verifier failed");
    }
  }
public:
  DifferentialResult run(const std::string &source, const std::vector<int> &levels={0,1,2,3}, ExecutionLimits limits={}) const {
    DifferentialResult result;
    try {
      auto ir=buildIRForDifferential(source); result.compiled=true;
      CoreInterpreter core; SSAInterpreter ssa; result.reference=core.run(ir.core,"main",limits);
      for(int level:levels) { SSAProgram candidate=ir.ssa; optimize(candidate,level); auto execution=ssa.run(candidate,"main",limits); result.levels.push_back({level,execution,sameObservableResult(result.reference,execution)}); }
      result.equivalent=std::all_of(result.levels.begin(),result.levels.end(),[](const auto &item){return item.equivalent;});
    } catch(const std::exception &error) { result.diagnostic=error.what(); }
    return result;
  }
  DifferentialResult runCustom(const std::string &source,
                               const DifferentialCustomPipeline &pipeline,
                               ExecutionLimits limits={}) const {
    DifferentialResult result;
    try {
      auto ir=buildIRForDifferential(source); result.compiled=true;
      CoreInterpreter core; SSAInterpreter ssa; result.reference=core.run(ir.core,"main",limits);
      SSAProgram candidate=ir.ssa; optimize(candidate,0,&pipeline.passIds);
      auto execution=ssa.run(candidate,"main",limits);
      result.levels.push_back({-1,execution,sameObservableResult(result.reference,execution)});
      result.equivalent=result.levels.front().equivalent;
    } catch(const std::exception &error) { result.diagnostic=error.what(); }
    return result;
  }
};

} // namespace nanopass
