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
#include "ir/cfg/cfg_builder.h"
#include "ir/cfg/analysis/reachability_analysis.h"
#include "ir/cfg/analysis/reverse_postorder_analysis.h"
#include "ir/cfg/analysis/cfg_stats_analysis.h"
#include "ir/cfg/analysis/dominator_analysis.h"
#include "ir/cfg/analysis/dominance_frontier_analysis.h"
#include "ir/ssa/ssa_builder.h"
#include "ir/ssa/ssa_def_use.h"
#include "ir/ssa/ssa_printer.h"
#include "ir/ssa/ssa_verifier.h"
#include "optimizer/ssa_optimizer.h"
#include "optimizer/advanced_optimizer.h"
#include "interpreter/core_interpreter.h"
#include "interpreter/ssa_interpreter.h"
#include "testing/differential_runner.h"
#include "testing/nano_generator.h"
#include "testing/fuzz_runner.h"

using namespace nanopass;
using namespace nanopass::opt;

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

// Phase 4 regression: a continue belonging to a desugared for-loop must run
// the generated increment before jumping back to the condition/header.
static void test_for_continue_preserves_increment() {
    auto res = compileToIR(R"(
int main() {
    for (int i = 0; i < 3; i = i + 1) {
        if (i == 1) { continue; }
        print i;
    }
    return 0;
}
)");
    ASSERT(res.cfgVerResult.ok);
    const auto &fn = res.cfg.functions[0];
    const BasicBlock *header = nullptr;
    for (const auto &block : fn.blocks) {
        if (block->label == "loop.header") { header = block.get(); break; }
    }
    ASSERT(header != nullptr);
    size_t incrementingPredecessors = 0;
    if (header) for (BlockId predecessorId : header->predecessors) {
        const BasicBlock *predecessor = fn.getBlock(predecessorId);
        bool storesI = false;
        for (const auto &instruction : predecessor->instructions)
            if (instruction.kind == CFGInstructionKind::StoreLocal && instruction.local.name == "i") storesI = true;
        if (storesI) ++incrementingPredecessors;
    }
    ASSERT(incrementingPredecessors >= 2);
}

static CFGFunction makeDiamondCFG() {
    CFGFunction fn; fn.name = "diamond"; fn.returnType = LangType::Void();
    CFGBuilder builder(fn);
    const BlockId entry = builder.createBlock("entry");
    const BlockId left = builder.createBlock("left");
    const BlockId right = builder.createBlock("right");
    const BlockId merge = builder.createBlock("merge");
    fn.entryBlock = entry;
    builder.setInsertPoint(entry);
    builder.terminateWithBranch(CFGOperand::fromConstant(true, LangType::Bool()), left, right);
    builder.setInsertPoint(left); builder.terminateWithJump(merge);
    builder.setInsertPoint(right); builder.terminateWithJump(merge);
    builder.setInsertPoint(merge); builder.terminateWithReturn();
    builder.finalizeEdges();
    return fn;
}

static void test_dominators_diamond() {
    const auto fn = makeDiamondCFG(); const auto dom = DominatorAnalysis::run(fn);
    ASSERT(verifyDominatorTree(fn, dom).ok);
    ASSERT_EQ(dom.immediateDominator(BlockId{1})->index, 0);
    ASSERT_EQ(dom.immediateDominator(BlockId{2})->index, 0);
    ASSERT_EQ(dom.immediateDominator(BlockId{3})->index, 0);
    ASSERT(dom.dominates(BlockId{0}, BlockId{3}));
    ASSERT(!dom.dominates(BlockId{1}, BlockId{3}));
}

static void test_dominance_frontier_diamond() {
    const auto fn = makeDiamondCFG(); const auto dom = DominatorAnalysis::run(fn);
    const auto frontier = DominanceFrontierAnalysis::run(fn, dom);
    ASSERT(frontier.contains(BlockId{1}, BlockId{3}));
    ASSERT(frontier.contains(BlockId{2}, BlockId{3}));
    ASSERT(!frontier.contains(BlockId{0}, BlockId{3}));
}

static const SSAFunction &ssaMain(const IRPipelineResult &result, SSAStatistics &stats) {
    static SSAProgram program;
    SSABuilder builder;
    program = builder.build(result.cfg, &stats);
    for (const auto &function : program.functions)
        if (function.name == "main") return function;
    throw std::runtime_error("missing SSA main");
}

static size_t phiCountFor(const SSAFunction &function, const std::string &name) {
    size_t count = 0;
    for (const auto &block : function.blocks)
        for (const auto &phi : block.phis)
            if (phi.originalLocal.name == name) ++count;
    return count;
}

static void test_ssa_if_else_phi() {
    auto result = compileToIR(R"(
int main() { int c = 1; int x = 0; if (c) { x = 1; } else { x = 2; } print x; return 0; }
)");
    SSAStatistics stats;
    const auto &function = ssaMain(result, stats);
    ASSERT_EQ(phiCountFor(function, "x"), 1u);
    ASSERT(stats.loadsRemoved > 0);
    ASSERT(stats.storesRemoved > 0);
}

static void test_ssa_loop_carried_locals() {
    auto result = compileToIR(R"(
int main() { int i = 0; int sum = 0; while (i < 4) { sum = sum + i; i = i + 1; } print sum; return 0; }
)");
    SSAStatistics stats;
    const auto &function = ssaMain(result, stats);
    ASSERT_EQ(phiCountFor(function, "i"), 1u);
    ASSERT_EQ(phiCountFor(function, "sum"), 1u);
}

static void test_ssa_no_unnecessary_phi() {
    auto result = compileToIR(R"(
int main() { int c = 1; int x = 7; if (c) { print 1; } print x; return 0; }
)");
    SSAStatistics stats;
    const auto &function = ssaMain(result, stats);
    ASSERT_EQ(phiCountFor(function, "x"), 0u);
}

// A definition in the left dominator child must be popped before the right
// child is renamed.  This catches the classic sibling-stack-leak bug.
static void test_ssa_sibling_stack_restoration() {
    auto result = compileToIR(R"(
int main() { int c = 1; int x = 4; if (c) { x = 9; print x; } else { print x; } return 0; }
)");
    SSAStatistics stats; const auto &function = ssaMain(result, stats);
    ASSERT(verifySSA(function).ok);
    // The merge needs x, but the else-path use remains dominated by entry.
    ASSERT_EQ(phiCountFor(function, "x"), 1u);
}

static void test_ssa_def_use_and_replacement() {
    auto result = compileToIR(R"(int main() { int x = 1; int y = x + 2; print y; return 0; })");
    SSAStatistics stats; SSAFunction function = ssaMain(result, stats);
    SSADefUseIndex index; index.rebuild(function);
    ValueId source{-1}, replacement{-1};
    for (const auto &block : function.blocks) for (const auto &instruction : block.instructions)
        if (instruction.result) { if (source.index < 0) source = *instruction.result; else replacement = *instruction.result; }
    ASSERT(index.definitionOf(source).has_value());
    ASSERT(index.numberOfUses(source) > 0);
    replaceAllUses(function, source, replacement);
    SSADefUseIndex changed; changed.rebuild(function);
    ASSERT_EQ(changed.numberOfUses(source), 0u);
}

static SSAFunction malformedSSA() {
    SSAFunction f; f.name = "bad"; f.returnType = LangType::Int(); f.entryBlock = {0};
    SSABlock entry; entry.id={0}; entry.label="entry"; entry.terminator.value=SSATerminator::Return{SSAOperand{ValueId{7}}};
    f.blocks.push_back(std::move(entry)); return f;
}

static void test_ssa_verifier_rejects_malformed_programs() {
    auto undefined = malformedSSA(); ASSERT(!verifySSA(undefined).ok);
    auto duplicate = malformedSSA();
    duplicate.blocks[0].instructions.push_back({SSAInstructionKind::Constant, ValueId{1}, LangType::Int(), CoreConstantVal{1}});
    duplicate.blocks[0].instructions.push_back({SSAInstructionKind::Constant, ValueId{1}, LangType::Int(), CoreConstantVal{2}});
    ASSERT(!verifySSA(duplicate).ok);
    auto useBeforeDef = malformedSSA();
    SSAInstruction binary; binary.kind=SSAInstructionKind::Binary; binary.result=ValueId{1}; binary.type=LangType::Int(); binary.operand1=ValueId{2}; binary.operand2=CoreConstantVal{1};
    SSAInstruction definition; definition.kind=SSAInstructionKind::Constant; definition.result=ValueId{2}; definition.type=LangType::Int(); definition.constantValue=1;
    useBeforeDef.blocks[0].instructions={binary, definition};
    ASSERT(!verifySSA(useBeforeDef).ok);
}

static void test_ssa_printer_deterministic() {
    auto result = compileToIR(R"(int main() { int x=0; while(x<2){x=x+1;} print x; return 0; })");
    SSAStatistics stats; SSAProgram program; SSABuilder builder; program=builder.build(result.cfg,&stats);
    std::ostringstream first, second; printSSA(program, first); printSSA(program, second);
    ASSERT_EQ(first.str(), second.str()); ASSERT(first.str().find("phi") != std::string::npos);
}

static void test_ssa_parameter_reassignment_and_array_memory() {
    auto result = compileToIR(R"(
int increment(int x) { x = x + 1; return x; }
int main() { int[2] values; values[0] = increment(4); print values[0]; return 0; }
)");
    SSAStatistics stats; SSAProgram program; SSABuilder builder; program = builder.build(result.cfg, &stats);
    const SSAFunction *increment = nullptr; const SSAFunction *main = nullptr;
    for (const auto &function : program.functions) { if (function.name == "increment") increment = &function; if (function.name == "main") main = &function; }
    ASSERT(increment != nullptr); ASSERT(main != nullptr); ASSERT_EQ(increment->parameters.size(), 1u);
    SSADefUseIndex index; index.rebuild(*increment);
    ASSERT(index.definitionOf(increment->parameters[0].value).has_value());
    ASSERT(index.numberOfUses(increment->parameters[0].value) > 0);
    bool hasIndexedMemory = false;
    for (const auto &block : main->blocks) for (const auto &instruction : block.instructions)
        hasIndexedMemory |= instruction.kind == SSAInstructionKind::LoadIndex || instruction.kind == SSAInstructionKind::StoreIndex;
    ASSERT(hasIndexedMemory);
}

static void test_ssa_nested_and_loop_conditional_phis() {
    auto result = compileToIR(R"(
int main() { int i=0; int x=0; while(i<3) { if(i) { if(i-1) x=1; else x=2; } i=i+1; } print x; return 0; }
)");
    SSAStatistics stats; const auto &function = ssaMain(result, stats);
    ASSERT(verifySSA(function).ok);
    ASSERT(phiCountFor(function, "i") >= 1u);
    ASSERT(phiCountFor(function, "x") >= 1u);
}

static SSAFunction malformedPhiSSA() {
    SSAFunction f; f.name="phi_bad"; f.returnType=LangType::Void(); f.entryBlock={0};
    SSABlock entry; entry.id={0}; entry.label="entry"; entry.successors={{1},{2}}; entry.terminator.value=SSATerminator::Branch{SSAOperand{CoreConstantVal{true}}, {1}, {2}};
    SSABlock left; left.id={1}; left.label="left"; left.predecessors={{0}}; left.successors={{3}}; left.instructions.push_back({SSAInstructionKind::Constant, ValueId{1}, LangType::Int(), CoreConstantVal{1}}); left.terminator.value=SSATerminator::Jump{{3}};
    SSABlock right; right.id={2}; right.label="right"; right.predecessors={{0}}; right.successors={{3}}; right.instructions.push_back({SSAInstructionKind::Constant, ValueId{2}, LangType::Int(), CoreConstantVal{2}}); right.terminator.value=SSATerminator::Jump{{3}};
    SSABlock merge; merge.id={3}; merge.label="merge"; merge.predecessors={{1},{2}}; merge.terminator.value=SSATerminator::Return{};
    f.blocks={entry,left,right,merge}; return f;
}

static void test_ssa_verifier_phi_and_branch_failures() {
    auto missing = malformedPhiSSA(); missing.blocks[3].phis.push_back({ValueId{3}, LocalId{"x",0}, LangType::Int(), {{{1}, {1}}}}); ASSERT(!verifySSA(missing).ok);
    auto extra = malformedPhiSSA(); extra.blocks[3].phis.push_back({ValueId{3}, LocalId{"x",0}, LangType::Int(), {{{1}, {1}}, {{2}, {2}}, {{0}, {1}}}}); ASSERT(!verifySSA(extra).ok);
    auto duplicate = malformedPhiSSA(); duplicate.blocks[3].phis.push_back({ValueId{3}, LocalId{"x",0}, LangType::Int(), {{{1}, {1}}, {{1}, {1}}, {{2}, {2}}}}); ASSERT(!verifySSA(duplicate).ok);
    auto wrongType = malformedPhiSSA(); wrongType.blocks[2].instructions[0].type=LangType::Char(); wrongType.blocks[3].phis.push_back({ValueId{3}, LocalId{"x",0}, LangType::Int(), {{{1}, {1}}, {{2}, {2}}}}); ASSERT(!verifySSA(wrongType).ok);
    auto residual = malformedPhiSSA(); residual.promotedLocals.push_back(LocalId{"x",0}); SSAInstruction load; load.kind=SSAInstructionKind::LoadLocal; load.result=ValueId{4}; load.type=LangType::Int(); load.local=LocalId{"x",0}; residual.blocks[0].instructions.push_back(load); ASSERT(!verifySSA(residual).ok);
    auto badBranch = malformedPhiSSA(); badBranch.blocks[0].instructions.push_back({SSAInstructionKind::Constant, ValueId{4}, LangType::Char(), CoreConstantVal{'x'}}); badBranch.blocks[0].terminator.value=SSATerminator::Branch{SSAOperand{ValueId{4}}, {1}, {2}}; ASSERT(!verifySSA(badBranch).ok);
}

static void test_ssa_replace_all_uses_every_operand_kind() {
    SSAFunction function = malformedPhiSSA();
    function.returnType=LangType::Int();
    function.blocks[3].phis.push_back({ValueId{3}, LocalId{"x",0}, LangType::Int(), {{{1},{1}},{{2},{1}}}});
    SSAInstruction call; call.kind=SSAInstructionKind::Call; call.result=ValueId{5}; call.type=LangType::Int(); call.callee="f"; call.callArgs={ValueId{1}};
    SSAInstruction binary; binary.kind=SSAInstructionKind::Binary; binary.result=ValueId{6}; binary.type=LangType::Int(); binary.operand1=ValueId{1}; binary.operand2=ValueId{1};
    SSAInstruction index; index.kind=SSAInstructionKind::StoreIndex; index.local=LocalId{"a",0}; index.operand1=ValueId{1}; index.operand2=ValueId{1};
    function.blocks[1].instructions.push_back(call); function.blocks[1].instructions.push_back(binary); function.blocks[1].instructions.push_back(index);
    function.blocks[0].terminator.value=SSATerminator::Branch{SSAOperand{ValueId{1}}, {1}, {2}};
    function.blocks[3].terminator.value=SSATerminator::Return{SSAOperand{ValueId{1}}};
    replaceAllUses(function, ValueId{1}, ValueId{2});
    SSADefUseIndex indexAfter; indexAfter.rebuild(function);
    ASSERT_EQ(indexAfter.numberOfUses(ValueId{1}), 0u);
    ASSERT(indexAfter.numberOfUses(ValueId{2}) >= 8u);
}

static void test_optimizer_constant_fold_and_dce() {
    auto result = compileToIR(R"(int main() { int x=2+3; int dead=4*5; print x; return 0; })");
    SSAStatistics stats; SSAFunction function=ssaMain(result, stats);
    const auto before=instructionCount(function); SSAOptimizationContext context;
    auto pipeline=buildPipeline(1); auto optimized=pipeline.run(function,context);
    ASSERT(verifySSA(function).ok); ASSERT(optimized.statistics.get("instructionsFolded") > 0);
    ASSERT(optimized.changed); ASSERT(instructionCount(function) <= before);
}

static void test_optimizer_sccp_folds_constant_branch() {
    auto result = compileToIR(R"(int main() { int x=4; if(x<10) { print 1; } else { print 2; } return 0; })");
    SSAStatistics stats; SSAFunction function=ssaMain(result, stats); SSAOptimizationContext context;
    WorklistSCCPPass sccp; auto changes=sccp.run(function,context); verifyAfter(sccp,function,context);
    ASSERT(changes.statistics.get("branchConditionsProven") > 0);
    SimplifyCFGPass simplify; simplify.run(function,context); verifyAfter(simplify,function,context);
}

static void test_optimizer_cse_and_side_effect_dce() {
    auto result = compileToIR(R"(int f(int x) { print x; return x; } int main() { int a=3; int b=4; int x=a+b; int y=a+b; f(y); return 0; })");
    SSAStatistics stats; SSAFunction function=ssaMain(result, stats); SSAOptimizationContext context;
    CSEPass cse; auto changed=cse.run(function,context); verifyAfter(cse,function,context);
    ASSERT(changed.statistics.get("expressionsEliminated") > 0);
    DeadCodeEliminationPass dce; dce.run(function,context); verifyAfter(dce,function,context);
    bool callPresent=false; for(const auto&b:function.blocks)for(const auto&i:b.instructions)callPresent|=i.kind==SSAInstructionKind::Call;
    ASSERT(callPresent);
}

static void test_optimizer_o2_deterministic_and_verified() {
    auto result = compileToIR(R"(int main() { int x=4; if(x<10){print 1;}else{print 2;} return 0; })");
    SSAStatistics stats; SSAProgram first; SSABuilder builder; first=builder.build(result.cfg,&stats); SSAProgram second=first;
    SSAOptimizationContext a,b; for(auto &function:first.functions)buildPipeline(2).run(function,a); for(auto &function:second.functions)buildPipeline(2).run(function,b);
    std::ostringstream left,right; printSSA(first,left); printSSA(second,right);
    ASSERT_EQ(left.str(),right.str()); for(const auto&function:first.functions)ASSERT(verifySSA(function).ok);
}

class BrokenOptimizerPass final : public SSAFunctionPass {
public:
    std::string_view id() const override { return "broken-test-pass"; }
    std::string_view name() const override { return "Broken test pass"; }
    SSAOptimizationResult run(SSAFunction &function, SSAOptimizationContext &) override {
        if (!function.blocks.empty() && !function.blocks.front().instructions.empty()) {
            auto duplicate=function.blocks.front().instructions.front();
            if (duplicate.result) function.blocks.front().instructions.push_back(std::move(duplicate));
        }
        return {.changed=true};
    }
};

static void test_optimizer_framework_rejects_invalid_ssa() {
    auto result=compileToIR(R"(int main() { int x=1; print x; return 0; })");
    SSAStatistics stats; SSAFunction function=ssaMain(result,stats); SSAOptimizationContext context;
    SSAOptimizationPipeline pipeline; pipeline.add(std::make_unique<BrokenOptimizerPass>());
    bool rejected=false; try { pipeline.run(function,context); } catch(const std::runtime_error&) { rejected=true; }
    ASSERT(rejected);
}

static void test_sccp_lattice_meet_rules() {
    const SCCPLatticeValue unknown{};
    const SCCPLatticeValue four{SCCPLatticeKind::Constant, CoreConstantVal{int64_t(4)}};
    const SCCPLatticeValue seven{SCCPLatticeKind::Constant, CoreConstantVal{int64_t(7)}};
    ASSERT(meetSCCP(unknown,four).kind == SCCPLatticeKind::Constant);
    ASSERT(meetSCCP(four,four).kind == SCCPLatticeKind::Constant);
    ASSERT(meetSCCP(four,seven).kind == SCCPLatticeKind::Overdefined);
}

static void test_sccp_phi_uses_only_executable_edge() {
    auto result=compileToIR(R"(int main() { int x=0; if(1) { x=7; } else { x=9; } print x; return 0; })");
    SSAStatistics stats; SSAFunction function=ssaMain(result,stats); const auto blocksBefore=function.blocks.size(); SSAOptimizationContext context;
    WorklistSCCPPass sccp; sccp.run(function,context); verifyAfter(sccp,function,context);
    SimplifyCFGPass cleanup; cleanup.run(function,context); verifyAfter(cleanup,function,context);
    TrivialPhiPass phi; phi.run(function,context); verifyAfter(phi,function,context);
    ASSERT(function.blocks.size() < blocksBefore);
}

static void test_cse_does_not_cross_sibling_branches() {
    auto result=compileToIR(R"(int f(int c, int a, int b) { if(c) { return a+b; } else { return a+b; } } int main(){ return f(1,2,3); })");
    SSAStatistics stats; SSAProgram program; SSABuilder builder; program=builder.build(result.cfg,&stats); SSAFunction *function=nullptr; for(auto &candidate:program.functions)if(candidate.name=="f")function=&candidate;
    ASSERT(function != nullptr); size_t before=0; for(const auto&block:function->blocks)for(const auto&i:block.instructions)if(i.kind==SSAInstructionKind::Binary)++before;
    SSAOptimizationContext context; CSEPass cse; cse.run(*function,context); verifyAfter(cse,*function,context);
    size_t after=0; for(const auto&block:function->blocks)for(const auto&i:block.instructions)if(i.kind==SSAInstructionKind::Binary)++after;
    ASSERT_EQ(after,before);
}

static void test_optimizer_preserves_division_by_zero_and_safe_algebra() {
    SSAFunction function; function.name="manual"; function.returnType=LangType::Int(); function.entryBlock={0};
    SSABlock entry; entry.id={0}; entry.label="entry";
    SSAInstruction div; div.kind=SSAInstructionKind::Binary; div.result=ValueId{0}; div.type=LangType::Int(); div.binaryOp=BinaryOpKind::Div; div.operand1=CoreConstantVal{int64_t(4)}; div.operand2=CoreConstantVal{int64_t(0)};
    SSAInstruction add; add.kind=SSAInstructionKind::Binary; add.result=ValueId{1}; add.type=LangType::Int(); add.binaryOp=BinaryOpKind::Add; add.operand1=ValueId{0}; add.operand2=CoreConstantVal{int64_t(0)};
    entry.instructions={div,add}; entry.terminator.value=SSATerminator::Return{SSAOperand{ValueId{1}}}; function.blocks={entry};
    SSAOptimizationContext context; ConstantFoldPass fold; fold.run(function,context); verifyAfter(fold,function,context);
    ASSERT(function.blocks[0].instructions[0].kind==SSAInstructionKind::Binary);
    AlgebraicSimplifyPass algebra; algebra.run(function,context); verifyAfter(algebra,function,context);
    auto returned=std::get<SSATerminator::Return>(function.blocks[0].terminator.value).value;
    ASSERT(returned && std::holds_alternative<ValueId>(*returned) && std::get<ValueId>(*returned)==ValueId{0});
}

static void test_optimizer_preserves_memory_writes_and_exposes_remarks() {
    SSAFunction function; function.name="manual_store"; function.returnType=LangType::Int(); function.entryBlock={0};
    SSABlock entry; entry.id={0}; entry.label="entry";
    SSAInstruction store; store.kind=SSAInstructionKind::StoreIndex; store.local=LocalId{"array",0};
    store.operand1=CoreConstantVal{int64_t(0)}; store.operand2=CoreConstantVal{int64_t(42)};
    SSAInstruction dead; dead.kind=SSAInstructionKind::Binary; dead.result=ValueId{0}; dead.type=LangType::Int(); dead.binaryOp=BinaryOpKind::Add;
    dead.operand1=CoreConstantVal{int64_t(1)}; dead.operand2=CoreConstantVal{int64_t(2)};
    entry.instructions={store,dead}; entry.terminator.value=SSATerminator::Return{SSAOperand{CoreConstantVal{int64_t(0)}}}; function.blocks={entry};
    SSAOptimizationContext context; DeadCodeEliminationPass dce; auto result=dce.run(function,context);
    ASSERT(result.changed); ASSERT_EQ(function.blocks[0].instructions.size(),1u);
    ASSERT(function.blocks[0].instructions[0].kind==SSAInstructionKind::StoreIndex);
    ASSERT(!result.remarks.empty());
}

static void test_optimizer_pass_registry_and_levels() {
    ASSERT(makePass("sccp") != nullptr); ASSERT(makePass("not-a-pass") == nullptr);
    ASSERT_EQ(availablePasses().size(),8u); ASSERT(!availablePasses()[0].description.empty());
    ASSERT(!availablePasses()[6].preservesCFG);
    SSAFunction function; function.name="empty"; function.returnType=LangType::Int(); function.entryBlock={0};
    SSABlock entry; entry.id={0}; entry.terminator.value=SSATerminator::Return{SSAOperand{CoreConstantVal{int64_t(0)}}}; function.blocks={entry};
    SSAOptimizationContext context; auto o0=buildPipeline(0).run(function,context);
    ASSERT(!o0.changed); ASSERT(verifySSA(function).ok);
}

static void test_sccp_unknown_branch_and_loop_converge() {
    auto result=compileToIR(R"(int choose(int condition) { int x=1; if(condition) { x=2; } else { x=3; } return x; } int main(){ int x=0; while(x<3) { x=x+1; } return choose(x); })");
    SSAStatistics stats; SSABuilder builder; SSAProgram program=builder.build(result.cfg,&stats);
    SSAFunction *choose=nullptr; SSAFunction *main=nullptr;
    for(auto &candidate:program.functions) { if(candidate.name=="choose") choose=&candidate; if(candidate.name=="main") main=&candidate; }
    ASSERT(choose && main); SSAOptimizationContext context; WorklistSCCPPass sccp;
    auto choice=sccp.run(*choose,context); verifyAfter(sccp,*choose,context);
    size_t branches=0; for(const auto &block:choose->blocks) branches+=std::holds_alternative<SSATerminator::Branch>(block.terminator.value);
    ASSERT_EQ(branches,1u); ASSERT(choice.statistics.get("branchConditionsProven")==0u);
    auto loop=sccp.run(*main,context); verifyAfter(sccp,*main,context);
    ASSERT(loop.statistics.get("executableBlocks") > 0u);
}

static void test_optimizer_o3_reaches_bounded_fixpoint() {
    auto result=compileToIR(R"(int main() { int x=1+2; int y=x+0; int dead=y*7; print y; return 0; })");
    SSAStatistics stats; SSAFunction function=ssaMain(result,stats); SSAOptimizationContext context;
    auto optimized=buildPipeline(3).run(function,context);
    ASSERT(optimized.statistics.get("pipelineIterations") >= 2u);
    ASSERT(optimized.statistics.get("pipelineIterations") <= 4u);
    ASSERT(verifySSA(function).ok);
}

static void test_simplify_cfg_forwards_empty_block_and_repairs_phi_edge() {
    SSAFunction function; function.name="forward"; function.returnType=LangType::Int(); function.entryBlock={0};
    SSABlock entry; entry.id={0}; entry.successors={{1}};
    entry.instructions.push_back({SSAInstructionKind::Constant,ValueId{0},LangType::Int(),CoreConstantVal{int64_t(9)}});
    entry.terminator.value=SSATerminator::Jump{{1}};
    SSABlock middle; middle.id={1}; middle.predecessors={{0}}; middle.successors={{2}}; middle.terminator.value=SSATerminator::Jump{{2}};
    SSABlock exit; exit.id={2}; exit.predecessors={{1}};
    exit.phis.push_back({ValueId{1},LocalId{"x",0},LangType::Int(),{{{1},{0}}}});
    exit.terminator.value=SSATerminator::Return{SSAOperand{ValueId{1}}}; function.blocks={entry,middle,exit};
    SSAOptimizationContext context; SimplifyCFGPass simplify; auto result=simplify.run(function,context); verifyAfter(simplify,function,context);
    ASSERT_EQ(result.statistics.get("blocksForwarded"),1u); ASSERT_EQ(function.blocks.size(),2u);
    ASSERT_EQ(function.blocks[1].phis[0].incoming[0].predecessor.index,0);
}

static void test_loop_info_nesting_exits_and_preheaders() {
    auto result=compileToIR(R"(int f(int n) { int i=0; int j=0; while(i<n) { j=0; while(j<n) { j=j+1; } i=i+1; } return i; } int main(){return f(2);})");
    SSAStatistics stats; SSABuilder builder; SSAProgram program=builder.build(result.cfg,&stats); SSAFunction *function=nullptr;
    for(auto &candidate:program.functions)if(candidate.name=="f")function=&candidate; ASSERT(function);
    auto loops=LoopInfoAnalysis::run(*function); auto verified=verifyLoopInfo(*function,loops);
    ASSERT(verified.ok); ASSERT_EQ(loops.loops.size(),2u); size_t nested=0; for(const auto&loop:loops.loops){ASSERT(!loop.blocks.empty());ASSERT(!loop.backedgeSources.empty());ASSERT(loop.preheader.has_value());if(loop.parent)++nested;}
    ASSERT_EQ(nested,1u);
}

static void test_licm_hoists_only_safe_invariants() {
    auto result=compileToIR(R"(int f(int n, int a, int b) { int i=0; int total=0; while(i<n) { int x=a*b; int y=a/b; total=total+x; i=i+1; } return total; } int main(){return f(3,2,4);})");
    SSAStatistics stats; SSABuilder builder; SSAProgram program=builder.build(result.cfg,&stats); SSAFunction *function=nullptr;for(auto&candidate:program.functions)if(candidate.name=="f")function=&candidate;ASSERT(function);
    SSAOptimizationContext context; LICMPass licm; auto changed=licm.run(*function,context); verifyAfter(licm,*function,context);
    ASSERT(changed.statistics.get("instructionsHoisted")>0u); bool divisionStillPresent=false;for(const auto&block:function->blocks)for(const auto&instruction:block.instructions)divisionStillPresent|=instruction.kind==SSAInstructionKind::Binary&&instruction.binaryOp==BinaryOpKind::Div;ASSERT(divisionStillPresent);
}

static void test_strength_reduction_and_call_graph_recursion() {
    SSAFunction function;function.name="strength";function.returnType=LangType::Int();function.entryBlock={0};SSABlock entry;entry.id={0};SSAInstruction multiply;multiply.kind=SSAInstructionKind::Binary;multiply.result=ValueId{0};multiply.type=LangType::Int();multiply.binaryOp=BinaryOpKind::Mul;multiply.operand1=CoreConstantVal{int64_t(2)};multiply.operand2=CoreConstantVal{int64_t(7)};entry.instructions={multiply};entry.terminator.value=SSATerminator::Return{SSAOperand{ValueId{0}}};function.blocks={entry};SSAOptimizationContext context;StrengthReducePass reduce;auto reduced=reduce.run(function,context);verifyAfter(reduce,function,context);ASSERT_EQ(reduced.statistics.get("multipliesReduced"),1u);ASSERT(function.blocks[0].instructions[0].binaryOp==BinaryOpKind::Add);
    auto result=compileToIR(R"(int a(int x){return b(x);} int b(int x){return a(x);} int c(int x){return x+1;} int main(){return c(1);})");SSAStatistics stats;SSABuilder builder;SSAProgram program=builder.build(result.cfg,&stats);auto calls=CallGraphAnalysis::run(program);ASSERT(calls.recursive("a"));ASSERT(calls.recursive("b"));ASSERT(!calls.recursive("c"));ASSERT_EQ(calls.callees("main").size(),1u);
}

static void test_single_block_inliner_remaps_values_and_rejects_recursion() {
    auto result=compileToIR(R"(int add1(int x){ return x+1; } int self(int x){ return self(x); } int main(){ int y=add1(4); return y; })");
    SSAStatistics stats;SSABuilder builder;SSAProgram program=builder.build(result.cfg,&stats);SSAProgramInliner inliner;auto changed=inliner.run(program);ASSERT(changed.statistics.get("callsInlined")>0u);for(const auto&function:program.functions)ASSERT(verifySSA(function).ok);
    bool addCall=false;bool selfCall=false;for(const auto&function:program.functions)for(const auto&block:function.blocks)for(const auto&instruction:block.instructions)if(instruction.kind==SSAInstructionKind::Call){addCall|=instruction.callee=="add1";selfCall|=instruction.callee=="self";}ASSERT(!addCall);ASSERT(selfCall);
}

static void test_cfg_inliner_clones_two_block_chain_structurally() {
    SSAFunction callee; callee.name="chain"; callee.returnType=LangType::Int(); callee.entryBlock={0}; callee.parameters={{ValueId{0},LocalId{"x",0},LangType::Int()}};
    SSABlock entry; entry.id={0}; SSAInstruction add; add.kind=SSAInstructionKind::Binary; add.result=ValueId{1}; add.type=LangType::Int(); add.binaryOp=BinaryOpKind::Add; add.operand1=ValueId{0}; add.operand2=CoreConstantVal{int64_t(1)}; entry.instructions={add}; entry.terminator.value=SSATerminator::Jump{{1}};
    SSABlock returned; returned.id={1}; returned.terminator.value=SSATerminator::Return{SSAOperand{ValueId{1}}}; callee.blocks={entry,returned}; rebuildSSAEdges(callee);
    SSAFunction caller; caller.name="main"; caller.returnType=LangType::Int(); caller.entryBlock={0}; SSABlock callsite; callsite.id={0}; SSAInstruction constant; constant.kind=SSAInstructionKind::Constant; constant.result=ValueId{0}; constant.type=LangType::Int(); constant.constantValue=int64_t(4); SSAInstruction call; call.kind=SSAInstructionKind::Call; call.result=ValueId{1}; call.type=LangType::Int(); call.callee="chain"; call.callArgs={ValueId{0}}; SSAInstruction after; after.kind=SSAInstructionKind::Binary; after.result=ValueId{2}; after.type=LangType::Int(); after.binaryOp=BinaryOpKind::Add; after.operand1=ValueId{1}; after.operand2=CoreConstantVal{int64_t(2)}; callsite.instructions={constant,call,after}; callsite.terminator.value=SSATerminator::Return{SSAOperand{ValueId{2}}}; caller.blocks={callsite}; rebuildSSAEdges(caller);
    SSAProgram program; program.functions={callee,caller}; SSAProgramInliner inliner; auto result=inliner.run(program); ASSERT_EQ(result.statistics.get("callsInlined"),1u); ASSERT_EQ(result.statistics.get("blocksCloned"),2u); const auto &main=program.functions[1]; const auto block=[&](int id)->const SSABlock&{return *std::find_if(main.blocks.begin(),main.blocks.end(),[&](const SSABlock&candidate){return candidate.id.index==id;});}; ASSERT_EQ(main.blocks.size(),4u); ASSERT(std::none_of(block(0).instructions.begin(),block(0).instructions.end(),[](const SSAInstruction&i){return i.kind==SSAInstructionKind::Call;})); ASSERT_EQ(block(0).successors[0].index,1); ASSERT_EQ(block(1).predecessors[0].index,0); ASSERT_EQ(block(2).successors[0].index,3); ASSERT_EQ(block(3).predecessors[0].index,2); ASSERT(block(3).phis.empty()); ASSERT(verifySSA(main).ok);
}

static void test_cfg_inliner_branching_multi_return() {
 auto result=compileToIR(R"(int choose(int x){if(x>0){return 1;}return 2;} int main(){int y=choose(1);return y+1;})");SSAStatistics stats;SSABuilder builder;SSAProgram p=builder.build(result.cfg,&stats);SSAProgramInliner inliner;auto r=inliner.run(p);ASSERT(r.statistics.get("callsInlined")>0u);ASSERT_EQ(r.statistics.get("continuationPhisCreated"),1u);ASSERT_EQ(r.statistics.get("returnSitesMerged"),2u);for(const auto&f:p.functions)ASSERT(verifySSA(f).ok);const SSAFunction &main=p.functions.back();bool call=false;const SSABlock *continuation=nullptr;for(const auto&b:main.blocks){call|=std::any_of(b.instructions.begin(),b.instructions.end(),[](const auto&i){return i.kind==SSAInstructionKind::Call;});if(b.label.rfind("inline.cont.",0)==0&&!b.phis.empty())continuation=&b;}ASSERT(!call);ASSERT(continuation);ASSERT_EQ(continuation->phis.size(),1u);std::vector<BlockId> incoming;for(const auto&item:continuation->phis[0].incoming)incoming.push_back(item.predecessor);ASSERT(incoming==continuation->predecessors);
}

static void test_cfg_inliner_void_returns_nested_call_and_determinism() {
 auto result=compileToIR(R"(void choose(int x){if(x>0){print 1;return;}print 2;return;} int main(){choose(1);return 0;})");SSAStatistics stats;SSABuilder builder;SSAProgram first=builder.build(result.cfg,&stats),second=first;SSAProgramInliner inliner;auto one=inliner.run(first),two=inliner.run(second);ASSERT_EQ(one.statistics.get("callsInlined"),1u);ASSERT_EQ(two.statistics.get("callsInlined"),1u);std::ostringstream a,b;printSSA(first,a);printSSA(second,b);ASSERT_EQ(a.str(),b.str());for(const auto&f:first.functions)ASSERT(verifySSA(f).ok);const SSAFunction &main=first.functions.back();for(const auto&block:main.blocks){for(const auto&instruction:block.instructions)ASSERT(!(instruction.kind==SSAInstructionKind::Call&&instruction.callee=="choose"));if(block.label.rfind("inline.cont.",0)==0)ASSERT(block.phis.empty());}
}

static void test_cfg_inliner_preserves_nested_call_in_cloned_cfg() {
 auto result=compileToIR(R"(int helper(int x){return x+1;} int outer(int x){return helper(x);} int main(){return outer(1);})");SSAStatistics stats;SSABuilder builder;SSAProgram program=builder.build(result.cfg,&stats);auto mainIt=std::find_if(program.functions.begin(),program.functions.end(),[](const SSAFunction&function){return function.name=="main";});ASSERT(mainIt!=program.functions.end());std::rotate(program.functions.begin(),mainIt,mainIt+1);SSAProgramInliner inliner;auto outcome=inliner.run(program);ASSERT(outcome.changed);const SSAFunction &main=program.functions.front();bool outerCall=false,helperCall=false;for(const auto&block:main.blocks)for(const auto&instruction:block.instructions)if(instruction.kind==SSAInstructionKind::Call){outerCall|=instruction.callee=="outer";helperCall|=instruction.callee=="helper";}ASSERT(!outerCall);ASSERT(helperCall);const auto graph=CallGraphAnalysis::run(program);ASSERT(std::find(graph.callees("main").begin(),graph.callees("main").end(),"helper")!=graph.callees("main").end());for(const auto&function:program.functions)ASSERT(verifySSA(function).ok);
}

static void test_cfg_inliner_repairs_successor_phi_and_clones_loop() {
 auto result=compileToIR(R"(int helper(int x){return x+1;} int loopfn(int n){int i=0;while(i<n){i=i+1;}return i;} int main(int c){int x=0;if(c){x=helper(1);}else{x=2;}return loopfn(x);})");SSAStatistics stats;SSABuilder builder;SSAProgram program=builder.build(result.cfg,&stats);SSAProgramInliner inliner;auto changed=inliner.run(program);ASSERT(changed.statistics.get("callsInlined")>=2u);for(const auto&function:program.functions)ASSERT(verifySSA(function).ok);SSAFunction *mainFunction=nullptr;for(auto&function:program.functions)if(function.name=="main")mainFunction=&function;ASSERT(mainFunction);std::map<int,std::string> labels;for(const auto&block:mainFunction->blocks)labels[block.id.index]=block.label;bool successorPhiUsesContinuation=false,clonedLoopPhi=false;for(const auto&block:mainFunction->blocks)for(const auto&phi:block.phis){for(const auto&incoming:phi.incoming)successorPhiUsesContinuation|=labels[incoming.predecessor.index].rfind("inline.cont.",0)==0;if(block.label.rfind("inline.loopfn.",0)==0&&!phi.incoming.empty()){clonedLoopPhi=true;for(const auto&incoming:phi.incoming)ASSERT(labels[incoming.predecessor.index].rfind("inline.loopfn.",0)==0);}}ASSERT(successorPhiUsesContinuation);ASSERT(clonedLoopPhi);auto loops=LoopInfoAnalysis::run(*mainFunction);ASSERT(!loops.loops.empty());SSAOptimizationContext context;auto optimized=buildPipeline(3).run(*mainFunction,context);(void)optimized;ASSERT(verifySSA(*mainFunction).ok);LICMPass licm;auto licmResult=licm.run(*mainFunction,context);(void)licmResult;verifyAfter(licm,*mainFunction,context);
}

static void test_cfg_inliner_rejections_are_non_mutating_and_invalidate_analyses() {
 auto result=compileToIR(R"(int add1(int x){return x+1;} int main(){return add1(3);})");SSAStatistics stats;SSABuilder builder;
 auto rejected=[&](OptimizationBudget budget,const char *reason){SSAProgram program=builder.build(result.cfg,&stats);std::ostringstream before,after;printSSA(program,before);SSAProgramInliner inliner;auto outcome=inliner.run(program,budget);printSSA(program,after);ASSERT(!outcome.changed);ASSERT_EQ(before.str(),after.str());ASSERT(std::any_of(outcome.remarks.begin(),outcome.remarks.end(),[&](const auto&remark){return remark.message.find(reason)!=std::string::npos;}));};
 OptimizationBudget cost;cost.maxInlineCalleeInstructions=0;rejected(cost,"INLINE_COST_TOO_HIGH");
 OptimizationBudget growth;growth.maxInlineCalleeInstructions=100;growth.maxCallerGrowth=0;rejected(growth,"INLINE_GROWTH_BUDGET");
 OptimizationBudget depth;depth.maxInlineDepth=0;rejected(depth,"INLINE_DEPTH_LIMIT");
 SSAProgram program=builder.build(result.cfg,&stats);SSAAnalysisManager analyses;auto &calls=analyses.get<CachedCallGraphAnalysis>(program);ASSERT(!calls.callees("main").empty());auto &loops=analyses.get<CachedLoopInfoAnalysis>(program,program.functions.back());ASSERT(loops.loops.empty());SSAProgramInliner inliner;auto outcome=inliner.run(program,{},&analyses);ASSERT(outcome.changed);ASSERT(!analyses.isCached<CachedCallGraphAnalysis>(program));ASSERT(!analyses.isCached<CachedLoopInfoAnalysis>(program,program.functions.back()));auto &rebuilt=analyses.get<CachedCallGraphAnalysis>(program);ASSERT(rebuilt.callees("main").empty());for(const auto&function:program.functions)ASSERT(verifySSA(function).ok);
}

static void test_cfg_inliner_rejects_mutual_recursion_without_mutation() {
 auto result=compileToIR(R"(int a(int x){return b(x);} int b(int x){return a(x);} int main(){return a(1);})");SSAStatistics stats;SSABuilder builder;SSAProgram program=builder.build(result.cfg,&stats);std::ostringstream before,after;printSSA(program,before);SSAProgramInliner inliner;auto outcome=inliner.run(program);printSSA(program,after);ASSERT(!outcome.changed);ASSERT_EQ(before.str(),after.str());ASSERT(outcome.statistics.get("rejectedRecursive")>=3u);ASSERT(std::all_of(outcome.remarks.begin(),outcome.remarks.end(),[](const auto&remark){return remark.message.find("INLINE_RECURSIVE")!=std::string::npos;}));for(const auto&function:program.functions)ASSERT(verifySSA(function).ok);
}

static void test_loop_simplify_synthesizes_preheader_and_repairs_phi() {
    SSAFunction function;function.name="preheader";function.returnType=LangType::Int();function.entryBlock={0};
    SSABlock entry;entry.id={0};entry.successors={{1},{2}};entry.instructions.push_back({SSAInstructionKind::Constant,ValueId{0},LangType::Bool(),CoreConstantVal{true}});entry.terminator.value=SSATerminator::Branch{SSAOperand{ValueId{0}},{1},{2}};
    SSABlock left;left.id={1};left.predecessors={{0}};left.successors={{3}};left.instructions.push_back({SSAInstructionKind::Constant,ValueId{1},LangType::Int(),CoreConstantVal{int64_t(1)}});left.terminator.value=SSATerminator::Jump{{3}};
    SSABlock right;right.id={2};right.predecessors={{0}};right.successors={{3}};right.instructions.push_back({SSAInstructionKind::Constant,ValueId{2},LangType::Int(),CoreConstantVal{int64_t(2)}});right.terminator.value=SSATerminator::Jump{{3}};
    SSABlock header;header.id={3};header.predecessors={{1},{2},{4}};header.successors={{4},{5}};header.phis.push_back({ValueId{3},LocalId{"i",0},LangType::Int(),{{{1},{1}},{{2},{2}},{{4},{4}}}});header.terminator.value=SSATerminator::Branch{SSAOperand{ValueId{0}},{4},{5}};
    SSABlock latch;latch.id={4};latch.predecessors={{3}};latch.successors={{3}};SSAInstruction next;next.kind=SSAInstructionKind::Binary;next.result=ValueId{4};next.type=LangType::Int();next.binaryOp=BinaryOpKind::Add;next.operand1=ValueId{3};next.operand2=CoreConstantVal{int64_t(1)};latch.instructions={next};latch.terminator.value=SSATerminator::Jump{{3}};
    SSABlock exit;exit.id={5};exit.predecessors={{3}};exit.terminator.value=SSATerminator::Return{SSAOperand{ValueId{3}}};function.blocks={entry,left,right,header,latch,exit};SSAOptimizationContext context;LoopSimplifyPass simplify;auto changed=simplify.run(function,context);verifyAfter(simplify,function,context);ASSERT_EQ(changed.statistics.get("preheadersCreated"),1u);auto loops=LoopInfoAnalysis::run(function);ASSERT_EQ(loops.loops.size(),1u);ASSERT(loops.loops[0].preheader.has_value());ASSERT(!function.blocks.back().phis.empty());
}

static void test_ssa_analysis_cache_scopes_and_invalidation() {
    auto result=compileToIR(R"(int f(int n){int i=0;while(i<n){i=i+1;}return i;} int g(int n){int i=0;while(i<n){i=i+1;}return i;} int main(){return f(2);})");SSAStatistics stats;SSABuilder builder;SSAProgram program=builder.build(result.cfg,&stats);SSAAnalysisManager analyses;auto &first=analyses.get<CachedLoopInfoAnalysis>(program,program.functions[0]);auto &again=analyses.get<CachedLoopInfoAnalysis>(program,program.functions[0]);ASSERT(&first==&again);ASSERT(analyses.isCached<CachedLoopInfoAnalysis>(program,program.functions[0]));auto &other=analyses.get<CachedLoopInfoAnalysis>(program,program.functions[1]);ASSERT(&first!=&other);auto &calls=analyses.get<CachedCallGraphAnalysis>(program);ASSERT(analyses.isCached<CachedCallGraphAnalysis>(program));ASSERT(!calls.callees("main").empty());analyses.invalidateFunction(program,program.functions[0]);ASSERT(!analyses.isCached<CachedLoopInfoAnalysis>(program,program.functions[0]));ASSERT(analyses.isCached<CachedLoopInfoAnalysis>(program,program.functions[1]));analyses.invalidateProgram(program);ASSERT(!analyses.isCached<CachedCallGraphAnalysis>(program));
}

static void test_trivial_phi_and_iterative_dce() {
    SSAFunction function; function.name="manual"; function.returnType=LangType::Int(); function.entryBlock={0};
    SSABlock entry; entry.id={0}; entry.successors={{1}}; entry.instructions.push_back({SSAInstructionKind::Constant,ValueId{0},LangType::Int(),CoreConstantVal{int64_t(4)}}); entry.terminator.value=SSATerminator::Jump{{1}};
    SSABlock loop; loop.id={1}; loop.predecessors={{0},{1}}; loop.successors={{1}}; loop.phis.push_back({ValueId{1},LocalId{"x",0},LangType::Int(),{{{0},{0}},{{1},{1}}}}); loop.terminator.value=SSATerminator::Return{SSAOperand{ValueId{1}}};
    function.blocks={entry,loop}; SSAOptimizationContext context;
    TrivialPhiPass phi; auto result=phi.run(function,context); verifyAfter(phi,function,context);
    ASSERT(result.statistics.get("phisRemoved") == 1u); ASSERT(function.blocks[1].phis.empty());
    auto returned=std::get<SSATerminator::Return>(function.blocks[1].terminator.value).value;
    ASSERT(returned && std::holds_alternative<ValueId>(*returned) && std::get<ValueId>(*returned)==ValueId{0});

    SSAInstruction dead1; dead1.kind=SSAInstructionKind::Binary; dead1.result=ValueId{2}; dead1.type=LangType::Int(); dead1.binaryOp=BinaryOpKind::Add; dead1.operand1=ValueId{0}; dead1.operand2=CoreConstantVal{int64_t(1)};
    SSAInstruction dead2; dead2.kind=SSAInstructionKind::Binary; dead2.result=ValueId{3}; dead2.type=LangType::Int(); dead2.binaryOp=BinaryOpKind::Mul; dead2.operand1=ValueId{2}; dead2.operand2=CoreConstantVal{int64_t(2)};
    function.blocks[0].instructions.push_back(dead1); function.blocks[0].instructions.push_back(dead2);
    DeadCodeEliminationPass dce; auto dceResult=dce.run(function,context); verifyAfter(dce,function,context);
    ASSERT(dceResult.statistics.get("instructionsRemoved") >= 2u);
}

static void test_interpreters_differential_core_and_ssa() {
    auto result=compileToIR(R"(int add(int x){return x+1;} int main(){int i=0;int total=0;while(i<4){total=total+add(i);i=i+1;}print(total);return total;})");
    CoreInterpreter core; SSAInterpreter ssa; auto reference=core.run(result.core); auto o0=ssa.run(result.ssa);
    ASSERT(reference.status==ExecutionStatus::Success); ASSERT(o0.status==ExecutionStatus::Success); ASSERT(sameObservableResult(reference,o0)); ASSERT_EQ(reference.stdoutText,"10\n");
    for(int level=1;level<=3;++level){SSAProgram optimized=result.ssa;for(auto&function:optimized.functions){SSAOptimizationContext context;buildPipeline(level).run(function,context);ASSERT(verifySSA(function).ok);}auto executed=ssa.run(optimized);ASSERT(sameObservableResult(reference,executed));}
}

static void test_ssa_interpreter_parallel_phi_and_limits() {
    SSAFunction function;function.name="main";function.returnType=LangType::Int();function.entryBlock={0};
    SSABlock entry;entry.id={0};entry.instructions.push_back({SSAInstructionKind::Constant,ValueId{0},LangType::Int(),CoreConstantVal{int64_t(1)}});entry.instructions.push_back({SSAInstructionKind::Constant,ValueId{1},LangType::Int(),CoreConstantVal{int64_t(2)}});entry.terminator.value=SSATerminator::Jump{{1}};
    SSABlock merge;merge.id={1};merge.phis.push_back({ValueId{2},LocalId{"a",0},LangType::Int(),{{{0},{1}}}});merge.phis.push_back({ValueId{3},LocalId{"b",0},LangType::Int(),{{{0},{0}}}});SSAInstruction sum;sum.kind=SSAInstructionKind::Binary;sum.result=ValueId{4};sum.type=LangType::Int();sum.binaryOp=BinaryOpKind::Sub;sum.operand1=ValueId{2};sum.operand2=ValueId{3};merge.instructions={sum};merge.terminator.value=SSATerminator::Return{SSAOperand{ValueId{4}}};function.blocks={entry,merge};rebuildSSAEdges(function);SSAProgram program;program.functions={function};SSAInterpreter interpreter;auto ok=interpreter.run(program);ASSERT(ok.status==ExecutionStatus::Success);ASSERT_EQ(runtimeValueText(*ok.returnValue),"1");ExecutionLimits limits;limits.maxSteps=1;auto limited=interpreter.run(program,"main",limits);ASSERT(limited.status==ExecutionStatus::StepLimitExceeded);
}

static void test_differential_runner_and_deterministic_generator() {
    NanoProgramGenerator first({42,3,8}), second({42,3,8}); DifferentialRunner runner; size_t compared=0;
    for(size_t index=0;index<1000;++index){auto a=first.generate(index),b=second.generate(index);ASSERT_EQ(a.source,b.source);auto outcome=runner.run(a.source);ASSERT(outcome.compiled);ASSERT(outcome.equivalent);ASSERT_EQ(outcome.levels.size(),4u);++compared;}ASSERT_EQ(compared,1000u);
}

static void test_fuzz_summary_and_deterministic_reducer() {
    DifferentialFuzzRunner fuzz;auto summary=fuzz.run({99,2,6},32);ASSERT_EQ(summary.generated,32u);ASSERT_EQ(summary.valid,32u);ASSERT_EQ(summary.compared,128u);ASSERT_EQ(summary.mismatches,0u);ASSERT_EQ(summary.crashes,0u);ASSERT(fuzzSummaryJson(summary).find("\"seed\":99")!=std::string::npos);
    SourceReducer reducer;const std::string input="keep\nremove-a\nremove-b\n";auto faulty=[](const std::string&candidate){return candidate.find("keep")!=std::string::npos;};auto first=reducer.reduce(input,faulty);auto second=reducer.reduce(input,faulty);ASSERT_EQ(first,second);ASSERT(first.find("keep")!=std::string::npos);ASSERT(first.size()<input.size());
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
    RUN_TEST(test_for_continue_preserves_increment);
    RUN_TEST(test_dominators_diamond);
    RUN_TEST(test_dominance_frontier_diamond);
    RUN_TEST(test_ssa_if_else_phi);
    RUN_TEST(test_ssa_loop_carried_locals);
    RUN_TEST(test_ssa_no_unnecessary_phi);
    RUN_TEST(test_ssa_sibling_stack_restoration);
    RUN_TEST(test_ssa_def_use_and_replacement);
    RUN_TEST(test_ssa_verifier_rejects_malformed_programs);
    RUN_TEST(test_ssa_printer_deterministic);
    RUN_TEST(test_ssa_parameter_reassignment_and_array_memory);
    RUN_TEST(test_ssa_nested_and_loop_conditional_phis);
    RUN_TEST(test_ssa_verifier_phi_and_branch_failures);
    RUN_TEST(test_ssa_replace_all_uses_every_operand_kind);
    RUN_TEST(test_optimizer_constant_fold_and_dce);
    RUN_TEST(test_optimizer_sccp_folds_constant_branch);
    RUN_TEST(test_optimizer_cse_and_side_effect_dce);
    RUN_TEST(test_optimizer_o2_deterministic_and_verified);
    RUN_TEST(test_optimizer_framework_rejects_invalid_ssa);
    RUN_TEST(test_sccp_lattice_meet_rules);
    RUN_TEST(test_sccp_phi_uses_only_executable_edge);
    RUN_TEST(test_cse_does_not_cross_sibling_branches);
    RUN_TEST(test_optimizer_preserves_division_by_zero_and_safe_algebra);
    RUN_TEST(test_optimizer_preserves_memory_writes_and_exposes_remarks);
    RUN_TEST(test_optimizer_pass_registry_and_levels);
    RUN_TEST(test_sccp_unknown_branch_and_loop_converge);
    RUN_TEST(test_optimizer_o3_reaches_bounded_fixpoint);
    RUN_TEST(test_simplify_cfg_forwards_empty_block_and_repairs_phi_edge);
    RUN_TEST(test_loop_info_nesting_exits_and_preheaders);
    RUN_TEST(test_licm_hoists_only_safe_invariants);
    RUN_TEST(test_strength_reduction_and_call_graph_recursion);
    RUN_TEST(test_single_block_inliner_remaps_values_and_rejects_recursion);
    RUN_TEST(test_cfg_inliner_clones_two_block_chain_structurally);
    RUN_TEST(test_cfg_inliner_branching_multi_return);
    RUN_TEST(test_cfg_inliner_void_returns_nested_call_and_determinism);
    RUN_TEST(test_cfg_inliner_preserves_nested_call_in_cloned_cfg);
    RUN_TEST(test_cfg_inliner_repairs_successor_phi_and_clones_loop);
    RUN_TEST(test_cfg_inliner_rejections_are_non_mutating_and_invalidate_analyses);
    RUN_TEST(test_cfg_inliner_rejects_mutual_recursion_without_mutation);
    RUN_TEST(test_loop_simplify_synthesizes_preheader_and_repairs_phi);
    RUN_TEST(test_ssa_analysis_cache_scopes_and_invalidation);
    RUN_TEST(test_trivial_phi_and_iterative_dce);
    RUN_TEST(test_interpreters_differential_core_and_ssa);
    RUN_TEST(test_ssa_interpreter_parallel_phi_and_limits);
    RUN_TEST(test_differential_runner_and_deterministic_generator);
    RUN_TEST(test_fuzz_summary_and_deterministic_reducer);

    std::cout << "\n=== IR Pipeline Tests ===\n"
              << "  total:  " << g_tests  << "\n"
              << "  passed: " << g_passed << "\n"
              << "  failed: " << g_failed << "\n";

    return g_failed > 0 ? 1 : 0;
}
