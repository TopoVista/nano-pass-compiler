// NanoPass Compiler Lab — main driver
//
// Compilation pipeline:
//   Source → Lex → Parse → Resolve → TypeCheck
//   → DesugarFor → DesugarIncDec → DesugarPlusAssign
//   → LLVM IR → (optional) native / JIT
//
// All desugaring passes run in a deterministic order on the typed AST before
// LLVM lowering.  Each pass transforms the AST in-place, returning an
// equivalent program with one fewer high-level construct.

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include "codegen/llvm_codegen.h"
#include "codegen/lower_stmt.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "passes/desugar_for.h"
#include "passes/desugar_inc_dec.h"
#include "passes/desugar_plus_assign.h"
#include "sema/resolve_scopes.h"
#include "sema/type_check.h"
#include "visualize/ast_graphviz.h"


// ============================================================
// Version
// ============================================================
namespace {
constexpr std::string_view kVersion = "nano 0.2.0";
constexpr std::string_view kBuildInfo =
    "NanoPass Compiler Lab — Phase 1 stabilized\n"
    "Pipeline: Lex → Parse → Resolve → TypeCheck\n"
    "        → DesugarFor → DesugarIncDec → DesugarPlusAssign\n"
    "        → LLVM IR\n";

// ============================================================
// Command / Option types
// ============================================================
enum class Command {
  Emit,
  Compile,
  Verify,
  Run,     // (stub — requires JIT or native execution)
  Explain, // (stub — full pipeline explanation)
  Graph,   // (stub — CFG/dominator output)
  Optimize,// (stub — custom optimization pipeline)
};

struct Options {
  Command command    = Command::Emit;
  std::string input;
  std::string format  = "llvm";   // tokens|ast|desugar|llvm
  std::string output;
  std::string graphTarget; // cfg | dominator-tree | callgraph
  std::string passes;      // comma-separated pass list for --optimize
  bool debugPassManager = false;
  bool verbose          = false;
};

// ============================================================
// Help
// ============================================================
void printHelp(std::ostream &out) {
  out << R"(NanoPass Compiler Lab )" << kVersion << R"(

Usage:
  nano <command> <file> [options]

Commands:
  emit <file>     Emit an intermediate representation to stdout.
  compile <file>  Compile to LLVM IR (or native when supported).
  verify <file>   Run all available compiler invariant checks.
  run <file>      Execute the program (requires JIT; not yet available).
  explain <file>  Show the full transformation journey (not yet available).
  graph <file>    Emit a Graphviz graph (not yet available).
  optimize <file> Run the Nano optimizer (not yet available).

  nano --help     Show this message.
  nano --version  Print version and exit.

Emit formats:
  --format=tokens     Tokenize only.
  --format=ast        Parse and print the AST.
  --format=desugar    Desugar (for/++/+=) and print the resulting AST.
  --format=llvm       Lower to LLVM IR (default).

Compile options:
  --emit-llvm         Emit LLVM textual IR.
  -o <file>           Write output to <file> instead of stdout.

Planned (not yet implemented):
  --format=anf        ANF-normalized AST.
  --format=cfg        Control-flow graph.
  --format=ssa        SSA IR.
  --format=optimized  Optimized SSA.
  --opt=<0|1|2|3>     Optimization level.
  --passes=<list>     Custom pass pipeline.
  --debug-pass-manager  Print pass execution log.
  --cfg / --dominator-tree / --callgraph  (nano graph)
  --diagnostics=json  Machine-readable diagnostics.
  --telemetry=<file>  Write per-pass JSON telemetry.
  nano jit <file>
  nano profile <file>
  nano benchmark <file>
)";
}

// ============================================================
// Argument parsing helpers
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
  if (argc < 2)
    return false;

  int i = 1;
  const std::string first = argv[i];

  // Command dispatch
  if      (first == "emit")     { o.command = Command::Emit;     ++i; }
  else if (first == "compile")  { o.command = Command::Compile;  ++i; }
  else if (first == "verify")   { o.command = Command::Verify;   ++i; }
  else if (first == "run")      { o.command = Command::Run;      ++i; }
  else if (first == "explain")  { o.command = Command::Explain;  ++i; }
  else if (first == "graph")    { o.command = Command::Graph;    ++i; }
  else if (first == "optimize") { o.command = Command::Optimize; ++i; }
  else if (!first.empty() && first.front() != '-') {
    // Bare file — compatibility: emit LLVM
    o.command = Command::Emit;
    o.format  = "llvm";
  } else {
    err << "error: unknown command '" << first << "'\n";
    return false;
  }

  for (; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (arg == "-o") {
      if (!takeValue(i, argc, argv, o.output, arg, err))
        return false;
    } else if (arg == "--emit-llvm") {
      o.format = "llvm";
    } else if (arg.starts_with("--format=")) {
      o.format = std::string(arg.substr(9));
    } else if (arg.starts_with("--passes=")) {
      o.passes = std::string(arg.substr(9));
    } else if (arg == "--cfg") {
      o.graphTarget = "cfg";
    } else if (arg == "--dominator-tree") {
      o.graphTarget = "dominator-tree";
    } else if (arg == "--callgraph") {
      o.graphTarget = "callgraph";
    } else if (arg == "--debug-pass-manager") {
      o.debugPassManager = true;
    } else if (arg == "--verbose" || arg == "-v") {
      o.verbose = true;
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

  if (o.input.empty()) {
    err << "error: expected an input file\n";
    return false;
  }
  if (o.command == Command::Compile && o.format != "llvm") {
    err << "error: native executable emission is not yet implemented; "
           "use --emit-llvm\n";
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
    TOK(PRINT); TOK(RETURN);
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
// Desugaring pipeline
//
// Runs in dependency order:
//   1. DesugarFor        : for(init;cond;inc) → {init; while(cond){body;inc}}
//   2. DesugarIncDec     : x++ → x = x + 1,  x-- → x = x - 1
//   3. DesugarPlusAssign : x += e → x = x + e
//
// All three passes must be applied before LLVM lowering because the LLVM
// backend does not handle ForStmt, nor do the increment / compound-assign
// surface forms have direct LLVM mappings in the current codegen.
// ============================================================
void runDesugarPipeline(std::vector<std::unique_ptr<Stmt>> &program,
                        bool debug) {
  DesugarForPass desugarFor;
  DesugarIncDecPass desugarIncDec;
  DesugarPlusAssignPass desugarPlusAssign;

  for (auto &stmt : program) {
    if (debug) std::cerr << "[pass] DesugarFor\n";
    stmt = desugarFor.transform(std::move(stmt));

    if (debug) std::cerr << "[pass] DesugarIncDec\n";
    stmt = desugarIncDec.transformStmt(std::move(stmt));

    if (debug) std::cerr << "[pass] DesugarPlusAssign\n";
    stmt = desugarPlusAssign.transformStmt(std::move(stmt));
  }
}

} // namespace

// ============================================================
// main
// ============================================================
int main(int argc, char **argv) {
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

  // -------------------------------------------------------------------
  // Stub commands that are planned but not yet implemented
  // -------------------------------------------------------------------
  if (options.command == Command::Run) {
    std::cerr << "error: 'nano run' requires JIT execution which is not yet "
                 "implemented.\n"
                 "       Compile with --emit-llvm and run via lli, or wait "
                 "for the ORC JIT phase.\n";
    return 2;
  }
  if (options.command == Command::Explain) {
    std::cerr << "note: 'nano explain' is planned. Use --format=tokens, "
                 "--format=ast, --format=desugar, or --format=llvm to inspect "
                 "individual stages.\n";
    return 2;
  }
  if (options.command == Command::Graph) {
    if (options.format == "ast" && !options.output.empty()) {
      // We can at least emit an AST Graphviz dot file now.
      // Full CFG/dominator graphs are planned.
    } else {
      std::cerr << "note: 'nano graph' is planned. Use --format=ast -o "
                    "out.dot for a basic AST dot file.\n";
      return 2;
    }
  }
  if (options.command == Command::Optimize) {
    std::cerr << "note: 'nano optimize' is planned. The SSA optimizer (SCCP, "
                 "DCE, LICM, ...) is not yet implemented.\n";
    return 2;
  }

  // -------------------------------------------------------------------
  // Read source
  // -------------------------------------------------------------------
  std::ifstream file(options.input);
  if (!file) {
    std::cerr << "error: could not open '" << options.input << "'\n";
    return 1;
  }
  std::stringstream buf;
  buf << file.rdbuf();
  const std::string source = buf.str();

  try {
    // ---- Lex ----
    if (options.debugPassManager) std::cerr << "[pass] Lex\n";
    Lexer lexer(source);
    auto tokens = lexer.scanTokens();

    if (options.command == Command::Emit && options.format == "tokens") {
      printTokens(tokens);
      return 0;
    }

    // ---- Parse ----
    if (options.debugPassManager) std::cerr << "[pass] Parse\n";
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();

    if (options.command == Command::Emit && options.format == "ast") {
      for (const auto &stmt : program)
        stmt->print(0);
      return 0;
    }

    // ---- Scope resolution ----
    if (options.debugPassManager) std::cerr << "[pass] Resolve\n";
    ResolveScopesPass resolver;
    resolver.resolve(program);

    // ---- Type checking ----
    if (options.debugPassManager) std::cerr << "[pass] TypeCheck\n";
    TypeCheckPass typeChecker;
    typeChecker.check(program);

    // ---- Desugaring ----
    runDesugarPipeline(program, options.debugPassManager);

    if (options.command == Command::Emit && options.format == "desugar") {
      for (const auto &stmt : program)
        stmt->print(0);
      return 0;
    }

    // ---- AST Graphviz (nano graph --format=ast -o out.dot) ----
    if (options.command == Command::Graph ||
        (options.command == Command::Emit && options.format == "dot")) {
      const std::string dotFile =
          options.output.empty() ? "ast.dot" : options.output;
      ASTGraphviz viz(dotFile);
      viz.draw(program);
      std::cout << "AST graph written to '" << dotFile << "'\n";
      return 0;
    }

    // ---- Verify (pre-LLVM stages) ----
    if (options.command == Command::Verify) {
      std::cout << "PASS  Parse\n"
                   "PASS  Resolve\n"
                   "PASS  TypeCheck\n"
                   "PASS  DesugarFor\n"
                   "PASS  DesugarIncDec\n"
                   "PASS  DesugarPlusAssign\n";
    }

    // ---- Reject non-LLVM formats that aren't implemented yet ----
    if (options.format != "llvm" &&
        options.command != Command::Verify) {
      std::cerr << "error: emit format '" << options.format
                << "' is planned but not yet implemented.\n"
                   "       Supported formats: tokens, ast, desugar, llvm\n";
      return 2;
    }

    // ---- LLVM lowering ----
    if (options.debugPassManager) std::cerr << "[pass] LLVMCodegen\n";
    llvm::LLVMContext context;
    llvm::Module module("nano_module", context);
    LLVMCodegen codegen(context, &module);

    // Predeclare all functions so forward calls and mutual recursion work.
    for (const auto &stmt : program) {
      if (auto *fn = dynamic_cast<FunctionStmt *>(stmt.get()))
        declareFunction(codegen, fn);
    }
    // Lower function bodies.
    for (const auto &stmt : program)
      lowerStmt(codegen, stmt.get());

    if (!module.getFunction("main")) {
      std::cerr << "error: no 'main' function defined\n";
      return 1;
    }

    if (llvm::verifyModule(module, &llvm::errs())) {
      std::cerr << "internal compiler error: LLVM module verification failed\n";
      return 1;
    }

    if (options.command == Command::Verify) {
      std::cout << "PASS  LLVM\n"
                   "Total: 7 stages, 0 failures\n";
      return 0;
    }

    // ---- Emit LLVM IR ----
    if (options.output.empty()) {
      module.print(llvm::outs(), nullptr);
    } else {
      std::error_code ec;
      llvm::raw_fd_ostream out(options.output, ec, llvm::sys::fs::OF_Text);
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
      std::cerr << " at " << options.input << ':' << err.line << ':' << err.col;
    std::cerr << '\n';
    return 1;
  } catch (const std::exception &err) {
    std::cerr << "error: " << err.what() << '\n';
    return 1;
  }

  return 0;
}
