#pragma once
#include <string>
#include <vector>
#include <memory>

enum class TypeKind {
    Int,
    Float,
    Bool,
    Char,
    String,
    Array,
    Function,
    Void,
    Unknown
};

struct Type {
    TypeKind kind;

    std::unique_ptr<Type> element; // array<T>
    std::vector<Type> params;      // function params
    std::unique_ptr<Type> ret;     // function return

    Type(TypeKind k = TypeKind::Unknown) : kind(k) {}

    // 🔥 COPY CONSTRUCTOR (DEEP COPY)
    Type(const Type& other) : kind(other.kind) {
        if (other.element)
            element = std::make_unique<Type>(*other.element);
        if (other.ret)
            ret = std::make_unique<Type>(*other.ret);
        params = other.params;
    }

    // 🔥 COPY ASSIGNMENT (DEEP COPY)
    Type& operator=(const Type& other) {
        if (this == &other) return *this;

        kind = other.kind;
        params = other.params;

        element.reset();
        ret.reset();

        if (other.element)
            element = std::make_unique<Type>(*other.element);
        if (other.ret)
            ret = std::make_unique<Type>(*other.ret);

        return *this;
    }

    // MOVE is fine (unique_ptr supports it)
    Type(Type&&) = default;
    Type& operator=(Type&&) = default;

    static Type Int() { return Type(TypeKind::Int); }
    static Type Float() { return Type(TypeKind::Float); }
    static Type Bool() { return Type(TypeKind::Bool); }
    static Type Char() { return Type(TypeKind::Char); }
    static Type String() { return Type(TypeKind::String); }
    static Type Void() { return Type(TypeKind::Void); }
    static Type Unknown() { return Type(TypeKind::Unknown); }

    static Type Array(Type elem) {
        Type t(TypeKind::Array);
        t.element = std::make_unique<Type>(std::move(elem));
        return t;
    }

    static Type Function(std::vector<Type> ps, Type r) {
        Type t(TypeKind::Function);
        t.params = std::move(ps);
        t.ret = std::make_unique<Type>(std::move(r));
        return t;
    }
};


inline std::string typeName(const Type& t) {
    switch (t.kind) {
        case TypeKind::Int: return "int";
        case TypeKind::Float: return "float";
        case TypeKind::Bool: return "bool";
        case TypeKind::Char: return "char";
        case TypeKind::String: return "string";
        case TypeKind::Void: return "void";
        case TypeKind::Unknown: return "unknown";
        case TypeKind::Array:
            return "array<" + typeName(*t.element) + ">";
        case TypeKind::Function: {
            std::string s = "function(";
            for (size_t i = 0; i < t.params.size(); i++) {
                s += typeName(t.params[i]);
                if (i + 1 < t.params.size()) s += ", ";
            }
            s += " -> " + typeName(*t.ret) + ")";
            return s;
        }
    }
    return "invalid";
}

inline bool sameType(const Type& a, const Type& b) {
    if (a.kind != b.kind) return false;

    if (a.kind == TypeKind::Array)
        return sameType(*a.element, *b.element);

    if (a.kind == TypeKind::Function) {
        if (a.params.size() != b.params.size()) return false;
        for (size_t i = 0; i < a.params.size(); i++)
            if (!sameType(a.params[i], b.params[i])) return false;
        return sameType(*a.ret, *b.ret);
    }

    return true;
}

inline bool isNumeric(const Type& t) {
    return t.kind == TypeKind::Int || t.kind == TypeKind::Float;
}
