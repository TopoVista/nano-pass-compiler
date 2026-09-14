#pragma once
// compiler/passes/PassInfo.h
//
// Static metadata every pass must provide.  IDs are stable string literals
// so they can be used in CLI options, JSON telemetry, and the AI advisor
// allowlist without depending on RTTI mangled names.
//
// Naming convention for IDs:  lowercase-kebab-case, e.g. "desugar-for".
// Category strings: "desugaring", "analysis", "optimization", "verification".

#include <string_view>
#include "IRUnit.h"

namespace nanopass {

struct PassInfo {
    std::string_view id;          // "desugar-for"  — stable, machine-readable
    std::string_view name;        // "DesugarFor"   — display name
    std::string_view description; // One-line description for `nano passes`
    std::string_view category;    // "desugaring" | "analysis" | "optimization"
    IRUnitKind       unit;        // Granularity this pass operates at
};

} // namespace nanopass
