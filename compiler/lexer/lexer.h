#pragma once

#include "../lexer/token.h"
#include <cctype>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

class Lexer {
  std::string src;
  size_t start = 0;
  size_t current = 0;
  int line = 1;
  int col = 1;

  std::unordered_map<std::string, TokenType> kw;

public:
  Lexer(std::string s) : src(std::move(s)) {

    // language keywords
    kw["let"] = TokenType::LET;
    kw["function"] = TokenType::FUNCTION;
    kw["if"] = TokenType::IF;
    kw["else"] = TokenType::ELSE;
    kw["while"] = TokenType::WHILE;
    kw["for"] = TokenType::FOR;
    kw["print"] = TokenType::PRINT;
    kw["return"] = TokenType::RETURN;

    kw["true"] = TokenType::TRUE;
    kw["false"] = TokenType::FALSE;

    // type keywords
    kw["int"] = TokenType::INT;
    kw["float"] = TokenType::FLOAT;
    kw["double"] = TokenType::DOUBLE;
    kw["short"] = TokenType::SHORT;
    kw["long"] = TokenType::LONG;
    kw["unsigned"] = TokenType::UNSIGNED;
    kw["char"] = TokenType::CHAR;
    kw["bool"] = TokenType::BOOL;
    kw["void"] = TokenType::VOID;
  }

  std::vector<Token> scanTokens() {
    std::vector<Token> tokens;

    while (!isAtEnd()) {
      start = current;
      char c = advance();

      switch (c) {

      case ' ':
      case '\t':
      case '\r':
        break;

      case '\n':
        line++;
        col = 1;
        break;

      case '+':
        tokens.push_back(makeToken(TokenType::PLUS));
        break;

      case '-':
        tokens.push_back(makeToken(TokenType::MINUS));
        break;

      case '*':
        tokens.push_back(makeToken(TokenType::STAR));
        break;

      case '%':
        tokens.push_back(makeToken(TokenType::MOD));
        break;

      case '&':
        if (match('&'))
          tokens.push_back(makeToken(TokenType::AND_AND));
        else
          throw std::runtime_error("Unexpected character '&'");
        break;

      case '|':
        if (match('|'))
          tokens.push_back(makeToken(TokenType::OR_OR));
        else
          throw std::runtime_error("Unexpected character '|'");
        break;

      case '/':
        if (match('/')) {
          while (peek() != '\n' && !isAtEnd())
            advance();
        } else if (match('*')) {
          while (!(peek() == '*' && peekNext() == '/') && !isAtEnd()) {
            if (peek() == '\n') {
              line++;
              col = 1;
            }
            advance();
          }
          if (!isAtEnd()) {
            advance();
            advance();
          }
        } else {
          tokens.push_back(makeToken(TokenType::SLASH));
        }
        break;

      case '=':
        tokens.push_back(
            makeToken(match('=') ? TokenType::EQUAL_EQUAL : TokenType::EQUAL));
        break;

      case '!':
        tokens.push_back(
            makeToken(match('=') ? TokenType::BANG_EQUAL : TokenType::BANG));
        break;

      case '<':
        tokens.push_back(
            makeToken(match('=') ? TokenType::LESS_EQUAL : TokenType::LESS));
        break;

      case '>':
        tokens.push_back(makeToken(match('=') ? TokenType::GREATER_EQUAL
                                              : TokenType::GREATER));
        break;

      case ':':
        tokens.push_back(makeToken(TokenType::COLON));
        break;

      case ';':
        tokens.push_back(makeToken(TokenType::SEMICOLON));
        break;

      case ',':
        tokens.push_back(makeToken(TokenType::COMMA));
        break;

      case '(':
        tokens.push_back(makeToken(TokenType::LPAREN));
        break;

      case ')':
        tokens.push_back(makeToken(TokenType::RPAREN));
        break;

      case '{':
        tokens.push_back(makeToken(TokenType::LBRACE));
        break;

      case '}':
        tokens.push_back(makeToken(TokenType::RBRACE));
        break;

      case '[':
        tokens.push_back(makeToken(TokenType::LBRACKET));
        break;

      case ']':
        tokens.push_back(makeToken(TokenType::RBRACKET));
        break;

      case '"': {
        int stringLine = line;
        int stringCol = col;

        while (peek() != '"' && !isAtEnd()) {
          if (peek() == '\n') {
            line++;
            col = 1;
          }
          advance();
        }

        if (isAtEnd())
          throw std::runtime_error("Unterminated string literal");

        advance();

        std::string value = src.substr(start + 1, current - start - 2);
        tokens.push_back(
            Token{TokenType::STRING, value, stringLine, stringCol});
        break;
      }

      default:
        if (isdigit(c)) {
          number(tokens);
        } else if (isAlpha(c)) {
          identifier(tokens);
        } else {
          throw std::runtime_error(std::string("Unexpected character: ") + c);
        }
        break;
      }
    }

    tokens.push_back(Token{TokenType::END_OF_FILE, "", line, col});
    return tokens;
  }

private:
  bool isAtEnd() const { return current >= src.size(); }

  char advance() {
    char c = src[current++];
    col++;
    return c;
  }

  char peek() const { return isAtEnd() ? '\0' : src[current]; }

  char peekNext() const {
    return current + 1 >= src.size() ? '\0' : src[current + 1];
  }

  bool match(char expected) {
    if (isAtEnd())
      return false;
    if (src[current] != expected)
      return false;
    current++;
    col++;
    return true;
  }

  Token makeToken(TokenType type) {
    std::string lex = src.substr(start, current - start);
    return Token{type, lex, line, col};
  }

  bool isAlpha(char c) const {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
  }

  bool isAlphaNum(char c) const {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
  }

  void number(std::vector<Token> &tokens) {
    while (isdigit(peek()))
      advance();

    if (peek() == '.' && isdigit(peekNext())) {
      advance();
      while (isdigit(peek()))
        advance();
    }

    tokens.push_back(makeToken(TokenType::NUMBER));
  }

  void identifier(std::vector<Token> &tokens) {
    while (isAlphaNum(peek()))
      advance();

    std::string text = src.substr(start, current - start);

    auto it = kw.find(text);
    if (it != kw.end())
      tokens.push_back(Token{it->second, text, line, col});
    else
      tokens.push_back(Token{TokenType::IDENTIFIER, text, line, col});
  }
};
