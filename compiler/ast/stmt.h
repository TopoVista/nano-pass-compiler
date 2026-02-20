#pragma once

#include "../common/source_location.h"
#include "../sema/type.h"
#include "expr.h"
#include <cctype> // for isdigit, isalpha, isalnum
#include <iostream>
#include <memory>
#include <stdexcept> // for runtime_error
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

/* ===================== STATEMENTS ===================== */

struct Stmt {
  SourceLocation loc;
  virtual ~Stmt() = default;
  virtual void print(int d) = 0;
};

struct VarDeclStmt : Stmt {
  std::string name;
  LangType type;
  std::unique_ptr<Expr> initializer;

  VarDeclStmt(std::string n, LangType t, std::unique_ptr<Expr> init)
      : name(std::move(n)), type(t), initializer(std::move(init)) {}

  void print(int d) override {
    std::cout << std::string(d, ' ') << "VarDecl " << name << "\n";
  }
};

struct ExprStmt : Stmt {
  unique_ptr<Expr> e;
  ExprStmt(unique_ptr<Expr> x) : e(std::move(x)) {}
  void print(int d) {
    cout << string(d, ' ') << "ExprStmt\n";
    e->print(d + 2);
  }
};

struct PrintStmt : Stmt {
  unique_ptr<Expr> e;
  PrintStmt(unique_ptr<Expr> x) : e(std::move(x)) {}
  void print(int d) {
    cout << string(d, ' ') << "PrintStmt\n";
    e->print(d + 2);
  }
};

struct BlockStmt : Stmt {
  vector<unique_ptr<Stmt>> stmts;
  void print(int d) {
    cout << string(d, ' ') << "Block\n";
    for (auto &s : stmts)
      s->print(d + 2);
  }
};

struct IfStmt : Stmt {
  unique_ptr<Expr> condition;
  unique_ptr<Stmt> thenBranch, elseBranch;
  IfStmt(unique_ptr<Expr> c, unique_ptr<Stmt> t, unique_ptr<Stmt> e)
      : condition(std::move(c)), thenBranch(std::move(t)),
        elseBranch(std::move(e)) {}
  void print(int d) {
    cout << string(d, ' ') << "If\n";
    condition->print(d + 2);
    thenBranch->print(d + 2);
    if (elseBranch) {
      cout << string(d, ' ') << "Else\n";
      elseBranch->print(d + 2);
    }
  }
};

// added on day 12
struct WhileStmt : Stmt {
  unique_ptr<Expr> condition;
  unique_ptr<Stmt> body;

  WhileStmt(unique_ptr<Expr> c, unique_ptr<Stmt> b)
      : condition(std::move(c)), body(std::move(b)) {}

  void print(int d) {
    cout << string(d, ' ') << "While\n";
    condition->print(d + 2);
    body->print(d + 2);
  }
};

// added on day 13
struct FunctionStmt : Stmt {
  string name;
  LangType returnType;
  vector<pair<string, LangType>> params;
  unique_ptr<BlockStmt> body;

  FunctionStmt(string n, LangType r, vector<pair<string, LangType>> p,
               unique_ptr<BlockStmt> b)
      : name(std::move(n)), returnType(r), params(std::move(p)),
        body(std::move(b)) {}

  void print(int d) override {
    cout << string(d, ' ') << "Function " << name << "\n";
    body->print(d + 2);
  }
};

struct ReturnStmt : Stmt {
  unique_ptr<Expr> value;
  ReturnStmt(unique_ptr<Expr> v) : value(std::move(v)) {}
  void print(int d) {
    cout << string(d, ' ') << "Return\n";
    if (value)
      value->print(d + 2);
  }
};

// added on day 15
struct ForStmt : Stmt {
  unique_ptr<Stmt> init;
  unique_ptr<Expr> condition;
  unique_ptr<Expr> increment;
  unique_ptr<Stmt> body;

  ForStmt(unique_ptr<Stmt> i, unique_ptr<Expr> c, unique_ptr<Expr> inc,
          unique_ptr<Stmt> b)
      : init(std::move(i)), condition(std::move(c)), increment(std::move(inc)),
        body(std::move(b)) {}

  void print(int d) {
    cout << string(d, ' ') << "For\n";
    if (init)
      init->print(d + 2);
    if (condition)
      condition->print(d + 2);
    if (increment) {
      cout << string(d + 2, ' ') << "Increment\n";
      increment->print(d + 4);
    }
    body->print(d + 2);
  }
};

// added on day 16
struct BreakStmt : Stmt {
  BreakStmt() = default;
  void print(int d) { cout << string(d, ' ') << "Break\n"; }
};

struct ContinueStmt : Stmt {
  ContinueStmt() = default;
  void print(int d) { cout << string(d, ' ') << "Continue\n"; }
};
