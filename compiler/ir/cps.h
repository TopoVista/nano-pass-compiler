#pragma once
#include <memory>
#include <string>
#include <vector>

using namespace std;

// ================= CPS VALUES =================
struct CPSValue {
  virtual ~CPSValue() = default;
};

struct CPSVar : CPSValue {
  string name;
  CPSVar(string n) : name(std::move(n)) {}
};

struct CPSInt : CPSValue {
  int value;
  CPSInt(int v) : value(v) {}
};

// ================= CPS EXPRESSIONS =================
struct CPSExpr {
  virtual ~CPSExpr() = default;
};

struct CPSCall : CPSExpr {
  string func;
  vector<string> args;
  CPSCall(string f, vector<string> a)
      : func(std::move(f)), args(std::move(a)) {}
};

struct CPSLet : CPSExpr {
  string var;
  unique_ptr<CPSExpr> rhs;
  unique_ptr<CPSExpr> body;

  CPSLet(string v, unique_ptr<CPSExpr> r, unique_ptr<CPSExpr> b)
      : var(std::move(v)), rhs(std::move(r)), body(std::move(b)) {}
};

struct CPSIf : CPSExpr {
  string cond;
  unique_ptr<CPSExpr> thenE;
  unique_ptr<CPSExpr> elseE;

  CPSIf(string c, unique_ptr<CPSExpr> t, unique_ptr<CPSExpr> e)
      : cond(std::move(c)), thenE(std::move(t)), elseE(std::move(e)) {}
};

struct CPSReturn : CPSExpr {
  string value;
  CPSReturn(string v) : value(std::move(v)) {}
};
