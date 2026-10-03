#include <format>
#include <print>

#include "wxl.gen.h"

import std;

namespace {

constexpr std::string_view usage = R"(winui-srcgen -- generates wxl wrappers from WinUI3 metadata.

  winui-srcgen --profile <file.json> [--profile <file.json>]...
               [--out <dir>] [--target <cmake target>]
               [--nuget-root <dir>] [--winmd <file.winmd>]...
               [--types <file.json>]

  --profile      Input profile; may be repeated. Profiles say which NuGet
                 packages to read, which types are the roots of the
                 dependency walk, and which of their members take part.
  --out          Output directory for the generated sources
                 (default: wxl.ui/src/generated).
  --target       CMake target the generated CMakeLists.txt adds its sources
                 to (default: wxl.ui).
  --nuget-root   NuGet package cache holding the packages the profiles name
                 (default: %NUGET_PACKAGES%, else %USERPROFILE%\.nuget\packages).
  --winmd        Extra .winmd file to read, on top of what the profiles resolve to.
  --types        What wxl implements itself: the types it supplies by hand,
                 the ones it already has an equivalent of, and the properties
                 carrying a tag (default: types.json beside the first profile).
)";

struct options {
    std::vector<std::filesystem::path> profiles;
    std::vector<std::filesystem::path> extra_metadata;
    // Default output lands in the library that consumes it; the generated
    // directory carries its own CMakeLists.txt (see gen/cmake.cpp).
    std::filesystem::path out_dir{"wxl.ui/src/generated"};
    std::string cmake_target{"wxl.ui"};
    std::filesystem::path nuget_root;
    std::filesystem::path type_map;
    std::filesystem::path symbols;
    bool help = false;
};

options parse_options(int argc, char** argv) {
    options opts;

    auto const value = [&](int& i, std::string_view option) {
        if (++i >= argc) {
            throw std::runtime_error(std::format("{} needs a value", option));
        }
        return std::string{argv[i]};
    };

    for (int i = 1; i < argc; ++i) {
        std::string_view const argument{argv[i]};
        if (argument == "--profile" || argument == "-p") {
            opts.profiles.emplace_back(value(i, argument));
        } else if (argument == "--out" || argument == "-o") {
            opts.out_dir = value(i, argument);
        } else if (argument == "--target") {
            opts.cmake_target = value(i, argument);
        } else if (argument == "--nuget-root") {
            opts.nuget_root = value(i, argument);
        } else if (argument == "--types") {
            opts.type_map = value(i, argument);
        } else if (argument == "--winmd") {
            opts.extra_metadata.emplace_back(value(i, argument));
        } else if (argument == "--help" || argument == "-h") {
            opts.help = true;
        } else {
            throw std::runtime_error(std::format("unknown argument: {}", argument));
        }
    }

    if (opts.nuget_root.empty()) {
        opts.nuget_root = default_nuget_root();
    }
    // Beside the first profile: the map belongs to the same tree the profiles
    // do, and naming it on every run would be noise.
    if (opts.type_map.empty() && !opts.profiles.empty()) {
        opts.type_map = opts.profiles.front().parent_path() / "types.json";
    }
    // The icon names live beside it, and for the same reason.
    if (opts.symbols.empty() && !opts.profiles.empty()) {
        opts.symbols = opts.profiles.front().parent_path() / "fluent-symbols.json";
    }
    return opts;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        auto const opts = parse_options(argc, argv);
        if (opts.help || opts.profiles.empty()) {
            std::print("{}", usage);
            return opts.help ? 0 : 1;
        }

        auto const types = load_type_map(opts.type_map);
        std::vector<symbol> symbols;
        if (std::filesystem::is_regular_file(opts.symbols)) {
            symbols = load_symbol_names(opts.symbols);
        }

        auto profiles = resolve_profiles(opts.profiles, opts.nuget_root);
        for (auto&& file : opts.extra_metadata) {
            if (!std::filesystem::is_regular_file(file)) {
                throw std::runtime_error(std::format("no such metadata file: {}", file.string()));
            }
            profiles.metadata.push_back(file);
        }
        if (profiles.metadata.empty()) {
            throw std::runtime_error("no metadata to read: the profiles name no packages");
        }
        if (profiles.types.empty()) {
            throw std::runtime_error("no roots to walk: the profiles list no types");
        }

        run(profiles, types, std::move(symbols), output{opts.out_dir, opts.cmake_target});
    } catch (std::exception const& e) {
        // Covers profile loading, metadata reading and file writing alike.
        std::print(stderr, "Error: {}\n", e.what());
        return 1;
    }

    return 0;
}
