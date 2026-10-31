#include "backend/llvm/llvm_runtime.h"

#include <iostream>

extern "C" void nano_print_bool(bool value) { std::cout << (value ? "true" : "false") << '\n'; }
extern "C" void nano_print_i8(std::int8_t value) { std::cout << static_cast<int>(value) << '\n'; }
extern "C" void nano_print_i16(std::int16_t value) { std::cout << value << '\n'; }
extern "C" void nano_print_i32(std::int32_t value) { std::cout << value << '\n'; }
extern "C" void nano_print_i64(std::int64_t value) { std::cout << value << '\n'; }
extern "C" void nano_print_f32(float value) { std::cout << value << '\n'; }
extern "C" void nano_print_f64(double value) { std::cout << value << '\n'; }
extern "C" void nano_print_string(const char *value) { std::cout << (value ? value : "") << '\n'; }
