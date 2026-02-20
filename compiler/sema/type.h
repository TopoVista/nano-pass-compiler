#pragma once
#include <memory>
#include <string>
#include <vector>

enum class LangTypeKind {
  Integer,
  Floating,
  Bool,
  Char,
  String,
  Array,
  Function,
  Void,
  Unknown
};

struct LangType {
  LangTypeKind kind;

  int bitWidth = 0;
  bool isUnsigned = false;

  // Array
  std::shared_ptr<LangType> element;
  int arraySize = 0;

  // Function
  std::vector<LangType> params;
  std::shared_ptr<LangType> ret;

  LangType(LangTypeKind k = LangTypeKind::Unknown) : kind(k) {}

  // =============================
  // Copy Constructor
  // =============================

  LangType(const LangType &other)
      : kind(other.kind), bitWidth(other.bitWidth),
        isUnsigned(other.isUnsigned), arraySize(other.arraySize),
        params(other.params) {

    if (other.element)
      element = std::make_unique<LangType>(*other.element);

    if (other.ret)
      ret = std::make_unique<LangType>(*other.ret);
  }

  LangType &operator=(const LangType &other) {
    if (this == &other)
      return *this;

    kind = other.kind;
    bitWidth = other.bitWidth;
    isUnsigned = other.isUnsigned;
    arraySize = other.arraySize;
    params = other.params;

    element.reset();
    ret.reset();

    if (other.element)
      element = std::make_unique<LangType>(*other.element);

    if (other.ret)
      ret = std::make_unique<LangType>(*other.ret);

    return *this;
  }

  LangType(LangType &&) = default;
  LangType &operator=(LangType &&) = default;

  // =============================
  // Factory Methods
  // =============================

  static LangType Int(int bits = 32, bool unsignedFlag = false) {
    LangType t(LangTypeKind::Integer);
    t.bitWidth = bits;
    t.isUnsigned = unsignedFlag;
    return t;
  }

  static LangType Float(int bits = 32) {
    LangType t(LangTypeKind::Floating);
    t.bitWidth = bits;
    return t;
  }

  static LangType Bool() {
    LangType t(LangTypeKind::Bool);
    t.bitWidth = 1;
    return t;
  }

  static LangType Char() {
    LangType t(LangTypeKind::Char);
    t.bitWidth = 8;
    return t;
  }

  static LangType String() { return LangType(LangTypeKind::String); }

  static LangType Void() { return LangType(LangTypeKind::Void); }

  static LangType Unknown() { return LangType(LangTypeKind::Unknown); }

  static LangType Array(LangType elem, int size) {
    LangType t(LangTypeKind::Array);
    t.element = std::make_shared<LangType>(std::move(elem));
    t.arraySize = size;
    return t;
  }

  static LangType Function(std::vector<LangType> ps, LangType r) {
    LangType t(LangTypeKind::Function);
    t.params = std::move(ps);
    t.ret = std::make_unique<LangType>(std::move(r));
    return t;
  }

  // =============================
  // Helper Methods
  // =============================

  bool isInt() const { return kind == LangTypeKind::Integer; }

  bool isFloat() const { return kind == LangTypeKind::Floating; }

  bool isBool() const { return kind == LangTypeKind::Bool; }

  bool isChar() const { return kind == LangTypeKind::Char; }

  bool isArray() const { return kind == LangTypeKind::Array; }

  bool isFunction() const { return kind == LangTypeKind::Function; }

  bool isNumeric() const {
    return kind == LangTypeKind::Integer || kind == LangTypeKind::Floating;
  }
};

// =============================
// Type Comparison
// =============================

inline bool sameType(const LangType &a, const LangType &b) {

  if (a.kind != b.kind)
    return false;

  switch (a.kind) {

  case LangTypeKind::Integer:
  case LangTypeKind::Floating:
    return a.bitWidth == b.bitWidth && a.isUnsigned == b.isUnsigned;

  case LangTypeKind::Bool:
  case LangTypeKind::Char:
    return a.bitWidth == b.bitWidth;

  case LangTypeKind::Array:
    if (!a.element || !b.element)
      return false;
    return a.arraySize == b.arraySize && sameType(*a.element, *b.element);

  case LangTypeKind::Function:
    if (a.params.size() != b.params.size())
      return false;

    for (size_t i = 0; i < a.params.size(); i++)
      if (!sameType(a.params[i], b.params[i]))
        return false;

    if (!a.ret || !b.ret)
      return false;

    return sameType(*a.ret, *b.ret);

  default:
    return true;
  }
}
