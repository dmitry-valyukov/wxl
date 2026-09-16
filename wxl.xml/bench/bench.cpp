// What the reader is measured against: one real resource dictionary, with the
// phases timed apart so a change to one of them is visible on its own.
//
// The document is WinUI3's generic.xaml, which ships inside a NuGet package
// rather than this repository, so its path is an argument. Any XML file works;
// the numbers only mean something when the same one is used before and after.
//
// Usage: wxl.xml.bench <document.xml> [rounds]

#include <chrono>
#include <cstdio>
#include <cstdlib>

import wxl.core;
import wxl.xml;

namespace {

using clock = std::chrono::steady_clock;
using ms = std::chrono::duration<double, std::milli>;

/// The smallest of several runs rather than the average: the interesting
/// quantity is what the machine does when nothing interrupts it, and noise in
/// a measurement like this only ever adds.
class best_time {
public:
    void add(const clock::duration duration) noexcept {
        const double milliseconds = ms(duration).count();

        if (milliseconds < best_)
            best_ = milliseconds;
    }

    double milliseconds() const noexcept { return best_; }

    double megabytes_per_second(const std::size_t bytes) const noexcept {
        return static_cast<double>(bytes) / (best_ * 1000.0);
    }

private:
    double best_ = 1e30;
};

/// What one document costs the reader, phase by phase.
struct timings {
    best_time allocate;                                     ///< Room for the document, before a byte is in it.
    best_time read;                                         ///< The file to bytes, that allocation included.
    best_time validate;                                     ///< Checking the bytes are UTF-8.
    best_time parse;                                        ///< The grammar itself.
    best_time whole;                                        ///< parse_file, which is what a caller waits for.
};

/// What the tree turned out to hold, counted rather than assumed.
struct shape {
    std::size_t elements = 0;
    std::size_t comments = 0;
    std::size_t text_nodes = 0;
    std::size_t attributes = 0;
    std::size_t text_bytes = 0;                             ///< Names and values, as the tree hands them out.
};

void measure(const wxl::xml::node& el, shape& into) {
    switch (el.type()) {
    case wxl::xml::node_type::element: ++into.elements; break;
    case wxl::xml::node_type::comment: ++into.comments; break;
    case wxl::xml::node_type::text: ++into.text_nodes; break;
    }

    into.text_bytes += el.name().size() + el.value().size();

    for (const wxl::xml::attribute& a : el.attributes()) {
        ++into.attributes;
        into.text_bytes += a.name().size() + a.value().size();
    }

    for (const wxl::xml::node& child : el.children())
        measure(child, into);
}

void report(const char* const name, const best_time& best, const std::size_t bytes) {
    std::printf("  %-10s %8.2f ms   %8.1f MB/s\n", name, best.milliseconds(), best.megabytes_per_second(bytes));
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::puts("usage: wxl.xml.bench <document.xml> [rounds]\n"
                  "\n"
                  "The reference document is WinUI3's generic.xaml, e.g.\n"
                  "  <nuget>/microsoft.windowsappsdk.winui/2.3.2/lib/native/Microsoft.UI/Themes/generic.xaml");
        return 2;
    }

    const std::filesystem::path path = argv[1];
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 20;

    try {
        timings times;
        shape counted;
        std::size_t bytes = 0;

        for (int round = 0; round < rounds; ++round) {
            const auto read_started = clock::now();
            std::string source = wxl::xml::read_file(path);
            times.read.add(clock::now() - read_started);

            bytes = source.size();

            // What a buffer of the document's size costs before anything is in
            // it -- the part no reading strategy avoids.
            {
                const auto allocate_started = clock::now();
                std::string room;
                room.resize_and_overwrite(bytes, [](char*, const std::size_t size) { return size; });
                times.allocate.add(clock::now() - allocate_started);
            }

            // Timed here and paid again inside the parse below, which
            // validates what it is handed: this run only shows what it costs.
            const auto validate_started = clock::now();
            wxl::xml::validate_utf8(source);
            times.validate.add(clock::now() - validate_started);

            // Comments kept, though a real consumer drops them: the report
            // below describes the document, and this is the heavier path.
            const wxl::xml::options options{.keep_comments = true};

            wxl::xml::document p(options);

            const auto parse_started = clock::now();
            const wxl::xml::node& root = p.load(std::move(source), wxl::core::to_utf8(path).chars());
            times.parse.add(clock::now() - parse_started);

            if (round == 0) {
                counted = shape{};
                measure(root, counted);
            }

            // The whole of what a caller asks for, in the one call it makes.
            wxl::xml::document whole(options);

            const auto whole_started = clock::now();
            whole.load_file(path);
            times.whole.add(clock::now() - whole_started);
        }

        std::printf("%s\n  %zu bytes, %d rounds\n\n", path.string().c_str(), bytes, rounds);

        report("allocate", times.allocate, bytes);
        report("read", times.read, bytes);
        report("validate", times.validate, bytes);
        report("parse", times.parse, bytes);
        report("parse_file", times.whole, bytes);
        std::putchar('\n');

        std::printf("  %zu elements, %zu comments, %zu text nodes, %zu attributes\n",
                    counted.elements, counted.comments, counted.text_nodes, counted.attributes);
        std::printf("  text the tree points at: %zu bytes (%.1f%% of the document)\n",
                    counted.text_bytes,
                    100.0 * static_cast<double>(counted.text_bytes) / static_cast<double>(bytes));
    } catch (const wxl::xml::exception& exception) {
        std::printf("failed: %s\n", exception.what());
        return 1;
    }

    return 0;
}
