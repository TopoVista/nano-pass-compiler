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

    // The branded NanoPass surface grammar is intentionally a parser-only
    // extension: it must resolve and type-check through the normal frontend.
    Lexer styledLexer(
        "fn choose(value: i32) -> i32 { "
        "when (value > 0) { yield value; } otherwise { yield 0; } } "
        "fn main() -> i32 { let result: i32 = choose(7); emit result; yield result; }");
    Parser styledParser(styledLexer.scanTokens());
    auto styledProgram = styledParser.parseProgram();
    ResolveScopesPass styledResolver;
    styledResolver.resolve(styledProgram);
    TypeCheckPass{}.check(styledProgram);

    Lexer loopLexer(
        "fn main() -> i32 { let i: i32 = 0; loop (i < 3) { i = i + 1; } yield i; }");
    Parser loopParser(loopLexer.scanTokens());
    auto loopProgram = loopParser.parseProgram();
    ResolveScopesPass loopResolver;
    loopResolver.resolve(loopProgram);
    TypeCheckPass{}.check(loopProgram);

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
