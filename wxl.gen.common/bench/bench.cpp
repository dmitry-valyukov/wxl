// What a documentation file costs, phase by phase: the XML reader's parse on
// its own, opening the file -- the parse and the index on top of it -- and
// finding one member.
//
// Usage: wxl.gen.common.bench <documentation.xml> [rounds] [id]

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
    std::size_t members = 0;
    bool found = false;
    for (int round = 0; round < rounds; ++round) {
        time(parse, [&] {
            wxl::xml::document document;
            document.load_file(file);
        });
        DocumentationFile const* opened = nullptr;
        std::optional<DocumentationFile> kept;
        time(open, [&] { opened = &kept.emplace(file); });
        members = opened->size();
        time(find, [&] { found = opened->find(id).has_value(); });
    }

    std::printf("  %zu members, %d rounds, %s %s\n\n", members, rounds, id, found ? "found" : "missing");
    std::printf("  parse (wxl.xml)    %9.2f ms\n", parse.milliseconds);
    std::printf("  open               %9.2f ms\n", open.milliseconds);
    std::printf("    of which ours    %9.2f ms\n", open.milliseconds - parse.milliseconds);
    std::printf("  find one member    %9.3f ms\n", find.milliseconds);
    return 0;
}
