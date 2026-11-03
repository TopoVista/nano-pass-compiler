#pragma once

// The service layer is deliberately an in-process adapter around the real
// compiler pipeline.  It owns no alternate parser, IR, optimizer, or runtime;
// every browser-visible fact comes from the same C++ implementation used by
// the command-line tools and tests.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "common/error.h"
#include "interpreter/core_interpreter.h"
#include "interpreter/ssa_interpreter.h"
#include "ir/pipeline.h"
#include "ir/ssa/call_graph.h"
#include "ir/ssa/loop_analysis.h"
#include "lexer/lexer.h"
#include "optimizer/advanced_optimizer.h"
#include "optimizer/optimization_policy.h"
#include "optimizer/ssa_optimizer.h"
#include "parser/parser.h"
#include "passes/PipelineBuilder.h"
#include "passes/ASTAnalyses.h"
#include "sema/resolve_scopes.h"
#include "sema/type_check.h"
#include "support/project_version.h"
#include "testing/differential_runner.h"

#if defined(NANO_SERVICE_HAS_LLVM)
#include "backend/llvm/ssa_llvm_lowering.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#endif
#if defined(NANO_SERVICE_HAS_JIT)
#include "backend/llvm/ssa_llvm_jit.h"
#endif

namespace nanopass::service {

inline constexpr uint32_t kCompilerServiceSchemaVersion = 1;

enum class ExecutionMode { CoreInterpreter, SSAInterpreter, JIT, AdaptiveJIT };
enum class ExecutionPolicy { Disabled, InterpreterOnly, LocalTrusted };

inline const char *executionPolicyName(ExecutionPolicy policy) {
  switch (policy) {
  case ExecutionPolicy::Disabled: return "disabled";
  case ExecutionPolicy::InterpreterOnly: return "interpreter-only";
  case ExecutionPolicy::LocalTrusted: return "local-trusted";
  }
  return "disabled";
}

struct ArtifactSelection {
  bool source = false;
  bool ast = false;
  bool desugaredAst = false;
  bool core = false;
  bool anf = false;
  bool cfg = false;
  bool ssa = false;
  bool optimizedSsa = false;
  bool llvm = false;
  bool graphs = false;
  bool remarks = false;
  bool telemetry = false;
  bool provenance = false;
};

struct CompileRequestV1 {
  uint32_t schemaVersion = kCompilerServiceSchemaVersion;
  std::string source;
  int optimizationLevel = 2;
  // `default` preserves the original service pipeline.  The other values are
  // a deliberately closed action space for the direct-call inliner; clients
  // cannot provide a model path, pass name, or generated transformation.
  std::string optimizationPolicy = "default";
  std::optional<std::vector<std::string>> passes;
  ArtifactSelection include;
};

struct ServiceLimits {
  size_t maxSourceBytes = 256 * 1024;
  size_t maxAstDepth = 256;
  size_t maxFunctions = 256;
  size_t maxIRBlocks = 16384;
  size_t maxSSAValues = 131072;
  size_t maxOptimizationIterations = 4;
  uint64_t maxCompileDurationMs = 5000;
  uint64_t maxExecutionDurationMs = 2000;
  uint64_t maxExecutionSteps = 250000;
  size_t maxCallDepth = 128;
  size_t maxOutputBytes = 64 * 1024;
  size_t maxArrayElements = 1 << 20;
  size_t maxFuzzCases = 64;
  size_t maxTokens = 50000;
  size_t maxProfileBytes = 1024 * 1024;
};

enum class ErrorKind { None, User, Runtime, ResourceLimit, Internal, Transport };

struct ServiceDiagnostic {
  std::string severity;
  std::string code;
  std::string message;
  int line = 0;
  int column = 0;
  ErrorKind kind = ErrorKind::None;
};

struct IRSnapshot { std::string stageId; std::string displayName; std::string text; };
struct TransformationSnapshot { std::string passId; std::string before; std::string after; };

struct PassEvent {
  std::string id;
  std::string displayName;
  std::string category;
  bool changed = false;
  uint64_t durationUs = 0;
  size_t instructionsBefore = 0, instructionsAfter = 0;
  size_t blocksBefore = 0, blocksAfter = 0;
  size_t valuesBefore = 0, valuesAfter = 0;
  bool verified = true;
  std::map<std::string, size_t> counters;
};

struct GraphNode { std::string id; std::string label; std::map<std::string, std::string> metadata; };
struct GraphEdge { std::string from; std::string to; std::string kind; std::map<std::string, std::string> metadata; };
struct GraphArtifact { std::string kind; std::string function; std::vector<GraphNode> nodes; std::vector<GraphEdge> edges; };

struct Remark { std::string kind; std::string passId; std::string message; std::optional<int> block; std::optional<int> value; int line = 0; int column = 0; };

struct SSAValueUse { std::string kind; int block = -1; std::string detail; };
struct SSAValueInfo {
  int value = -1;
  std::string function;
  std::string kind;
  std::string type;
  int definingBlock = -1;
  int line = 0;
  int column = 0;
  std::vector<SSAValueUse> uses;
};
struct PolicyFeatureInfo {
  std::string function;
  uint64_t instructions = 0, blocks = 0, loops = 0, callers = 0, callees = 0;
  uint64_t dynamicCalls = 0, dynamicBlocks = 0, dynamicLoopBackedges = 0;
  std::string hotness;
  std::string hotnessReason;
};

struct CompileResponseV1 {
  uint32_t schemaVersion = kCompilerServiceSchemaVersion;
  bool success = false;
  std::string compilerRevision = std::string(build::kCompilerRevision);
  std::vector<ServiceDiagnostic> diagnostics;
  std::vector<IRSnapshot> snapshots;
  std::vector<TransformationSnapshot> transformations;
  std::vector<PassEvent> passes;
  std::vector<Remark> remarks;
  std::vector<GraphArtifact> graphs;
  std::vector<SSAValueInfo> values;
  std::vector<PolicyFeatureInfo> policyFeatures;
  // Valid JSON policy-decision records emitted by the optimizer.  Keeping
  // them structured allows the Lab to explain factual model inputs without
  // pretending that model confidence is a correctness proof.
  std::vector<std::string> policyRecords;
  uint64_t totalCompileUs = 0;
  size_t functions = 0, blocks = 0, valuesCount = 0;
};

struct RunRequestV1 { CompileRequestV1 compile; ExecutionMode mode = ExecutionMode::SSAInterpreter; };
struct AdaptiveEvent {
  std::string kind;
  std::string function;
  uint64_t calls = 0;
  std::string detail;
};
struct RunResponseV1 {
  uint32_t schemaVersion = kCompilerServiceSchemaVersion;
  bool success = false;
  ErrorKind errorKind = ErrorKind::None;
  std::string stdoutText, stderrText, returnValue, status;
  uint64_t stepsExecuted = 0;
  uint64_t executionDurationUs = 0;
  std::string runtimeProfileJson;
  std::vector<AdaptiveEvent> adaptiveEvents;
  std::vector<PolicyFeatureInfo> policyFeatures;
  CompileResponseV1 compilation;
};
struct CorrectnessCheck {
  std::string stage;
  bool passed = false;
  std::string detail;
};
struct CorrectnessResponseV1 {
  uint32_t schemaVersion = kCompilerServiceSchemaVersion;
  bool success = false;
  bool equivalent = false;
  CompileResponseV1 compilation;
  std::vector<CorrectnessCheck> checks;
};

inline std::string jsonEscape(std::string_view value) {
  std::string escaped;
  for (unsigned char ch : value) {
    switch (ch) {
    case '\\': escaped += "\\\\"; break; case '"': escaped += "\\\""; break;
    case '\n': escaped += "\\n"; break; case '\r': escaped += "\\r"; break;
    case '\t': escaped += "\\t"; break;
    default: if (ch < 0x20) { std::ostringstream hex; hex << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(ch); escaped += hex.str(); } else escaped += static_cast<char>(ch);
    }
  }
  return escaped;
}
inline const char *errorKindName(ErrorKind kind) {
  switch (kind) { case ErrorKind::None: return "none"; case ErrorKind::User: return "user"; case ErrorKind::Runtime: return "runtime"; case ErrorKind::ResourceLimit: return "resource_limit"; case ErrorKind::Internal: return "internal"; case ErrorKind::Transport: return "transport"; }
  return "internal";
}
inline const char *remarkKindName(opt::OptimizationRemarkKind kind) {
  switch (kind) { case opt::OptimizationRemarkKind::Applied: return "applied"; case opt::OptimizationRemarkKind::Missed: return "missed"; case opt::OptimizationRemarkKind::Analysis: return "analysis"; }
  return "analysis";
}
#if defined(NANO_SERVICE_HAS_JIT)
inline const char *adaptiveEventKindName(llvm_backend::AdaptiveJITEventKind kind) {
  switch (kind) {
  case llvm_backend::AdaptiveJITEventKind::Tier0Compiled: return "tier0_compiled";
  case llvm_backend::AdaptiveJITEventKind::HotThresholdReached: return "hot_threshold_reached";
  case llvm_backend::AdaptiveJITEventKind::Tier1CompilationStarted: return "tier1_compilation_started";
  case llvm_backend::AdaptiveJITEventKind::Tier1Installed: return "tier1_installed";
  case llvm_backend::AdaptiveJITEventKind::Tier1Rejected: return "tier1_rejected";
  }
  return "unknown";
}
#endif
inline std::string astText(const std::vector<std::unique_ptr<Stmt>> &program) {
  // Legacy AST printing targets std::cout.  Capture it only inside this
  // single-threaded local service call; all durable API artifacts use streams.
  std::ostringstream output; auto *previous = std::cout.rdbuf(output.rdbuf());
  for (const auto &statement : program) statement->print(0);
  std::cout.rdbuf(previous); return output.str();
}
inline std::string toText(const CoreProgram &program) { std::ostringstream out; printCoreIR(program, out); return out.str(); }
inline std::string toText(const ANFProgram &program) { std::ostringstream out; printANF(program, out); return out.str(); }
inline std::string toText(const CFGProgram &program) { std::ostringstream out; printCFG(program, out); return out.str(); }
inline std::string toText(const SSAProgram &program) { std::ostringstream out; printSSA(program, out); return out.str(); }
inline size_t programBlocks(const SSAProgram &program) { size_t count = 0; for (const auto &function : program.functions) count += function.blocks.size(); return count; }
inline size_t programValues(const SSAProgram &program) { size_t count = 0; for (const auto &function : program.functions) count += opt::valueCount(function); return count; }
inline size_t programInstructions(const SSAProgram &program) { size_t count = 0; for (const auto &function : program.functions) count += opt::instructionCount(function); return count; }

inline GraphArtifact cfgGraph(const SSAFunction &function) {
  GraphArtifact graph{"cfg", function.name, {}, {}};
  const auto loops = LoopInfoAnalysis::run(function);
  for (const auto &block : function.blocks) {
    GraphNode node{function.name + ":bb" + std::to_string(block.id.index), block.id.str() + (block.label.empty() ? "" : "." + block.label), {}};
    node.metadata["block_id"] = std::to_string(block.id.index);
    node.metadata["instructions"] = std::to_string(block.instructions.size() + block.phis.size());
    if (const auto loop = loops.innermostLoopContaining(block.id)) node.metadata["loop_id"] = std::to_string(loop->index);
    graph.nodes.push_back(std::move(node));
    for (const auto successor : block.successors) graph.edges.push_back({function.name + ":bb" + std::to_string(block.id.index), function.name + ":bb" + std::to_string(successor.index), "cfg", {}});
  }
  return graph;
}
inline GraphArtifact dominatorGraph(const SSAFunction &function) {
  CFGFunction view; view.entryBlock = function.entryBlock;
  for (const auto &block : function.blocks) { auto item = std::make_unique<BasicBlock>(); item->id = block.id; item->predecessors = block.predecessors; item->successors = block.successors; view.blockIndexMap[block.id.index] = view.blocks.size(); view.blocks.push_back(std::move(item)); }
  const auto dom = DominatorAnalysis::run(view); GraphArtifact graph{"dominator-tree", function.name, {}, {}};
  for (const auto &block : function.blocks) { graph.nodes.push_back({function.name + ":bb" + std::to_string(block.id.index), block.id.str(), {}}); if (const auto idom = dom.immediateDominator(block.id)) graph.edges.push_back({function.name + ":bb" + std::to_string(idom->index), function.name + ":bb" + std::to_string(block.id.index), "idom", {}}); }
  return graph;
}
inline GraphArtifact loopGraph(const SSAFunction &function) {
  GraphArtifact graph{"loops", function.name, {}, {}}; const auto loops = LoopInfoAnalysis::run(function);
  for (const auto &loop : loops.loops) { GraphNode node{function.name + ":loop" + std::to_string(loop.id.index), "loop " + std::to_string(loop.id.index) + " / " + loop.header.str(), {}}; node.metadata["header"] = std::to_string(loop.header.index); node.metadata["depth"] = std::to_string(loop.depth); if (loop.preheader) node.metadata["preheader"] = std::to_string(loop.preheader->index); node.metadata["latches"] = std::to_string(loop.backedgeSources.size()); node.metadata["exits"] = std::to_string(loop.exitEdges.size()); graph.nodes.push_back(std::move(node)); if (loop.parent) graph.edges.push_back({function.name + ":loop" + std::to_string(loop.parent->index), function.name + ":loop" + std::to_string(loop.id.index), "contains", {}}); }
  return graph;
}
inline GraphArtifact callGraph(const SSAProgram &program) {
  GraphArtifact graph{"call-graph", "program", {}, {}}; const auto calls = CallGraphAnalysis::run(program);
  for (const auto &function : program.functions) { GraphNode node{"fn:" + function.name, "@" + function.name, {}}; node.metadata["recursive"] = calls.recursive(function.name) ? "true" : "false"; graph.nodes.push_back(std::move(node)); }
  for (const auto &site : calls.sites) graph.edges.push_back({"fn:" + site.caller, "fn:" + site.callee, "call", {{"block", std::to_string(site.block.index)}}});
  return graph;
}
inline std::vector<SSAValueInfo> valueInformation(const SSAProgram &program) {
  std::vector<SSAValueInfo> result;
  for (const auto &function : program.functions) {
    SSADefUseIndex uses; uses.rebuild(function);
    for (const auto &parameter : function.parameters) result.push_back({parameter.value.index, function.name, "parameter", ssaTypeName(parameter.type), function.entryBlock.index, 0, 0, {}});
    for (const auto &block : function.blocks) {
      for (const auto &phi : block.phis) result.push_back({phi.result.index, function.name, "phi", ssaTypeName(phi.type), block.id.index, phi.origin.loc.line, phi.origin.loc.col, {}});
      for (const auto &instruction : block.instructions) if (instruction.result) result.push_back({instruction.result->index, function.name, "instruction", ssaTypeName(instruction.type), block.id.index, instruction.origin.loc.line, instruction.origin.loc.col, {}});
    }
    for (auto &info : result) if (info.function == function.name) for (const auto &use : uses.usesOf({info.value})) info.uses.push_back({use.kind == SSAUseKind::Phi ? "phi" : use.kind == SSAUseKind::Instruction ? "instruction" : "terminator", use.block.index, ""});
  }
  std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) { return std::tie(a.function, a.value) < std::tie(b.function, b.value); }); return result;
}
inline std::vector<PolicyFeatureInfo> policyFeatureInformation(
    const SSAProgram &program, const profile::ProgramRuntimeProfile *profile = nullptr) {
  std::vector<PolicyFeatureInfo> result;
  for (const auto &function : program.functions) {
    const auto features = opt::extractOptimizationFeatures(program, function, profile);
    const profile::FunctionRuntimeProfile *dynamic = nullptr;
    if (profile) { const auto found = profile->functions.find(function.name); if (found != profile->functions.end()) dynamic = &found->second; }
    const auto hotness = opt::classifyHotness(dynamic);
    result.push_back({function.name, features.instructionCount, features.blockCount, features.loopCount,
      features.callerCount, features.calleeCount, features.dynamicCallCount, features.dynamicBlockExecutions,
      features.dynamicLoopBackedges, opt::hotnessName(hotness.hotness), opt::hotnessReasonName(hotness.reason)});
  }
  return result;
}

class CompilerService {
public:
  explicit CompilerService(ServiceLimits limits = {},
                           ExecutionPolicy executionPolicy = ExecutionPolicy::LocalTrusted)
      : limits_(limits), executionPolicy_(executionPolicy) {}
  const ServiceLimits &limits() const noexcept { return limits_; }
  ExecutionPolicy executionPolicy() const noexcept { return executionPolicy_; }

  CompileResponseV1 compile(const CompileRequestV1 &request) const {
    CompileResponseV1 response; const auto started = std::chrono::steady_clock::now();
    if (request.schemaVersion != kCompilerServiceSchemaVersion) return fail(response, ErrorKind::User, "API_SCHEMA_UNSUPPORTED", "unsupported compile request schema version", 0, 0, started);
    if (request.source.size() > limits_.maxSourceBytes) return fail(response, ErrorKind::ResourceLimit, "SOURCE_LIMIT_EXCEEDED", "source exceeds server byte limit", 0, 0, started);
    if (request.optimizationLevel < 0 || request.optimizationLevel > 3) return fail(response, ErrorKind::User, "INVALID_OPTIMIZATION_LEVEL", "optimization level must be O0 through O3", 0, 0, started);
    if (request.optimizationPolicy != "default" && request.optimizationPolicy != "heuristic" && request.optimizationPolicy != "learned")
      return fail(response, ErrorKind::User, "INVALID_OPTIMIZATION_POLICY", "optimization policy must be default, heuristic, or learned", 0, 0, started);
    if (request.passes && request.optimizationPolicy != "default")
      return fail(response, ErrorKind::User, "POLICY_WITH_CUSTOM_PASSES_UNSUPPORTED", "optimization policy cannot be combined with a custom pass list", 0, 0, started);
    try {
      const auto compileTimedOut = [&] {
        return limits_.maxCompileDurationMs != 0 &&
            std::chrono::steady_clock::now() - started >=
                std::chrono::milliseconds(limits_.maxCompileDurationMs);
      };
      const auto recordStage = [&](std::string id, std::string displayName,
                                   std::string category,
                                   std::chrono::steady_clock::time_point began,
                                   std::map<std::string, size_t> counters = {}) {
        PassEvent event;
        event.id = std::move(id);
        event.displayName = std::move(displayName);
        event.category = std::move(category);
        event.durationUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - began).count());
        event.counters = std::move(counters);
        response.passes.push_back(std::move(event));
      };
      const auto recordMeasuredStage = [&](std::string id, std::string displayName,
                                           std::string category, uint64_t durationUs,
                                           std::map<std::string, size_t> counters = {}) {
        PassEvent event;
        event.id = std::move(id);
        event.displayName = std::move(displayName);
        event.category = std::move(category);
        event.durationUs = durationUs;
        event.counters = std::move(counters);
        response.passes.push_back(std::move(event));
      };
      if (request.include.source) response.snapshots.push_back({"source", "Source", request.source});
      auto phaseStarted = std::chrono::steady_clock::now();
      Lexer lexer(request.source); auto tokens = lexer.scanTokens();
      if (tokens.size() > limits_.maxTokens) return fail(response, ErrorKind::ResourceLimit, "TOKEN_LIMIT_EXCEEDED", "source exceeds compiler token limit", 0, 0, started);
      recordStage("lex", "Lex", "frontend", phaseStarted, {{"tokens", tokens.size()}});
      phaseStarted = std::chrono::steady_clock::now();
      Parser parser(std::move(tokens)); auto program = parser.parseProgram();
      recordStage("parse", "Parse", "frontend", phaseStarted);
      const auto astCounts = NodeCountAnalysis{}.compute(program);
      const auto astDepth = ASTDepthAnalysis{}.compute(program);
      if (astDepth.maxDepth > limits_.maxAstDepth)
        return fail(response, ErrorKind::ResourceLimit, "AST_DEPTH_LIMIT_EXCEEDED", "source exceeds compiler AST depth limit", 0, 0, started);
      if (astCounts.functions > limits_.maxFunctions)
        return fail(response, ErrorKind::ResourceLimit, "FUNCTION_LIMIT_EXCEEDED", "source exceeds compiler function limit", 0, 0, started);
      if (compileTimedOut())
        return fail(response, ErrorKind::ResourceLimit, "COMPILE_TIMEOUT", "compile wall-clock limit exceeded", 0, 0, started);
      if (request.include.ast) response.snapshots.push_back({"ast", "AST", astText(program)});
      phaseStarted = std::chrono::steady_clock::now();
      ResolveScopesPass resolver; resolver.resolve(program);
      recordStage("resolve", "Resolve", "frontend", phaseStarted);
      if (compileTimedOut()) return fail(response, ErrorKind::ResourceLimit, "COMPILE_TIMEOUT", "compile wall-clock limit exceeded", 0, 0, started);
      phaseStarted = std::chrono::steady_clock::now();
      TypeCheckPass checker; checker.check(program);
      recordStage("type-check", "Type check", "frontend", phaseStarted);
      if (compileTimedOut()) return fail(response, ErrorKind::ResourceLimit, "COMPILE_TIMEOUT", "compile wall-clock limit exceeded", 0, 0, started);
      PipelineBuilder::initialise(); AnalysisManager analyses; PipelineBuilder::registerAnalysesWithManager(analyses); CompilerPassOptions options; std::ostringstream diagnostics; PassContext context(analyses, options, diagnostics); auto desugar = PipelineBuilder::buildDesugarPipeline();
      desugar.instrumentation().onPassEnd = [&](const PassTelemetry &event) { PassEvent pass; pass.id=event.passId; pass.displayName=event.passName; pass.category="desugar"; pass.changed=event.changed; pass.durationUs=static_cast<uint64_t>(event.durationUs()); pass.verified=true; for (const auto &[key,value] : event.stats.all()) if (const auto *integer=std::get_if<int64_t>(&value)) pass.counters[key]=static_cast<size_t>(*integer); response.passes.push_back(std::move(pass)); };
      desugar.run(program, context); if (request.include.desugaredAst) response.snapshots.push_back({"desugared-ast", "Desugared AST", astText(program)});
      phaseStarted = std::chrono::steady_clock::now();
      auto pipeline = IRPipeline::runAll(program);
      recordStage("ir-lowering", "Core → ANF → CFG → SSA", "lowering", phaseStarted,
                  {{"functions", pipeline.ssa.functions.size()}, {"blocks", programBlocks(pipeline.ssa)}, {"values", programValues(pipeline.ssa)}});
      recordMeasuredStage("ssa-construction", "CFG → SSA + verify", "lowering",
                          pipeline.ssaConstructionUs,
                          {{"functions", pipeline.ssa.functions.size()},
                           {"blocks", programBlocks(pipeline.ssa)},
                           {"values", programValues(pipeline.ssa)}});
      if (pipeline.ssa.functions.size() > limits_.maxFunctions)
        return fail(response, ErrorKind::ResourceLimit, "FUNCTION_LIMIT_EXCEEDED", "IR exceeds compiler function limit", 0, 0, started);
      if (programBlocks(pipeline.ssa) > limits_.maxIRBlocks)
        return fail(response, ErrorKind::ResourceLimit, "IR_BLOCK_LIMIT_EXCEEDED", "IR exceeds compiler block limit", 0, 0, started);
      if (programValues(pipeline.ssa) > limits_.maxSSAValues)
        return fail(response, ErrorKind::ResourceLimit, "SSA_VALUE_LIMIT_EXCEEDED", "IR exceeds compiler SSA value limit", 0, 0, started);
      if (compileTimedOut()) return fail(response, ErrorKind::ResourceLimit, "COMPILE_TIMEOUT", "compile wall-clock limit exceeded", 0, 0, started);
      if (request.include.core) response.snapshots.push_back({"core", "CoreIR", toText(pipeline.core)});
      if (request.include.anf) response.snapshots.push_back({"anf", "ANF", toText(pipeline.anf)});
      if (request.include.cfg) response.snapshots.push_back({"cfg", "CFG", toText(pipeline.cfg)});
      if (request.include.ssa) response.snapshots.push_back({"ssa", "SSA", toText(pipeline.ssa)});
      SSAProgram optimized = pipeline.ssa; const std::string before = toText(optimized); opt::SSAOptimizationContext optimizationContext;
      // Policy selection is intentionally separate from ordinary pass
      // selection.  The learned policy observes only candidates that the
      // inliner has already accepted as legal and within hard budgets.
      if (!request.passes && request.optimizationLevel >= 2 && request.optimizationPolicy != "default") {
        const auto inliningStarted = std::chrono::steady_clock::now();
        opt::SSAAnalysisManager ssaAnalyses;
        const auto instructionsBefore = programInstructions(optimized);
        const auto blocksBefore = programBlocks(optimized);
        const auto valuesBefore = programValues(optimized);
        std::optional<opt::LearnedInlinePolicy> learned;
        const opt::InlineProfitabilityPolicy *policy = nullptr;
        if (request.optimizationPolicy == "learned") {
          learned = opt::LearnedInlinePolicy::builtinInlineImitationV1();
          policy = &*learned;
        }
        const auto inlineResult = opt::SSAProgramInliner{}.run(optimized, {}, &ssaAnalyses, policy);
        for (const auto &function : optimized.functions) {
          const auto verified = verifySSA(function);
          if (!verified.ok) throw std::runtime_error("internal compiler error: SSA verification failed after policy inlining in @" + function.name);
        }
        PassEvent event;
        event.id = request.optimizationPolicy == "learned" ? "learned-inline-policy" : "heuristic-inline-policy";
        event.displayName = request.optimizationPolicy == "learned" ? "Learned Inline Policy" : "Heuristic Inline Policy";
        event.category = "optimization-policy";
        event.changed = inlineResult.changed;
        event.durationUs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - inliningStarted).count());
        event.instructionsBefore = instructionsBefore; event.instructionsAfter = programInstructions(optimized);
        event.blocksBefore = blocksBefore; event.blocksAfter = programBlocks(optimized);
        event.valuesBefore = valuesBefore; event.valuesAfter = programValues(optimized);
        event.counters = inlineResult.statistics.counters; event.verified = true;
        response.passes.push_back(std::move(event));
        response.policyRecords.insert(response.policyRecords.end(), inlineResult.policyRecords.begin(), inlineResult.policyRecords.end());
        for (const auto &remark : inlineResult.remarks) response.remarks.push_back({remarkKindName(remark.kind),remark.passId,remark.message,remark.block ? std::optional<int>(remark.block->index):std::nullopt,remark.value ? std::optional<int>(remark.value->index):std::nullopt,remark.origin.loc.line,remark.origin.loc.col});
      }
      if (request.optimizationLevel > 0 || request.passes) {
        for (auto &function : optimized.functions) {
          const auto instructionsBefore=opt::instructionCount(function), blocksBefore=function.blocks.size(), valuesBefore=opt::valueCount(function);
          const auto passStarted=std::chrono::steady_clock::now(); opt::SSAOptimizationResult result;
          if (request.passes) { opt::SSAOptimizationPipeline custom; for (const auto &id : *request.passes) { auto pass=opt::makePass(id); if (!pass) throw CompileError("unknown SSA pass '" + id + "'", 0, 0); custom.add(std::move(pass)); } result=custom.run(function, optimizationContext); }
          else { auto standard=opt::buildPipeline(request.optimizationLevel, limits_.maxOptimizationIterations); result=standard.run(function, optimizationContext); }
          const auto passEnded=std::chrono::steady_clock::now(); PassEvent event; event.id=(request.passes?"custom-ssa":"ssa-o"+std::to_string(request.optimizationLevel))+"@"+function.name; event.displayName=(request.passes?"Custom SSA Pipeline":"Nano SSA Optimization")+std::string(" @")+function.name; event.category="optimization"; event.changed=result.changed; event.durationUs=std::chrono::duration_cast<std::chrono::microseconds>(passEnded-passStarted).count(); event.instructionsBefore=instructionsBefore; event.instructionsAfter=opt::instructionCount(function); event.blocksBefore=blocksBefore; event.blocksAfter=function.blocks.size(); event.valuesBefore=valuesBefore; event.valuesAfter=opt::valueCount(function); event.counters=result.statistics.counters; event.verified=verifySSA(function).ok; response.passes.push_back(std::move(event));
          for (const auto &remark : result.remarks) response.remarks.push_back({remarkKindName(remark.kind),remark.passId,remark.message,remark.block ? std::optional<int>(remark.block->index):std::nullopt,remark.value ? std::optional<int>(remark.value->index):std::nullopt,remark.origin.loc.line,remark.origin.loc.col});
        }
      }
      if (compileTimedOut()) return fail(response, ErrorKind::ResourceLimit, "COMPILE_TIMEOUT", "compile wall-clock limit exceeded", 0, 0, started);
      const std::string after = toText(optimized); if (before != after && (request.include.telemetry || request.include.optimizedSsa)) response.transformations.push_back({request.passes ? "custom-ssa" : "ssa-o"+std::to_string(request.optimizationLevel), before, after});
      if (request.include.optimizedSsa) response.snapshots.push_back({"optimized-ssa", "Optimized SSA", after});
      if (request.include.graphs) { for (const auto &function : optimized.functions) { response.graphs.push_back(cfgGraph(function)); response.graphs.push_back(dominatorGraph(function)); response.graphs.push_back(loopGraph(function)); } response.graphs.push_back(callGraph(optimized)); }
      if (request.include.provenance) response.values=valueInformation(optimized);
      if (request.include.telemetry) response.policyFeatures=policyFeatureInformation(optimized);
#if defined(NANO_SERVICE_HAS_LLVM)
      if (request.include.llvm) {
        phaseStarted = std::chrono::steady_clock::now();
        llvm::LLVMContext context; llvm_backend::SSALLVMLowering lowering(context);
        auto lowered=lowering.lower(optimized, "nanopass_service");
        size_t llvmInstructions = 0;
        for (const auto &function : *lowered.module)
          for (const auto &block : function)
            llvmInstructions += block.size();
        recordStage("llvm-lowering", "SSA → LLVM", "backend", phaseStarted,
                    {{"llvmInstructions", llvmInstructions}});
        phaseStarted = std::chrono::steady_clock::now();
        std::string verifierText; llvm::raw_string_ostream verifierOut(verifierText);
        if (llvm::verifyModule(*lowered.module, &verifierOut)) {
          verifierOut.flush();
          throw std::runtime_error("internal compiler error: LLVM verification failed: " + verifierText);
        }
        recordStage("llvm-verify", "LLVM verify", "backend", phaseStarted);
        std::string text; llvm::raw_string_ostream out(text); lowered.module->print(out, nullptr); out.flush();
        response.snapshots.push_back({"llvm", "LLVM IR", std::move(text)});
      }
#endif
      if (compileTimedOut()) return fail(response, ErrorKind::ResourceLimit, "COMPILE_TIMEOUT", "compile wall-clock limit exceeded", 0, 0, started);
      response.functions=optimized.functions.size(); response.blocks=programBlocks(optimized); response.valuesCount=programValues(optimized); response.success=true;
    } catch (const CompileError &error) { return fail(response, ErrorKind::User, "E1000", error.message, error.line, error.col, started); }
    catch (const std::exception &error) {
      // A few legacy frontend branches still throw std::runtime_error rather
      // than CompileError. They are user-facing unless the pipeline itself
      // explicitly labels the failure as an internal compiler error.
      const std::string message = error.what();
      const bool internal = message.starts_with("internal compiler error");
      return fail(response, internal ? ErrorKind::Internal : ErrorKind::User,
                  internal ? "INTERNAL_COMPILER_ERROR" : "E1000", message,
                  0, 0, started);
    }
    response.totalCompileUs=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-started).count(); return response;
  }

  RunResponseV1 run(const RunRequestV1 &request) const {
    RunResponseV1 response; response.compilation=compile(request.compile); if (!response.compilation.success) { response.errorKind=response.compilation.diagnostics.empty()?ErrorKind::Internal:response.compilation.diagnostics.front().kind; response.status="compile_error"; return response; }
    if (!allows(request.mode)) {
      response.errorKind = ErrorKind::Runtime;
      response.status = "execution_policy_denied";
      response.stderrText = std::string("execution mode is disabled by server policy ") + executionPolicyName(executionPolicy_);
      return response;
    }
    const auto attachRuntimeProfile = [&](const profile::ProgramRuntimeProfile &profile) {
      std::ostringstream json;
      profile::writeRuntimeProfileJson(profile, json);
      const std::string serialized = json.str();
      if (serialized.size() > limits_.maxProfileBytes) {
        response.success = false;
        response.errorKind = ErrorKind::ResourceLimit;
        response.status = "profile_limit_exceeded";
        response.stderrText = "runtime profile exceeds server profile byte limit";
        response.runtimeProfileJson.clear();
        return false;
      }
      response.runtimeProfileJson = serialized;
      return true;
    };
    try {
      Lexer lexer(request.compile.source); Parser parser(lexer.scanTokens()); auto ast=parser.parseProgram(); ResolveScopesPass resolver; resolver.resolve(ast); TypeCheckPass{}.check(ast); PipelineBuilder::initialise(); AnalysisManager analyses; PipelineBuilder::registerAnalysesWithManager(analyses); CompilerPassOptions options; std::ostringstream diagnostics; PassContext context(analyses,options,diagnostics); auto desugar=PipelineBuilder::buildDesugarPipeline(); desugar.run(ast,context); auto pipeline=IRPipeline::runAll(ast); SSAProgram ssa=pipeline.ssa;
      if (!request.compile.passes && request.compile.optimizationLevel >= 2 && request.compile.optimizationPolicy != "default") {
        opt::SSAAnalysisManager ssaAnalyses;
        std::optional<opt::LearnedInlinePolicy> learned;
        const opt::InlineProfitabilityPolicy *policy = nullptr;
        if (request.compile.optimizationPolicy == "learned") { learned = opt::LearnedInlinePolicy::builtinInlineImitationV1(); policy = &*learned; }
        opt::SSAProgramInliner{}.run(ssa, {}, &ssaAnalyses, policy);
      }
      if (request.compile.optimizationLevel>0) for(auto &function:ssa.functions){auto optimizer=opt::buildPipeline(request.compile.optimizationLevel, limits_.maxOptimizationIterations); opt::SSAOptimizationContext ctx; optimizer.run(function,ctx);}
      ExecutionLimits limits; limits.maxSteps=limits_.maxExecutionSteps; limits.maxCallDepth=limits_.maxCallDepth; limits.maxOutputBytes=limits_.maxOutputBytes; limits.maxArrayElements=limits_.maxArrayElements; limits.maxWallTimeMs=limits_.maxExecutionDurationMs;
      ExecutionResult execution; profile::ProgramRuntimeProfile profile;
      const auto executionStarted = std::chrono::steady_clock::now();
      if (request.mode == ExecutionMode::CoreInterpreter) execution=CoreInterpreter{}.run(pipeline.core,"main",limits);
      else if (request.mode == ExecutionMode::SSAInterpreter) execution=SSAInterpreter{}.run(ssa,"main",limits,&profile);
#if defined(NANO_SERVICE_HAS_JIT)
      else if (request.mode == ExecutionMode::JIT) {
        const auto jit=llvm_backend::executeWithLLJIT(ssa);
        response.stdoutText=jit.stdoutText; response.returnValue=std::to_string(jit.returnValue);
        response.success=jit.error.empty(); response.errorKind=jit.error.empty()?ErrorKind::None:ErrorKind::Runtime;
        response.status=jit.error.empty()?"success":"runtime_error"; response.stderrText=jit.error;
        response.executionDurationUs=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-executionStarted).count();
        return response;
      } else if (request.mode == ExecutionMode::AdaptiveJIT) {
        llvm_backend::AdaptiveJITConfig config; config.hotFunctionCallThreshold=2;
        const auto adaptive=llvm_backend::executeAdaptivelyWithLLJIT(pipeline.ssa,config);
        response.stdoutText=adaptive.execution.stdoutText; response.returnValue=std::to_string(adaptive.execution.returnValue);
        response.success=adaptive.execution.error.empty(); response.errorKind=response.success?ErrorKind::None:ErrorKind::Runtime;
        response.status=response.success?"success":"runtime_error"; response.stderrText=adaptive.execution.error;
        response.executionDurationUs=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-executionStarted).count();
        profile=adaptive.runtimeProfile; if (!attachRuntimeProfile(profile)) return response;
        for(const auto &event:adaptive.events) response.adaptiveEvents.push_back({adaptiveEventKindName(event.kind), event.function, event.calls, event.detail});
        response.policyFeatures=policyFeatureInformation(pipeline.ssa, &profile);
        return response;
      }
#else
      else { response.errorKind=ErrorKind::Runtime; response.status="capability_unavailable"; response.stderrText="JIT execution is unavailable in this service build; configure nano-server with NANO_BUILD_JIT=ON."; return response; }
#endif
      response.stdoutText=execution.stdoutText; response.stderrText=execution.stderrText; response.stepsExecuted=execution.stepsExecuted; response.executionDurationUs=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-executionStarted).count(); response.status=execution.status==ExecutionStatus::Success?"success":"runtime_error"; response.success=execution.status==ExecutionStatus::Success; response.errorKind=execution.status==ExecutionStatus::Success?ErrorKind::None:(execution.status==ExecutionStatus::StepLimitExceeded||execution.status==ExecutionStatus::TimeLimitExceeded||execution.status==ExecutionStatus::OutputLimitExceeded||execution.status==ExecutionStatus::RecursionLimitExceeded?ErrorKind::ResourceLimit:ErrorKind::Runtime); if(execution.returnValue)response.returnValue=runtimeValueText(*execution.returnValue); if(execution.error)response.stderrText=execution.error->message; if(request.mode==ExecutionMode::SSAInterpreter){profile.executionMode="ssa-interpreter";if (!attachRuntimeProfile(profile)) return response;response.policyFeatures=policyFeatureInformation(ssa,&profile);}
    } catch(const std::exception &error) { response.errorKind=ErrorKind::Internal; response.status="internal_error"; response.stderrText=error.what(); }
    return response;
  }

  CorrectnessResponseV1 checkCorrectness(const CompileRequestV1 &request) const {
    CorrectnessResponseV1 response;
    response.compilation = compile(request);
    if (!response.compilation.success) return response;
    ExecutionLimits limits;
    limits.maxSteps = limits_.maxExecutionSteps;
    limits.maxCallDepth = limits_.maxCallDepth;
    limits.maxOutputBytes = limits_.maxOutputBytes;
    limits.maxArrayElements = limits_.maxArrayElements;
    limits.maxWallTimeMs = limits_.maxExecutionDurationMs;
    try {
      const auto differential = DifferentialRunner{}.run(request.source, {0, 1, 2, 3}, limits);
      if (!differential.compiled) {
        response.checks.push_back({"compiler", false, differential.diagnostic});
        return response;
      }
      const bool corePassed = differential.reference.status == ExecutionStatus::Success;
      response.checks.push_back({"CoreIR", corePassed, corePassed ? "reference execution completed" : "reference execution failed"});
      bool equivalent = corePassed;
      for (const auto &level : differential.levels) {
        response.checks.push_back({"SSA O" + std::to_string(level.level), level.equivalent,
          level.equivalent ? "observable output matches CoreIR" : "observable output differs from CoreIR"});
        equivalent = equivalent && level.equivalent;
      }
#if defined(NANO_SERVICE_HAS_JIT)
      if (allows(ExecutionMode::JIT)) {
        auto pipeline = buildIRForDifferential(request.source);
        SSAProgram native = pipeline.ssa;
        for (auto &function : native.functions) {
          opt::SSAOptimizationContext context;
          opt::buildPipeline(3, limits_.maxOptimizationIterations).run(function, context);
        }
        const std::string expectedOutput = differential.reference.stdoutText;
        const std::string expectedReturn = differential.reference.returnValue
            ? runtimeValueText(*differential.reference.returnValue) : "";
        const auto jit = llvm_backend::executeWithLLJIT(native);
        const bool jitPassed = jit.error.empty() && jit.stdoutText == expectedOutput &&
            std::to_string(jit.returnValue) == expectedReturn;
        response.checks.push_back({"JIT", jitPassed,
            jitPassed ? "native output matches CoreIR" : (jit.error.empty() ? "native output differs from CoreIR" : jit.error)});
        equivalent = equivalent && jitPassed;
        llvm_backend::AdaptiveJITConfig config; config.hotFunctionCallThreshold = 2;
        const auto adaptive = llvm_backend::executeAdaptivelyWithLLJIT(pipeline.ssa, config);
        const bool adaptivePassed = adaptive.execution.error.empty() &&
            adaptive.execution.stdoutText == expectedOutput &&
            std::to_string(adaptive.execution.returnValue) == expectedReturn;
        response.checks.push_back({"Adaptive JIT", adaptivePassed,
            adaptivePassed ? "adaptive native output matches CoreIR" : (adaptive.execution.error.empty() ? "adaptive native output differs from CoreIR" : adaptive.execution.error)});
        equivalent = equivalent && adaptivePassed;
      } else {
        response.checks.push_back({"JIT", true, "not executed under current server execution policy"});
        response.checks.push_back({"Adaptive JIT", true, "not executed under current server execution policy"});
      }
#else
      response.checks.push_back({"JIT", false, "capability unavailable in this service build"});
      response.checks.push_back({"Adaptive JIT", false, "capability unavailable in this service build"});
#endif
      response.success = true;
      response.equivalent = equivalent;
    } catch (const std::exception &error) {
      response.checks.push_back({"internal", false, error.what()});
    }
    return response;
  }
private:
  ServiceLimits limits_;
  ExecutionPolicy executionPolicy_;

  bool allows(ExecutionMode mode) const noexcept {
    if (executionPolicy_ == ExecutionPolicy::Disabled) return false;
    return executionPolicy_ == ExecutionPolicy::LocalTrusted ||
        mode == ExecutionMode::CoreInterpreter || mode == ExecutionMode::SSAInterpreter;
  }
  static CompileResponseV1 fail(CompileResponseV1 response, ErrorKind kind, std::string code, std::string message, int line, int column, std::chrono::steady_clock::time_point started) { response.success=false; response.diagnostics.push_back({kind==ErrorKind::Internal?"fatal":"error",std::move(code),std::move(message),line,column,kind}); response.totalCompileUs=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-started).count(); return response; }
};

inline void writePolicyFeaturesJson(std::ostream &out, const std::vector<PolicyFeatureInfo> &features) {
  out << '[';
  for (size_t index = 0; index < features.size(); ++index) {
    if (index) out << ',';
    const auto &feature = features[index];
    out << "{\"function\":\"" << jsonEscape(feature.function)
        << "\",\"instructions\":" << feature.instructions
        << ",\"blocks\":" << feature.blocks << ",\"loops\":" << feature.loops
        << ",\"callers\":" << feature.callers << ",\"callees\":" << feature.callees
        << ",\"dynamicCalls\":" << feature.dynamicCalls
        << ",\"dynamicBlocks\":" << feature.dynamicBlocks
        << ",\"dynamicLoopBackedges\":" << feature.dynamicLoopBackedges
        << ",\"hotness\":\"" << jsonEscape(feature.hotness)
        << "\",\"hotnessReason\":\"" << jsonEscape(feature.hotnessReason) << "\"}";
  }
  out << ']';
}

inline std::string compileResponseJson(const CompileResponseV1 &response) {
  std::ostringstream out; out << "{\"schemaVersion\":" << response.schemaVersion << ",\"success\":" << (response.success?"true":"false") << ",\"compilerRevision\":\"" << jsonEscape(response.compilerRevision) << "\",\"diagnostics\":[";
  for(size_t i=0;i<response.diagnostics.size();++i){if(i)out<<',';const auto &d=response.diagnostics[i];out<<"{\"severity\":\""<<jsonEscape(d.severity)<<"\",\"code\":\""<<jsonEscape(d.code)<<"\",\"message\":\""<<jsonEscape(d.message)<<"\",\"line\":"<<d.line<<",\"column\":"<<d.column<<",\"kind\":\""<<errorKindName(d.kind)<<"\"}";} out<<"],\"snapshots\":[";
  for(size_t i=0;i<response.snapshots.size();++i){if(i)out<<',';const auto&s=response.snapshots[i];out<<"{\"stageId\":\""<<jsonEscape(s.stageId)<<"\",\"displayName\":\""<<jsonEscape(s.displayName)<<"\",\"text\":\""<<jsonEscape(s.text)<<"\"}";} out<<"],\"transformations\":[";
  for(size_t i=0;i<response.transformations.size();++i){if(i)out<<',';const auto&t=response.transformations[i];out<<"{\"passId\":\""<<jsonEscape(t.passId)<<"\",\"before\":\""<<jsonEscape(t.before)<<"\",\"after\":\""<<jsonEscape(t.after)<<"\"}";} out<<"],\"passes\":[";
  for(size_t i=0;i<response.passes.size();++i){if(i)out<<',';const auto&p=response.passes[i];out<<"{\"id\":\""<<jsonEscape(p.id)<<"\",\"displayName\":\""<<jsonEscape(p.displayName)<<"\",\"category\":\""<<p.category<<"\",\"changed\":"<<(p.changed?"true":"false")<<",\"durationUs\":"<<p.durationUs<<",\"instructionsBefore\":"<<p.instructionsBefore<<",\"instructionsAfter\":"<<p.instructionsAfter<<",\"blocksBefore\":"<<p.blocksBefore<<",\"blocksAfter\":"<<p.blocksAfter<<",\"valuesBefore\":"<<p.valuesBefore<<",\"valuesAfter\":"<<p.valuesAfter<<",\"verified\":"<<(p.verified?"true":"false")<<",\"counters\":{";size_t n=0;for(const auto&[key,value]:p.counters){if(n++)out<<',';out<<"\""<<jsonEscape(key)<<"\":"<<value;}out<<"}}";} out<<"],\"remarks\":[";
  for(size_t i=0;i<response.remarks.size();++i){if(i)out<<',';const auto&r=response.remarks[i];out<<"{\"kind\":\""<<r.kind<<"\",\"passId\":\""<<jsonEscape(r.passId)<<"\",\"message\":\""<<jsonEscape(r.message)<<"\",\"block\":"<<(r.block?std::to_string(*r.block):"null")<<",\"value\":"<<(r.value?std::to_string(*r.value):"null")<<",\"line\":"<<r.line<<",\"column\":"<<r.column<<"}";} out<<"],\"graphs\":[";
  for(size_t i=0;i<response.graphs.size();++i){if(i)out<<',';const auto&g=response.graphs[i];out<<"{\"kind\":\""<<g.kind<<"\",\"function\":\""<<jsonEscape(g.function)<<"\",\"nodes\":[";for(size_t j=0;j<g.nodes.size();++j){if(j)out<<',';const auto&node=g.nodes[j];out<<"{\"id\":\""<<jsonEscape(node.id)<<"\",\"label\":\""<<jsonEscape(node.label)<<"\",\"metadata\":{";size_t n=0;for(const auto&[key,value]:node.metadata){if(n++)out<<',';out<<"\""<<jsonEscape(key)<<"\":\""<<jsonEscape(value)<<"\"";}out<<"}}";}out<<"],\"edges\":[";for(size_t j=0;j<g.edges.size();++j){if(j)out<<',';const auto&edge=g.edges[j];out<<"{\"from\":\""<<jsonEscape(edge.from)<<"\",\"to\":\""<<jsonEscape(edge.to)<<"\",\"kind\":\""<<edge.kind<<"\",\"metadata\":{";size_t n=0;for(const auto&[key,value]:edge.metadata){if(n++)out<<',';out<<"\""<<jsonEscape(key)<<"\":\""<<jsonEscape(value)<<"\"";}out<<"}}";}out<<"]}";} out<<"],\"values\":[";
  for(size_t i=0;i<response.values.size();++i){if(i)out<<',';const auto&v=response.values[i];out<<"{\"value\":"<<v.value<<",\"function\":\""<<jsonEscape(v.function)<<"\",\"kind\":\""<<v.kind<<"\",\"type\":\""<<v.type<<"\",\"definingBlock\":"<<v.definingBlock<<",\"line\":"<<v.line<<",\"column\":"<<v.column<<",\"uses\":[";for(size_t j=0;j<v.uses.size();++j){if(j)out<<',';const auto&use=v.uses[j];out<<"{\"kind\":\""<<use.kind<<"\",\"block\":"<<use.block<<",\"detail\":\""<<jsonEscape(use.detail)<<"\"}";}out<<"]}";} out<<"],\"policyFeatures\":";writePolicyFeaturesJson(out,response.policyFeatures);out<<",\"policyRecords\":[";for(size_t i=0;i<response.policyRecords.size();++i){if(i)out<<',';out<<response.policyRecords[i];}out<<"],\"telemetry\":{\"totalCompileUs\":"<<response.totalCompileUs<<",\"functions\":"<<response.functions<<",\"blocks\":"<<response.blocks<<",\"values\":"<<response.valuesCount<<"}}"; return out.str();
}
inline std::string runResponseJson(const RunResponseV1 &response) { std::ostringstream out; out<<"{\"schemaVersion\":"<<response.schemaVersion<<",\"success\":"<<(response.success?"true":"false")<<",\"errorKind\":\""<<errorKindName(response.errorKind)<<"\",\"status\":\""<<jsonEscape(response.status)<<"\",\"stdout\":\""<<jsonEscape(response.stdoutText)<<"\",\"stderr\":\""<<jsonEscape(response.stderrText)<<"\",\"returnValue\":\""<<jsonEscape(response.returnValue)<<"\",\"stepsExecuted\":"<<response.stepsExecuted<<",\"executionDurationUs\":"<<response.executionDurationUs<<",\"runtimeProfile\":"<< (response.runtimeProfileJson.empty()?"null":response.runtimeProfileJson) <<",\"adaptiveEvents\":[";for(size_t i=0;i<response.adaptiveEvents.size();++i){if(i)out<<',';const auto&event=response.adaptiveEvents[i];out<<"{\"kind\":\""<<jsonEscape(event.kind)<<"\",\"function\":\""<<jsonEscape(event.function)<<"\",\"calls\":"<<event.calls<<",\"detail\":\""<<jsonEscape(event.detail)<<"\"}";}out<<"],\"policyFeatures\":";writePolicyFeaturesJson(out,response.policyFeatures);out<<",\"compilation\":"<<compileResponseJson(response.compilation)<<"}";return out.str(); }
inline std::string correctnessResponseJson(const CorrectnessResponseV1 &response) { std::ostringstream out; out << "{\"schemaVersion\":" << response.schemaVersion << ",\"success\":" << (response.success ? "true" : "false") << ",\"equivalent\":" << (response.equivalent ? "true" : "false") << ",\"checks\":["; for (size_t index = 0; index < response.checks.size(); ++index) { if (index) out << ','; const auto &check = response.checks[index]; out << "{\"stage\":\"" << jsonEscape(check.stage) << "\",\"passed\":" << (check.passed ? "true" : "false") << ",\"detail\":\"" << jsonEscape(check.detail) << "\"}"; } out << "],\"compilation\":" << compileResponseJson(response.compilation) << '}'; return out.str(); }

} // namespace nanopass::service
