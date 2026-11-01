#pragma once

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "ir/ssa/call_graph.h"
#include "ir/ssa/loop_analysis.h"
#include "profile/runtime_profile.h"

namespace nanopass::opt {

enum class Hotness { Cold, Warm, Hot };
enum class HotnessReason {
  ColdFunction,
  HotFunctionCallCount,
  HotFunctionDynamicExecution,
  HotLoopBackedgeCount,
  HotLoopTripCount,
  InsufficientProfileData,
};

inline const char *hotnessName(Hotness value) {
  switch (value) { case Hotness::Cold: return "cold"; case Hotness::Warm: return "warm"; case Hotness::Hot: return "hot"; }
  return "cold";
}
inline const char *hotnessReasonName(HotnessReason value) {
  switch (value) {
  case HotnessReason::ColdFunction: return "COLD_FUNCTION";
  case HotnessReason::HotFunctionCallCount: return "HOT_FUNCTION_CALL_COUNT";
  case HotnessReason::HotFunctionDynamicExecution: return "HOT_FUNCTION_DYNAMIC_EXECUTION";
  case HotnessReason::HotLoopBackedgeCount: return "HOT_LOOP_BACKEDGE_COUNT";
  case HotnessReason::HotLoopTripCount: return "HOT_LOOP_TRIP_COUNT";
  case HotnessReason::InsufficientProfileData: return "INSUFFICIENT_PROFILE_DATA";
  }
  return "INSUFFICIENT_PROFILE_DATA";
}

struct HotnessThresholds {
  uint64_t warmFunctionCalls = 10;
  uint64_t hotFunctionCalls = 1000;
  uint64_t warmDynamicBlocks = 100;
  uint64_t hotDynamicBlocks = 10000;
  uint64_t hotLoopBackedges = 1000;
  double hotLoopTripCount = 16.0;
};

struct HotnessDecision {
  Hotness hotness = Hotness::Cold;
  HotnessReason reason = HotnessReason::InsufficientProfileData;
  uint64_t observed = 0;
};

inline HotnessDecision classifyHotness(
    const profile::FunctionRuntimeProfile *profile,
    const HotnessThresholds &thresholds = {}) {
  if (!profile) return {Hotness::Cold, HotnessReason::InsufficientProfileData, 0};
  if (profile->calls >= thresholds.hotFunctionCalls)
    return {Hotness::Hot, HotnessReason::HotFunctionCallCount, profile->calls};
  if (profile->blocksExecuted >= thresholds.hotDynamicBlocks)
    return {Hotness::Hot, HotnessReason::HotFunctionDynamicExecution, profile->blocksExecuted};
  for (const auto &[header, loop] : profile->loops) {
    (void)header;
    if (loop.backedges >= thresholds.hotLoopBackedges)
      return {Hotness::Hot, HotnessReason::HotLoopBackedgeCount, loop.backedges};
    if (loop.averageTripCount() >= thresholds.hotLoopTripCount)
      return {Hotness::Hot, HotnessReason::HotLoopTripCount,
              static_cast<uint64_t>(loop.averageTripCount())};
  }
  if (profile->calls >= thresholds.warmFunctionCalls ||
      profile->blocksExecuted >= thresholds.warmDynamicBlocks)
    return {Hotness::Warm, HotnessReason::ColdFunction,
            std::max(profile->calls, profile->blocksExecuted)};
  return {Hotness::Cold, HotnessReason::ColdFunction,
          std::max(profile->calls, profile->blocksExecuted)};
}

// This ordered schema is intentionally plain-data. It contains no addresses,
// pointer identities, or local ValueIds and can therefore be safely exported
// for future learned-policy experiments.
struct OptimizationFeaturesV1 {
  static constexpr uint64_t kSchemaVersion = 1;
  uint64_t instructionCount = 0;
  uint64_t blockCount = 0;
  uint64_t cfgEdgeCount = 0;
  uint64_t phiCount = 0;
  uint64_t callCount = 0;
  uint64_t branchCount = 0;
  uint64_t memoryOperationCount = 0;
  uint64_t loopCount = 0;
  uint64_t maxLoopDepth = 0;
  uint64_t callerCount = 0;
  uint64_t calleeCount = 0;
  uint64_t dynamicCallCount = 0;
  uint64_t dynamicBlockExecutions = 0;
  uint64_t hotBlockCount = 0;
  uint64_t dynamicLoopBackedges = 0;
  double hottestLoopTripCount = 0.0;
};

inline OptimizationFeaturesV1 extractOptimizationFeatures(
    const SSAProgram &program, const SSAFunction &function,
    const profile::ProgramRuntimeProfile *runtimeProfile = nullptr) {
  OptimizationFeaturesV1 features;
  const auto calls = CallGraphAnalysis::run(program);
  const auto loops = LoopInfoAnalysis::run(function);
  features.blockCount = function.blocks.size();
  features.loopCount = loops.loops.size();
  for (const auto &loop : loops.loops) features.maxLoopDepth = std::max<uint64_t>(features.maxLoopDepth, loop.depth);
  features.calleeCount = calls.callees(function.name).size();
  for (const auto &candidate : program.functions) {
    const auto candidateCallees = calls.callees(candidate.name);
    if (std::find(candidateCallees.begin(), candidateCallees.end(), function.name) != candidateCallees.end())
      ++features.callerCount;
  }
  for (const auto &block : function.blocks) {
    features.cfgEdgeCount += block.successors.size();
    features.phiCount += block.phis.size();
    if (std::holds_alternative<SSATerminator::Branch>(block.terminator.value)) ++features.branchCount;
    for (const auto &instruction : block.instructions) {
      ++features.instructionCount;
      if (instruction.kind == SSAInstructionKind::Call) ++features.callCount;
      if (instruction.kind == SSAInstructionKind::LoadLocal || instruction.kind == SSAInstructionKind::StoreLocal ||
          instruction.kind == SSAInstructionKind::LoadIndex || instruction.kind == SSAInstructionKind::StoreIndex)
        ++features.memoryOperationCount;
    }
  }
  if (!runtimeProfile) return features;
  const auto dynamic = runtimeProfile->functions.find(function.name);
  if (dynamic == runtimeProfile->functions.end()) return features;
  const auto &record = dynamic->second;
  features.dynamicCallCount = record.calls;
  features.dynamicBlockExecutions = record.blocksExecuted;
  const uint64_t hotThreshold = record.blocksExecuted == 0 ? UINT64_MAX : std::max<uint64_t>(1, record.blocksExecuted / 10);
  for (const auto &[block, count] : record.blockCounts) { (void)block; if (count >= hotThreshold) ++features.hotBlockCount; }
  for (const auto &[header, loop] : record.loops) {
    (void)header;
    features.dynamicLoopBackedges += loop.backedges;
    features.hottestLoopTripCount = std::max(features.hottestLoopTripCount, loop.averageTripCount());
  }
  return features;
}

inline std::string optimizationFeaturesJson(const OptimizationFeaturesV1 &f) {
  std::ostringstream out;
  out << "{\"schema_version\":" << OptimizationFeaturesV1::kSchemaVersion
      << ",\"instruction_count\":" << f.instructionCount
      << ",\"block_count\":" << f.blockCount
      << ",\"cfg_edge_count\":" << f.cfgEdgeCount
      << ",\"phi_count\":" << f.phiCount
      << ",\"call_count\":" << f.callCount
      << ",\"branch_count\":" << f.branchCount
      << ",\"memory_operation_count\":" << f.memoryOperationCount
      << ",\"loop_count\":" << f.loopCount
      << ",\"max_loop_depth\":" << f.maxLoopDepth
      << ",\"caller_count\":" << f.callerCount
      << ",\"callee_count\":" << f.calleeCount
      << ",\"dynamic_call_count\":" << f.dynamicCallCount
      << ",\"dynamic_block_executions\":" << f.dynamicBlockExecutions
      << ",\"hot_block_count\":" << f.hotBlockCount
      << ",\"dynamic_loop_backedges\":" << f.dynamicLoopBackedges
      << ",\"hottest_loop_trip_count\":" << std::fixed << std::setprecision(6)
      << f.hottestLoopTripCount << "}";
  return out.str();
}

struct OptimizationPolicyDecision {
  uint64_t schemaVersion = 1;
  std::string decisionType;
  std::string subject;
  OptimizationFeaturesV1 features;
  std::string action;
  HotnessReason reason = HotnessReason::InsufficientProfileData;
  double estimatedCost = 0.0;
};

inline std::string policyJsonEscape(std::string_view text) {
  std::string escaped;
  for (const char character : text) {
    if (character == '\\') escaped += "\\\\";
    else if (character == '"') escaped += "\\\"";
    else if (character == '\n') escaped += "\\n";
    else escaped += character;
  }
  return escaped;
}

inline std::string optimizationPolicyDecisionJson(
    const OptimizationPolicyDecision &decision) {
  return "{\"schema_version\":" + std::to_string(decision.schemaVersion) +
      ",\"decision_type\":\"" + policyJsonEscape(decision.decisionType) +
      "\",\"subject\":\"" + policyJsonEscape(decision.subject) +
      "\",\"features\":" + optimizationFeaturesJson(decision.features) +
      ",\"action\":\"" + policyJsonEscape(decision.action) +
      "\",\"reason\":\"" + hotnessReasonName(decision.reason) +
      "\",\"estimated_cost\":" + std::to_string(decision.estimatedCost) + "}";
}

class OptimizationPolicy {
public:
  virtual ~OptimizationPolicy() = default;
  virtual HotnessDecision decideTier(const profile::FunctionRuntimeProfile *profile,
                                     const HotnessThresholds &thresholds) const = 0;
  virtual std::vector<std::string> choosePipeline(Hotness hotness) const = 0;
};

class HeuristicOptimizationPolicy final : public OptimizationPolicy {
public:
  HotnessDecision decideTier(const profile::FunctionRuntimeProfile *profile,
                             const HotnessThresholds &thresholds) const override {
    return classifyHotness(profile, thresholds);
  }
  std::vector<std::string> choosePipeline(Hotness hotness) const override {
    switch (hotness) {
    case Hotness::Cold: return {"constant-fold", "dce"};
    case Hotness::Warm: return {"sccp", "simplify-cfg", "cse", "dce"};
    case Hotness::Hot: return {"sccp", "simplify-cfg", "loop-simplify", "licm", "cse", "dce"};
    }
    return {};
  }
};

} // namespace nanopass::opt
