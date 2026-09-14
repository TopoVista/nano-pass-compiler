#pragma once
// compiler/passes/IRUnit.h
//
// Explicit IR unit granularity.  Every pass declares which unit kind it
// operates on.  The enum is defined here so it can be referenced by Pass.h,
// PassManager.h, and PassRegistry.h without pulling in AST headers.
//
// Current units:
//   Program  — the whole top-level statement list (all current passes)
//   Function — a single function body (planned: SSA, DCE, LICM)
//   Statement / BasicBlock — planned for later phases
//
// Adding a new unit kind here never requires changes to PassManager or Pass.

#include <string_view>

namespace nanopass {

enum class IRUnitKind {
    Program,     // vector<unique_ptr<Stmt>> — Phase 2 passes operate here
    Function,    // single FunctionStmt + its CFG  (Phase 4+)
    Statement,   // single Stmt node               (reserved)
    BasicBlock   // single basic block in CFG IR   (Phase 4+)
};

inline std::string_view toString(IRUnitKind k) noexcept {
    switch (k) {
        case IRUnitKind::Program:    return "program";
        case IRUnitKind::Function:   return "function";
        case IRUnitKind::Statement:  return "statement";
        case IRUnitKind::BasicBlock: return "basic-block";
    }
    return "unknown";
}

} // namespace nanopass
