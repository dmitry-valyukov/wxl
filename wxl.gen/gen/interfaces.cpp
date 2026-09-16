module;

#include <format>
#include <ostream>
#include <print>

module wxl.gen;

import std;

// Interfaces.h -- "просто список": a reference list of every interface the
// closure discovered, namespace-qualified, one per line, in the closure's
// dependency order. No C++ declarations: the interfaces themselves are
// consumed inside each class's `Impl` through the real cppwinrt
// projection (see gen/classes.cpp), so wxl has nothing of its own to
// declare for them. This file is documentation/index output.

using namespace md;

namespace gen {

void write_interfaces(Output const& out, Model const& model, Emitted& emitted) {
    auto const path = out.dir / "Interfaces.h";
    {
        auto file = open_output(path);

        std::print(file, R"({}// Every interface discovered in the type closure -- reference list
// only, no C++ declarations (each class's Impl holds the real
// cppwinrt-projected interface instead; see the generated .cpp files).
#pragma once

)",
                   banner);

        for (auto&& type : model.interfaces) {
            std::print(file, "// {}.{}\n", type.TypeNamespace(), type.TypeName());
        }
    }

    emitted.add(path);
    std::print("wrote {} ({} interfaces)\n", path.string(), model.interfaces.size());
}

}  // namespace gen
