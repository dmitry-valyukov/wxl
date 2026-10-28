#include <format>
#include <ostream>
#include <print>

#include "wxl.gen.h"

import std;
import wxl.core;

// parts/wrapper-00.cpp ... wrapper-<N-1>.cpp: every out-of-line body the
// writers handed over (body_file), in N translation units, N = output::parts,
// a number the build fixes. Not one source per file group: the bodies of the
// controls alone, Microsoft.UI.Xaml.Controls, make the longest compile of the
// tree as one source, 85 s of the full profile. And names known before the run
// are what lets the build declare the sources it compiles as outputs of the
// generation step.
//
// A part takes a contiguous run of the classes in the order the writers hand
// them over -- group after group, base before derived inside one -- so it holds
// each of its classes' bodies whole, keeps the order they came in, and touches
// as few groups, and parses as few of their headers, as an even split allows.
// The runs are as even as the class count is: the first (C mod N) parts take
// one class more than the rest, and with fewer classes than parts the last
// ones hold none. The same input gives the same parts, byte for byte, so a run
// that changes nothing rewrites none of them; a class added or removed moves
// the boundaries after it, and with them every later part.
//
// A part includes what each of its groups names (body_file::includes), group
// after group, a header once, where it is first named. The first group's
// private header leads and brings in winrt/base.h and, through the public
// headers, the standard headers core.h includes ahead of its import of
// wxl.core. Every header named after that is included past the import, as a
// group's own includes after its private header are; that holds as long as
// none of them brings in a standard header first, since MSVC refuses one
// first seen after the import.

namespace gen {

void write_parts(output const& out, std::vector<body_file> const& bodies, emitted& em) {
    if (out.parts < 1 || out.parts > max_parts) {
        throw std::invalid_argument(
            std::format("{} parts: the number of parts is from 1 to {}", out.parts, max_parts));
    }

    // Every class as the group it belongs to and its place in the group.
    std::vector<std::pair<std::size_t, std::size_t>> classes;
    for (std::size_t file = 0; file != bodies.size(); ++file) {
        for (std::size_t at = 0; at != bodies[file].classes.size(); ++at) {
            classes.emplace_back(file, at);
        }
    }

    auto const dir = out.dir / "parts";
    std::size_t const count = classes.size();
    std::size_t begin = 0;
    for (std::size_t part = 0; part != out.parts; ++part) {
        std::size_t const end = begin + count / out.parts + (part < count % out.parts ? 1 : 0);
        auto const path = dir / std::format("wrapper-{:02}.cpp", part);
        {
            auto file = open_output(path);
            std::print(file, "{}", banner);

            if (begin == end) {
                std::print(file,
                           "\n// No class falls to this part: the run has fewer classes "
                           "than parts.\n");
            } else {
                std::set<std::string_view> included;
                std::size_t group = bodies.size();
                for (std::size_t at = begin; at != end; ++at) {
                    if (classes[at].first == group) {
                        continue;
                    }
                    group = classes[at].first;
                    for (auto&& include : bodies[group].includes) {
                        if (included.insert(include).second) {
                            write_include(file, include, "../");
                        }
                    }
                }

                std::print(file, "\nnamespace wxl {{\n");
                group = bodies.size();
                for (std::size_t at = begin; at != end; ++at) {
                    auto const [owner, index] = classes[at];
                    if (owner != group) {
                        group = owner;
                        std::print(file, "\n// {}\n", bodies[group].name);
                    }
                    std::print(file, "{}", bodies[group].classes[index]);
                }
                std::print(file, "\n}}  // namespace wxl\n");
            }
        }
        em.add(path);
        std::print("generated {} ({} classes)\n", wxl::core::unicode::to_utf8(path).chars(),
                   end - begin);
        begin = end;
    }
}

}  // namespace gen
