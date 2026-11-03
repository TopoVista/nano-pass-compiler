#pragma once

#include <string_view>

// This is deliberately a source-controlled pre-release identity rather than a
// generated Git description. It is stable across reproducible builds and is
// emitted with user-visible artifacts. A release tag is created only after the
// container and hosted-CI gates documented in docs/ROADMAP.md have passed.
namespace nanopass::build {

inline constexpr std::string_view kProjectVersion = "0.3.0";
inline constexpr std::string_view kReleaseChannel = "phase11-dev";
inline constexpr std::string_view kCompilerRevision = "0.3.0+phase11-dev";
inline constexpr std::string_view kCliVersion = "nano 0.3.0+phase11-dev";

} // namespace nanopass::build
