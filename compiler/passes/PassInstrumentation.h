#pragma once
// compiler/passes/PassInstrumentation.h
//
// Observer interface around pass and analysis lifecycle events.
// PassManager holds one PassInstrumentation instance.  By default every
// callback is null (no-op).  The CLI installs a printing instrumentation
// when --debug-pass-manager is set.
//
// Event ordering guarantee:
//   onPassStart  → (pass executes) → onPassEnd
//   onAnalysisAccess fires during pass execution (inside AnalysisManager::get)
//   onAnalysisInvalidated fires during PassManager's post-pass invalidation
//
// Callers should not rely on the relative order of onAnalysisAccess vs
// onPassEnd — both may occur in the same synchronous call stack.

#include <functional>
#include <string_view>
#include "PassInfo.h"
#include "PassTelemetry.h"

namespace nanopass {

struct PassInstrumentation {
    // Fired just before Pass::run() is called.
    std::function<void(const PassInfo &)> onPassStart;

    // Fired after Pass::run() returns and telemetry is assembled.
    std::function<void(const PassTelemetry &)> onPassEnd;

    // Fired by AnalysisManager::get<T>() on every access.
    //   id  — analysis stable ID (e.g. "ast-node-count")
    //   hit — true if the result was already cached
    std::function<void(std::string_view id, bool hit)> onAnalysisAccess;

    // Fired by AnalysisManager when a cached result is dropped.
    std::function<void(std::string_view id)> onAnalysisInvalidated;

    // Fired once at the start of a PassManager::run() call.
    std::function<void(std::string_view pipelineName)> onPipelineStart;

    // Fired once after all passes in a pipeline have executed.
    //   totalDurationNs — sum of all pass durations
    //   anyChanged      — true if at least one pass reported changed=true
    std::function<void(std::string_view pipelineName,
                       uint64_t totalDurationNs,
                       bool anyChanged)> onPipelineEnd;

    // Helper: returns true if at least one callback is non-null.
    bool active() const noexcept {
        return onPassStart || onPassEnd || onAnalysisAccess
            || onAnalysisInvalidated || onPipelineStart || onPipelineEnd;
    }
};

} // namespace nanopass
