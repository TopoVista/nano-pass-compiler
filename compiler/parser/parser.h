#pragma once

#include "../ast/expr.h"
#include "../ast/stmt.h"
#include "../lexer/token.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

using namespace std;

class Parser {
  vector<Token> tokens;
  int current = 0;

public:
  Parser(vector<Token> t) : tokens(std::move(t)) {}

  vector<unique_ptr<Stmt>> parseProgram() {
    vector<unique_ptr<Stmt>> program;
    while (!isAtEnd()) {
      program.push_back(statement());
    }
    return program;
  }

private:
  Token &peek() { return tokens[current]; }
  Token &previous() { return tokens[current - 1]; }
  bool isAtEnd() { return peek().type == TokenType::END_OF_FILE; }

  bool check(TokenType type) {
    if (isAtEnd())
      return false;
    return peek().type == type;
  }

  bool match(initializer_list<TokenType> types) {
    for (auto t : types) {
      if (check(t)) {
        current++;
        return true;
      }
    }
    return false;
  }

  Token consume(TokenType type, const string &msg) {
    if (check(type))
      return tokens[current++];
    throw runtime_error(msg);
  }

  // ============================================================
  // STATEMENTS
  // ============================================================

  unique_ptr<Stmt> statement() {

    if (check(TokenType::INT) || check(TokenType::FLOAT) ||
        check(TokenType::DOUBLE) || check(TokenType::BOOL) ||
        check(TokenType::CHAR) || check(TokenType::VOID)) {

      int save = current;
      LangType type = parseType();

      if (check(TokenType::IDENTIFIER)) {
        if (current + 1 < tokens.size() &&
            tokens[current + 1].type == TokenType::LPAREN) {
          current = save;
          return functionStatement();
        }
      }

      current = save;
      return varDeclaration();
    }

    if (match({TokenType::IF}))
      return ifStatement();
    if (match({TokenType::WHILE}))
      return whileStatement();
    if (match({TokenType::FOR}))
      return forStatement();
    if (match({TokenType::PRINT}))
      return printStatement();
    if (match({TokenType::LBRACE}))
      return blockStatement();

    if (match({TokenType::RETURN})) {
      Token retToken = previous();
      return returnStatement(retToken);
    }

    return expressionStatement();
  }

  // ============================================================
  // VARIABLE DECLARATION (NOW SUPPORTS ARRAYS)
  // ============================================================

  unique_ptr<Stmt> varDeclaration() {

    LangType baseType = parseType();

    int arraySize = -1;

    if (match({TokenType::LBRACKET})) {
      Token sizeTok = consume(TokenType::NUMBER, "Expected array size");
      arraySize = stoi(sizeTok.lexeme);
      consume(TokenType::RBRACKET, "Expected ']'");
    }

    Token name = consume(TokenType::IDENTIFIER, "Expected variable name");

    LangType finalType = baseType;

    if (arraySize != -1) {
      finalType = LangType::Array(baseType, arraySize);
    }

    unique_ptr<Expr> initializer = nullptr;

    if (match({TokenType::EQUAL})) {
      initializer = expression();
    }

    consume(TokenType::SEMICOLON, "Expected ';' after variable declaration");

    return make_unique<VarDeclStmt>(name.lexeme, finalType,
                                    std::move(initializer));
  }

  // ============================================================
  // TYPE PARSING
  // ============================================================

  LangType parseType() {

    if (match({TokenType::INT}))
      return LangType::Int(32, false);

    if (match({TokenType::FLOAT}))
      return LangType::Float(32);

    if (match({TokenType::DOUBLE}))
      return LangType::Float(64);

    if (match({TokenType::BOOL}))
      return LangType::Bool();

    if (match({TokenType::CHAR}))
      return LangType::Char();

    if (match({TokenType::VOID}))
      return LangType::Void();

    throw runtime_error("Expected type");
  }

  // ============================================================
  // OTHER STATEMENTS
  // ============================================================

  unique_ptr<Stmt> expressionStatement() {
    auto expr = expression();
    consume(TokenType::SEMICOLON, "Expected ';' after expression");
    return make_unique<ExprStmt>(std::move(expr));
  }

  unique_ptr<Stmt> printStatement() {
    auto value = expression();
    consume(TokenType::SEMICOLON, "Expected ';' after print");
    return make_unique<PrintStmt>(std::move(value));
  }

  unique_ptr<Stmt> blockStatement() {
    auto block = make_unique<BlockStmt>();
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
      block->stmts.push_back(statement());
    }
    consume(TokenType::RBRACE, "Expected '}' after block");
    return block;
  }

  unique_ptr<Stmt> returnStatement(Token retToken) {

    unique_ptr<Expr> value = nullptr;

    if (!check(TokenType::SEMICOLON))
      value = expression();

    consume(TokenType::SEMICOLON, "Expected ';' after return");

    auto stmt = make_unique<ReturnStmt>(std::move(value));
    stmt->loc.line = retToken.line;
    stmt->loc.col = retToken.col;

    return stmt;
  }

  unique_ptr<Stmt> ifStatement() {
    consume(TokenType::LPAREN, "Expected '(' after if");
    auto condition = expression();
    consume(TokenType::RPAREN, "Expected ')'");
    auto thenBranch = statement();
    unique_ptr<Stmt> elseBranch = nullptr;
    if (match({TokenType::ELSE}))
      elseBranch = statement();
    return make_unique<IfStmt>(std::move(condition), std::move(thenBranch),
                               std::move(elseBranch));
  }

  unique_ptr<Stmt> whileStatement() {
    consume(TokenType::LPAREN, "Expected '(' after while");
    auto condition = expression();
    consume(TokenType::RPAREN, "Expected ')'");
    auto body = statement();
    return make_unique<WhileStmt>(std::move(condition), std::move(body));
  }

  unique_ptr<Stmt> forStatement() {

    consume(TokenType::LPAREN, "Expected '(' after for");

    unique_ptr<Stmt> init = nullptr;

    if (!check(TokenType::SEMICOLON)) {
      auto initExpr = expression();
      consume(TokenType::SEMICOLON, "Expected ';'");
      init = make_unique<ExprStmt>(std::move(initExpr));
    } else {
      consume(TokenType::SEMICOLON, "Expected ';'");
    }

    unique_ptr<Expr> condition = nullptr;
    if (!check(TokenType::SEMICOLON))
      condition = expression();
    consume(TokenType::SEMICOLON, "Expected ';'");

    unique_ptr<Expr> increment = nullptr;
    if (!check(TokenType::RPAREN))
      increment = expression();
    consume(TokenType::RPAREN, "Expected ')'");

    auto body = statement();

    return make_unique<ForStmt>(std::move(init), std::move(condition),
                                std::move(increment), std::move(body));
  }

  unique_ptr<Stmt> functionStatement() {

    LangType returnType = parseType();

    Token name = consume(TokenType::IDENTIFIER, "Expected function name");

    consume(TokenType::LPAREN, "Expected '(' after function name");

    vector<pair<string, LangType>> params;

    if (!check(TokenType::RPAREN)) {
      do {
        LangType paramType = parseType();
        Token paramName =
            consume(TokenType::IDENTIFIER, "Expected parameter name");
        params.push_back({paramName.lexeme, paramType});
      } while (match({TokenType::COMMA}));

      unordered_set<string> seen;
      for (auto &p : params) {
        if (seen.count(p.first))
          throw runtime_error("Duplicate parameter name '" + p.first + "'");
        seen.insert(p.first);
      }
    }

    consume(TokenType::RPAREN, "Expected ')'");
    consume(TokenType::LBRACE, "Expected '{'");

    auto body = blockStatement();

    return make_unique<FunctionStmt>(
        name.lexeme, returnType, std::move(params),
        unique_ptr<BlockStmt>(static_cast<BlockStmt *>(body.release())));
  }

  // ============================================================
  // EXPRESSIONS
  // ============================================================

  unique_ptr<Expr> expression() { return assignment(); }

  unique_ptr<Expr> assignment() {
    auto expr = logical_or();

    if (match({TokenType::EQUAL})) {
      auto value = assignment();

      if (dynamic_cast<VariableExpr *>(expr.get()) ||
          dynamic_cast<IndexExpr *>(expr.get())) {

        return make_unique<BinaryExpr>("=", std::move(expr), std::move(value));
      }

      throw runtime_error("Invalid assignment target");
    }

    return expr;
  }

  unique_ptr<Expr> logical_or() {
    auto expr = logical_and();
    while (match({TokenType::OR_OR})) {
      string op = previous().lexeme;
      auto right = logical_and();
      expr = make_unique<BinaryExpr>(op, std::move(expr), std::move(right));
    }
    return expr;
  }

  unique_ptr<Expr> logical_and() {
    auto expr = equality();
    while (match({TokenType::AND_AND})) {
      string op = previous().lexeme;
      auto right = equality();
      expr = make_unique<BinaryExpr>(op, std::move(expr), std::move(right));
    }
    return expr;
  }

  unique_ptr<Expr> equality() {
    auto expr = comparison();
    while (match({TokenType::EQUAL_EQUAL, TokenType::BANG_EQUAL})) {
      string op = previous().lexeme;
      auto right = comparison();
      expr = make_unique<BinaryExpr>(op, std::move(expr), std::move(right));
    }
    return expr;
  }

  unique_ptr<Expr> comparison() {
    auto expr = term();
    while (match({TokenType::LESS, TokenType::LESS_EQUAL, TokenType::GREATER,
                  TokenType::GREATER_EQUAL})) {
      string op = previous().lexeme;
      auto right = term();
      expr = make_unique<BinaryExpr>(op, std::move(expr), std::move(right));
    }
    return expr;
  }

  unique_ptr<Expr> term() {
    auto expr = factor();
    while (match({TokenType::PLUS, TokenType::MINUS})) {
      string op = previous().lexeme;
      auto right = factor();
      expr = make_unique<BinaryExpr>(op, std::move(expr), std::move(right));
    }
    return expr;
  }

  unique_ptr<Expr> factor() {
    auto expr = unary();
    while (match({TokenType::STAR, TokenType::SLASH, TokenType::MOD})) {
      string op = previous().lexeme;
      auto right = unary();
      expr = make_unique<BinaryExpr>(op, std::move(expr), std::move(right));
    }
    return expr;
  }

  unique_ptr<Expr> unary() {
    if (match({TokenType::BANG, TokenType::MINUS})) {
      string op = previous().lexeme;
      auto right = unary();
      return make_unique<UnaryExpr>(op, std::move(right));
    }
    return postfix();
  }

  // ============================================================
  // POSTFIX (INDEX SUPPORT)
  // ============================================================

  unique_ptr<Expr> postfix() {

    auto expr = primary();

    while (true) {

      if (match({TokenType::LBRACKET})) {

        auto indexExpr = expression();
        consume(TokenType::RBRACKET, "Expected ']'");

        expr = make_unique<IndexExpr>(std::move(expr), std::move(indexExpr));
      } else {
        break;
      }
    }

    return expr;
  }

  unique_ptr<Expr> primary() {

    if (match({TokenType::STRING}))
      return make_unique<StringExpr>(previous().lexeme);

    if (match({TokenType::NUMBER})) {

      string lex = previous().lexeme;

      if (lex.find('.') != string::npos)
        return make_unique<NumberExpr>(stod(lex));

      return make_unique<NumberExpr>(stoll(lex));
    }

    if (match({TokenType::TRUE}))
      return make_unique<BoolExpr>(true);

    if (match({TokenType::FALSE}))
      return make_unique<BoolExpr>(false);

    if (match({TokenType::IDENTIFIER})) {

      string name = previous().lexeme;

      if (match({TokenType::LPAREN})) {

        vector<unique_ptr<Expr>> args;

        if (!check(TokenType::RPAREN)) {
          do {
            args.push_back(expression());
          } while (match({TokenType::COMMA}));
        }

        consume(TokenType::RPAREN, "Expected ')'");

        return make_unique<CallExpr>(name, std::move(args));
      }

      return make_unique<VariableExpr>(name);
    }

    if (match({TokenType::LPAREN})) {
      auto expr = expression();
      consume(TokenType::RPAREN, "Expected ')'");
      return expr;
    }

    throw runtime_error("Expected expression");
  }
};
