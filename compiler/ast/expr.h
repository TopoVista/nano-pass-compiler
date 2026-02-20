#pragma once

#include <cctype>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "../common/source_location.h"
#include "../lexer/token.h"
#include "../sema/symbol.h"
#include "../sema/type.h"

using namespace std;

// ============================================================
// Base Expression
// ============================================================

struct Expr {
  SourceLocation loc;
  LangType type = LangType::Unknown();
  virtual ~Expr() = default;
  virtual void print(int d) = 0;
};

// ============================================================
// Number Literal (int + float)
// ============================================================

struct NumberExpr : Expr {

  bool isFloat;
  long long intValue;
  double floatValue;

  NumberExpr(long long v) : isFloat(false), intValue(v), floatValue(0.0) {}

  NumberExpr(double v) : isFloat(true), intValue(0), floatValue(v) {}

  void print(int d) override {
    cout << string(d, ' ');
    if (isFloat)
      cout << "Float(" << floatValue << ")\n";
    else
      cout << "Int(" << intValue << ")\n";
  }
};

// ============================================================
// Boolean Literal
// ============================================================

struct BoolExpr : Expr {
  bool value;

  BoolExpr(bool v) : value(v) {}

  void print(int d) override {
    cout << string(d, ' ') << "Bool(" << (value ? "true" : "false") << ")\n";
  }
};

// ============================================================
// String Literal
// ============================================================

struct StringExpr : Expr {
  string value;

  StringExpr(string v) : value(std::move(v)) {}

  void print(int d) override {
    cout << string(d, ' ') << "String(\"" << value << "\")\n";
  }
};

// ============================================================
// Variable
// ============================================================

struct VariableExpr : Expr {
  string name;
  Symbol *symbol = nullptr;

  VariableExpr(string n) : name(std::move(n)) {}

  void print(int d) override {
    cout << string(d, ' ') << "Var(" << name;
    if (symbol)
      cout << " -> depth " << symbol->depth;
    cout << ")\n";
  }
};

// ============================================================
// Array Index Expression  ← DAY 5 ADDITION
// ============================================================

struct IndexExpr : Expr {
  unique_ptr<Expr> array;
  unique_ptr<Expr> index;

  IndexExpr(unique_ptr<Expr> a, unique_ptr<Expr> i)
      : array(std::move(a)), index(std::move(i)) {}

  void print(int d) override {
    cout << string(d, ' ') << "Index\n";
    array->print(d + 2);
    index->print(d + 2);
  }
};

// ============================================================
// Unary Expression
// ============================================================

struct UnaryExpr : Expr {
  string op;
  unique_ptr<Expr> right;

  UnaryExpr(string o, unique_ptr<Expr> r)
      : op(std::move(o)), right(std::move(r)) {}

  void print(int d) override {
    cout << string(d, ' ') << "Unary(" << op << ")\n";
    right->print(d + 2);
  }
};

// ============================================================
// Binary Expression
// ============================================================

struct BinaryExpr : Expr {
  string op;
  unique_ptr<Expr> left;
  unique_ptr<Expr> right;

  BinaryExpr(string o, unique_ptr<Expr> l, unique_ptr<Expr> r)
      : op(std::move(o)), left(std::move(l)), right(std::move(r)) {}

  void print(int d) override {
    cout << string(d, ' ') << "Binary(" << op << ")\n";
    left->print(d + 2);
    right->print(d + 2);
  }
};

// ============================================================
// Function Call
// ============================================================

struct CallExpr : Expr {
  string callee;
  vector<unique_ptr<Expr>> args;
  Symbol *symbol = nullptr;

  CallExpr(string c, vector<unique_ptr<Expr>> a)
      : callee(std::move(c)), args(std::move(a)) {}

  void print(int d) override {
    cout << string(d, ' ') << "Call(" << callee << ")\n";
    for (auto &arg : args)
      arg->print(d + 2);
  }
};
