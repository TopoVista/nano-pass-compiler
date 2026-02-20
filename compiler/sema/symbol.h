#pragma once
#include "type.h"
#include <string>
#include <vector>

using namespace std;

enum class SymbolKind { Variable, Function };

struct Symbol {
  string name;
  SymbolKind kind;
  int depth;
  LangType type;

  // Function-specific metadata
  vector<LangType> paramTypes;

  Symbol(string n, SymbolKind k, int d, LangType t = LangType::Unknown())
      : name(std::move(n)), kind(k), depth(d), type(t) {}
};
