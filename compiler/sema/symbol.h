#pragma once
#include <string>
#include "type.h"

using namespace std;

enum class SymbolKind {
    Variable,
    Function
};

struct Symbol {
    string name;
    SymbolKind kind;
    int depth;
    Type type;

    Symbol(string n, SymbolKind k, int d, Type t = Type::Unknown())
        : name(move(n)), kind(k), depth(d), type(t) {}
};
