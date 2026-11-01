#pragma once

#include <cstdint>
#include <map>
#include <ostream>
#include <string>
#include <string_view>

#include "interpreter/profile_sink.h"

namespace nanopass::profile {

struct BranchCounts {
  uint64_t taken = 0;
  uint64_t notTaken = 0;
};

// Versioned, deterministic Nano-level profile. std::map is deliberate: the
// on-disk representation is stable across runs for the same execution.
class NanoProfile final : public ExecutionProfileSink {
public:
  static constexpr int kSchemaVersion = 1;

  void recordFunctionCall(std::string_view function) override {
    ++functionCounts[std::string(function)];
  }
  void recordBranch(std::string_view site, bool taken) override {
    auto &counts = branchCounts[std::string(site)];
    if (taken) ++counts.taken;
    else ++counts.notTaken;
  }
  void recordLoopIteration(std::string_view site) override {
    ++loopIterations[std::string(site)];
  }

  std::map<std::string, uint64_t> functionCounts;
  std::map<std::string, BranchCounts> branchCounts;
  std::map<std::string, uint64_t> loopIterations;
};

inline std::string escapeJson(std::string_view text) {
  std::string escaped;
  for (const char ch : text) {
    switch (ch) {
    case '\\': escaped += "\\\\"; break;
    case '\"': escaped += "\\\""; break;
    case '\n': escaped += "\\n"; break;
    case '\r': escaped += "\\r"; break;
    case '\t': escaped += "\\t"; break;
    default: escaped += ch; break;
    }
  }
  return escaped;
}

inline void writeProfileJson(const NanoProfile &profile, std::ostream &out) {
  out << "{\n  \"schema_version\": " << NanoProfile::kSchemaVersion;
  auto writeMapPrefix = [&](std::string_view name) { out << ",\n  \"" << name << "\": {"; };
  writeMapPrefix("function_counts");
  bool first = true;
  for (const auto &[name, count] : profile.functionCounts) {
    out << (first ? "\n" : ",\n") << "    \"" << escapeJson(name) << "\": " << count;
    first = false;
  }
  out << (first ? "}" : "\n  }");
  writeMapPrefix("branch_counts");
  first = true;
  for (const auto &[site, counts] : profile.branchCounts) {
    out << (first ? "\n" : ",\n") << "    \"" << escapeJson(site)
        << "\": {\"taken\": " << counts.taken << ", \"not_taken\": " << counts.notTaken << "}";
    first = false;
  }
  out << (first ? "}" : "\n  }");
  writeMapPrefix("loop_iterations");
  first = true;
  for (const auto &[site, count] : profile.loopIterations) {
    out << (first ? "\n" : ",\n") << "    \"" << escapeJson(site) << "\": " << count;
    first = false;
  }
  out << (first ? "}\n}" : "\n  }\n}");
}

} // namespace nanopass::profile
