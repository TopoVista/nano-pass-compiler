#pragma once
#include <stdexcept>
#include <string>

struct CompileError : public std::exception {
  std::string message;
  int line;
  int col;

  CompileError(std::string msg, int l, int c)
      : message(std::move(msg)), line(l), col(c) {}

  const char *what() const noexcept override { return message.c_str(); }
};
