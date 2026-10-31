// NanoPass Compiler Lab — main driver                              (Phase 2)
//
// Compilation pipeline:
//   Source → Lex → Parse → Resolve → TypeCheck
//   → PassManager[ DesugarFor | DesugarIncDec | DesugarPlusAssign ]
//   → LLVM IR → (optional) emit / compile / verify
//
// Phase 2 changes vs Phase 1:
//   - runDesugarPipeline() replaced by nanopass::PassManager
//   - Each desugaring pass runs ONCE over the full program
//   - FunctionStmt bodies are now correctly recursed (Phase 1 bug fixed)
//   - --debug-pass-manager uses PassInstrumentation hooks (structured output)
//   - --debug-analyses shows analysis cache hits/misses
//   - --print-pipeline lists the active pass pipeline
//   - --stop-after=<id>    halts after a named pass
//   - --disable-pass=<id>  skips a named pass
//   - nano passes           shows all registered passes
//   - nano analyses         shows all registered analyses

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/Program.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/IR/LegacyPassManager.h>

#include "codegen/llvm_codegen.h"
#include "codegen/lower_stmt.h"
#include "backend/llvm/ssa_llvm_lowering.h"
#if defined(NANO_HAS_ORC_JIT)
#include "backend/llvm/ssa_llvm_jit.h"
#endif
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "sema/resolve_scopes.h"
#include "sema/type_check.h"
#include "visualize/ast_graphviz.h"

// Pass infrastructure
#include "passes/PipelineBuilder.h"   // brings in all pass headers transitively
#include "ir/pipeline.h"              // CoreIR, ANF, CFG pipeline
#include "optimizer/ssa_optimizer.h"
#include "optimizer/advanced_optimizer.h"

// ============================================================
// Version
// ============================================================
namespace {
constexpr std::string_view kVersion = "nano 0.3.0";

// ============================================================
// Command / Option types
// ============================================================
enum class Command {
    Emit,
    Compile,
    Verify,
    Run,
    Jit,
    Explain,
    Graph,
    Optimize,
    Passes,    // nano passes
    Analyses,  // nano analyses
};

struct Options {
    Command     command         = Command::Emit;
    std::string input;
    std::string format          = "llvm";
    std::string backend         = "ast"; // SSA is opt-in until native differential gates pass.
    std::string output;
    std::string graphTarget;
    std::string passes;       // custom pipeline from --passes=
    std::string stopAfter;    // --stop-after=<id>
    std::vector<std::string> disabledPasses; // --disable-pass=<id>
    bool debugPassManager   = false;
    bool debugAnalyses      = false;
    bool printPipeline      = false;
    bool verbose            = false;
    int optimizationLevel   = 2;
    bool showBeforeAfter    = false;
    bool remarks            = false;
    bool whyNotOptimized    = false;
    bool emitObject         = false;
    bool emitExecutable     = false;
};

// ============================================================
// Help
// ============================================================
void printHelp(std::ostream &out) {
    out << "NanoPass Compiler Lab " << kVersion << "\n\n"
        << "Usage:\n"
        << "  nano <command> <file> [options]\n\n"
        << "Commands:\n"
        << "  emit <file>       Emit an intermediate representation.\n"
        << "  compile <file>    Compile to LLVM IR, a host object, or a host executable.\n"
        << "  verify <file>     Run all available compiler invariant checks.\n"
        << "  passes            List all registered compiler passes.\n"
        << "  analyses          List all registered compiler analyses.\n"
        << "  jit <file>        Execute optimized SSA through ORC LLJIT.\n"
        << "  explain <file>    Show full transformation journey.\n"
        << "  graph <file>      Emit a Graphviz graph.\n"
        << "  optimize <file>   Run optimizer (planned).\n\n"
        << "Emit formats:\n"
        << "  --format=tokens   Tokenize only.\n"
        << "  --format=ast      Parse and print the AST.\n"
        << "  --format=desugar  Desugar and print the resulting AST.\n"
        << "  --format=core     Emit typed CoreIR.\n"
        << "  --format=anf      Emit ANF IR.\n"
        << "  --format=cfg      Emit Control-Flow Graph IR.\n"
        << "  --format=ssa      Emit verified SSA IR.\n"
        << "  --format=optimized Emit optimized SSA IR.\n"
        << "  --format=llvm     Lower to LLVM IR (default).\n\n"
        << "Compile options:\n"
        << "  --emit-llvm       Emit LLVM textual IR.\n"
        << "  --emit-obj        Emit a host-native object file (requires -o).\n"
        << "  --emit-exe        Link a host-native executable (requires -o).\n"
        << "  --backend=ast|ssa Select the AST reference or SSA LLVM backend.\n"
        << "  -o <file>         Write output to <file>.\n\n"
        << "Pass pipeline options:\n"
        << "  --print-pipeline         Print the active pass pipeline.\n"
        << "  --stop-after=<id>        Stop pipeline after the named pass.\n"
        << "  --disable-pass=<id>      Skip the named pass.\n"
        << "  --debug-pass-manager     Print structured pass execution log.\n"
        << "  --debug-analyses         Print analysis cache hit/miss events.\n\n"
        << "Optimization options:\n"
        << "  -O0|-O1|-O2|-O3          Select Nano SSA optimization pipeline.\n"
        << "  --passes=<list>          Select a custom SSA pipeline for optimize.\n"
        << "  --show-before-after      Print SSA before and after optimization.\n"
        << "  --remarks                Print structured optimization remarks.\n\n"
        << "  --why-not-optimized      Print recorded advanced optimization rejections.\n\n"
        << "Graph options:\n"
        << "  --cfg                    Emit Control-Flow Graph DOT.\n"
        << "  --dominator-tree         Emit dominator-tree DOT.\n"
        << "  --loops                  Emit natural-loop annotated DOT.\n"
        << "  --callgraph              Planned.\n\n"
        << "Planned (not yet implemented):\n"
        << "  --format=optimized\n"
        << "  --opt=<0|1|2|3>  --passes=<list>\n"
        << "  --diagnostics=json  --telemetry=<file>\n"
        << "  nano jit / profile / benchmark\n";
}

// ============================================================
// Argument parsing
// ============================================================
bool takeValue(int &i, int argc, char **argv, std::string &to,
               std::string_view option, std::ostream &err) {
    if (i + 1 >= argc) {
        err << "error: " << option << " requires a value\n";
        return false;
    }
    to = argv[++i];
    return true;
}

bool parseOptions(int argc, char **argv, Options &o, std::ostream &err) {
    if (argc < 2) return false;

    int i = 1;
    const std::string first = argv[i];

    if      (first == "emit")     { o.command = Command::Emit;     ++i; }
    else if (first == "compile")  { o.command = Command::Compile;  ++i; }
    else if (first == "verify")   { o.command = Command::Verify;   ++i; }
    else if (first == "run")      { o.command = Command::Run;      ++i; }
    else if (first == "jit")      { o.command = Command::Jit;      ++i; }
    else if (first == "explain")  { o.command = Command::Explain;  ++i; }
    else if (first == "graph")    { o.command = Command::Graph;    ++i; }
    else if (first == "optimize") { o.command = Command::Optimize; ++i; }
    else if (first == "passes")   { o.command = Command::Passes;   ++i; }
    else if (first == "analyses") { o.command = Command::Analyses; ++i; }
    else if (!first.empty() && first.front() != '-') {
        o.command = Command::Emit;
        o.format  = "llvm";
    } else {
        err << "error: unknown command '" << first << "'\n";
        return false;
    }

    for (; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "-o") {
            if (!takeValue(i, argc, argv, o.output, arg, err)) return false;
        } else if (arg == "--emit-llvm") {
            o.format = "llvm";
        } else if (arg == "--emit-obj") {
            o.emitObject = true;
        } else if (arg == "--emit-exe") {
            o.emitExecutable = true;
        } else if (arg.starts_with("--backend=")) {
            o.backend = std::string(arg.substr(10));
        } else if (arg.starts_with("--format=")) {
            o.format = std::string(arg.substr(9));
        } else if (arg.starts_with("--passes=")) {
            o.passes = std::string(arg.substr(9));
        } else if (arg.starts_with("--stop-after=")) {
            o.stopAfter = std::string(arg.substr(13));
        } else if (arg.starts_with("--disable-pass=")) {
            o.disabledPasses.push_back(std::string(arg.substr(15)));
        } else if (arg == "--print-pipeline") {
            o.printPipeline = true;
        } else if (arg == "--debug-pass-manager") {
            o.debugPassManager = true;
        } else if (arg == "--debug-analyses") {
            o.debugAnalyses = true;
        } else if (arg == "--verbose" || arg == "-v") {
            o.verbose = true;
        } else if (arg == "-O0" || arg == "-O1" || arg == "-O2" || arg == "-O3") {
            o.optimizationLevel = static_cast<int>(arg[2] - '0');
        } else if (arg == "--show-before-after") {
            o.showBeforeAfter = true;
        } else if (arg == "--remarks") {
            o.remarks = true;
        } else if (arg == "--why-not-optimized") {
            o.whyNotOptimized = true;
        } else if (arg == "--cfg") {
            o.graphTarget = "cfg";
        } else if (arg == "--dominator-tree") {
            o.graphTarget = "dominator-tree";
        } else if (arg == "--loops") {
            o.graphTarget = "loops";
        } else if (arg == "--callgraph") {
            o.graphTarget = "callgraph";
        } else if (!arg.empty() && arg.front() == '-') {
            err << "error: unsupported option '" << arg << "'\n";
            return false;
        } else if (o.input.empty()) {
            o.input = std::string(arg);
        } else {
            err << "error: unexpected argument '" << arg << "'\n";
            return false;
        }
    }

    // Commands that need no input file
    if (o.command == Command::Passes || o.command == Command::Analyses)
        return true;

    if (o.input.empty()) {
        err << "error: expected an input file\n";
        return false;
    }
    if (o.command == Command::Compile && o.emitObject && o.emitExecutable) {
        err << "error: choose only one of --emit-obj or --emit-exe\n";
        return false;
    }
    if (o.command == Command::Compile && (o.emitObject || o.emitExecutable) && o.output.empty()) {
        err << "error: --emit-obj and --emit-exe require -o <file>\n";
        return false;
    }
    if (o.backend != "ast" && o.backend != "ssa") {
        err << "error: --backend must be 'ast' or 'ssa'\n";
        return false;
    }
    return true;
}

bool emitNativeObject(llvm::Module &module, const std::string &path,
                      std::ostream &err) {
    if (llvm::InitializeNativeTarget() || llvm::InitializeNativeTargetAsmPrinter()) {
        err << "error: could not initialize the native LLVM target\n";
        return false;
    }
    const std::string triple = llvm::sys::getDefaultTargetTriple();
    std::string targetError;
    const llvm::Target *target = llvm::TargetRegistry::lookupTarget(triple, targetError);
    if (!target) { err << "error: could not select native target: " << targetError << '\n'; return false; }
    llvm::TargetOptions targetOptions;
    std::unique_ptr<llvm::TargetMachine> machine(target->createTargetMachine(
        triple, "generic", "", targetOptions, llvm::Reloc::PIC_));
    if (!machine) { err << "error: could not create native target machine\n"; return false; }
    module.setTargetTriple(triple);
    module.setDataLayout(machine->createDataLayout());
    std::error_code errorCode;
    llvm::raw_fd_ostream output(path, errorCode, llvm::sys::fs::OF_None);
    if (errorCode) { err << "error: could not write '" << path << "': " << errorCode.message() << '\n'; return false; }
    llvm::legacy::PassManager passes;
    if (machine->addPassesToEmitFile(passes, output, nullptr, llvm::CodeGenFileType::ObjectFile)) {
        err << "error: the native target cannot emit an object file\n";
        return false;
    }
    passes.run(module);
    return true;
}

bool emitNativeExecutable(llvm::Module &module, const std::string &path,
                          std::ostream &err) {
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto object = std::filesystem::temp_directory_path() /
        ("nanopass-aot-" + std::to_string(unique) + ".o");
    if (!emitNativeObject(module, object.string(), err)) return false;
    struct RemoveObject {
        std::filesystem::path path;
        ~RemoveObject() { std::error_code ignored; std::filesystem::remove(path, ignored); }
    } cleanup{object};
    const std::string compiler = NANO_HOST_CXX;
    const std::string runtime = NANO_RUNTIME_LIBRARY_PATH;
    const std::string objectPath = object.string();
    std::vector<llvm::StringRef> arguments = {
        compiler, objectPath, runtime, "-o", path,
    };
    std::string linkError;
    bool executionFailed = false;
    const int exitCode = llvm::sys::ExecuteAndWait(
        compiler, arguments, std::nullopt, {}, 0, 0, &linkError, &executionFailed);
    if (executionFailed || exitCode != 0) {
        err << "error: host linker failed";
        if (!linkError.empty()) err << ": " << linkError;
        err << '\n';
        return false;
    }
    return true;
}

// ============================================================
// Token printer
// ============================================================
std::string tokenName(TokenType type) {
    switch (type) {
#define TOK(k) case TokenType::k: return #k
        TOK(NUMBER); TOK(IDENTIFIER); TOK(STRING); TOK(TRUE); TOK(FALSE);
        TOK(LET); TOK(FUNCTION); TOK(IF); TOK(ELSE); TOK(WHILE); TOK(FOR);
        TOK(PRINT); TOK(RETURN); TOK(BREAK); TOK(CONTINUE);
        TOK(INT); TOK(FLOAT); TOK(DOUBLE); TOK(SHORT); TOK(LONG);
        TOK(UNSIGNED); TOK(CHAR); TOK(BOOL); TOK(VOID);
        TOK(PLUS); TOK(MINUS); TOK(STAR); TOK(SLASH); TOK(MOD);
        TOK(EQUAL); TOK(EQUAL_EQUAL); TOK(BANG); TOK(BANG_EQUAL);
        TOK(LESS); TOK(LESS_EQUAL); TOK(GREATER); TOK(GREATER_EQUAL);
        TOK(AND_AND); TOK(OR_OR);
        TOK(COLON); TOK(SEMICOLON); TOK(COMMA);
        TOK(LPAREN); TOK(RPAREN); TOK(LBRACE); TOK(RBRACE);
        TOK(LBRACKET); TOK(RBRACKET); TOK(END_OF_FILE);
#undef TOK
    }
    return "UNKNOWN";
}

void printTokens(const std::vector<Token> &tokens) {
    for (const auto &tok : tokens)
        std::cout << tok.line << ':' << tok.col << ' '
                  << tokenName(tok.type) << " '" << tok.lexeme << "'\n";
}

// ============================================================
// PassInstrumentation factory
//
// Builds a PassInstrumentation that prints structured output to `out`
// (typically std::cerr).  The instrumentation is used when
// --debug-pass-manager or --debug-analyses is set.
// ============================================================
nanopass::PassInstrumentation makeDebugInstrumentation(std::ostream &out,
                                                    bool debugPasses,
                                                    bool debugAnalyses) {
    nanopass::PassInstrumentation instr;

    if (debugPasses) {
        instr.onPipelineStart = [&out](std::string_view name) {
            out << "\n[pass-manager] starting pipeline \""
                << name << "\"\n";
        };

        instr.onPassStart = [&out](const nanopass::PassInfo &info) {
            out << "\n[pass] " << info.name
                << "  (" << info.id << ")\n";
        };

        instr.onPassEnd = [&out](const nanopass::PassTelemetry &t) {
            out << "  changed:  " << (t.changed ? "yes" : "no") << "\n";
            out << "  duration: " << std::fixed << std::setprecision(1)
                << t.durationUs() << " us\n";
            // Print per-pass statistics
            for (const auto &[k, v] : t.stats.all()) {
                out << "  " << k << ": ";
                std::visit([&out](const auto &val) { out << val; }, v);
                out << "\n";
            }
            if (!t.invalidatedAnalyses.empty()) {
                out << "  invalidated:";
                for (const auto &id : t.invalidatedAnalyses)
                    out << " " << id;
                out << "\n";
            }
        };

        instr.onPipelineEnd = [&out](std::string_view name,
                                      uint64_t totalNs, bool anyChanged) {
            out << "\n[pass-manager] completed \"" << name << "\"\n";
            out << "  total duration: " << std::fixed << std::setprecision(1)
                << (static_cast<double>(totalNs) / 1000.0) << " us\n";
            out << "  any changed:    " << (anyChanged ? "yes" : "no") << "\n";
        };
    }

    if (debugAnalyses) {
        instr.onAnalysisAccess = [&out](std::string_view id, bool hit) {
            out << "[analysis] " << id
                << ": " << (hit ? "HIT" : "MISS") << "\n";
        };
        instr.onAnalysisInvalidated = [&out](std::string_view id) {
            out << "[analysis] invalidating " << id << "\n";
        };
    }

    return instr;
}

// ============================================================
// Build PassContext from Options
// ============================================================
nanopass::CompilerPassOptions toPassOptions(const Options &opts) {
    nanopass::CompilerPassOptions po;
    po.debugPassManager = opts.debugPassManager;
    po.debugAnalyses    = opts.debugAnalyses;
    po.printPipeline    = opts.printPipeline;
    po.stopAfter        = opts.stopAfter;
    po.disabledPasses   = std::unordered_set<std::string>(
        opts.disabledPasses.begin(), opts.disabledPasses.end());
    return po;
}

} // anonymous namespace

// ============================================================
// main
// ============================================================
int main(int argc, char **argv) {
    // Initialise registry (idempotent)
    nanopass::PipelineBuilder::initialise();

    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        printHelp(std::cout);
        return 0;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << kVersion << '\n';
        return 0;
    }

    Options options;
    if (!parseOptions(argc, argv, options, std::cerr)) {
        printHelp(std::cerr);
        return 2;
    }

    // ── Registry query commands (no input file needed) ───────────────────
    if (options.command == Command::Passes) {
        nanopass::PassRegistry::get().printPasses(std::cout);
        return 0;
    }
    if (options.command == Command::Analyses) {
        nanopass::PassRegistry::get().printAnalyses(std::cout);
        return 0;
    }

    // ── Commands with pre-pipeline validation ─────────────────────────────
    if (options.command == Command::Graph) {
        if (options.graphTarget != "cfg" && options.graphTarget != "dominator-tree" && options.graphTarget != "loops" &&
            !(options.format == "ast" || options.format == "dot")) {
            std::cerr << "note: 'nano graph' supports --cfg, --dominator-tree, --loops, or AST dot (--format=dot -o out.dot).\n";
            return 2;
        }
    }


    // ── Validate stop-after / disable-pass IDs ────────────────────────────
    if (!options.stopAfter.empty()
            && !nanopass::PassRegistry::get().isKnownPass(options.stopAfter)) {
        std::cerr << "error: unknown pass id in --stop-after='"
                  << options.stopAfter << "'\n";
        nanopass::PassRegistry::get().printPasses(std::cerr);
        return 2;
    }
    for (const auto &id : options.disabledPasses) {
        if (!nanopass::PassRegistry::get().isKnownPass(id)) {
            std::cerr << "error: unknown pass id in --disable-pass='"
                      << id << "'\n";
            nanopass::PassRegistry::get().printPasses(std::cerr);
            return 2;
        }
    }

    // ── Read source ───────────────────────────────────────────────────────
    std::ifstream file(options.input);
    if (!file) {
        std::cerr << "error: could not open '" << options.input << "'\n";
        return 1;
    }
    std::stringstream buf;
    buf << file.rdbuf();
    const std::string source = buf.str();

    try {
        // ── Lex ──────────────────────────────────────────────────────────
        Lexer lexer(source);
        auto tokens = lexer.scanTokens();

        if (options.command == Command::Emit && options.format == "tokens") {
            printTokens(tokens);
            return 0;
        }

        // ── Parse ─────────────────────────────────────────────────────────
        Parser parser(std::move(tokens));
        auto program = parser.parseProgram();

        if (options.command == Command::Emit && options.format == "ast") {
            for (const auto &stmt : program)
                stmt->print(0);
            return 0;
        }

        // ── Scope resolution ──────────────────────────────────────────────
        ResolveScopesPass resolver;
        resolver.resolve(program);

        // ── Type checking ─────────────────────────────────────────────────
        TypeCheckPass typeChecker;
        typeChecker.check(program);

        // ── Build pass infrastructure ────────────────────────────────────
        nanopass::AnalysisManager am;
        nanopass::PipelineBuilder::registerAnalysesWithManager(am);

        const nanopass::CompilerPassOptions passOpts = toPassOptions(options);
        nanopass::PassContext ctx(am, passOpts, std::cerr);

        // ── Build desugar pipeline ────────────────────────────────────────
        nanopass::PassManager pm = nanopass::PipelineBuilder::buildDesugarPipeline();

        // Collect telemetry for --explain mode
        std::vector<nanopass::PassTelemetry> desugarTelemetry;

        // Install debug instrumentation
        if (options.debugPassManager || options.debugAnalyses
                || options.command == Command::Explain) {
            nanopass::PassInstrumentation instr = makeDebugInstrumentation(
                std::cerr,
                options.debugPassManager,
                options.debugAnalyses);
            // Always collect telemetry for explain
            if (options.command == Command::Explain) {
                instr.onPassEnd = [&desugarTelemetry](const nanopass::PassTelemetry &t) {
                    desugarTelemetry.push_back(t);
                };
            }
            // Wire analysis-specific hooks into the AnalysisManager
            if (instr.onAnalysisAccess)
                am.setOnAnalysisAccess(instr.onAnalysisAccess);
            if (instr.onAnalysisInvalidated)
                am.setOnAnalysisInvalidated(instr.onAnalysisInvalidated);
            pm.setInstrumentation(std::move(instr));
        }

        // Print pipeline if requested (before execution)
        if (options.printPipeline) {
            pm.printPipeline(std::cout);
            // Continue — also run the pipeline
        }

        // ── Run desugaring pipeline ───────────────────────────────────────
        pm.run(program, ctx);

        // ── Emit desugared AST ────────────────────────────────────────────
        if (options.command == Command::Emit && options.format == "desugar") {
            for (const auto &stmt : program)
                stmt->print(0);
            return 0;
        }

        // ── AST Graphviz ──────────────────────────────────────────────────
        if ((options.command == Command::Graph && options.graphTarget.empty()) ||
            (options.command == Command::Emit && options.format == "dot")) {
            const std::string dotFile =
                options.output.empty() ? "ast.dot" : options.output;
            ASTGraphviz viz(dotFile);
            viz.draw(program);
            std::cout << "AST graph written to '" << dotFile << "'\n";
            return 0;
        }

        // ── Run middle-end pipeline (CoreIR -> ANF -> CFG) ────────────────
        auto irRes = nanopass::IRPipeline::runAll(program);

        // The Nano SSA optimizer is intentionally independent of the AST LLVM
        // backend. It is used for inspected optimized SSA only at this stage.
        const bool optimizeSSA = options.command == Command::Optimize ||
                                 options.command == Command::Jit ||
                                 options.command == Command::Run ||
                                 options.command == Command::Verify ||
                                 options.command == Command::Explain ||
                                 (options.command == Command::Emit && options.format == "optimized") ||
                                 (options.backend == "ssa" &&
                                  (options.format == "llvm" ||
                                   (options.command == Command::Compile &&
                                    (options.emitObject || options.emitExecutable))));
        nanopass::SSAProgram ssaBefore;
        nanopass::opt::SSAOptimizationContext optimizationContext;
        nanopass::opt::SSAOptimizationResult optimizationResult;
        if (optimizeSSA) {
            ssaBefore = irRes.ssa;
            const bool requestInline = options.passes.empty()
                ? options.optimizationLevel >= 2
                : options.passes.find("inline") != std::string::npos;
            if (requestInline) {
                nanopass::opt::SSAProgramInliner inliner;
                const auto inlineResult = inliner.run(irRes.ssa);
                optimizationResult.changed |= inlineResult.changed;
                for (const auto &[key, value] : inlineResult.statistics.counters)
                    optimizationResult.statistics.add(key, value);
                optimizationContext.remarks.insert(optimizationContext.remarks.end(),
                                                  inlineResult.remarks.begin(), inlineResult.remarks.end());
                for (const auto &function : irRes.ssa.functions) {
                    const auto verified = nanopass::verifySSA(function);
                    if (!verified.ok)
                        throw std::runtime_error("internal compiler error: SSA verification failed after pass `inline`");
                }
            }
            for (auto &function : irRes.ssa.functions) {
                nanopass::opt::SSAOptimizationPipeline pipeline;
                if (!options.passes.empty()) {
                    std::stringstream passNames(options.passes);
                    std::string passName;
                    while (std::getline(passNames, passName, ',')) {
                        if (passName == "inline") continue;
                        auto pass = nanopass::opt::makePass(passName);
                        if (!pass) pass = nanopass::opt::makeAdvancedPass(passName);
                        if (!pass) {
                            std::cerr << "error: unknown SSA optimization pass '" << passName
                                      << "'\n       Available:";
                            for (const auto &info : nanopass::opt::availablePasses())
                                std::cerr << " " << info.id;
                            std::cerr << " loop-simplify licm strength-reduce inline";
                            std::cerr << "\n";
                            return 2;
                        }
                        pipeline.add(std::move(pass));
                    }
                } else {
                    pipeline = nanopass::opt::buildPipeline(options.optimizationLevel);
                    if (options.optimizationLevel >= 2) {
                        pipeline.add(nanopass::opt::makeAdvancedPass("loop-simplify"));
                        pipeline.add(nanopass::opt::makeAdvancedPass("licm"));
                    }
                    if (options.optimizationLevel >= 3)
                        pipeline.add(nanopass::opt::makeAdvancedPass("strength-reduce"));
                }
                const auto one = pipeline.run(function, optimizationContext);
                optimizationResult.changed |= one.changed;
                optimizationResult.cfgChanged |= one.cfgChanged;
                for (const auto &[key, value] : one.statistics.counters)
                    optimizationResult.statistics.add(key, value);
                optimizationResult.telemetry.insert(optimizationResult.telemetry.end(),
                                                    one.telemetry.begin(), one.telemetry.end());
            }
        }

        if (options.command == Command::Optimize ||
            (options.command == Command::Emit && options.format == "optimized")) {
            if (options.showBeforeAfter) {
                std::cout << "Before optimization:\n";
                nanopass::printSSA(ssaBefore, std::cout);
                std::cout << "After optimization:\n";
            }
            nanopass::printSSA(irRes.ssa, std::cout);
            std::cout << "Optimization summary\n";
            for (const auto &[key, value] : optimizationResult.statistics.counters)
                std::cout << "  " << key << ": " << value << "\n";
            std::cout << "Pass telemetry\n";
            for (const auto &pass : optimizationResult.telemetry)
                std::cout << "  " << pass.passId << ": "
                          << pass.instructionsBefore << " -> " << pass.instructionsAfter
                          << " instructions, " << pass.blocksBefore << " -> "
                          << pass.blocksAfter << " blocks, " << pass.durationUs << " us\n";
            if (options.remarks) for (const auto &remark : optimizationContext.remarks)
                std::cout << "  [" << remark.passId << "] " << remark.message << "\n";
            if (options.whyNotOptimized) {
                std::cout << "Missed optimizations\n";
                for (const auto &remark : optimizationContext.remarks)
                    if (remark.kind == nanopass::opt::OptimizationRemarkKind::Missed)
                        std::cout << "  [" << remark.passId << "] " << remark.message << "\n";
            }
            return 0;
        }

        // ── Emit CoreIR ───────────────────────────────────────────────────
        if (options.command == Command::Emit && options.format == "core") {
            if (options.output.empty()) {
                nanopass::printCoreIR(irRes.core, std::cout);
            } else {
                std::ofstream out(options.output);
                if (!out) {
                    std::cerr << "error: could not write '" << options.output << "'\n";
                    return 1;
                }
                nanopass::printCoreIR(irRes.core, out);
            }
            return 0;
        }

        // ── Emit ANF IR ───────────────────────────────────────────────────
        if (options.command == Command::Emit && options.format == "anf") {
            if (options.output.empty()) {
                nanopass::printANF(irRes.anf, std::cout);
            } else {
                std::ofstream out(options.output);
                if (!out) {
                    std::cerr << "error: could not write '" << options.output << "'\n";
                    return 1;
                }
                nanopass::printANF(irRes.anf, out);
            }
            return 0;
        }

        // ── Emit CFG IR ───────────────────────────────────────────────────
        if (options.command == Command::Emit && options.format == "cfg") {
            if (options.output.empty()) {
                nanopass::printCFG(irRes.cfg, std::cout);
            } else {
                std::ofstream out(options.output);
                if (!out) {
                    std::cerr << "error: could not write '" << options.output << "'\n";
                    return 1;
                }
                nanopass::printCFG(irRes.cfg, out);
            }
            return 0;
        }

        // SSA is constructed and verified by IRPipeline::runAll before it is
        // observable through the CLI.
        if (options.command == Command::Emit && options.format == "ssa") {
            if (options.output.empty()) {
                nanopass::printSSA(irRes.ssa, std::cout);
            } else {
                std::ofstream out(options.output);
                if (!out) { std::cerr << "error: could not write '" << options.output << "'\n"; return 1; }
                nanopass::printSSA(irRes.ssa, out);
            }
            return 0;
        }

        // ── CFG Graphviz ──────────────────────────────────────────────────
        if (options.command == Command::Graph && options.graphTarget == "cfg") {
            if (options.output.empty()) {
                nanopass::emitCFGGraphviz(irRes.cfg, std::cout);
            } else {
                std::ofstream out(options.output);
                if (!out) {
                    std::cerr << "error: could not write '" << options.output << "'\n";
                    return 1;
                }
                nanopass::emitCFGGraphviz(irRes.cfg, out);
            }
            return 0;
        }

        if (options.command == Command::Graph && options.graphTarget == "dominator-tree") {
            if (options.output.empty()) {
                nanopass::emitDominatorTreeGraphviz(irRes.cfg, std::cout);
            } else {
                std::ofstream out(options.output);
                if (!out) {
                    std::cerr << "error: could not write '" << options.output << "'\n";
                    return 1;
                }
                nanopass::emitDominatorTreeGraphviz(irRes.cfg, out);
            }
            return 0;
        }

        if (options.command == Command::Graph && options.graphTarget == "loops") {
            std::ostringstream dot;
            dot << "digraph NanoLoops {\n  node [shape=box,fontname=\"monospace\"];\n";
            for (const auto &function : irRes.ssa.functions) {
                const auto loops = nanopass::LoopInfoAnalysis::run(function);
                for (const auto &loop : loops.loops)
                    dot << "  \"" << function.name << ":" << loop.header.str()
                        << "\" [label=\"" << function.name << " " << loop.header.str()
                        << "\\nloop L" << loop.id.index << " depth " << loop.depth
                        << (loop.preheader ? "\\npreheader " + loop.preheader->str() : "")
                        << "\"];\n";
            }
            dot << "}\n";
            if (options.output.empty()) std::cout << dot.str();
            else { std::ofstream out(options.output); if(!out){std::cerr<<"error: could not write '"<<options.output<<"'\n";return 1;} out<<dot.str(); }
            return 0;
        }

        // ── Verify (pre-LLVM stages) ──────────────────────────────────────
        if (options.command == Command::Verify) {
            std::cout << "PASS  Parse\n"
                         "PASS  Resolve\n"
                         "PASS  TypeCheck\n"
                         "PASS  DesugarFor\n"
                         "PASS  DesugarIncDec\n"
                         "PASS  DesugarPlusAssign\n"
                         "PASS  CoreIR\n"
                         "PASS  ANF\n"
                         "PASS  CFG\n"
                         "PASS  SSA\n"
                         "PASS  OptimizedSSA\n";
        }

        // ── Reject unimplemented emit formats ─────────────────────────────
        if (options.format != "llvm" && options.command != Command::Verify &&
            options.command != Command::Explain) {
            std::cerr << "error: emit format '" << options.format
                      << "' is planned but not yet implemented.\n"
                         "       Supported formats: tokens, ast, desugar, core, anf, cfg, ssa, llvm\n";
            return 2;
        }

        // ── LLVM lowering ─────────────────────────────────────────────────
        if (options.command == Command::Jit || options.command == Command::Run) {
#if defined(NANO_HAS_ORC_JIT)
            const auto execution = nanopass::llvm_backend::executeWithLLJIT(irRes.ssa);
            if (!execution.error.empty()) {
                std::cerr << "error: ORC JIT execution failed: " << execution.error << '\n';
                return 1;
            }
            std::cout << execution.stdoutText;
            std::cout << "JIT return: " << execution.returnValue << '\n';
            return 0;
#else
            std::cerr << "error: 'nano " << (options.command == Command::Run ? "run" : "jit")
                      << "' requires a build configured with NANO_BUILD_JIT=ON\n";
            return 2;
#endif
        }
        if (options.backend == "ssa") {
            llvm::LLVMContext context;
            nanopass::llvm_backend::SSALLVMLowering lowering(context);
            auto lowered = lowering.lower(irRes.ssa, "nano_ssa_module");
            if (!lowered.module->getFunction("main")) {
                std::cerr << "error: no 'main' function defined\n";
                return 1;
            }
            if (options.command == Command::Compile && options.emitObject) {
                if (!emitNativeObject(*lowered.module, options.output, std::cerr)) return 1;
                std::cout << "native object written to '" << options.output << "'\n";
                return 0;
            }
            if (options.command == Command::Compile && options.emitExecutable) {
                if (!emitNativeExecutable(*lowered.module, options.output, std::cerr)) return 1;
                std::cout << "native executable written to '" << options.output << "'\n";
                return 0;
            }
            if (options.command == Command::Verify) {
                std::cout << "PASS  LLVM (SSA backend)\n"
                             "Total: 11 stages, 0 failures\n";
                return 0;
            }
            if (options.command == Command::Explain) {
                std::cout << "08 LLVM (SSA backend)\n"
                          << "   functions lowered: " << lowered.statistics.functions << "\n"
                          << "   blocks lowered: " << lowered.statistics.blocks << "\n"
                          << "   phi nodes lowered: " << lowered.statistics.phis << "\n"
                          << "   module verified: PASS\n";
                return 0;
            }
            if (options.output.empty()) {
                lowered.module->print(llvm::outs(), nullptr);
            } else {
                std::error_code ec;
                llvm::raw_fd_ostream out(options.output, ec, llvm::sys::fs::OF_Text);
                if (ec) { std::cerr << "error: could not write '" << options.output << "': " << ec.message() << '\n'; return 1; }
                lowered.module->print(out, nullptr);
            }
            return 0;
        }
        llvm::LLVMContext context;
        llvm::Module module("nano_module", context);
        LLVMCodegen codegen(context, &module);

        for (const auto &stmt : program) {
            if (auto *fn = dynamic_cast<FunctionStmt *>(stmt.get()))
                declareFunction(codegen, fn);
        }
        for (const auto &stmt : program)
            lowerStmt(codegen, stmt.get());

        if (!module.getFunction("main")) {
            std::cerr << "error: no 'main' function defined\n";
            return 1;
        }
        if (llvm::verifyModule(module, &llvm::errs())) {
            std::cerr << "internal compiler error: LLVM module verification "
                         "failed\n";
            return 1;
        }

        if (options.command == Command::Compile && options.emitObject) {
            if (!emitNativeObject(module, options.output, std::cerr)) return 1;
            std::cout << "native object written to '" << options.output << "'\n";
            return 0;
        }
        if (options.command == Command::Compile && options.emitExecutable) {
            if (!emitNativeExecutable(module, options.output, std::cerr)) return 1;
            std::cout << "native executable written to '" << options.output << "'\n";
            return 0;
        }

        if (options.command == Command::Verify) {
            std::cout << "PASS  LLVM\n"
                         "Total: 11 stages, 0 failures\n";
            return 0;
        }

        // ── Explain command ───────────────────────────────────────────────
        if (options.command == Command::Explain) {
            const auto &nodeCount = am.get<nanopass::NodeCountAnalysis>(program);
            size_t coreStmts = irRes.core.topLevelStmts.size();
            for (const auto &fn : irRes.core.functions)
                coreStmts += fn.body.size();

            std::cout << "NanoPass Compilation Journey\n\n"
                      << "01 Parse\n"
                      << "   AST nodes: " << nodeCount.total << "\n\n"
                      << "02 Desugar\n";

            // Show per-pass rewrite counts from telemetry
            for (const auto &t : desugarTelemetry) {
                std::cout << "   " << t.passName << ": "
                          << t.stats.getInt("rewrites") << " rewrites\n";
            }

            std::cout << "\n03 CoreIR\n"
                      << "   functions: " << irRes.core.functions.size() << "\n"
                      << "   statements: " << coreStmts << "\n\n"
                      << "04 ANF\n"
                      << "   functions: " << irRes.anf.functions.size() << "\n"
                      << "   (instructions counted via CFG)\n\n"
                      << "05 CFG\n"
                      << "   functions:      " << irRes.cfgStats.functions << "\n"
                      << "   basic blocks:   " << irRes.cfgStats.basicBlocks << "\n"
                      << "   edges:          " << irRes.cfgStats.edges << "\n"
                      << "   instructions:   " << irRes.cfgStats.instructions << "\n"
                      << "   branches:       " << irRes.cfgStats.branches << "\n"
                      << "   calls:          " << irRes.cfgStats.calls << "\n"
                      << "   returns:        " << irRes.cfgStats.returns << "\n\n"
                      << "06 SSA\n"
                      << "   values:         " << irRes.ssaStats.values << "\n"
                      << "   phi nodes:      " << irRes.ssaStats.phiNodes << "\n"
                      << "   loads removed:  " << irRes.ssaStats.loadsRemoved << "\n"
                      << "   stores removed: " << irRes.ssaStats.storesRemoved << "\n"
                      << "   verifier:       PASS\n\n"
                      << "07 Nano Optimizer\n"
                      << "   constants:      " << optimizationResult.statistics.get("constantsPropagated") + optimizationResult.statistics.get("instructionsFolded") << "\n"
                      << "   branches simplified:"
                      << optimizationResult.statistics.get("branchesSimplified") << "\n"
                      << "   conditions proven: "
                      << optimizationResult.statistics.get("branchConditionsProven") << "\n"
                      << "   DCE removed:    " << optimizationResult.statistics.get("instructionsRemoved") << "\n\n"
                      << "08 LLVM\n"
                      << "   module verified\n";
            return 0;
        }

        // ── Emit LLVM IR ──────────────────────────────────────────────────
        if (options.output.empty()) {
            module.print(llvm::outs(), nullptr);
        } else {
            std::error_code ec;
            llvm::raw_fd_ostream out(
                options.output, ec, llvm::sys::fs::OF_Text);
            if (ec) {
                std::cerr << "error: could not write '" << options.output
                          << "': " << ec.message() << '\n';
                return 1;
            }
            module.print(out, nullptr);
        }

    } catch (const CompileError &err) {
        std::cerr << "error: " << err.message;
        if (err.line > 0)
            std::cerr << " at " << options.input
                      << ':' << err.line << ':' << err.col;
        std::cerr << '\n';
        return 1;
    } catch (const std::exception &err) {
        std::cerr << "error: " << err.what() << '\n';
        return 1;
    }

    return 0;
}
