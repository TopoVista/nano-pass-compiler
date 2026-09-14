// tests/pass_manager_tests.cpp
//
// Unit tests for the Phase 2 PassManager + AnalysisManager infrastructure.
//
// Tests are self-contained: they build small AST programs directly without
// going through the lexer/parser so they remain fast and do not require LLVM.
//
// Test categories:
//   1.  Pass execution order
//   2.  changed flag accuracy (true / false)
//   3.  Per-pass rewrite counters
//   4.  Nested desugaring (for-loop inside function body)
//   5.  Analysis cache: first access computes, second hits cache
//   6.  Analysis invalidation after mutating pass
//   7.  Read-only pass preserves all analyses
//   8.  Selective preservation
//   9.  Stop-after support
//   10. Disable-pass support
//   11. Pipeline executes each pass once (not once per statement)
//   12. PassRegistry lookup and validation
//   13. PreservedAnalyses semantics

#include <cassert>
#include <cstddef>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// Compiler headers — no LLVM headers pulled in
#include "ast/expr.h"
#include "ast/stmt.h"
#include "passes/Pass.h"
#include "passes/PassManager.h"
#include "passes/PassContext.h"
#include "passes/AnalysisManager.h"
#include "passes/ASTAnalyses.h"
#include "passes/PassRegistry.h"
#include "passes/PipelineBuilder.h"
#include "passes/PreservedAnalyses.h"
#include "passes/desugar_passes.h"

using namespace nanopass;

// ─────────────────────────────────────────────────────────────────────────────
// Test harness — minimal, no framework dependency
// ─────────────────────────────────────────────────────────────────────────────

static int  g_tests  = 0;
static int  g_passed = 0;
static int  g_failed = 0;

#define ASSERT(cond)                                                    \
    do {                                                                \
        ++g_tests;                                                      \
        if (!(cond)) {                                                  \
            ++g_failed;                                                 \
            std::cerr << "FAIL [" << __FILE__ << ":" << __LINE__       \
                      << "] " << #cond << "\n";                        \
        } else {                                                        \
            ++g_passed;                                                 \
        }                                                               \
    } while (false)

#define ASSERT_EQ(a, b)                                                 \
    do {                                                                \
        ++g_tests;                                                      \
        if (!((a) == (b))) {                                            \
            ++g_failed;                                                 \
            std::cerr << "FAIL [" << __FILE__ << ":" << __LINE__       \
                      << "] " << #a << " == " << #b                    \
                      << "  (" << (a) << " != " << (b) << ")\n";      \
        } else {                                                        \
            ++g_passed;                                                 \
        }                                                               \
    } while (false)

// ─────────────────────────────────────────────────────────────────────────────
// AST builder helpers
// ─────────────────────────────────────────────────────────────────────────────

// Make a VarDeclStmt: int <name> = <intval>;
static std::unique_ptr<VarDeclStmt> makeVarDecl(
        const std::string &name, long long val = 0) {
    return std::make_unique<VarDeclStmt>(
        name, LangType::Int(32),
        std::make_unique<NumberExpr>(val));
}

// Make an ExprStmt wrapping x++ (UnaryExpr with op "++")
static std::unique_ptr<ExprStmt> makeIncrement(const std::string &var) {
    return std::make_unique<ExprStmt>(
        std::make_unique<UnaryExpr>("++",
            std::make_unique<VariableExpr>(var)));
}

// Make an ExprStmt wrapping x += val
static std::unique_ptr<ExprStmt> makePlusAssign(
        const std::string &var, long long val) {
    return std::make_unique<ExprStmt>(
        std::make_unique<BinaryExpr>(
            "+=",
            std::make_unique<VariableExpr>(var),
            std::make_unique<NumberExpr>(val)));
}

// Make a simple ForStmt: for (;; ) { body }
// init = nullptr, cond = nullptr (means true), increment = nullptr
static std::unique_ptr<ForStmt> makeInfiniteFor(
        std::unique_ptr<Stmt> body) {
    return std::make_unique<ForStmt>(
        nullptr,  // init
        nullptr,  // condition  (becomes NumberExpr(1))
        nullptr,  // increment
        std::move(body));
}

// Make a function with an empty body
static std::unique_ptr<FunctionStmt> makeFunction(
        const std::string &name,
        std::vector<std::unique_ptr<Stmt>> bodyStmts) {
    auto body = std::make_unique<BlockStmt>();
    for (auto &s : bodyStmts)
        body->stmts.push_back(std::move(s));

    auto fn = std::make_unique<FunctionStmt>(
        name,
        LangType::Int(32),
        std::vector<std::pair<std::string, LangType>>{},
        std::move(body));
    return fn;
}

// Count ForStmt nodes in a program (to verify desugaring)
static int countForStmts(const Program &program);
static int countForInStmt(const Stmt *s) {
    if (!s) return 0;
    if (dynamic_cast<const ForStmt *>(s)) return 1;
    int n = 0;
    if (auto *fn = dynamic_cast<const FunctionStmt *>(s))
        return countForInStmt(fn->body.get());
    if (auto *blk = dynamic_cast<const BlockStmt *>(s)) {
        for (const auto &c : blk->stmts) n += countForInStmt(c.get());
        return n;
    }
    if (auto *ws = dynamic_cast<const WhileStmt *>(s))
        return countForInStmt(ws->body.get());
    if (auto *ifs = dynamic_cast<const IfStmt *>(s)) {
        n += countForInStmt(ifs->thenBranch.get());
        if (ifs->elseBranch) n += countForInStmt(ifs->elseBranch.get());
        return n;
    }
    return 0;
}
static int countForStmts(const Program &program) {
    int n = 0;
    for (const auto &s : program) n += countForInStmt(s.get());
    return n;
}

// Count WhileStmt nodes in a program
static int countWhileInStmt(const Stmt *s) {
    if (!s) return 0;
    int n = 0;
    if (dynamic_cast<const WhileStmt *>(s)) ++n;
    if (auto *fn = dynamic_cast<const FunctionStmt *>(s))
        return n + countWhileInStmt(fn->body.get());
    if (auto *blk = dynamic_cast<const BlockStmt *>(s)) {
        for (const auto &c : blk->stmts) n += countWhileInStmt(c.get());
        return n;
    }
    if (auto *ws = dynamic_cast<const WhileStmt *>(s))
        return n + countWhileInStmt(ws->body.get());
    if (auto *ifs = dynamic_cast<const IfStmt *>(s)) {
        n += countWhileInStmt(ifs->thenBranch.get());
        if (ifs->elseBranch) n += countWhileInStmt(ifs->elseBranch.get());
    }
    return n;
}
static int countWhileStmts(const Program &program) {
    int n = 0;
    for (const auto &s : program) n += countWhileInStmt(s.get());
    return n;
}

// Build a default PassContext (debug off, no stop-after)
static std::pair<AnalysisManager, nanopass::CompilerPassOptions>
makeTestContext() {
    nanopass::PipelineBuilder::initialise();
    nanopass::CompilerPassOptions opts;
    return {AnalysisManager{}, opts};
}

// ─────────────────────────────────────────────────────────────────────────────
// Sentinel pass — records invocation order and reports changed=false
// ─────────────────────────────────────────────────────────────────────────────

struct OrderRecorder : Pass {
    std::string id_;
    std::vector<std::string> &log_;

    OrderRecorder(std::string id, std::vector<std::string> &log)
        : id_(std::move(id)), log_(log) {}

    PassInfo info() const noexcept override {
        return {id_, id_, "test-only order recorder", "test",
                IRUnitKind::Program};
    }
    PassResult run(Program & /*p*/, PassContext & /*ctx*/) override {
        log_.push_back(id_);
        return PassResult::noChange();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Test 1 — Pass execution order
// ─────────────────────────────────────────────────────────────────────────────
static void test_passExecutionOrder() {
    std::vector<std::string> log;
    PassManager pm("test");
    pm.addPass(std::make_unique<OrderRecorder>("A", log));
    pm.addPass(std::make_unique<OrderRecorder>("B", log));
    pm.addPass(std::make_unique<OrderRecorder>("C", log));

    Program program;
    auto [am, opts] = makeTestContext();
    PassContext ctx(am, opts, std::cerr);

    pm.run(program, ctx);

    ASSERT_EQ(log.size(), static_cast<std::size_t>(3));
    ASSERT(log[0] == "A");
    ASSERT(log[1] == "B");
    ASSERT(log[2] == "C");
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 2 — changed flag: DesugarFor on program with no ForStmt → false
// ─────────────────────────────────────────────────────────────────────────────
static void test_changedFlagFalseWhenNoFor() {
    Program program;
    program.push_back(makeVarDecl("x", 1));
    program.push_back(makeVarDecl("y", 2));

    DesugarForPass pass;
    auto [am, opts] = makeTestContext();
    PassContext ctx(am, opts, std::cerr);
    PassResult result = pass.run(program, ctx);

    ASSERT(!result.changed);
    ASSERT_EQ(result.stats.getInt("rewrites"), 0LL);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 3 — changed flag: DesugarFor on program with ForStmt → true
// ─────────────────────────────────────────────────────────────────────────────
static void test_changedFlagTrueWhenFor() {
    Program program;
    program.push_back(makeInfiniteFor(makeVarDecl("tmp")));

    DesugarForPass pass;
    auto [am, opts] = makeTestContext();
    PassContext ctx(am, opts, std::cerr);
    PassResult result = pass.run(program, ctx);

    ASSERT(result.changed);
    ASSERT_EQ(result.stats.getInt("rewrites"), 1LL);
    // No ForStmt should remain
    ASSERT_EQ(countForStmts(program), 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 4 — rewrite counter: two for-loops → rewrites == 2
// ─────────────────────────────────────────────────────────────────────────────
static void test_rewriteCounterForTwoLoops() {
    Program program;
    program.push_back(makeInfiniteFor(makeVarDecl("a")));
    program.push_back(makeInfiniteFor(makeVarDecl("b")));

    DesugarForPass pass;
    auto [am, opts] = makeTestContext();
    PassContext ctx(am, opts, std::cerr);
    PassResult result = pass.run(program, ctx);

    ASSERT(result.changed);
    ASSERT_EQ(result.stats.getInt("rewrites"), 2LL);
    ASSERT_EQ(countForStmts(program), 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 5 — nested desugaring: for-loop INSIDE a function body
// This tests the Phase 1 FunctionStmt recursion fix.
// ─────────────────────────────────────────────────────────────────────────────
static void test_nestedDesugaringInsideFunction() {
    // Build: int main() { for (;;) { int x = 0; } }
    std::vector<std::unique_ptr<Stmt>> bodyStmts;
    bodyStmts.push_back(makeInfiniteFor(makeVarDecl("x")));

    Program program;
    program.push_back(makeFunction("main", std::move(bodyStmts)));

    // Before desugaring: 1 ForStmt inside the function
    ASSERT_EQ(countForStmts(program), 1);

    DesugarForPass pass;
    auto [am, opts] = makeTestContext();
    PassContext ctx(am, opts, std::cerr);
    PassResult result = pass.run(program, ctx);

    // After desugaring: 0 ForStmts, function body now has a while
    ASSERT(result.changed);
    ASSERT_EQ(result.stats.getInt("rewrites"), 1LL);
    ASSERT_EQ(countForStmts(program), 0);
    // The function body should now contain a BlockStmt with a WhileStmt
    ASSERT_EQ(countWhileStmts(program), 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 6 — DesugarIncDec: x++ → changed, no x++ → not changed
// ─────────────────────────────────────────────────────────────────────────────
static void test_desugarIncDecChangedFlag() {
    // Program with x++
    {
        Program prog;
        prog.push_back(makeVarDecl("x", 0));
        prog.push_back(makeIncrement("x"));

        DesugarIncDecPass pass;
        auto [am, opts] = makeTestContext();
        PassContext ctx(am, opts, std::cerr);
        PassResult r = pass.run(prog, ctx);
        ASSERT(r.changed);
        ASSERT_EQ(r.stats.getInt("rewrites"), 1LL);
    }
    // Program with no ++
    {
        Program prog;
        prog.push_back(makeVarDecl("y", 5));

        DesugarIncDecPass pass;
        auto [am, opts] = makeTestContext();
        PassContext ctx(am, opts, std::cerr);
        PassResult r = pass.run(prog, ctx);
        ASSERT(!r.changed);
        ASSERT_EQ(r.stats.getInt("rewrites"), 0LL);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 7 — DesugarIncDec inside function body (FunctionStmt fix)
// ─────────────────────────────────────────────────────────────────────────────
static void test_incDecInsideFunction() {
    std::vector<std::unique_ptr<Stmt>> bodyStmts;
    bodyStmts.push_back(makeVarDecl("i", 0));
    bodyStmts.push_back(makeIncrement("i"));  // i++

    Program program;
    program.push_back(makeFunction("main", std::move(bodyStmts)));

    DesugarIncDecPass pass;
    auto [am, opts] = makeTestContext();
    PassContext ctx(am, opts, std::cerr);
    PassResult r = pass.run(program, ctx);
    ASSERT(r.changed);
    ASSERT_EQ(r.stats.getInt("rewrites"), 1LL);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 8 — DesugarPlusAssign: a += 5 → changed; no += → not changed
// ─────────────────────────────────────────────────────────────────────────────
static void test_desugarPlusAssignChangedFlag() {
    {
        Program prog;
        prog.push_back(makeVarDecl("a", 0));
        prog.push_back(makePlusAssign("a", 5));

        DesugarPlusAssignPass pass;
        auto [am, opts] = makeTestContext();
        PassContext ctx(am, opts, std::cerr);
        PassResult r = pass.run(prog, ctx);
        ASSERT(r.changed);
        ASSERT_EQ(r.stats.getInt("rewrites"), 1LL);
    }
    {
        Program prog;
        prog.push_back(makeVarDecl("b", 3));

        DesugarPlusAssignPass pass;
        auto [am, opts] = makeTestContext();
        PassContext ctx(am, opts, std::cerr);
        PassResult r = pass.run(prog, ctx);
        ASSERT(!r.changed);
        ASSERT_EQ(r.stats.getInt("rewrites"), 0LL);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 9 — Analysis cache: NodeCountAnalysis computed once, hit on reuse
// ─────────────────────────────────────────────────────────────────────────────
static void test_analysisCacheHit() {
    nanopass::PipelineBuilder::initialise();
    AnalysisManager am;
    nanopass::PipelineBuilder::registerAnalysesWithManager(am);

    Program program;
    program.push_back(makeVarDecl("x", 1));
    program.push_back(makeVarDecl("y", 2));

    int accessCount = 0;
    int hitCount    = 0;
    am.setOnAnalysisAccess([&](std::string_view /*id*/, bool hit) {
        ++accessCount;
        if (hit) ++hitCount;
    });

    // First access — must compute (miss)
    const auto &r1 = am.get<NodeCountAnalysis>(program);
    ASSERT_EQ(accessCount, 1);
    ASSERT_EQ(hitCount,    0);
    ASSERT(r1.statements >= 2u); // at least the two VarDeclStmts

    // Second access — must hit cache
    const auto &r2 = am.get<NodeCountAnalysis>(program);
    ASSERT_EQ(accessCount, 2);
    ASSERT_EQ(hitCount,    1);
    ASSERT_EQ(r1.total, r2.total); // same result
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 10 — Analysis invalidation after mutating pass
// ─────────────────────────────────────────────────────────────────────────────
static void test_analysisInvalidatedAfterMutation() {
    nanopass::PipelineBuilder::initialise();
    AnalysisManager am;
    nanopass::PipelineBuilder::registerAnalysesWithManager(am);

    Program program;
    program.push_back(makeInfiniteFor(makeVarDecl("z")));

    // Prime cache
    (void)am.get<NodeCountAnalysis>(program);
    ASSERT(am.isCached<NodeCountAnalysis>());

    // Run a mutating pass through the PassManager
    CompilerPassOptions opts;
    PassContext ctx(am, opts, std::cerr);

    // DesugarFor will change the program and return PreservedAnalyses::none()
    PassManager pm("test");
    pm.addPass(std::make_unique<DesugarForPass>());
    bool changed = pm.run(program, ctx);

    ASSERT(changed);
    // NodeCountAnalysis should be invalidated
    ASSERT(!am.isCached<NodeCountAnalysis>());
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 11 — Read-only pass (no change) preserves all analyses
// ─────────────────────────────────────────────────────────────────────────────
static void test_readOnlyPassPreservesAnalyses() {
    nanopass::PipelineBuilder::initialise();
    AnalysisManager am;
    nanopass::PipelineBuilder::registerAnalysesWithManager(am);

    Program program;
    program.push_back(makeVarDecl("a", 1));

    // Prime the cache
    (void)am.get<NodeCountAnalysis>(program);
    ASSERT(am.isCached<NodeCountAnalysis>());
    const std::size_t cacheSize = am.cacheSize();

    // Run a pass on a program with no for-loops → changed=false
    // → preserved=all → nothing invalidated
    CompilerPassOptions opts;
    PassContext ctx(am, opts, std::cerr);

    PassManager pm("test");
    pm.addPass(std::make_unique<DesugarForPass>());
    bool changed = pm.run(program, ctx);

    ASSERT(!changed);
    // Cache should be intact
    ASSERT(am.isCached<NodeCountAnalysis>());
    ASSERT_EQ(am.cacheSize(), cacheSize);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 12 — Analysis values are correct
// ─────────────────────────────────────────────────────────────────────────────
static void test_analysisValues() {
    nanopass::PipelineBuilder::initialise();
    AnalysisManager am;
    nanopass::PipelineBuilder::registerAnalysesWithManager(am);

    // Program: int x = 1 + 2;
    auto initExpr = std::make_unique<BinaryExpr>(
        "+",
        std::make_unique<NumberExpr>(1LL),
        std::make_unique<NumberExpr>(2LL));
    auto decl = std::make_unique<VarDeclStmt>(
        "x", LangType::Int(32), std::move(initExpr));

    Program program;
    program.push_back(std::move(decl));

    const auto &nc = am.get<NodeCountAnalysis>(program);
    // 1 statement (VarDeclStmt) + 3 exprs (BinaryExpr, NumberExpr, NumberExpr) = 4 total
    ASSERT_EQ(nc.statements,   1u);
    ASSERT_EQ(nc.expressions,  3u);
    ASSERT_EQ(nc.total,        4u);
    ASSERT_EQ(nc.functions,    0u);

    const auto &depth = am.get<ASTDepthAnalysis>(program);
    // VarDeclStmt(d=1) → BinaryExpr(d=2) → NumberExpr(d=3)  → max = 3
    ASSERT(depth.maxDepth >= 3u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 13 — ASTDepthAnalysis on nested structure
// ─────────────────────────────────────────────────────────────────────────────
static void test_astDepthAnalysis() {
    nanopass::PipelineBuilder::initialise();
    AnalysisManager am;
    nanopass::PipelineBuilder::registerAnalysesWithManager(am);

    // Flat program: two VarDecls → depth should be small
    {
        Program prog;
        prog.push_back(makeVarDecl("a", 0));
        prog.push_back(makeVarDecl("b", 1));
        const auto &d = am.get<ASTDepthAnalysis>(prog);
        ASSERT(d.maxDepth >= 1u);
        ASSERT(d.maxDepth <= 5u);
        am.invalidateAll();
    }

    // Function containing a variable → deeper
    {
        Program prog;
        std::vector<std::unique_ptr<Stmt>> body;
        body.push_back(makeVarDecl("x", 42));
        prog.push_back(makeFunction("f", std::move(body)));
        const auto &d = am.get<ASTDepthAnalysis>(prog);
        // FunctionStmt(1) → BlockStmt(2) → VarDeclStmt(3) → NumberExpr(4)
        ASSERT(d.maxDepth >= 4u);
        am.invalidateAll();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 14 — PassManager runs each pass ONCE (not once per top-level stmt)
// ─────────────────────────────────────────────────────────────────────────────
static void test_passRunsOnceNotPerStatement() {
    std::vector<std::string> log;

    // A pass that records how many times run() is called
    struct CountingPass : Pass {
        std::vector<std::string> &log_;
        explicit CountingPass(std::vector<std::string> &l) : log_(l) {}
        PassInfo info() const noexcept override {
            return {"counter", "Counter", "test", "test", IRUnitKind::Program};
        }
        PassResult run(Program & /*p*/, PassContext & /*c*/) override {
            log_.push_back("run");
            return PassResult::noChange();
        }
    };

    // Program with 4 top-level statements
    Program program;
    program.push_back(makeVarDecl("a", 1));
    program.push_back(makeVarDecl("b", 2));
    program.push_back(makeVarDecl("c", 3));
    program.push_back(makeVarDecl("d", 4));

    PassManager pm("test");
    pm.addPass(std::make_unique<CountingPass>(log));

    auto [am, opts] = makeTestContext();
    PassContext ctx(am, opts, std::cerr);
    pm.run(program, ctx);

    // The pass must run exactly ONCE regardless of how many top-level stmts
    ASSERT_EQ(log.size(), static_cast<std::size_t>(1));
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 15 — Stop-after halts pipeline after named pass
// ─────────────────────────────────────────────────────────────────────────────
static void test_stopAfter() {
    std::vector<std::string> log;

    PassManager pm("test");
    pm.addPass(std::make_unique<OrderRecorder>("P1", log));
    pm.addPass(std::make_unique<OrderRecorder>("P2", log));
    pm.addPass(std::make_unique<OrderRecorder>("P3", log));

    Program program;
    AnalysisManager am;
    CompilerPassOptions opts;
    opts.stopAfter = "P2";
    PassContext ctx(am, opts, std::cerr);

    pm.run(program, ctx);

    // P1 and P2 ran, P3 did not
    ASSERT_EQ(log.size(), static_cast<std::size_t>(2));
    ASSERT(log[0] == "P1");
    ASSERT(log[1] == "P2");
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 16 — Disable-pass skips the named pass
// ─────────────────────────────────────────────────────────────────────────────
static void test_disablePass() {
    std::vector<std::string> log;

    PassManager pm("test");
    pm.addPass(std::make_unique<OrderRecorder>("A", log));
    pm.addPass(std::make_unique<OrderRecorder>("B", log));
    pm.addPass(std::make_unique<OrderRecorder>("C", log));

    Program program;
    AnalysisManager am;
    CompilerPassOptions opts;
    opts.disabledPasses.insert("B");
    PassContext ctx(am, opts, std::cerr);

    pm.run(program, ctx);

    // A and C ran; B was skipped
    ASSERT_EQ(log.size(), static_cast<std::size_t>(2));
    ASSERT(log[0] == "A");
    ASSERT(log[1] == "C");
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 17 — PassRegistry lookup
// ─────────────────────────────────────────────────────────────────────────────
static void test_passRegistryLookup() {
    nanopass::PipelineBuilder::initialise();
    const PassRegistry &reg = PassRegistry::get();

    ASSERT(reg.isKnownPass("desugar-for"));
    ASSERT(reg.isKnownPass("desugar-inc-dec"));
    ASSERT(reg.isKnownPass("desugar-plus-assign"));
    ASSERT(!reg.isKnownPass("nonexistent-pass"));

    const PassEntry *e = reg.lookupPass("desugar-for");
    ASSERT(e != nullptr);
    ASSERT(e->id       == "desugar-for");
    ASSERT(e->category == "desugaring");
    ASSERT(e->unit     == IRUnitKind::Program);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 18 — PassRegistry analysis lookup
// ─────────────────────────────────────────────────────────────────────────────
static void test_analysisRegistryLookup() {
    nanopass::PipelineBuilder::initialise();
    const PassRegistry &reg = PassRegistry::get();

    ASSERT(reg.isKnownAnalysis("ast-node-count"));
    ASSERT(reg.isKnownAnalysis("ast-depth"));
    ASSERT(!reg.isKnownAnalysis("dominators")); // not yet

    const AnalysisEntry *e = reg.lookupAnalysis("ast-node-count");
    ASSERT(e != nullptr);
    ASSERT(e->id       == "ast-node-count");
    ASSERT(e->category == "ast");
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 19 — PreservedAnalyses semantics
// ─────────────────────────────────────────────────────────────────────────────
static void test_preservedAnalysesSemantics() {
    {
        auto pa = PreservedAnalyses::all();
        ASSERT(pa.isAll());
        ASSERT(!pa.isNone());
        ASSERT(pa.isPreserved<NodeCountAnalysis>());
        ASSERT(pa.isPreserved<ASTDepthAnalysis>());
    }
    {
        auto pa = PreservedAnalyses::none();
        ASSERT(!pa.isAll());
        ASSERT(pa.isNone());
        ASSERT(!pa.isPreserved<NodeCountAnalysis>());
        ASSERT(!pa.isPreserved<ASTDepthAnalysis>());
    }
    {
        auto pa = PreservedAnalyses::none();
        pa.preserve<NodeCountAnalysis>();
        ASSERT(!pa.isAll());
        ASSERT(!pa.isNone());
        ASSERT(pa.isPreserved<NodeCountAnalysis>());
        ASSERT(!pa.isPreserved<ASTDepthAnalysis>());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 20 — Full desugar pipeline integration
// Source: function with for-loop, x++, and x += 3
// After full pipeline: no ForStmt, no ++, no +=
// ─────────────────────────────────────────────────────────────────────────────
static void test_fullDesugarPipeline() {
    nanopass::PipelineBuilder::initialise();

    // Build:
    //   int main() {
    //     int i = 0;
    //     for (;;) { i++; }
    //     i += 3;
    //   }
    std::vector<std::unique_ptr<Stmt>> body;
    body.push_back(makeVarDecl("i", 0));
    body.push_back(makeInfiniteFor(makeIncrement("i")));
    body.push_back(makePlusAssign("i", 3));

    Program program;
    program.push_back(makeFunction("main", std::move(body)));

    // Before: 1 ForStmt
    ASSERT_EQ(countForStmts(program), 1);

    AnalysisManager am;
    nanopass::PipelineBuilder::registerAnalysesWithManager(am);
    CompilerPassOptions opts;
    PassContext ctx(am, opts, std::cerr);

    PassManager pm = nanopass::PipelineBuilder::buildDesugarPipeline();
    bool anyChanged = pm.run(program, ctx);

    ASSERT(anyChanged);
    // No ForStmt remaining
    ASSERT_EQ(countForStmts(program), 0);
    // At least one WhileStmt was created by DesugarFor
    ASSERT(countWhileStmts(program) >= 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 21 — PipelineBuilder::buildDesugarPipeline has correct pass count
// ─────────────────────────────────────────────────────────────────────────────
static void test_pipelineBuilderPassCount() {
    nanopass::PipelineBuilder::initialise();
    PassManager pm = nanopass::PipelineBuilder::buildDesugarPipeline();
    ASSERT_EQ(pm.size(), static_cast<std::size_t>(3));
    ASSERT(pm.passAt(0).id() == "desugar-for");
    ASSERT(pm.passAt(1).id() == "desugar-inc-dec");
    ASSERT(pm.passAt(2).id() == "desugar-plus-assign");
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 22 — PassManager::run returns false when nothing changed
// ─────────────────────────────────────────────────────────────────────────────
static void test_runReturnsFalseWhenUnchanged() {
    nanopass::PipelineBuilder::initialise();
    // Plain program: no for, no ++, no +=
    Program program;
    program.push_back(makeVarDecl("x", 42));

    AnalysisManager am;
    CompilerPassOptions opts;
    PassContext ctx(am, opts, std::cerr);

    PassManager pm = nanopass::PipelineBuilder::buildDesugarPipeline();
    bool anyChanged = pm.run(program, ctx);

    ASSERT(!anyChanged);
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    test_passExecutionOrder();
    test_changedFlagFalseWhenNoFor();
    test_changedFlagTrueWhenFor();
    test_rewriteCounterForTwoLoops();
    test_nestedDesugaringInsideFunction();
    test_desugarIncDecChangedFlag();
    test_incDecInsideFunction();
    test_desugarPlusAssignChangedFlag();
    test_analysisCacheHit();
    test_analysisInvalidatedAfterMutation();
    test_readOnlyPassPreservesAnalyses();
    test_analysisValues();
    test_astDepthAnalysis();
    test_passRunsOnceNotPerStatement();
    test_stopAfter();
    test_disablePass();
    test_passRegistryLookup();
    test_analysisRegistryLookup();
    test_preservedAnalysesSemantics();
    test_fullDesugarPipeline();
    test_pipelineBuilderPassCount();
    test_runReturnsFalseWhenUnchanged();

    std::cout << "\n=== Pass Manager Tests ===\n"
              << "  total:  " << g_tests  << "\n"
              << "  passed: " << g_passed << "\n"
              << "  failed: " << g_failed << "\n";

    return g_failed > 0 ? 1 : 0;
}
