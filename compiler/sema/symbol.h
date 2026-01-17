#pragma once
#include <string>

using namespace std;

enum class SymbolKind {
    Variable,
    Function
};

struct Symbol {
    string name;
    SymbolKind kind;
    int depth;

    Symbol(string n, SymbolKind k, int d)
        : name(move(n)), kind(k), depth(d) {}
};
