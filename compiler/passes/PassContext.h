#pragma once
// compiler/passes/PassContext.h
//
// Shared context available to every pass during execution.
// PassContext holds references — it does NOT own the objects it points to.
// The PassManager constructs and owns the PassContext and must outlive it.
//
// Responsibilities:
//   - Provide access to the AnalysisManager (analysis requests and caching)
//   - Carry compiler option flags relevant to pass execution
//   - Expose the debug output stream
//   - Stop-after and disable-pass support
//
// What PassContext is NOT:
//   - It does not contain the program IR.  IR is passed directly to Pass::run().
//   - It does not accumulate global mutable state between passes.
//   - It is not a service locator.

#include <functional>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_set>

// Forward declaration to avoid circular include with AnalysisManager.h
namespace nanopass { class AnalysisManager; }

namespace nanopass {

struct CompilerPassOptions {
    bool debugPassManager = false;
    bool debugAnalyses    = false;
    bool printPipeline    = false;
    std::string stopAfter;      // empty = no stop; else stable pass ID
    std::unordered_set<std::string> disabledPasses; // stable IDs
};

class PassContext {
public:
    PassContext(AnalysisManager &am,
                const CompilerPassOptions &opts,
                std::ostream &debugOut)
        : am_(am), opts_(opts), debugOut_(debugOut) {}

    // Access the analysis manager.
    AnalysisManager &analyses() noexcept { return am_; }
    const AnalysisManager &analyses() const noexcept { return am_; }

    // Option accessors.
    bool debugPassManager() const noexcept { return opts_.debugPassManager; }
    bool debugAnalyses()    const noexcept { return opts_.debugAnalyses; }
    bool printPipeline()    const noexcept { return opts_.printPipeline; }

    // Stop-after: returns true when the pipeline should halt after the named pass.
    bool shouldStopAfter(std::string_view passId) const noexcept {
        return !opts_.stopAfter.empty() && opts_.stopAfter == passId;
    }

    // Disable-pass: returns true when a specific pass should be skipped.
    bool isPassDisabled(std::string_view passId) const noexcept {
        return opts_.disabledPasses.count(std::string(passId)) > 0;
    }

    // Debug output stream (usually std::cerr, redirectable in tests).
    std::ostream &debugStream() noexcept { return debugOut_; }

private:
    AnalysisManager          &am_;
    const CompilerPassOptions &opts_;
    std::ostream             &debugOut_;
};

} // namespace nanopass
