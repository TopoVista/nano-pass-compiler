#pragma once
#include <stdexcept>
#include <string>
#include "source_location.h"

inline void errorAt(const SourceLocation& loc, const std::string& msg) {
    if (loc.line != -1) {
        throw std::runtime_error(
            "Error at line " + std::to_string(loc.line) +
            ", column " + std::to_string(loc.col) + ":\n" +
            msg
        );
    }
    throw std::runtime_error(msg);
}
