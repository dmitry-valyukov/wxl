// What a documentation file costs, phase by phase: the XML reader's parse of
// the whole document for comparison, opening the file -- reading it and
// indexing its members --, finding one member, and parsing the rest in slices.
//
// Usage: wxl.gen.common.bench <documentation.xml> [rounds] [id]

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>

#include "xml_input.h"

import wxl.xml;

namespace {

using clock = std::chrono::steady_clock;

// The smallest of several runs: noise in a measurement like this only adds.
struct best_time {
    double milliseconds = 1e30;

    void add(clock::duration duration) {
        double const ms = std::chrono::duration<double, std::milli>(duration).count();
        milliseconds = ms < milliseconds ? ms : milliseconds;
    }
};

template <typename F>
void time(best_time& best, F&& f) {
    auto const start = clock::now();
    f();
    best.add(clock::now() - start);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: wxl.gen.common.bench <documentation.xml> [rounds] [id]\n");
        return 2;
    }
    std::filesystem::path const file {argv[1]};
    int const rounds = argc > 2 ? std::atoi(argv[2]) : 5;
    char const* const id = argc > 3 ? argv[3] : "T:Windows.Foundation.Uri";

    best_time parse;
    best_time open;
    best_time find;
    best_time rest;
    double longest = 0;
    std::size_t members = 0;
    std::size_t slices = 0;
    bool found = false;
    for (int round = 0; round < rounds; ++round) {
        time(parse, [&] {
            wxl::xml::document document;
            document.load_file(file);
        });

        std::optional<DocumentationFile> opened;
        time(open, [&] { opened.emplace(file); });
        members = opened->size();
        time(find, [&] { found = opened->find(id).has_value(); });

        // The rest in 4 ms slices, the way the editor takes it between events;
        // the longest slice is what one step keeps the window waiting.
        slices = 0;
        time(rest, [&] {
            for (bool more = true; more; ++slices) {
                auto const start = clock::now();
                more = opened->parse_some(start + std::chrono::milliseconds {4});
                longest = std::max(longest, std::chrono::duration<double, std::milli>(clock::now() - start).count());
            }
        });
    }

    std::printf("  %zu members, %d rounds, %s %s\n\n", members, rounds, id, found ? "found" : "missing");
    std::printf("  whole document (wxl.xml)   %9.2f ms\n", parse.milliseconds);
    std::printf("  open: read and index        %9.2f ms\n", open.milliseconds);
    std::printf("  find one member, parsed     %9.3f ms\n", find.milliseconds);
    std::printf("  the rest, %5zu slices      %9.2f ms\n", slices, rest.milliseconds);
    std::printf("  longest slice               %9.2f ms\n", longest);
    return 0;
}
