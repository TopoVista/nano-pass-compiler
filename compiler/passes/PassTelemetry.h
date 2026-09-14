#pragma once
// compiler/passes/PassTelemetry.h
//
// Structured telemetry record produced for each pass execution.
// Only fields that are actually populated by the PassManager are present.
// All timing values use std::chrono::steady_clock so they are monotonically
// increasing and not affected by NTP adjustments.
//
// This struct is passed to PassInstrumentation::onPassEnd and can later be
// serialised to JSON for the `--pass-telemetry=json` flag.

#include <cstdint>
#include <string>
#include <vector>
#include "PassInfo.h"
#include "PassStatistics.h"

namespace nanopass {

struct PassTelemetry {
    std::string  passId;          // matches PassInfo::id
    std::string  passName;        // matches PassInfo::name
    IRUnitKind   unit;

    bool         changed      = false;
    uint64_t     durationNs   = 0;   // wall-clock nanoseconds

    // Per-pass statistics copied from PassResult::stats.
    PassStatistics stats;

    // Analysis IDs invalidated as a consequence of this pass.
    std::vector<std::string> invalidatedAnalyses;

    // Derived helpers
    double durationUs() const noexcept {
        return static_cast<double>(durationNs) / 1000.0;
    }
};

} // namespace nanopass
