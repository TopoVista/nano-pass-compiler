#pragma once

#include <sstream>

#include "interpreter/core_interpreter.h"
#include "interpreter/ssa_interpreter.h"
#include "ir/pipeline.h"
#include "lexer/lexer.h"
#include "optimizer/ssa_optimizer.h"
#include "optimizer/advanced_optimizer.h"
#include "parser/parser.h"
#include "passes/PassContext.h"
#include "passes/PassManager.h"
#include "passes/PipelineBuilder.h"
#include "sema/resolve_scopes.h"
#include "sema/type_check.h"

namespace nanopass {

struct DifferentialLevelResult {
  int level = 0;
  std::string pipeline;
  ExecutionResult execution;
  bool equivalent = false;
};
struct DifferentialResult {
  bool compiled = false;
  std::string diagnostic;
  ExecutionResult reference;
  std::vector<DifferentialLevelResult> levels;
  bool equivalent = false;
  size_t policyDecisions = 0;
  size_t policyApplied = 0;
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
  static std::string pipelineName(const std::vector<std::string> &passIds) {
    std::ostringstream out;
    for (size_t index = 0; index < passIds.size(); ++index) {
      if (index) out << ',';
      out << passIds[index];
    }
    return out.str();
  }
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
      for(int level:levels) { SSAProgram candidate=ir.ssa; optimize(candidate,level); auto execution=ssa.run(candidate,"main",limits); result.levels.push_back({level,"O"+std::to_string(level),execution,sameObservableResult(result.reference,execution)}); }
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
      result.levels.push_back({-1,pipeline.name+":"+pipelineName(pipeline.passIds),execution,sameObservableResult(result.reference,execution)});
      result.equivalent=result.levels.front().equivalent;
    } catch(const std::exception &error) { result.diagnostic=error.what(); }
    return result;
  }
  // The learned policy changes only profitability.  It is deliberately run
  // before the normal O2 cleanup pipeline so a fuzz campaign covers both the
  // policy gate and all deterministic transformations it may expose.
  DifferentialResult runLearnedInline(const std::string &source,
                                      ExecutionLimits limits={}) const {
    DifferentialResult result;
    try {
      auto ir = buildIRForDifferential(source); result.compiled = true;
      result.reference = CoreInterpreter{}.run(ir.core, "main", limits);
      SSAProgram candidate = ir.ssa;
      opt::SSAAnalysisManager analyses;
      const auto policy = opt::LearnedInlinePolicy::builtinInlineImitationV1();
      const auto inlineResult = opt::SSAProgramInliner{}.run(candidate, {}, &analyses, &policy);
      result.policyDecisions = inlineResult.statistics.get("learnedInlineDecisions");
      result.policyApplied = inlineResult.statistics.get("callsInlined");
      for (auto &function : candidate.functions) {
        opt::SSAOptimizationContext context;
        opt::buildPipeline(2).run(function, context);
        const auto verified = verifySSA(function);
        if (!verified.ok) throw std::runtime_error("learned policy produced invalid SSA");
      }
      const auto execution = SSAInterpreter{}.run(candidate, "main", limits);
      result.levels.push_back({2, "O2+learned-inline", execution,
                               sameObservableResult(result.reference, execution)});
      result.equivalent = result.levels.front().equivalent;
    } catch (const std::exception &error) { result.diagnostic = error.what(); }
    return result;
  }

  // The deterministic inliner is the policy baseline: every candidate first
  // passes the same legality, budget, and SSA-verification gates as the
  // learned variant, but profitability comes from InlineCostModel alone.
  DifferentialResult runHeuristicInline(const std::string &source,
                                        ExecutionLimits limits={}) const {
    DifferentialResult result;
    try {
      auto ir = buildIRForDifferential(source); result.compiled = true;
      result.reference = CoreInterpreter{}.run(ir.core, "main", limits);
      SSAProgram candidate = ir.ssa;
      opt::SSAAnalysisManager analyses;
      const auto inlineResult = opt::SSAProgramInliner{}.run(candidate, {}, &analyses);
      result.policyDecisions = inlineResult.statistics.get("callsConsidered");
      result.policyApplied = inlineResult.statistics.get("callsInlined");
      for (auto &function : candidate.functions) {
        opt::SSAOptimizationContext context;
        opt::buildPipeline(2).run(function, context);
        const auto verified = verifySSA(function);
        if (!verified.ok) throw std::runtime_error("heuristic policy produced invalid SSA");
      }
      const auto execution = SSAInterpreter{}.run(candidate, "main", limits);
      result.levels.push_back({2, "O2+heuristic-inline", execution,
                               sameObservableResult(result.reference, execution)});
      result.equivalent = result.levels.front().equivalent;
    } catch (const std::exception &error) { result.diagnostic = error.what(); }
    return result;
  }
};

} // namespace nanopass
