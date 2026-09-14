#pragma once
// compiler/passes/PassManager.h
//
// Orchestrates a sequential pipeline of Pass objects over a Program.
//
// Run semantics:
//   Each pass executes ONCE over the full program, in the order they were
//   added.  A pass is NOT called once per top-level statement — the pass
//   itself recurses into the AST as needed.
//
// Instrumentation:
//   PassManager fires PassInstrumentation callbacks around each pass and
//   around the full pipeline.  Install a printing instrumentation to get
//   --debug-pass-manager output.
//
// Stop-after:
//   If PassContext::shouldStopAfter(id) returns true after a pass finishes,
//   the pipeline halts and the remaining passes are not executed.
//
// Disable-pass:
//   If PassContext::isPassDisabled(id) returns true, the pass is skipped but
//   its analysis invalidation is NOT performed (because it didn't run).
//
// Analysis invalidation:
//   After each pass the manager inspects PassResult::preserved and calls
//   AnalysisManager::invalidate() for every cached analysis that is not in
//   the preserved set.
//
// The PassManager does not own the Program.  It holds unique_ptr<Pass> objects.
// It may be called multiple times (e.g. fixpoint iteration) with the same or
// different programs.

#include <chrono>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "Pass.h"
#include "PassContext.h"
#include "PassInstrumentation.h"
#include "PassTelemetry.h"
#include "AnalysisManager.h"

namespace nanopass {

class PassManager {
public:
    explicit PassManager(std::string name = "unnamed")
        : name_(std::move(name)) {}

    // Register a pass.  Passes execute in insertion order.
    void addPass(std::unique_ptr<Pass> pass) {
        pipeline_.push_back(std::move(pass));
    }

    // Install instrumentation callbacks.
    void setInstrumentation(PassInstrumentation instr) {
        instr_ = std::move(instr);
    }

    // Access instrumentation (for adding callbacks incrementally).
    PassInstrumentation &instrumentation() noexcept { return instr_; }

    // Run the full pipeline.
    // Returns true if any pass reported changed=true.
    // Precondition: every slot in program is non-null.
    // Postcondition: every slot in program is non-null.
    bool run(Program &program, PassContext &ctx) {
        using Clock = std::chrono::steady_clock;

        if (instr_.onPipelineStart)
            instr_.onPipelineStart(name_);

        bool     anyChanged    = false;
        uint64_t totalDurationNs = 0;

        for (auto &pass : pipeline_) {
            const PassInfo &info = pass->info();

            // Skip disabled passes.
            if (ctx.isPassDisabled(info.id))
                continue;

            if (instr_.onPassStart)
                instr_.onPassStart(info);

            const auto t0 = Clock::now();
            PassResult result = pass->run(program, ctx);
            const auto t1 = Clock::now();

            const uint64_t durationNs =
                static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        t1 - t0).count());
            totalDurationNs += durationNs;

            if (result.changed)
                anyChanged = true;

            // Invalidate stale analyses.
            std::vector<std::string> invalidated =
                ctx.analyses().invalidate(result.preserved, instr_);

            // Assemble telemetry.
            PassTelemetry telemetry;
            telemetry.passId             = std::string(info.id);
            telemetry.passName           = std::string(info.name);
            telemetry.unit               = info.unit;
            telemetry.changed            = result.changed;
            telemetry.durationNs         = durationNs;
            telemetry.stats              = std::move(result.stats);
            telemetry.invalidatedAnalyses = std::move(invalidated);

            if (instr_.onPassEnd)
                instr_.onPassEnd(telemetry);

            // Stop-after support.
            if (ctx.shouldStopAfter(info.id))
                break;
        }

        if (instr_.onPipelineEnd)
            instr_.onPipelineEnd(name_, totalDurationNs, anyChanged);

        return anyChanged;
    }

    // Print the registered pipeline to `out` (for --print-pipeline).
    void printPipeline(std::ostream &out) const {
        out << "Pipeline: " << name_ << "\n";
        for (const auto &pass : pipeline_) {
            const PassInfo &i = pass->info();
            out << "  " << i.id
                << "  [" << toString(i.unit) << "]"
                << "  " << i.description << "\n";
        }
    }

    std::string_view name() const noexcept { return name_; }
    std::size_t size() const noexcept { return pipeline_.size(); }
    bool empty() const noexcept { return pipeline_.empty(); }

    // Iteration support (for tests / introspection).
    const Pass &passAt(std::size_t idx) const { return *pipeline_.at(idx); }

private:
    std::string                     name_;
    std::vector<std::unique_ptr<Pass>> pipeline_;
    PassInstrumentation             instr_;
};

} // namespace nanopass
