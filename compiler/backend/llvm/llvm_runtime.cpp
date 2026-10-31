#include "backend/llvm/llvm_runtime.h"

#include <iostream>
#include <string>

namespace {
thread_local std::string *outputSink = nullptr;
void write(std::string text) {
  text += '\n';
  if (outputSink) *outputSink += text;
  else std::cout << text;
}
}

std::string *nanopass::llvm_backend::setRuntimeOutputSink(std::string *sink) noexcept {
  auto *previous = outputSink; outputSink = sink; return previous;
}

extern "C" void nano_print_bool(bool value) { write(value ? "true" : "false"); }
extern "C" void nano_print_i8(std::int8_t value) { write(std::to_string(static_cast<int>(value))); }
extern "C" void nano_print_i16(std::int16_t value) { write(std::to_string(value)); }
extern "C" void nano_print_i32(std::int32_t value) { write(std::to_string(value)); }
extern "C" void nano_print_i64(std::int64_t value) { write(std::to_string(value)); }
extern "C" void nano_print_f32(float value) { write(std::to_string(value)); }
extern "C" void nano_print_f64(double value) { write(std::to_string(value)); }
extern "C" void nano_print_string(const char *value) { write(value ? value : ""); }
