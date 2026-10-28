#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

#include "md.h"

// Output side of winui-srcgen: everything that turns the discovered type
// closure into C++ sources under `output::dir`. The metadata walk itself
// (wxl.gen.common's crawl.h) knows nothing about file layout or C++ syntax;
// analyze() (gen/model.h) works out what every file says, and the writers in
// gen/*.cpp, declared in gen/writers.h, write it.

struct model;

// Where generation writes, and into how many translation units the
// out-of-line bodies go: parts/wrapper-00.cpp ... wrapper-<parts-1>.cpp, two
// digits, so 1 to max_parts of them (gen/parts.cpp).
struct output {
    std::filesystem::path dir;
    std::size_t parts = 1;
};

inline constexpr std::size_t max_parts = 100;

// Creates `out.dir` and its parts/ and runs every writer, in order, finishing
// with the parts and the removal of what an earlier run wrote and this one did
// not.
void write_all(output const& out, model const& m);
