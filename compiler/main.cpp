#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>


#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include "codegen/llvm_codegen.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "sema/resolve_scopes.h"
#include "sema/type_check.h"

extern void lowerStmt(LLVMCodegen &, Stmt *);

int main(int argc, char **argv) {

  if (argc < 2) {
    std::cerr << "Usage: compiler <file>\n";
    return 1;
  }

  std::ifstream file(argv[1]);
  if (!file) {
    std::cerr << "Could not open file\n";
    return 1;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string source = buffer.str();

  try {

    // -------------------------
    // LEX
    // -------------------------
    Lexer lexer(source);
    auto tokens = lexer.scanTokens();

    // -------------------------
    // PARSE
    // -------------------------
    Parser parser(tokens);
    auto program = parser.parseProgram();

    // --------------------------------
    // SEMANTIC ANALYSIS
    // --------------------------------
    ResolveScopesPass resolver;
    resolver.resolve(program);

    TypeCheckPass typeChecker;
    typeChecker.check(program);

    // --------------------------------
    // LLVM SETUP (Only if semantic OK)
    // --------------------------------
    llvm::LLVMContext ctx;
    llvm::Module module("nano_module", ctx);
    LLVMCodegen cg(ctx, &module);

    bool foundMain = false;

    for (auto &stmt : program) {

      auto *fn = dynamic_cast<FunctionStmt *>(stmt.get());

      if (!fn) {
        std::cerr
            << "Error: Only function declarations allowed at top level.\n";
        return 1;
      }

      if (fn->name == "main")
        foundMain = true;

      lowerStmt(cg, stmt.get());
    }

    if (!foundMain) {
      std::cerr << "Error: No 'main' function defined.\n";
      return 1;
    }

    // -------------------------
    // VERIFY
    // -------------------------
    if (llvm::verifyModule(module, &llvm::errs())) {
      llvm::errs() << "LLVM verification failed\n";
      return 1;
    }

    module.print(llvm::outs(), nullptr);

  } catch (const CompileError &e) {
    std::cerr << "Compilation failed:\n";
    std::cerr << "Error at line " << e.line << ", column " << e.col << ": "
              << e.message << "\n";
    return 1;
  } catch (const std::exception &e) {
    std::cerr << "Internal compiler error:\n";
    std::cerr << e.what() << "\n";
    return 1;
  }

  return 0;
}
