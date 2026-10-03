#pragma once

#include <filesystem>
#include <string>

#include "md.h"

// Output side of winui-srcgen: everything that turns the discovered type
// closure into C++ sources under `output::dir`. The metadata walk itself
// (wxl.gen.common's crawl.h) knows nothing about file layout or C++ syntax;
// analyze() (gen/model.h) works out what every file says, and the writers in
// gen/*.cpp, declared in gen/writers.h, write it.

struct model;

// Where generation writes, and which CMake target the generated
// CMakeLists.txt adds its sources to.
struct output {
    std::filesystem::path dir;
    std::string cmake_target;
};

// Creates `out.dir` and runs every writer, in order, finishing with the
// CMakeLists.txt that lists everything written.
void write_all(output const& out, model const& m);
