#pragma once
// compiler/passes/AnalysisManager.h
//
// Caches analysis results and manages invalidation.
//
// Design:
//   Each analysis type AnalysisT must satisfy:
//     static std::string_view id()    — stable ID, e.g. "ast-node-count"
//     static std::string_view name()  — display name
//     Result compute(const Program &) — pure computation
//     (Result must be copyable/movable)
//
//   Results are stored as std::any keyed by std::type_index.
//   No raw pointers into AST nodes are stored — only plain value structs.
//   This makes invalidation safe: dropping a cached result never creates
//   dangling pointers.
//
// Invalidation:
//   PassManager calls invalidate(PreservedAnalyses) after each pass.
//   Every cached analysis NOT in the preserved set is dropped.
//   Returns the list of invalidated IDs (for telemetry / debug output).
//
// Debug hook:
//   The PassInstrumentation::onAnalysisAccess callback is fired on every
//   get<T>() call.  The PassManager forwards the PassInstrumentation to the
//   AnalysisManager after construction.

#include <any>
#include <functional>
#include <string>
#include <string_view>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include "PreservedAnalyses.h"
#include "PassInstrumentation.h"  // for onAnalysisAccess / onAnalysisInvalidated
#include "Pass.h"                 // for Program alias

namespace nanopass {

class AnalysisManager {
public:
    // ----- Main API ---------------------------------------------------------

    // Get the result of AnalysisT, computing it if not cached.
    template<typename AnalysisT>
    const typename AnalysisT::Result &
    get(const Program &program) {
        const std::type_index key(typeid(AnalysisT));
        const bool hit = cache_.count(key) > 0;

        if (onAnalysisAccess_)
            onAnalysisAccess_(AnalysisT::id(), hit);

        if (!hit) {
            AnalysisT analysis;
            cache_[key] = analysis.compute(program);
            // Track the stable ID so we can emit it on invalidation.
            idForKey_[key] = std::string(AnalysisT::id());
        }

        return std::any_cast<typename AnalysisT::Result &>(cache_.at(key));
    }

    // Explicitly invalidate one analysis type.
    template<typename AnalysisT>
    void invalidate() {
        const std::type_index key(typeid(AnalysisT));
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            const std::string id = idForKey_.count(key) ? idForKey_[key]
                                                        : "?";
            cache_.erase(it);
            if (onAnalysisInvalidated_)
                onAnalysisInvalidated_(id);
        }
    }

    // Called by PassManager after each pass.
    // Drops every cached analysis NOT covered by `preserved`.
    // Returns the list of invalidated analysis IDs (for telemetry).
    std::vector<std::string>
    invalidate(const PreservedAnalyses &preserved,
               const PassInstrumentation &instr) {
        std::vector<std::string> dropped;

        for (auto it = cache_.begin(); it != cache_.end(); ) {
            const std::type_index &key = it->first;
            // Use the internal preserved_ set via a per-key query.
            // We reconstruct a synthetic type_index check — preserved.isAll()
            // short-circuits everything.
            if (preserved.isAll()) {
                // Nothing to drop.
                ++it;
                continue;
            }
            // Check whether this specific analysis is preserved.
            // We can't call preserved.isPreserved<AnalysisT>() generically
            // without knowing T, so we mirror the internal check:
            // isPreserved returns true iff the type_index is in the set OR
            // allPreserved_ is true.  We checked !isAll() above, so we need
            // the set check.  Access it through the public none/preserve API
            // indirectly: we know isAll() == false; check the preserved set
            // by asking PreservedAnalyses directly.
            // Since PreservedAnalyses only exposes template<T> isPreserved(),
            // and we don't know T here, we must use a workaround: check our
            // own keyToPreserved map built when passes call preserve<T>().
            //
            // Simpler design: let PassResult carry both the PreservedAnalyses
            // (for type-safe per-pass usage) AND a set<type_index> for the
            // manager to iterate.  We compute this here by asking each cached
            // key if it's preserved using a helper stored at registration.
            //
            // For Phase 2 the set of analyses is small (2 types).  We use
            // the checking function registered via registerPreservationCheck.
            bool isPreserved = false;
            auto checkIt = preservationChecks_.find(key);
            if (checkIt != preservationChecks_.end()) {
                isPreserved = checkIt->second(preserved);
            }
            // If no check registered, treat as not preserved (safe default).

            if (!isPreserved) {
                const std::string id = idForKey_.count(key) ? idForKey_[key]
                                                            : "?";
                dropped.push_back(id);
                if (instr.onAnalysisInvalidated)
                    instr.onAnalysisInvalidated(id);
                it = cache_.erase(it);
            } else {
                ++it;
            }
        }
        return dropped;
    }

    // Drop everything.
    void invalidateAll() {
        for (auto &[key, _] : cache_) {
            const std::string id = idForKey_.count(key) ? idForKey_[key] : "?";
            if (onAnalysisInvalidated_) onAnalysisInvalidated_(id);
        }
        cache_.clear();
    }

    // ----- Registration (called once at analysis definition time) -----------

    // Register a preservation-check function for an analysis type.
    // Called automatically by the NANO_REGISTER_ANALYSIS macro.
    template<typename AnalysisT>
    void registerAnalysis() {
        const std::type_index key(typeid(AnalysisT));
        idForKey_[key] = std::string(AnalysisT::id());
        preservationChecks_[key] = [](const PreservedAnalyses &pa) -> bool {
            return pa.isPreserved<AnalysisT>();
        };
    }

    // ----- Debug hooks (set by PassManager from PassInstrumentation) --------

    void setOnAnalysisAccess(
            std::function<void(std::string_view, bool)> fn) {
        onAnalysisAccess_ = std::move(fn);
    }
    void setOnAnalysisInvalidated(
            std::function<void(std::string_view)> fn) {
        onAnalysisInvalidated_ = std::move(fn);
    }

    // ----- Introspection ----------------------------------------------------

    // True if a result for AnalysisT is currently in the cache.
    template<typename AnalysisT>
    bool isCached() const {
        return cache_.count(std::type_index(typeid(AnalysisT))) > 0;
    }

    std::size_t cacheSize() const noexcept { return cache_.size(); }

private:
    // Stored results.  Values are std::any holding AnalysisT::Result.
    std::unordered_map<std::type_index, std::any> cache_;

    // Maps type_index → stable string ID for telemetry/debug output.
    std::unordered_map<std::type_index, std::string> idForKey_;

    // Maps type_index → function that queries PreservedAnalyses for that type.
    std::unordered_map<
        std::type_index,
        std::function<bool(const PreservedAnalyses &)>>
        preservationChecks_;

    std::function<void(std::string_view, bool)> onAnalysisAccess_;
    std::function<void(std::string_view)>       onAnalysisInvalidated_;
};

} // namespace nanopass
