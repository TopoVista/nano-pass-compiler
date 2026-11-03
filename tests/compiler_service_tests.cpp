#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

#include "service/compiler_service.h"
#include "support/project_version.h"

namespace {
int assertions = 0;
#define ASSERT(condition) do { ++assertions; if (!(condition)) { std::cerr << "assertion failed: " #condition " at " << __FILE__ << ':' << __LINE__ << '\n'; return 1; } } while (false)

nanopass::service::CompileRequestV1 request(std::string source) {
  nanopass::service::CompileRequestV1 result;
  result.source = std::move(source); result.optimizationLevel = 2;
  result.include.ast = result.include.desugaredAst = result.include.core = true;
  result.include.anf = result.include.cfg = result.include.ssa = true;
  result.include.optimizedSsa = result.include.graphs = result.include.remarks = true;
  result.include.telemetry = result.include.provenance = true;
  result.include.source = true;
  return result;
}
}

int main() {
  using namespace nanopass::service;
  CompilerService service;
  const auto valid = request(R"(int choose(int x) { if (x > 0) { return 7; } return 3; } int main() { int y = choose(1); print(y); return y; })");
  const auto compiled = service.compile(valid);
  ASSERT(compiled.success); ASSERT(compiled.schemaVersion == kCompilerServiceSchemaVersion);
  ASSERT(compiled.compilerRevision == nanopass::build::kCompilerRevision);
  ASSERT(!compiled.snapshots.empty()); ASSERT(!compiled.passes.empty()); ASSERT(!compiled.graphs.empty()); ASSERT(!compiled.values.empty());
  ASSERT(std::any_of(compiled.passes.begin(), compiled.passes.end(), [](const auto &event) { return event.id == "lex" && event.counters.contains("tokens"); }));
  ASSERT(std::any_of(compiled.passes.begin(), compiled.passes.end(), [](const auto &event) { return event.id == "type-check" && event.category == "frontend"; }));
  ASSERT(std::any_of(compiled.passes.begin(), compiled.passes.end(), [](const auto &event) { return event.id == "ir-lowering" && event.category == "lowering"; }));
  ASSERT(std::any_of(compiled.passes.begin(), compiled.passes.end(), [](const auto &event) { return event.id == "ssa-construction" && event.category == "lowering"; }));
  ASSERT(compiled.snapshots.front().stageId == "source");
  ASSERT(compiled.totalCompileUs >= 0); ASSERT(compiled.functions == 2); ASSERT(compiled.blocks > 0); ASSERT(compiled.valuesCount > 0);
  bool cfg=false,dom=false,loops=false,calls=false; for(const auto &graph:compiled.graphs){cfg|=graph.kind=="cfg";dom|=graph.kind=="dominator-tree";loops|=graph.kind=="loops";calls|=graph.kind=="call-graph";} ASSERT(cfg); ASSERT(dom); ASSERT(loops); ASSERT(calls);
  const auto firstJson = compileResponseJson(compiled); const auto secondJson = compileResponseJson(compiled); ASSERT(firstJson == secondJson); ASSERT(firstJson.find("\"schemaVersion\":1") != std::string::npos); ASSERT(firstJson.find("\"transformations\"") != std::string::npos); ASSERT(firstJson.find("\"uses\"") != std::string::npos); ASSERT(firstJson.find("C:\\\\") == std::string::npos);
  auto bad = valid; bad.source = "int main(){ return 1 }"; const auto parseError=service.compile(bad); ASSERT(!parseError.success); ASSERT(parseError.diagnostics.size()==1); ASSERT(parseError.diagnostics.front().kind==ErrorKind::User); ASSERT(parseError.diagnostics.front().line>0);
  auto typeError = valid; typeError.source = "int main(){ bool x = 7; return 0; }"; const auto typed=service.compile(typeError); ASSERT(!typed.success); ASSERT(typed.diagnostics.front().code=="E1000");
  auto schemaError=valid; schemaError.schemaVersion=99; const auto unsupported=service.compile(schemaError); ASSERT(!unsupported.success); ASSERT(unsupported.diagnostics.front().code=="API_SCHEMA_UNSUPPORTED");
  ServiceLimits tiny; tiny.maxSourceBytes=16; CompilerService limited(tiny); const auto tooLarge=limited.compile(valid); ASSERT(!tooLarge.success); ASSERT(tooLarge.diagnostics.front().kind==ErrorKind::ResourceLimit);
  ServiceLimits tokenTiny; tokenTiny.maxTokens=4; CompilerService tokenLimited(tokenTiny); const auto tooManyTokens=tokenLimited.compile(valid); ASSERT(!tooManyTokens.success); ASSERT(tooManyTokens.diagnostics.front().code=="TOKEN_LIMIT_EXCEEDED");
  ServiceLimits shallow; shallow.maxAstDepth=2; CompilerService depthLimited(shallow); const auto tooDeep=depthLimited.compile(valid); ASSERT(!tooDeep.success); ASSERT(tooDeep.diagnostics.front().code=="AST_DEPTH_LIMIT_EXCEEDED");
  ServiceLimits functionTiny; functionTiny.maxFunctions=1; CompilerService functionLimited(functionTiny); const auto tooManyFunctions=functionLimited.compile(valid); ASSERT(!tooManyFunctions.success); ASSERT(tooManyFunctions.diagnostics.front().code=="FUNCTION_LIMIT_EXCEEDED");
  ServiceLimits blockTiny; blockTiny.maxIRBlocks=1; CompilerService blockLimited(blockTiny); const auto tooManyBlocks=blockLimited.compile(valid); ASSERT(!tooManyBlocks.success); ASSERT(tooManyBlocks.diagnostics.front().code=="IR_BLOCK_LIMIT_EXCEEDED");
  ServiceLimits valuesTiny; valuesTiny.maxSSAValues=1; CompilerService valuesLimited(valuesTiny); const auto tooManyValues=valuesLimited.compile(valid); ASSERT(!tooManyValues.success); ASSERT(valuesLimited.limits().maxSSAValues==1); ASSERT(tooManyValues.diagnostics.front().code=="SSA_VALUE_LIMIT_EXCEEDED");
  RunRequestV1 run; run.compile=request("int main(){ print(7); return 7; }"); run.compile.optimizationLevel=0; run.mode=ExecutionMode::SSAInterpreter; const auto ran=service.run(run); ASSERT(ran.success); ASSERT(ran.stdoutText=="7\n"); ASSERT(ran.returnValue=="7"); ASSERT(!ran.runtimeProfileJson.empty()); ASSERT(runResponseJson(ran).find("runtimeProfile")!=std::string::npos); ASSERT(runResponseJson(ran).find("executionDurationUs")!=std::string::npos);
  ServiceLimits profileTiny; profileTiny.maxProfileBytes=1; CompilerService profileLimited(profileTiny); const auto oversizedProfile=profileLimited.run(run); ASSERT(!oversizedProfile.success); ASSERT(oversizedProfile.errorKind==ErrorKind::ResourceLimit); ASSERT(oversizedProfile.status=="profile_limit_exceeded");
  RunRequestV1 called; called.compile=request(R"(int twice(int x) { return x + x; } int main() { print(twice(4)); return twice(4); })"); called.compile.optimizationLevel=0; called.mode=ExecutionMode::SSAInterpreter; const auto calledRun=service.run(called); ASSERT(calledRun.success); ASSERT(calledRun.stdoutText=="8\n"); ASSERT(calledRun.returnValue=="8");
  run.mode=ExecutionMode::JIT; const auto unavailable=service.run(run); ASSERT(!unavailable.success); ASSERT(unavailable.status=="capability_unavailable");
  CompilerService interpreterOnly({}, ExecutionPolicy::InterpreterOnly); const auto nativeDenied=interpreterOnly.run(run); ASSERT(!nativeDenied.success); ASSERT(nativeDenied.status=="execution_policy_denied");
  run.mode=ExecutionMode::SSAInterpreter; const auto interpreterAllowed=interpreterOnly.run(run); ASSERT(interpreterAllowed.success);
  auto heuristicPolicy=valid; heuristicPolicy.optimizationPolicy="heuristic"; const auto heuristicCompiled=service.compile(heuristicPolicy); ASSERT(heuristicCompiled.success); ASSERT(!heuristicCompiled.policyRecords.empty()); ASSERT(std::any_of(heuristicCompiled.passes.begin(),heuristicCompiled.passes.end(),[](const auto &event){return event.id=="heuristic-inline-policy"&&event.verified;}));
  auto learnedPolicy=valid; learnedPolicy.optimizationPolicy="learned"; const auto learnedCompiled=service.compile(learnedPolicy); ASSERT(learnedCompiled.success); ASSERT(!learnedCompiled.policyRecords.empty()); ASSERT(std::any_of(learnedCompiled.remarks.begin(),learnedCompiled.remarks.end(),[](const auto &remark){return remark.passId=="learned-inline-policy";})); ASSERT(compileResponseJson(learnedCompiled).find("policyRecords")!=std::string::npos);
  auto invalidPolicy=valid; invalidPolicy.optimizationPolicy="anything-goes"; const auto invalidPolicyCompiled=service.compile(invalidPolicy); ASSERT(!invalidPolicyCompiled.success); ASSERT(invalidPolicyCompiled.diagnostics.front().code=="INVALID_OPTIMIZATION_POLICY");
  auto custom=valid; custom.passes=std::vector<std::string>{"sccp","dce"}; const auto customResult=service.compile(custom); ASSERT(customResult.success); ASSERT(!customResult.passes.empty());
  auto unknown=valid; unknown.passes=std::vector<std::string>{"malicious-pass"}; const auto unknownResult=service.compile(unknown); ASSERT(!unknownResult.success); ASSERT(unknownResult.diagnostics.front().kind==ErrorKind::User || unknownResult.diagnostics.front().kind==ErrorKind::Internal);
  const auto correctness = service.checkCorrectness(request("int main(){ print(2); return 2; }")); ASSERT(correctness.success); ASSERT(correctness.equivalent); ASSERT(correctness.checks.size() == 7); ASSERT(correctness.checks.front().stage == "CoreIR");
  std::cout << "compiler service: " << assertions << "/" << assertions << " assertions passed\n";
}
