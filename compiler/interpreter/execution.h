#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "ir/core/core_ir.h"

namespace nanopass {

using RuntimeArray = std::shared_ptr<std::vector<CoreConstantVal>>;
using RuntimeValue = std::variant<int64_t, double, bool, std::string, RuntimeArray>;

enum class ExecutionStatus {
  Success,
  RuntimeError,
  StepLimitExceeded,
  TimeLimitExceeded,
  RecursionLimitExceeded,
  OutputLimitExceeded,
  UnsupportedFeature,
  InternalError,
};

struct RuntimeError { std::string category; std::string message; };
struct ExecutionLimits {
  uint64_t maxSteps = 100000;
  size_t maxCallDepth = 128;
  size_t maxOutputBytes = 1 << 20;
  size_t maxArrayElements = 1 << 20;
  // Zero leaves the wall-clock watchdog disabled for command-line tools.
  // Services set a finite value; both interpreters check it on every step.
  uint64_t maxWallTimeMs = 0;
};
struct ExecutionResult {
  ExecutionStatus status = ExecutionStatus::Success;
  std::optional<RuntimeValue> returnValue;
  std::string stdoutText;
  std::string stderrText;
  std::optional<RuntimeError> error;
  uint64_t stepsExecuted = 0;
};

inline std::string runtimeValueText(const RuntimeValue &value) {
  return std::visit([](const auto &item) -> std::string {
    using T = std::decay_t<decltype(item)>;
    if constexpr (std::is_same_v<T, bool>) return item ? "true" : "false";
    else if constexpr (std::is_same_v<T, std::string>) return item;
    else if constexpr (std::is_same_v<T, RuntimeArray>) return "<array>";
    else return std::to_string(item);
  }, value);
}

inline bool sameObservableResult(const ExecutionResult &left,
                                 const ExecutionResult &right) {
  if (left.status != right.status || left.stdoutText != right.stdoutText)
    return false;
  if (left.status != ExecutionStatus::Success)
    return (!left.error && !right.error) ||
           (left.error && right.error && left.error->category == right.error->category);
  if (left.returnValue.has_value() != right.returnValue.has_value()) return false;
  if (!left.returnValue) return true;
  return runtimeValueText(*left.returnValue) == runtimeValueText(*right.returnValue);
}

} // namespace nanopass
