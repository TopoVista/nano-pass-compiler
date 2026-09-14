#pragma once
// compiler/passes/PipelineBuilder.h
//
// Constructs named compiler pipelines and populates the PassRegistry.
//
// This is the single authoritative source for:
//   - which passes exist
//   - which passes belong to which pipeline
//   - which analyses are available
//
// Calling PipelineBuilder::initialise() once at program start registers
// everything in PassRegistry::get().  It is idempotent.
//
// Current pipelines
// -----------------
//   desugar   DesugarFor → DesugarIncDec → DesugarPlusAssign
//
// Future pipelines (planned, not implemented here):
//   O0 / O1 / O2   — optimisation pipelines (Phase 5)
//   verify          — invariant checking pipeline (Phase 3)

#include <memory>
#include "PassManager.h"
#include "PassRegistry.h"
#include "ASTAnalyses.h"

// Forward-declare the concrete passes so this header doesn't pull in their
// full implementations (those headers include AST headers).
namespace nanopass {
class DesugarForPass;
class DesugarIncDecPass;
class DesugarPlusAssignPass;
}

// Include concrete pass headers here — they are small headers-only files.
#include "../passes/desugar_passes.h"

namespace nanopass {

class PipelineBuilder {
public:
    // Must be called once before any pipeline is built or the registry is
    // queried.  Safe to call multiple times (noop after first call).
    static void initialise() {
        static bool done = false;
        if (done) return;
        done = true;

        PassRegistry &reg = PassRegistry::get();

        // ── Register passes ------------------------------------------------
        reg.registerPass({
            "desugar-for",
            "DesugarFor",
            "Lower for-loops to equivalent while-loops.",
            "desugaring",
            IRUnitKind::Program
        });
        reg.registerPass({
            "desugar-inc-dec",
            "DesugarIncDec",
            "Lower ++ and -- operators to explicit additions/subtractions.",
            "desugaring",
            IRUnitKind::Program
        });
        reg.registerPass({
            "desugar-plus-assign",
            "DesugarPlusAssign",
            "Lower += compound assignment to simple assignment.",
            "desugaring",
            IRUnitKind::Program
        });

        // ── Register analyses -----------------------------------------------
        reg.registerAnalysis({
            std::string(NodeCountAnalysis::id()),
            std::string(NodeCountAnalysis::name()),
            "Count total AST nodes (statements, expressions, functions).",
            "ast",
            IRUnitKind::Program
        });
        reg.registerAnalysis({
            std::string(ASTDepthAnalysis::id()),
            std::string(ASTDepthAnalysis::name()),
            "Compute maximum nesting depth of the AST.",
            "ast",
            IRUnitKind::Program
        });
    }

    // Build the standard desugaring pipeline.
    static PassManager buildDesugarPipeline() {
        initialise();
        PassManager pm("desugar");
        pm.addPass(std::make_unique<DesugarForPass>());
        pm.addPass(std::make_unique<DesugarIncDecPass>());
        pm.addPass(std::make_unique<DesugarPlusAssignPass>());
        return pm;
    }

    // Register the two AST analyses with the given AnalysisManager so that
    // preservation checks work correctly.
    static void registerAnalysesWithManager(AnalysisManager &am) {
        am.registerAnalysis<NodeCountAnalysis>();
        am.registerAnalysis<ASTDepthAnalysis>();
    }
};

} // namespace nanopass
