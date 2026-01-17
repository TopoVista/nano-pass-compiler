#pragma once

struct SourceLocation {
    int line;
    int col;

    SourceLocation(int l = -1, int c = -1)
        : line(l), col(c) {}
};
