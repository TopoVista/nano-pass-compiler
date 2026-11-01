#pragma once

#include <charconv>
#include <climits>
#include <compare>
#include <cstdint>
#include <iomanip>
#include <iterator>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "ir/ssa/loop_analysis.h"
#include "ir/ssa/ssa_printer.h"

namespace nanopass::profile {

// Stable runtime identities deliberately use function names and numeric SSA
// block IDs; neither is a pointer or address-dependent implementation detail.
struct EdgeKey {
  int from = -1;
  int to = -1;
  auto operator<=>(const EdgeKey &) const = default;
};

struct LoopRuntimeProfile {
  uint64_t entries = 0;
  uint64_t backedges = 0;
  uint64_t exits = 0;
  // This is an edge-frequency estimate, not an exact trip count for arbitrary
  // multi-entry/multi-exit loops.
  double averageTripCount() const noexcept {
    return entries == 0 ? 0.0 : static_cast<double>(backedges) / static_cast<double>(entries);
  }
};

struct FunctionRuntimeProfile {
  uint64_t calls = 0;
  uint64_t blocksExecuted = 0;
  std::map<int, uint64_t> blockCounts;
  std::map<EdgeKey, uint64_t> edgeCounts;
  std::map<int, LoopRuntimeProfile> loops; // keyed by loop header BlockId
};

enum class ProfileStatus { Complete, Partial, Failed };

struct ProgramRuntimeProfile {
  static constexpr uint64_t kSchemaVersion = 1;
  uint64_t schemaVersion = kSchemaVersion;
  std::string programFingerprint;
  ProfileStatus status = ProfileStatus::Complete;
  std::map<std::string, FunctionRuntimeProfile> functions;

  void functionEntry(const std::string &name) { ++functions[name].calls; }
  void blockEntry(const std::string &name, BlockId block) {
    auto &profile = functions[name]; ++profile.blocksExecuted; ++profile.blockCounts[block.index];
  }
  void edge(const std::string &name, BlockId from, BlockId to) {
    ++functions[name].edgeCounts[{from.index, to.index}];
  }
  void finalizeLoops(const SSAProgram &program) {
    for (const auto &function : program.functions) {
      const auto profileIt = functions.find(function.name);
      if (profileIt == functions.end()) continue;
      auto &profile = profileIt->second;
      const auto info = LoopInfoAnalysis::run(function);
      for (const auto &loop : info.loops) {
        auto &destination = profile.loops[loop.header.index];
        // A header executes once more on the terminating condition. Count
        // incoming edges from outside the natural loop instead: that is the
        // runtime number of loop invocations and makes backedges / entries a
        // useful simple-loop trip estimate.
        std::map<int, bool> member;
        for (const auto block : loop.blocks) member[block.index] = true;
        for (const auto &block : function.blocks) {
          if (member.contains(block.id.index)) continue;
          destination.entries += profile.edgeCounts[{block.id.index, loop.header.index}];
        }
        for (const auto latch : loop.backedgeSources)
          destination.backedges += profile.edgeCounts[{latch.index, loop.header.index}];
        for (const auto &exit : loop.exitEdges)
          destination.exits += profile.edgeCounts[{exit.from.index, exit.to.index}];
      }
    }
  }
};

inline std::string runtimeProfileStatusName(ProfileStatus status) {
  switch (status) { case ProfileStatus::Complete: return "complete"; case ProfileStatus::Partial: return "partial"; case ProfileStatus::Failed: return "failed"; }
  return "failed";
}

inline std::optional<ProfileStatus> parseRuntimeProfileStatus(
    std::string_view name) {
  if (name == "complete") return ProfileStatus::Complete;
  if (name == "partial") return ProfileStatus::Partial;
  if (name == "failed") return ProfileStatus::Failed;
  return std::nullopt;
}

// A compact compatibility guard, derived from the canonical SSA textual
// representation.  It intentionally excludes paths, pointers, timestamps and
// machine state. FNV-1a is used for accidental-profile-mismatch detection,
// not as a cryptographic guarantee.
inline std::string stableProgramFingerprint(const SSAProgram &program) {
  std::ostringstream canonical;
  printSSA(program, canonical);
  uint64_t hash = 14695981039346656037ull;
  for (const unsigned char byte : canonical.str()) {
    hash ^= byte;
    hash *= 1099511628211ull;
  }
  std::ostringstream encoded;
  encoded << "ssa-fnv1a64:" << std::hex << std::setfill('0') << std::setw(16)
          << hash;
  return encoded.str();
}

inline bool runtimeProfileMatches(const ProgramRuntimeProfile &profile,
                                  const SSAProgram &program) {
  return profile.schemaVersion == ProgramRuntimeProfile::kSchemaVersion &&
         !profile.programFingerprint.empty() &&
         profile.programFingerprint == stableProgramFingerprint(program);
}

inline std::string escapeRuntimeProfileJson(std::string_view text) {
  std::string escaped;
  for (const unsigned char ch : text) {
    switch (ch) {
    case '\\': escaped += "\\\\"; break;
    case '"': escaped += "\\\""; break;
    case '\n': escaped += "\\n"; break;
    case '\r': escaped += "\\r"; break;
    case '\t': escaped += "\\t"; break;
    default: escaped += static_cast<char>(ch); break;
    }
  }
  return escaped;
}

inline void writeRuntimeProfileJson(const ProgramRuntimeProfile &profile,
                                    std::ostream &out) {
  out << "{\n  \"schema_version\": " << profile.schemaVersion
      << ",\n  \"profile_kind\": \"ssa-runtime\""
      << ",\n  \"program_fingerprint\": \""
      << escapeRuntimeProfileJson(profile.programFingerprint) << "\""
      << ",\n  \"status\": \"" << runtimeProfileStatusName(profile.status)
      << "\",\n  \"functions\": {";
  bool firstFunction = true;
  for (const auto &[name, function] : profile.functions) {
    out << (firstFunction ? "\n" : ",\n") << "    \""
        << escapeRuntimeProfileJson(name) << "\": {\n"
        << "      \"calls\": " << function.calls << ",\n"
        << "      \"blocks_executed\": " << function.blocksExecuted
        << ",\n      \"block_counts\": {";
    bool first = true;
    for (const auto &[block, count] : function.blockCounts) {
      out << (first ? "\n" : ",\n") << "        \"" << block
          << "\": " << count;
      first = false;
    }
    out << (first ? "},\n" : "\n      },\n")
        << "      \"edge_counts\": [";
    first = true;
    for (const auto &[edge, count] : function.edgeCounts) {
      out << (first ? "\n" : ",\n") << "        {\"from\": "
          << edge.from << ", \"to\": " << edge.to << ", \"count\": "
          << count << "}";
      first = false;
    }
    out << (first ? "],\n" : "\n      ],\n") << "      \"loops\": {";
    first = true;
    for (const auto &[header, loop] : function.loops) {
      out << (first ? "\n" : ",\n") << "        \"" << header
          << "\": {\"entries\": " << loop.entries
          << ", \"backedges\": " << loop.backedges
          << ", \"exits\": " << loop.exits << "}";
      first = false;
    }
    out << (first ? "}\n" : "\n      }\n") << "    }";
    firstFunction = false;
  }
  out << (firstFunction ? "}\n}" : "\n  }\n}");
}

namespace detail {

// A deliberately small, strict JSON reader keeps the profile file independent
// of a third-party JSON dependency. It accepts the JSON forms produced by the
// writer (objects, arrays, strings and unsigned counters), rejects duplicate
// keys, and never treats malformed data as a usable PGO input.
struct JsonValue {
  enum class Kind { Object, Array, String, Number } kind;
  std::map<std::string, JsonValue> object;
  std::vector<JsonValue> array;
  std::string string;
  uint64_t number = 0;
  explicit JsonValue(Kind valueKind) : kind(valueKind) {}
};

class JsonReader {
  std::string_view input_;
  size_t position_ = 0;
  std::string &error_;
  void skipSpace() {
    while (position_ < input_.size() &&
           (input_[position_] == ' ' || input_[position_] == '\n' ||
            input_[position_] == '\r' || input_[position_] == '\t')) ++position_;
  }
  bool take(char character) {
    skipSpace();
    if (position_ == input_.size() || input_[position_] != character) return false;
    ++position_;
    return true;
  }
  std::optional<std::string> parseString() {
    if (!take('"')) return std::nullopt;
    std::string result;
    while (position_ < input_.size()) {
      const char character = input_[position_++];
      if (character == '"') return result;
      if (static_cast<unsigned char>(character) < 0x20) return std::nullopt;
      if (character != '\\') { result += character; continue; }
      if (position_ == input_.size()) return std::nullopt;
      switch (input_[position_++]) {
      case '"': result += '"'; break;
      case '\\': result += '\\'; break;
      case '/': result += '/'; break;
      case 'b': result += '\b'; break;
      case 'f': result += '\f'; break;
      case 'n': result += '\n'; break;
      case 'r': result += '\r'; break;
      case 't': result += '\t'; break;
      default: return std::nullopt;
      }
    }
    return std::nullopt;
  }
  std::optional<uint64_t> parseNumber() {
    skipSpace();
    const char *begin = input_.data() + position_;
    const char *end = input_.data() + input_.size();
    uint64_t value = 0;
    const auto parsed = std::from_chars(begin, end, value);
    if (parsed.ec != std::errc{} || parsed.ptr == begin) return std::nullopt;
    position_ = static_cast<size_t>(parsed.ptr - input_.data());
    return value;
  }
  std::optional<JsonValue> parseObject() {
    if (!take('{')) return std::nullopt;
    JsonValue result{JsonValue::Kind::Object};
    skipSpace();
    if (take('}')) return result;
    while (true) {
      auto key = parseString();
      if (!key || !take(':')) return std::nullopt;
      auto item = parseValue();
      if (!item || !result.object.emplace(std::move(*key), std::move(*item)).second)
        return std::nullopt;
      if (take('}')) return result;
      if (!take(',')) return std::nullopt;
    }
  }
  std::optional<JsonValue> parseArray() {
    if (!take('[')) return std::nullopt;
    JsonValue result{JsonValue::Kind::Array};
    skipSpace();
    if (take(']')) return result;
    while (true) {
      auto item = parseValue();
      if (!item) return std::nullopt;
      result.array.push_back(std::move(*item));
      if (take(']')) return result;
      if (!take(',')) return std::nullopt;
    }
  }
  std::optional<JsonValue> parseValue() {
    skipSpace();
    if (position_ == input_.size()) return std::nullopt;
    if (input_[position_] == '{') return parseObject();
    if (input_[position_] == '[') return parseArray();
    if (input_[position_] == '"') {
      auto text = parseString();
      if (!text) return std::nullopt;
      JsonValue result{JsonValue::Kind::String};
      result.string = std::move(*text);
      return result;
    }
    auto number = parseNumber();
    if (!number) return std::nullopt;
    JsonValue result{JsonValue::Kind::Number}; result.number = *number;
    return result;
  }
public:
  JsonReader(std::string_view input, std::string &error) : input_(input), error_(error) {}
  std::optional<JsonValue> parse() {
    auto root = parseValue();
    skipSpace();
    if (!root || position_ != input_.size()) {
      error_ = "malformed JSON profile";
      return std::nullopt;
    }
    return root;
  }
};

inline const JsonValue *profileField(const JsonValue &object,
                                     std::string_view name,
                                     JsonValue::Kind expected) {
  if (object.kind != JsonValue::Kind::Object) return nullptr;
  const auto found = object.object.find(std::string(name));
  if (found == object.object.end() || found->second.kind != expected) return nullptr;
  return &found->second;
}

inline std::optional<int> parseBlockNumber(std::string_view text) {
  int value = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value < 0)
    return std::nullopt;
  return value;
}

} // namespace detail

inline std::optional<ProgramRuntimeProfile> readRuntimeProfileJson(
    std::istream &input, std::string &error) {
  const std::string text((std::istreambuf_iterator<char>(input)), {});
  auto root = detail::JsonReader(text, error).parse();
  if (!root) return std::nullopt;
  const auto *version = detail::profileField(*root, "schema_version", detail::JsonValue::Kind::Number);
  const auto *kind = detail::profileField(*root, "profile_kind", detail::JsonValue::Kind::String);
  const auto *fingerprint = detail::profileField(*root, "program_fingerprint", detail::JsonValue::Kind::String);
  const auto *status = detail::profileField(*root, "status", detail::JsonValue::Kind::String);
  const auto *functions = detail::profileField(*root, "functions", detail::JsonValue::Kind::Object);
  if (!version || !kind || !fingerprint || !status || !functions ||
      version->number != ProgramRuntimeProfile::kSchemaVersion || kind->string != "ssa-runtime") {
    error = "unsupported or incomplete SSA runtime profile";
    return std::nullopt;
  }
  const auto parsedStatus = parseRuntimeProfileStatus(status->string);
  if (!parsedStatus) { error = "unknown runtime profile status"; return std::nullopt; }
  ProgramRuntimeProfile profile;
  profile.programFingerprint = fingerprint->string;
  profile.status = *parsedStatus;
  for (const auto &[name, encodedFunction] : functions->object) {
    const auto *calls = detail::profileField(encodedFunction, "calls", detail::JsonValue::Kind::Number);
    const auto *executed = detail::profileField(encodedFunction, "blocks_executed", detail::JsonValue::Kind::Number);
    const auto *blocks = detail::profileField(encodedFunction, "block_counts", detail::JsonValue::Kind::Object);
    const auto *edges = detail::profileField(encodedFunction, "edge_counts", detail::JsonValue::Kind::Array);
    const auto *loops = detail::profileField(encodedFunction, "loops", detail::JsonValue::Kind::Object);
    if (!calls || !executed || !blocks || !edges || !loops) { error = "malformed function runtime profile"; return std::nullopt; }
    FunctionRuntimeProfile function;
    function.calls = calls->number;
    function.blocksExecuted = executed->number;
    for (const auto &[block, count] : blocks->object) {
      const auto id = detail::parseBlockNumber(block);
      if (!id || count.kind != detail::JsonValue::Kind::Number ||
          !function.blockCounts.emplace(*id, count.number).second) {
        error = "invalid block counter"; return std::nullopt;
      }
    }
    for (const auto &edge : edges->array) {
      const auto *from = detail::profileField(edge, "from", detail::JsonValue::Kind::Number);
      const auto *to = detail::profileField(edge, "to", detail::JsonValue::Kind::Number);
      const auto *count = detail::profileField(edge, "count", detail::JsonValue::Kind::Number);
      if (!from || !to || !count || from->number > static_cast<uint64_t>(INT_MAX) ||
          to->number > static_cast<uint64_t>(INT_MAX) ||
          !function.edgeCounts.emplace(EdgeKey{static_cast<int>(from->number), static_cast<int>(to->number)}, count->number).second) {
        error = "invalid edge counter"; return std::nullopt;
      }
    }
    for (const auto &[header, encodedLoop] : loops->object) {
      const auto id = detail::parseBlockNumber(header);
      const auto *entries = detail::profileField(encodedLoop, "entries", detail::JsonValue::Kind::Number);
      const auto *backedges = detail::profileField(encodedLoop, "backedges", detail::JsonValue::Kind::Number);
      const auto *exits = detail::profileField(encodedLoop, "exits", detail::JsonValue::Kind::Number);
      if (!id || !entries || !backedges || !exits ||
          !function.loops.emplace(*id, LoopRuntimeProfile{entries->number, backedges->number, exits->number}).second) {
        error = "invalid loop counter"; return std::nullopt;
      }
    }
    profile.functions.emplace(name, std::move(function));
  }
  return profile;
}

} // namespace nanopass::profile
