#pragma once

#include <string>
#include <vector>

struct BoundaryVertex {
    int id = 0;
    std::wstring kind;
    std::wstring label;
    int complexity = 0;
};

struct BoundaryChoice {
    int id = 0;
    std::vector<int> successors;
    std::vector<int> subgraph;
    int boundary = 0;
    int complexity = 0;
};

struct BoundaryReport {
    int vertices = 0;
    int Sa = 0;
    double So = 0;
    std::vector<BoundaryVertex> nodes;
    std::vector<BoundaryChoice> choices;
};

// Parses the supported Perl control-flow constructs and calculates Sa and So
// directly from the current source text. Throws on malformed or unsupported
// control-flow syntax instead of silently using a stale graph.
BoundaryReport analyzePerlBoundary(const std::wstring& source);
