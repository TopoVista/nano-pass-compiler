#pragma once

#include <cstdint>
#include <string>

namespace nanopass::llvm_backend {
// Returns the prior thread-local sink. A null sink writes to std::cout.
std::string *setRuntimeOutputSink(std::string *sink) noexcept;
}

// These symbols intentionally use a C ABI: they are referenced by generated
// modules and explicitly registered with ORC rather than discovered by chance
// through host linker visibility.
extern "C" void nano_print_bool(bool value);
extern "C" void nano_print_i8(std::int8_t value);
extern "C" void nano_print_i16(std::int16_t value);
extern "C" void nano_print_i32(std::int32_t value);
extern "C" void nano_print_i64(std::int64_t value);
extern "C" void nano_print_f32(float value);
extern "C" void nano_print_f64(double value);
extern "C" void nano_print_string(const char *value);

// Called by generated SSA LLVM array accesses. A failing check reports a
// deterministic runtime diagnostic and terminates; it intentionally does not
// attempt to recover into an arbitrary JIT stack frame.
extern "C" void nano_check_array_bounds_i64(std::int64_t index,
                                              std::int64_t size);
