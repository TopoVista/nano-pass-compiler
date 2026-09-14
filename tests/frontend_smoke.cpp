#include <exception>
#include <iostream>

#include "common/error.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "sema/resolve_scopes.h"
#include "sema/type_check.h"

int main() {
  try {
    Lexer lexer("int main() { return twice(21); } "
                "int twice(int x) { return x + x; }");
    Parser parser(lexer.scanTokens());
    auto program = parser.parseProgram();
    ResolveScopesPass resolver;
    resolver.resolve(program);
    TypeCheckPass checker;
    checker.check(program);

    try {
      Lexer badLexer("int main() { /*");
      (void)badLexer.scanTokens();
      std::cerr << "unterminated comment was accepted\n";
      return 1;
    } catch (const CompileError &) {
    }

    try {
      Lexer badProgram("int main() { print missing; return 0; }");
      Parser badParser(badProgram.scanTokens());
      auto badAst = badParser.parseProgram();
      ResolveScopesPass{}.resolve(badAst);
      std::cerr << "undeclared variable was accepted\n";
      return 1;
    } catch (const CompileError &error) {
      if (error.line != 1 || error.col <= 1) {
        std::cerr << "undeclared variable did not retain a useful location\n";
        return 1;
      }
    }
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
