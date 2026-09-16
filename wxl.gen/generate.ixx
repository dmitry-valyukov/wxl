module;

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

// Output side of winui-srcgen: everything that turns the discovered type
// closure into C++ sources under `Output::dir`. The metadata walk itself
// (:crawl) knows nothing about file layout or C++ syntax -- it only hands
// over the Model below; the individual writers live in gen/*.cpp, declared
// in gen/writers.ixx.

export module wxl.gen:generate;

import :crawl;
import :md;
export {

// What the metadata walk discovered, already grouped by category. The
// generator does its own further filtering (EventArgs classes are picked
// out of `classes` by naming convention, for instance), so the walk
// doesn't have to know which subsets matter to which output file.
//
// Every vector below is in the closure's own dependency order -- sources
// of dependencies first, the types built on them after (see crawl.h) --
// so generation emits in that order rather than re-sorting by name. The
// topological sorts inside the writers only refine that order within a
// single output file, where C++ requires a complete type before use.
struct Model {
    std::vector<md::TypeDef> enums;
    std::vector<md::TypeDef> structs;
    std::vector<md::TypeDef> classes;
    std::vector<md::TypeDef> interfaces;

    // The interfaces a profile names directly, in the same closure order.
    // The class writer wraps these like classes -- an interface listed by
    // name is one some member hands back, and a wrapper is the only way to
    // hold what came back.
    std::vector<md::TypeDef> listed_interfaces;

    // Per class: the interfaces that survived the filter, i.e. the ones
    // this level has to hold a field for on its own `Impl` (see crawl.h).
    std::map<md::TypeDef, std::vector<md::TypeDef>> interfaces_of;

    // Per class: the interfaces its parameterised constructors come from
    // (see crawl.h). Reached through the activation factory, like the
    // statics, and like them holding no field on any Impl.
    std::map<md::TypeDef, std::vector<md::TypeDef>> factories_of;

    // Per class: the interfaces carrying its static members (see crawl.h).
    // Reached through the activation factory, not through an object, so they
    // hold no field on any Impl.
    std::map<md::TypeDef, std::vector<md::TypeDef>> statics_of;

    // Per class: the properties a profile added although WinRT declares
    // none (see profile.h). Emitted like any other member, with a call to a
    // hand-written function as the body.
    std::map<md::TypeDef, std::vector<Closure::Synthetic>> synthetic_of;

    // Per class: the attached properties its statics declare (see crawl.h).
    // The tag is written on the child, the call goes to the owner.
    std::map<md::TypeDef, std::vector<std::string>> attached_of;

    // Per type: the member names that survived the profile filter. A class
    // looks its own members up under the interfaces it implements, which is
    // where WinRT declares them.
    std::map<md::TypeDef, std::set<std::string>> members;

    std::set<std::string> property_names;
    std::set<std::string> event_names;
};

// Where generation writes, and which CMake target the generated
// CMakeLists.txt adds its sources to.
struct Output {
    std::filesystem::path dir;
    std::string cmake_target;
};

// Creates `out.dir` and runs every generation step, in order, finishing
// with the CMakeLists.txt that lists everything written.
// `resource_dictionaries` are the XAML files the profile named -- the one
// input that is not metadata, since a named style has a key and a target
// type and no type of its own for the walk to find.
void write_all(Output const& out, Model const& model,
               std::vector<std::filesystem::path> const& resource_dictionaries);

}  // export
