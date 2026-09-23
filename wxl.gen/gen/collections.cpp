#include <format>
#include <ostream>
#include <print>

#include "wxl.gen.h"
import std;
// collections.h -- the container aliases the generated code uses instead of
// the std ones. All of them are backed by wxl.core's STA allocator
// (wxl::core::sta_allocator, ~15-16x faster than the default heap
// allocator), which is legitimate precisely because of wxl's foundational
// single-main-STA-thread guarantee: every container a wrapper owns is
// created, mutated and destroyed on that one thread.
//
// Only what generated code actually names is here. An alias nothing returns
// is a name in the public namespace that carries no meaning, and these read
// as std's own -- which is exactly why an unused one would mislead.
//
// AI note: this is the place to add containers as generated code starts
// needing them (a deque for a virtualizing panel, a flat_map for
// property tables, ...). Keep the pattern: alias std's container, replace
// only its allocator, and keep the wxl:: name identical to the std one --
// that std-mirroring spelling is the one deliberate exception to wxl's
// PascalCase for public types, and it is what makes these recognisable at a
// glance. Emitted rather than hand-maintained in the output tree so the
// whole generated directory stays reproducible from the generator alone.
namespace gen {
void write_collections(Output const& out, Emitted& emitted) {
    auto const path = out.dir / "collections.h";
    auto file = open_output(path);
    std::print(file, R"({}#pragma once

// core.h carries the wxl.core import (and the standard headers that have to
// precede it) for every wxl header alike.
#include "../core.h"

namespace wxl {{

// The alias below differs from its std counterpart in exactly one way: the
// allocator. wxl::core::sta_allocator hands out memory from the STA pool --
// valid because all of this lives on the single main STA thread wxl
// guarantees: every container a wrapper owns is created, mutated and
// destroyed on that one thread.
//
// It keeps std's spelling rather than wxl's PascalCase, because what it is
// meant to say is "std::wstring, with our allocator", and a WString would
// say something else.
//
// WinRT strings are UTF-16 (HSTRING), so this one is too.
using wstring = std::basic_string<char16_t, std::char_traits<char16_t>,
                                  core::sta_allocator<char16_t>>;

}}  // namespace wxl
)",
               banner);

    emitted.add(path);
    std::print("wrote {}\n", path.string());
}

}  // namespace gen
