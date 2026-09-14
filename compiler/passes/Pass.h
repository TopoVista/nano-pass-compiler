#pragma once
// compiler/passes/Pass.h
//
// Abstract base class for all NanoPass compiler passes.
//
// Every pass:
//   1. Provides static metadata via info().
//   2. Implements run() which receives the full program and a PassContext.
//   3. Returns a PassResult indicating whether the IR was changed and which
//      analyses are still valid.
//
// Granularity contract:
//   Passes operating at IRUnitKind::Program receive the full
//   vector<unique_ptr<Stmt>>.  They are responsible for recursing into
//   FunctionStmt bodies, BlockStmts, etc.  The PassManager calls run() ONCE
//   per pipeline execution — not once per top-level statement.
//
// Ownership contract:
//   The program is passed by reference.  Passes may move unique_ptrs within
//   the vector (e.g. replace a ForStmt with a BlockStmt) but must not leave
//   null entries.  After run() returns, every slot in the vector must contain
//   a non-null unique_ptr<Stmt>.
//
// Thread safety:
//   Pass objects are not required to be thread-safe.  The PassManager runs
//   passes sequentially.
//
// Naming:
//   info().id   must be a stable lowercase-kebab-case string literal.
//   info().name must be a stable PascalCase display name.

#include <memory>
#include <string_view>
#include <vector>

#include "PassInfo.h"
#include "PassResult.h"
#include "PassContext.h"

// Forward-declare AST types to avoid including heavy headers here.
struct Stmt;

namespace nanopass {

// Type alias so pass signatures read naturally.
using Program = std::vector<std::unique_ptr<Stmt>>;

class Pass {
public:
    virtual ~Pass() = default;

    // Static metadata — must not change across calls.
    virtual PassInfo info() const noexcept = 0;

    // Execute the pass.  The default implementation asserts false so accidental
    // instantiation of an incomplete pass is caught at runtime.
    virtual PassResult run(Program &program, PassContext &ctx) = 0;

    // Convenience: stable pass ID without constructing PassInfo.
    std::string_view id()   const noexcept { return info().id; }
    std::string_view name() const noexcept { return info().name; }
};

} // namespace nanopass
