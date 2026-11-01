#pragma once

#include <cstdint>
#include <string_view>

namespace nanopass {

// Narrow instrumentation boundary shared by the reference interpreter and
// profile collectors.  It intentionally exposes execution facts only; it
// cannot alter evaluation or optimization decisions.
class ExecutionProfileSink {
public:
  virtual ~ExecutionProfileSink() = default;
  virtual void recordFunctionCall(std::string_view function) = 0;
  virtual void recordBranch(std::string_view site, bool taken) = 0;
  virtual void recordLoopIteration(std::string_view site) = 0;
};

} // namespace nanopass
