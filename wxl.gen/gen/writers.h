#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "gen/emit.h"
#include "gen/model.h"
#include "gen/types.h"
#include "generate.h"
#include "md.h"
#include "xml_input.h"

// One entry point per kind of generated artefact; each lives in its own
// gen/*.cpp, beside the analysis of the same part (gen/model.h), so the file
// you open matches the output you're changing.
//
// A writer takes where to write, its part of the model, and the record of
// what has been written (which the CMakeLists writer consumes), and reads
// nothing else. write_all() (generate.cpp) calls them in order.

namespace gen {

// PropertyKey.h / EventKey.h -- the flat key enums the builder syntax
// uses for named-argument-style assignment.
void write_key_enums(output const& out, model const& m, emitted& em);

// One <Namespace>.Enums.h / <Namespace>.Structs.h per source WinRT
// namespace, plus the Enums.h / Structs.h umbrella headers.
void write_enums_and_structs(output const& out, model const& m, emitted& em);

// Structs.impl.h -- the private to_winrt/from_winrt overloads for every
// generated struct, added to the same overload sets impl/conversions.h
// opens. Written by gen/enums_structs.cpp alongside the structs themselves.

// One <Namespace>.EventArgs.h per source namespace -- plus a .cpp beside it
// wherever an args class has members -- and the EventArgs.h umbrella header.
void write_event_args(output const& out, model const& m, emitted& em);

// <Class>.h / <Class>.impl.h / <Class>.cpp for every file group: the
// public wrapper hierarchy and the matching `Impl` chain.
void write_classes(output const& out, model const& m, emitted& em);

// styles.h / styles.cpp -- the framework's named styles as a path,
// `styles.textBlock.title`. Read out of the XAML resource dictionaries the
// profile names, not out of metadata: a resource has a key and a target
// type, and no type of its own for the walk to find.
void write_styles(output const& out, model const& m, emitted& em);

// brushes.h / brush_names.h -- the framework's named brushes as a path,
// `brushes.Card.BackgroundFillColorDefault`. From the same dictionaries as
// the styles, and theme-resolved at lookup, which is what a hard-coded
// colour in application code can never be. The class names come in because a
// brush key names no target of its own: which control a key belongs to is
// guessed, and a type the run knows is the stronger of the two guesses.
void write_brushes(output const& out, model const& m, emitted& em);

// FluentSymbol.h -- the named glyphs of the icon font as an enumeration,
// from profiles/fluent-symbols.json. Not from metadata and not from the
// dictionaries: the font carries no glyph names, so the list is data typed
// in once and shared by every profile, the way types.json is.
void write_symbols(output const& out, model const& m, emitted& em);

// aliases.txt -- a reference sheet, not code: every StaticResource alias of
// the theme dictionaries and where it forwards, per theme where the themes
// disagree. How a developer learns which base resource dresses which part
// of which control, without reading a 3 MB dictionary.
void write_alias_index(output const& out, std::vector<dictionary_resource> const& resources,
                       emitted& em);

// Tags.h -- the tag types an unnamed argument uses for properties whose own
// type cannot identify them: `Margin{20}`, where the property itself still
// takes the plain Thickness the metadata declares.
void write_tags(output const& out, model const& m, emitted& em);

// Members.h -- the property and event tags the DSL writes (`content`,
// `onClick`) and the per-key dispatch behind them. One entry per member,
// none of it per class: the dispatch is a template on the object.
void write_dsl(output const& out, dsl const& d, emitted& em);

// schema.h -- the per-class vocabulary (schema, gen/model.h) -- and, beside it,
// schema_surface.cpp: one compile-only line per element of it, which is what
// keeps the schema honest. Every new type and every new member lands in both
// files by the same run, so coverage cannot drift from the surface. The dsl
// comes in for the enumerators an anchor of enum type carries, which the test
// reaches through the anchor; everything else the schema needs is in the
// schema itself.
void write_schema(output const& out, model const& m, emitted& em);

// CMakeLists.txt adding everything above to the consuming target.
void write_cmake_lists(output const& out, emitted const& em);

// An enum's values the profiles kept (`kept` is the walk's record of members):
// the member name each is surfaced under and the enumerator's own name.
// Defined in gen/enums_structs.cpp, which is where what counts as a value of
// an enum is decided.
std::vector<std::pair<std::string, std::string>> enum_members(
    md::TypeDef const& type, std::map<md::TypeDef, std::set<std::string>> const& kept);

// WinRT has no formal "EventArgs" category -- it's a naming convention.
// Defined in gen/event_args.cpp; gen/classes.cpp uses it to leave those
// classes to the EventArgs writer.
bool is_event_args_class(md::TypeDef const& type);

// The file group of every namespace, keyed by namespace: namespaces that
// depend on each other share one group, named after its shortest member.
// `edges` maps a namespace to the ones its base classes live in. Defined in
// gen/classes.cpp.
std::map<std::string, std::string> group_namespaces(
    std::map<std::string, std::set<std::string>> edges);

}  // namespace gen
