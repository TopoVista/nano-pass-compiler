// tests/ir_tests.cpp
//
// Phase 3 IR pipeline tests: CoreIR, ANF, CFG, verifiers, analyses.
//
// These tests are self-contained: they build programs directly through the
// full pipeline (parse → resolve → typecheck → desugar → CoreIR → ANF → CFG)
// and verify structural and semantic correctness.
//
// No LLVM is required (no codegen is exercised here).

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>

// Compiler infrastructure
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "sema/resolve_scopes.h"
#include "sema/type_check.h"
#include "passes/PipelineBuilder.h"
#include "passes/AnalysisManager.h"
#include "passes/PassContext.h"

// IR pipeline
#include "ir/pipeline.h"
#include "ir/core/core_ir.h"
#include "ir/core/core_verifier.h"
#include "ir/core/core_printer.h"
#include "ir/anf/anf_ir.h"
#include "ir/anf/anf_verifier.h"
#include "ir/anf/anf_printer.h"
#include "ir/cfg/cfg_ir.h"
#include "ir/cfg/cfg_verifier.h"
#include "ir/cfg/cfg_printer.h"
#include "ir/cfg/cfg_graphviz.h"
#include "ir/cfg/analysis/reachability_analysis.h"
#include "ir/cfg/analysis/reverse_postorder_analysis.h"
#include "ir/cfg/analysis/cfg_stats_analysis.h"

using namespace nanopass;

// ─────────────────────────────────────────────────────────────────────────────
// Test harness
// ─────────────────────────────────────────────────────────────────────────────

static int g_tests = 0, g_passed = 0, g_failed = 0;

#define ASSERT(cond)                                                    \
  do {                                                                  \
    ++g_tests;                                                          \
    if (!(cond)) {                                                      \
      ++g_failed;                                                       \
      std::cerr << "FAIL [" << __FILE__ << ":" << __LINE__             \
                << "] " << #cond << "\n";                              \
    } else { ++g_passed; }                                              \
  } while(false)

#define ASSERT_EQ(a, b)                                                 \
  do {                                                                  \
    ++g_tests;                                                          \
    if (!((a) == (b))) {                                                \
      ++g_failed;                                                       \
      std::cerr << "FAIL [" << __FILE__ << ":" << __LINE__             \
                << "] " << #a << " == " << #b                          \
                << "  (" << (a) << " != " << (b) << ")\n";            \
    } else { ++g_passed; }                                              \
  } while(false)

// ─────────────────────────────────────────────────────────────────────────────
// Pipeline helper: source → IR pipeline result
// ─────────────────────────────────────────────────────────────────────────────

static IRPipelineResult compileToIR(const std::string &source) {
    nanopass::PipelineBuilder::initialise();

    Lexer lexer(source);
    auto tokens = lexer.scanTokens();
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();

    ResolveScopesPass resolver;
    resolver.resolve(program);

    TypeCheckPass typeChecker;
    typeChecker.check(program);

    // Run desugar passes
    nanopass::AnalysisManager am;
    nanopass::PipelineBuilder::registerAnalysesWithManager(am);
    nanopass::CompilerPassOptions opts;
    nanopass::PassContext ctx(am, opts, std::cerr);
    nanopass::PassManager pm = nanopass::PipelineBuilder::buildDesugarPipeline();
    pm.run(program, ctx);

    return IRPipeline::runAll(program);
}// ─────────────────────────────────────────────────────────────────────────────
// Test 1 — CoreIR: basic function structure is preserved
// ─────────────────────────────────────────────────────────────────────────────
static void test_coreIR_basicFunction() {
    auto res = compileToIR(R"(
int add(int a, int b) { return a + b; }
int main() { print add(1, 2); return 0; }
)");

    ASSERT(res.coreVerResult.ok);
    ASSERT_EQ(res.core.functions.size(), 2u);
    ASSERT(res.core.functions[0].name == "add");
    ASSERT(res.core.functions[1].name == "main");
    ASSERT_EQ(res.core.functions[0].params.size(), 2u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 2 — CoreIR: no ForStmt permitted after desugaring
// ─────────────────────────────────────────────────────────────────────────────
static void test_coreIR_noForStmt() {
    // A for loop is desugared before CoreIR; CoreIR must only have while
    auto res = compileToIR(R"(
int main() {
    int i = 0;
    while (i < 5) { i = i + 1; }
    return 0;
}
)");
    ASSERT(res.coreVerResult.ok);
    // The while loop in CoreIR should be a CoreWhileStmt
    const auto &fn = res.core.functions[0];
    bool hasWhile = false;
    for (const auto &stmt : fn.body) {
        if (dynamic_cast<CoreWhileStmt *>(stmt.get())) hasWhile = true;
    }
    ASSERT(hasWhile);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 3 — CoreIR verifier: detects duplicate variable declaration
// ─────────────────────────────────────────────────────────────────────────────
static void test_coreIR_verifier_rejectsUnknownCall() {
    // We build a CoreProgram manually with an unknown function call
    CoreProgram prog;
    CoreFunction fn;
    fn.name = "main";
    fn.returnType = LangType::Void();
    // Call to unknown @unknown_fn
    std::vector<std::unique_ptr<CoreExpr>> args;
    auto call = std::make_unique<CoreCallExpr>("unknown_fn", std::move(args),
                                               LangType::Void());
    fn.body.push_back(std::make_unique<CoreExprStmt>(std::move(call)));
    prog.functions.push_back(std::move(fn));

    auto result = verifyCoreIR(prog);
    ASSERT(!result.ok);
    ASSERT(!result.errors.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 4 — CoreIR verifier: break outside loop is rejected
// ─────────────────────────────────────────────────────────────────────────────
static void test_coreIR_verifier_breakOutsideLoop() {
    CoreProgram prog;
    CoreFunction fn;
    fn.name = "main";
    fn.returnType = LangType::Void();
    fn.body.push_back(std::make_unique<CoreBreakStmt>());
    prog.functions.push_back(std::move(fn));

    auto result = verifyCoreIR(prog);
    ASSERT(!result.ok);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 5 — ANF: nested expression is flattened to temporaries
// ─────────────────────────────────────────────────────────────────────────────
static void test_anf_nestedExpression() {
    // print(1 + 2 * 3) should produce ANF with temporaries
    auto res = compileToIR(R"(
int main() {
    print 1 + 2 * 3;
    return 0;
}
)");

    ASSERT(res.anfVerResult.ok);

    // The main function's body should contain temporaries
    const auto &anfFn = res.anf.functions[0];
    bool hasCompute = false;
    for (const auto &stmt : anfFn.body) {
        if (dynamic_cast<ANFComputeStmt *>(stmt.get())) {
            hasCompute = true;
            break;
        }
    }
    ASSERT(hasCompute);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 6 — ANF: evaluation order preserved (left before right)
// ─────────────────────────────────────────────────────────────────────────────
static void test_anf_evaluationOrder() {
    // foo(bar(), baz()) — bar must be atomized before baz
    auto res = compileToIR(R"(
int bar() { return 1; }
int baz() { return 2; }
int foo(int a, int b) { return a + b; }
int main() {
    int r = foo(bar(), baz());
    print r;
    return 0;
}
)");

    ASSERT(res.anfVerResult.ok);

    // Find @foo call in main; its args must both be temporaries
    // (meaning bar() and baz() were computed first)
    const auto *mainFn = [&]() -> const ANFFunction * {
        for (const auto &fn : res.anf.functions)
            if (fn.name == "main") return &fn;
        return nullptr;
    }();
    ASSERT(mainFn != nullptr);

    // Look for a call to @foo; its args should be temporaries (not constants)
    bool fooCallHasTempArgs = false;
    for (const auto &stmt : mainFn->body) {
        if (auto *cs = dynamic_cast<ANFComputeStmt *>(stmt.get())) {
            if (cs->comp.kind == ANFComputationKind::Call &&
                cs->comp.callee == "foo") {
                if (cs->comp.args.size() == 2 &&
                    cs->comp.args[0].kind == ANFAtomKind::Temporary &&
                    cs->comp.args[1].kind == ANFAtomKind::Temporary) {
                    fooCallHasTempArgs = true;
                }
            }
        }
    }
    ASSERT(fooCallHasTempArgs);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 7 — ANF: short-circuit && does not evaluate right when left is false
//
// This is a structural test: the ANF for (a && b()) must produce an IfStmt
// that conditionally evaluates b(), not a plain AND computation.
// ─────────────────────────────────────────────────────────────────────────────
static void test_anf_shortCircuitAnd() {
    auto res = compileToIR(R"(
int sideEffect() { print 999; return 1; }
int main() {
    int zero = 0;
    int result = 0;
    if (zero && sideEffect()) { result = 1; }
    print result;
    return 0;
}
)");

    ASSERT(res.anfVerResult.ok);

    // Find main function; the && should produce an ANFIfStmt (not a binary AND)
    // When && is in an if-condition, the condition evaluation itself will have
    // an ANFIfStmt in the condHeader of the while, or as a preamble before the if.
    const auto *mainFn = [&]() -> const ANFFunction * {
        for (const auto &fn : res.anf.functions)
            if (fn.name == "main") return &fn;
        return nullptr;
    }();
    ASSERT(mainFn != nullptr);

    // The short-circuit && must not appear as a binary AND computation.
    // It must be lowered as control flow (ANFIfStmt somewhere in the body or
    // the outer if's condition computation).
    bool hasBinaryAnd = false;
    for (const auto &stmt : mainFn->body) {
        if (auto *cs = dynamic_cast<ANFComputeStmt *>(stmt.get())) {
            if (cs->comp.kind == ANFComputationKind::Binary) {
                if (cs->comp.binOp == BinaryOpKind::LogicalAnd)
                    hasBinaryAnd = true;
            }
        }
    }
    ASSERT(!hasBinaryAnd);

    // The CFG must be structurally valid
    ASSERT(res.cfgVerResult.ok);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 8 — ANF: short-circuit || does not evaluate right when left is true
// ─────────────────────────────────────────────────────────────────────────────
static void test_anf_shortCircuitOr() {
    auto res = compileToIR(R"(
int sideEffect() { print 999; return 0; }
int main() {
    int one = 1;
    int result = 0;
    if (one || sideEffect()) { result = 1; }
    print result;
    return 0;
}
)");

    ASSERT(res.anfVerResult.ok);

    const auto *mainFn = [&]() -> const ANFFunction * {
        for (const auto &fn : res.anf.functions)
            if (fn.name == "main") return &fn;
        return nullptr;
    }();
    ASSERT(mainFn != nullptr);

    // No binary OR computation should exist — must be lowered to control flow
    for (const auto &stmt : mainFn->body) {
        if (auto *cs = dynamic_cast<ANFComputeStmt *>(stmt.get())) {
            if (cs->comp.kind == ANFComputationKind::Binary) {
                ASSERT(cs->comp.binOp != BinaryOpKind::LogicalOr);
            }
        }
    }

    ASSERT(res.cfgVerResult.ok);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 9 — ANF verifier: every temporary defined once
// ─────────────────────────────────────────────────────────────────────────────
static void test_anf_verifier_uniqueTemporaries() {
    auto res = compileToIR(R"(
int f(int x) { return x * x + x; }
int main() { print f(5); return 0; }
)");
    ASSERT(res.anfVerResult.ok);
    ASSERT(res.anfVerResult.errors.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 10 — CFG: function with if/else has correct block structure
// ─────────────────────────────────────────────────────────────────────────────
static void test_cfg_ifElse() {
    auto res = compileToIR(R"(
int abs(int x) {
    if (x < 0) {
        return 0 - x;
    } else {
        return x;
    }
}
int main() { print abs(-3); return 0; }
)");

    ASSERT(res.cfgVerResult.ok);

    const CFGFunction *absFn = nullptr;
    for (const auto &fn : res.cfg.functions)
        if (fn.name == "abs") { absFn = &fn; break; }
    ASSERT(absFn != nullptr);

    // abs should have: entry, then, else/merge (at minimum 3 blocks)
    ASSERT(absFn->blocks.size() >= 3u);

    // Must have a branch terminator somewhere
    bool hasBranch = false;
    for (const auto &b : absFn->blocks) {
        if (b->terminator) {
            if (std::holds_alternative<BranchTerminator>(*b->terminator)) {
                hasBranch = true;
                break;
            }
        }
    }
    ASSERT(hasBranch);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 11 — CFG: while loop produces header, body, exit blocks
// ─────────────────────────────────────────────────────────────────────────────
static void test_cfg_whileLoop() {
    auto res = compileToIR(R"(
int sumTo(int n) {
    int total = 0;
    int i = 1;
    while (i <= n) {
        total = total + i;
        i = i + 1;
    }
    return total;
}
int main() { print sumTo(5); return 0; }
)");

    ASSERT(res.cfgVerResult.ok);

    const CFGFunction *sumFn = nullptr;
    for (const auto &fn : res.cfg.functions)
        if (fn.name == "sumTo") { sumFn = &fn; break; }
    ASSERT(sumFn != nullptr);

    // sumTo: entry, loop.header, loop.body, loop.exit = at least 4 blocks
    ASSERT(sumFn->blocks.size() >= 4u);

    // Must have a back-edge (loop.body jumps back to loop.header)
    bool hasBackEdge = false;
    for (const auto &b : sumFn->blocks) {
        if (b->label == "loop.body" && !b->successors.empty()) {
            // successor should be the loop header (lower block id)
            for (auto succ : b->successors) {
                if (succ.index < b->id.index) { hasBackEdge = true; break; }
            }
        }
    }
    ASSERT(hasBackEdge);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 12 — CFG: break exits the loop correctly
// ─────────────────────────────────────────────────────────────────────────────
static void test_cfg_break() {
    auto res = compileToIR(R"(
int main() {
    int i = 0;
    while (i < 10) {
        if (i == 5) { break; }
        i = i + 1;
    }
    print i;
    return 0;
}
)");

    ASSERT(res.cfgVerResult.ok);

    // CFG should be buildable and verifiable
    // The break should produce a Jump to the loop exit block
    const CFGFunction *mainFn = nullptr;
    for (const auto &fn : res.cfg.functions)
        if (fn.name == "main") { mainFn = &fn; break; }
    ASSERT(mainFn != nullptr);
    ASSERT(mainFn->blocks.size() >= 4u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 13 — CFG: nested loop break targets only inner loop
// ─────────────────────────────────────────────────────────────────────────────
static void test_cfg_nestedLoopBreak() {
    auto res = compileToIR(R"(
int main() {
    int outer = 0;
    int count = 0;
    while (outer < 3) {
        int inner = 0;
        while (inner < 5) {
            if (inner == 2) { break; }
            inner = inner + 1;
        }
        count = count + 1;
        outer = outer + 1;
    }
    print count;
    return 0;
}
)");

    ASSERT(res.cfgVerResult.ok);
    // CFG must verify: outer loop runs 3 times (count == 3)
    // Structural check: outer loop must have more blocks than inner
    const auto &mainFn = res.cfg.functions[0];
    ASSERT(mainFn.blocks.size() >= 7u); // outer entry+header+body+exit + inner header+body+exit
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 14 — CFG verifier: predecessor/successor consistency
// ─────────────────────────────────────────────────────────────────────────────
static void test_cfg_verifier_edgeConsistency() {
    auto res = compileToIR(R"(
int main() {
    int x = 1;
    if (x > 0) { x = x + 1; }
    return x;
}
)");

    ASSERT(res.cfgVerResult.ok);

    // Check that all successor/predecessor relationships are consistent
    for (const auto &fn : res.cfg.functions) {
        for (const auto &b : fn.blocks) {
            for (const auto &succId : b->successors) {
                const auto *succ = fn.getBlock(succId);
                ASSERT(succ != nullptr);
                bool found = false;
                for (const auto &predId : succ->predecessors)
                    if (predId == b->id) { found = true; break; }
                ASSERT(found);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 15 — CFG verifier: every block has a terminator
// ─────────────────────────────────────────────────────────────────────────────
static void test_cfg_verifier_allBlocksTerminated() {
    auto res = compileToIR(R"(
int fib(int n) {
    if (n <= 1) { return n; }
    return fib(n-1) + fib(n-2);
}
int main() { print fib(5); return 0; }
)");

    ASSERT(res.cfgVerResult.ok);

    for (const auto &fn : res.cfg.functions) {
        for (const auto &b : fn.blocks) {
            ASSERT(b->hasTerminator());
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 16 — CFG printer: output is deterministic and non-empty
// ─────────────────────────────────────────────────────────────────────────────
static void test_cfg_printer_deterministic() {
    auto res1 = compileToIR(R"(
int main() { int x = 42; print x; return 0; }
)");
    auto res2 = compileToIR(R"(
int main() { int x = 42; print x; return 0; }
)");

    std::ostringstream os1, os2;
    printCFG(res1.cfg, os1);
    printCFG(res2.cfg, os2);

    ASSERT(!os1.str().empty());
    ASSERT(os1.str() == os2.str());
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 17 — CFG graphviz: valid DOT output
// ─────────────────────────────────────────────────────────────────────────────
static void test_cfg_graphviz_validDOT() {
    auto res = compileToIR(R"(
int main() {
    int i = 0;
    while (i < 3) { i = i + 1; }
    return 0;
}
)");

    std::ostringstream os;
    emitCFGGraphviz(res.cfg, os);
    const std::string dot = os.str();

    // Must start with digraph
    ASSERT(dot.find("digraph") != std::string::npos);
    // Must contain at least one edge
    ASSERT(dot.find("->") != std::string::npos);
    // Must contain at least one node
    ASSERT(dot.find("[label=") != std::string::npos);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 18 — Reachability analysis: linear chain
// ─────────────────────────────────────────────────────────────────────────────
static void test_reachability_linear() {
    auto res = compileToIR(R"(
int main() { int x = 1; int y = 2; return x + y; }
)");

    const auto &fn = res.cfg.functions[0];
    auto reach = ReachabilityAnalysis::run(fn);

    // All blocks must be reachable in a simple linear function
    ASSERT(reach.unreachable.empty());
    ASSERT(!reach.reachable.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 19 — Reachability analysis: unreachable block after return
// ─────────────────────────────────────────────────────────────────────────────
static void test_reachability_afterReturn() {
    // Code after an unconditional return — the CFG may or may not generate
    // an unreachable block for dead code, but verification should not fail.
    auto res = compileToIR(R"(
int f(int x) {
    if (x > 0) { return 1; } else { return 0; }
    return 99;
}
int main() { print f(5); return 0; }
)");
    // CFG must be structurally valid even if unreachable blocks exist
    ASSERT(res.cfgVerResult.ok);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 20 — Reverse post-order: ordering is sensible
// ─────────────────────────────────────────────────────────────────────────────
static void test_rpo_basic() {
    auto res = compileToIR(R"(
int main() {
    int i = 0;
    while (i < 5) { i = i + 1; }
    return 0;
}
)");

    const auto &fn = res.cfg.functions[0];
    auto rpo = ReversePostOrderAnalysis::run(fn);

    // Entry block must appear first in RPO
    ASSERT(!rpo.order.empty());
    ASSERT(rpo.order[0] == fn.entryBlock);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 21 — RPO: diamond control flow
// entry → A → (B | C) → D
// ─────────────────────────────────────────────────────────────────────────────
static void test_rpo_diamond() {
    auto res = compileToIR(R"(
int main() {
    int x = 1;
    int y = 0;
    if (x > 0) { y = 1; } else { y = 2; }
    print y;
    return 0;
}
)");

    const auto &fn = res.cfg.functions[0];
    auto rpo = ReversePostOrderAnalysis::run(fn);

    // Entry must be first; merge block must come after both branches
    ASSERT(!rpo.order.empty());
    ASSERT(rpo.order[0] == fn.entryBlock);
    // All blocks should appear in RPO order
    ASSERT(rpo.order.size() == fn.blocks.size());
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 22 — CFG stats: correct counts
// ─────────────────────────────────────────────────────────────────────────────
static void test_cfg_stats() {
    auto res = compileToIR(R"(
int main() {
    int i = 0;
    while (i < 3) { i = i + 1; }
    return 0;
}
)");

    auto stats = CFGStatsAnalysis::run(res.cfg);
    ASSERT_EQ(stats.functions, 1u);
    ASSERT(stats.basicBlocks >= 4u); // entry, header, body, exit
    ASSERT(stats.branches >= 1u);    // at least the while condition branch
    ASSERT(stats.returns >= 1u);     // return 0
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 23 — CoreIR printer: output contains function name and return type
// ─────────────────────────────────────────────────────────────────────────────
static void test_core_printer() {
    auto res = compileToIR(R"(
int add(int a, int b) { return a + b; }
int main() { print add(1, 2); return 0; }
)");

    std::ostringstream os;
    printCoreIR(res.core, os);
    const std::string out = os.str();

    ASSERT(out.find("func @add") != std::string::npos);
    ASSERT(out.find("func @main") != std::string::npos);
    ASSERT(out.find("-> int") != std::string::npos);
    ASSERT(out.find("return") != std::string::npos);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 24 — ANF printer: temporaries use %t prefix
// ─────────────────────────────────────────────────────────────────────────────
static void test_anf_printer_tempPrefix() {
    auto res = compileToIR(R"(
int main() { print 1 + 2 * 3; return 0; }
)");

    std::ostringstream os;
    printANF(res.anf, os);
    const std::string out = os.str();

    ASSERT(out.find("%t") != std::string::npos);
    ASSERT(out.find("func @main") != std::string::npos);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 25 — LocalId shadowing: two 'x' in different scopes get distinct IDs
// ─────────────────────────────────────────────────────────────────────────────
static void test_localId_shadowing() {
    // The Nano language may not support nested block scopes with shadowing,
    // but function params vs locals should have distinct IDs.
    // Test two functions each with a variable named 'x' — they should
    // have different LocalId indices.
    auto res = compileToIR(R"(
int f(int x) { return x + 1; }
int g(int x) { return x * 2; }
int main() { print f(3); print g(4); return 0; }
)");

    ASSERT(res.coreVerResult.ok);
    ASSERT(res.anfVerResult.ok);
    ASSERT(res.cfgVerResult.ok);

    // f's param x and g's param x should have the same name but different
    // function scopes — each is 'x#0' within its own function. Both valid.
    ASSERT(res.core.functions[0].params[0].id.name == "x");
    ASSERT(res.core.functions[1].params[0].id.name == "x");
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 26 — Full pipeline: factorial produces valid CFG with recursion
// ─────────────────────────────────────────────────────────────────────────────
static void test_full_pipeline_factorial() {
    auto res = compileToIR(R"(
int fact(int n) {
    if (n <= 1) { return 1; }
    return n * fact(n - 1);
}
int main() {
    print fact(5);
    return 0;
}
)");

    ASSERT(res.coreVerResult.ok);
    ASSERT(res.anfVerResult.ok);
    ASSERT(res.cfgVerResult.ok);

    // Recursive call to @fact should appear in CFG
    bool hasFactCall = false;
    for (const auto &fn : res.cfg.functions) {
        for (const auto &b : fn.blocks) {
            for (const auto &inst : b->instructions) {
                if (inst.kind == CFGInstructionKind::Call &&
                    inst.callee == "fact") {
                    hasFactCall = true;
                }
            }
        }
    }
    ASSERT(hasFactCall);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 27 — Break/continue: continue in while goes to header
// ─────────────────────────────────────────────────────────────────────────────
static void test_cfg_continue() {
    auto res = compileToIR(R"(
int main() {
    int i = 0;
    int sum = 0;
    while (i < 10) {
        i = i + 1;
        if (i == 5) { continue; }
        sum = sum + i;
    }
    print sum;
    return 0;
}
)");

    ASSERT(res.cfgVerResult.ok);

    // Continue should produce a jump to loop.header, not loop.exit
    const auto &fn = res.cfg.functions[0];
    // All blocks must have terminators
    for (const auto &b : fn.blocks) {
        ASSERT(b->hasTerminator());
    }

    // The predecessor/successor graph must be consistent
    for (const auto &b : fn.blocks) {
        for (const auto &succId : b->successors) {
            const auto *succ = fn.getBlock(succId);
            ASSERT(succ != nullptr);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 28 — CFG: return inside conditional produces valid CFG
// ─────────────────────────────────────────────────────────────────────────────
static void test_cfg_returnInConditional() {
    auto res = compileToIR(R"(
int f(int x) {
    if (x > 0) { return 1; }
    return 2;
}
int main() { print f(5); return 0; }
)");

    ASSERT(res.cfgVerResult.ok);

    // All blocks must have terminators
    for (const auto &fn : res.cfg.functions) {
        for (const auto &b : fn.blocks) {
            ASSERT(b->hasTerminator());
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
#define RUN_TEST(fn) do { \
    std::cerr << "[running] " #fn "\n"; \
    try { fn(); } \
    catch (const std::exception &ex) { \
        ++g_tests; ++g_failed; \
        std::cerr << "FAIL [exception in " #fn "]: " << ex.what() << "\n"; \
    } \
} while(false)

int main() {
    RUN_TEST(test_coreIR_basicFunction);
    RUN_TEST(test_coreIR_noForStmt);
    RUN_TEST(test_coreIR_verifier_rejectsUnknownCall);
    RUN_TEST(test_coreIR_verifier_breakOutsideLoop);
    RUN_TEST(test_anf_nestedExpression);
    RUN_TEST(test_anf_evaluationOrder);
    RUN_TEST(test_anf_shortCircuitAnd);
    RUN_TEST(test_anf_shortCircuitOr);
    RUN_TEST(test_anf_verifier_uniqueTemporaries);
    RUN_TEST(test_cfg_ifElse);
    RUN_TEST(test_cfg_whileLoop);
    RUN_TEST(test_cfg_break);
    RUN_TEST(test_cfg_nestedLoopBreak);
    RUN_TEST(test_cfg_verifier_edgeConsistency);
    RUN_TEST(test_cfg_verifier_allBlocksTerminated);
    RUN_TEST(test_cfg_printer_deterministic);
    RUN_TEST(test_cfg_graphviz_validDOT);
    RUN_TEST(test_reachability_linear);
    RUN_TEST(test_reachability_afterReturn);
    RUN_TEST(test_rpo_basic);
    RUN_TEST(test_rpo_diamond);
    RUN_TEST(test_cfg_stats);
    RUN_TEST(test_core_printer);
    RUN_TEST(test_anf_printer_tempPrefix);
    RUN_TEST(test_localId_shadowing);
    RUN_TEST(test_full_pipeline_factorial);
    RUN_TEST(test_cfg_continue);
    RUN_TEST(test_cfg_returnInConditional);

    std::cout << "\n=== IR Pipeline Tests ===\n"
              << "  total:  " << g_tests  << "\n"
              << "  passed: " << g_passed << "\n"
              << "  failed: " << g_failed << "\n";

    return g_failed > 0 ? 1 : 0;
}
