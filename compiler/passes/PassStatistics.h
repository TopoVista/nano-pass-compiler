#pragma once
// compiler/passes/PassStatistics.h
//
// Per-pass named statistics.  Each pass may record arbitrary named counters.
// The map is intentionally open — no central enum required — so each pass owns
// its own stat names without a global registry.
//
// Supported value types: int64_t (counts), double (ratios/times), string.
//
// Example usage inside a pass:
//
//   stats_.record("rewrites", rewrites_);
//   stats_.record("nodes_before", before);
//   stats_.record("nodes_after", after);
//
// The PassManager stores these in PassTelemetry and forwards them to
// instrumentation hooks.

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace nanopass {

using StatValue = std::variant<int64_t, double, std::string>;

class PassStatistics {
public:
    void record(std::string_view key, int64_t value) {
        stats_[std::string(key)] = value;
    }
    void record(std::string_view key, double value) {
        stats_[std::string(key)] = value;
    }
    void record(std::string_view key, std::string value) {
        stats_[std::string(key)] = std::move(value);
    }
    // Convenience: record a size_t as int64_t
    void record(std::string_view key, std::size_t value) {
        stats_[std::string(key)] = static_cast<int64_t>(value);
    }

    bool has(std::string_view key) const {
        return stats_.count(std::string(key)) > 0;
    }

    // Returns nullptr if key is absent.
    const StatValue *get(std::string_view key) const {
        auto it = stats_.find(std::string(key));
        if (it == stats_.end()) return nullptr;
        return &it->second;
    }

    int64_t getInt(std::string_view key, int64_t defaultVal = 0) const {
        const StatValue *v = get(key);
        if (!v) return defaultVal;
        if (auto *i = std::get_if<int64_t>(v)) return *i;
        return defaultVal;
    }

    const std::unordered_map<std::string, StatValue> &all() const {
        return stats_;
    }

    bool empty() const { return stats_.empty(); }

private:
    std::unordered_map<std::string, StatValue> stats_;
};

} // namespace nanopass
