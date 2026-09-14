#pragma once
// compiler/passes/PreservedAnalyses.h
//
// Encodes which analyses a pass guarantees are still valid after it runs.
//
// Design:
//   - Passes that change nothing return PreservedAnalyses::all()
//   - Passes that structurally rewrite the AST return PreservedAnalyses::none()
//     (or selectively preserve type information if no type changes occurred)
//   - Read-only passes always return PreservedAnalyses::all()
//
// Internally the set uses std::type_index so each analysis type registers
// exactly one slot.  The public API uses template parameters so callers never
// touch type_index directly.
//
// Future CFG analyses will follow the same pattern — no changes to this header
// are needed to support them.

#include <typeindex>
#include <typeinfo>
#include <unordered_set>

namespace nanopass {

class PreservedAnalyses {
public:
    // Factory: all analyses are preserved (default for read-only / no-change passes).
    static PreservedAnalyses all() noexcept {
        PreservedAnalyses pa;
        pa.allPreserved_ = true;
        return pa;
    }

    // Factory: no analyses are preserved (use after structural rewrites).
    static PreservedAnalyses none() noexcept {
        return PreservedAnalyses{};
    }

    // Mark a specific analysis as preserved.
    // Example:  result.preserved.preserve<NodeCountAnalysis>();
    template<typename AnalysisT>
    void preserve() {
        allPreserved_ = false; // explicit set overrides the "all" shortcut
        preserved_.insert(std::type_index(typeid(AnalysisT)));
    }

    // Query whether a specific analysis is preserved.
    template<typename AnalysisT>
    bool isPreserved() const noexcept {
        if (allPreserved_) return true;
        return preserved_.count(std::type_index(typeid(AnalysisT))) > 0;
    }

    // True when constructed via all() and no explicit preserve<T>() was called.
    bool isAll() const noexcept { return allPreserved_; }

    // True when nothing is preserved.
    bool isNone() const noexcept {
        return !allPreserved_ && preserved_.empty();
    }

private:
    bool allPreserved_ = false;
    std::unordered_set<std::type_index> preserved_;
};

} // namespace nanopass
