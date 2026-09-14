#pragma once
// compiler/passes/PassResult.h
//
// Value returned by every Pass::run().
//
// Fields:
//   changed   — true if the pass modified the program in any way.
//               Used for: analysis invalidation, fixpoint detection, telemetry.
//               Passes MUST return false when they made no changes.
//
//   preserved — which analyses remain valid after this pass.
//               Defaults to PreservedAnalyses::none() — the safe conservative
//               choice.  Passes that don't change anything should return
//               PreservedAnalyses::all() or use PassResult::unchanged().
//
//   stats     — per-pass named counters (rewrites, nodes_before, etc.)
//
// Convenience factories:
//
//   PassResult::noChange()
//     changed=false, preserved=all, empty stats
//
//   PassResult::changed(PreservedAnalyses pa, PassStatistics stats)
//     changed=true, preserved=pa, stats

#include "PreservedAnalyses.h"
#include "PassStatistics.h"

namespace nanopass {

struct PassResult {
    bool              changed   = false;
    PreservedAnalyses preserved = PreservedAnalyses::none();
    PassStatistics    stats;

    // Build a "nothing changed" result — all analyses preserved, no stats.
    static PassResult noChange() {
        PassResult r;
        r.changed   = false;
        r.preserved = PreservedAnalyses::all();
        return r;
    }

    // Build a "changed" result with explicit preservation and stats.
    static PassResult didChange(PreservedAnalyses pa = PreservedAnalyses::none(),
                                PassStatistics    stats = {}) {
        PassResult r;
        r.changed   = true;
        r.preserved = std::move(pa);
        r.stats     = std::move(stats);
        return r;
    }
};

} // namespace nanopass
