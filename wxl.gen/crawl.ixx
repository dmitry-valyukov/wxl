module;

#include <map>
#include <set>
#include <string>
#include <vector>

// Metadata side of winui-srcgen: takes the resolved profile input and
// collects the type closure it describes -- the types listed in the
// profiles are the *roots* of the walk, and each type's member filter
// bounds what that walk follows (a member that isn't generated can't drag
// its parameter/return types in). Knows nothing about C++ output; that's
// :generate.

export module wxl.gen:crawl;

import :profile;
import :md;
export {

// Types "given from above": already written by hand in wxl.ui, so the
// walk records them as a boundary and never expands their members. Which
// ones those are comes from the type map (profiles/types.json).
bool is_given_from_above(md::TypeDef const& type);

struct Closure {
    // Every discovered type, ordered from dependency sources to their
    // consumers: a type appears after everything it needs (base classes
    // before derived ones, member/field types before the types using
    // them). Best effort -- class/interface metadata is genuinely cyclic
    // (a class implements an interface whose members mention the class),
    // and a cycle is broken at whichever member closes it.
    std::vector<md::TypeDef> ordered;
    std::map<md::TypeDef, size_t> rank;  // index into `ordered`

    // Effective member surface per discovered type, after merging every
    // profile that reached it.
    std::map<md::TypeDef, MemberFilter> surface;

    // The members that actually survived the filter, per type that
    // declares them.
    std::map<md::TypeDef, std::set<std::string>> members;

    // Per class: the interfaces it implements directly that survived the
    // filter, i.e. still contribute at least one member. Those are
    // exactly the interfaces the wrapper has to hold -- one field per
    // interface on that level's `Impl`; an interface all of whose members
    // needs no field, no lazy QueryInterface slot, no code.
    std::map<md::TypeDef, std::vector<md::TypeDef>> interfaces;

    // Interfaces a class implements directly that did *not* survive --
    // reported so the trimming is visible rather than silent.
    std::map<md::TypeDef, std::vector<md::TypeDef>> dropped_interfaces;

    // Interfaces a profile names directly. These become wrappers of their
    // own, like classes: an interface listed by name is one some member
    // hands *back* (GetRange returning ITextRange), and a wrapper is the
    // only way to hold what came back. Every other interface stays what it
    // was -- a field on the class implementing it, or a parameter-only
    // Object.
    std::set<md::TypeDef> listed_interfaces;

    // Per class: the interfaces its parameterised constructors come from,
    // named by ActivatableAttribute. Like the statics they are reached
    // through the activation factory rather than through an object, and
    // like them they are not among the interfaces the class implements.
    std::map<md::TypeDef, std::vector<md::TypeDef>> factories;

    // Per class: the interfaces carrying its *static* members, named by
    // StaticAttribute. They are not among the interfaces the class
    // implements -- an instance has no static members -- so they are
    // collected separately and reached through the activation factory
    // rather than through the object.
    std::map<md::TypeDef, std::vector<md::TypeDef>> statics;

    // A property a profile added to a class (see profile.h), with the type
    // of its value resolved: naming it is what pulls that type into the
    // closure, since no signature in the metadata mentions the member.
    struct Synthetic {
        SyntheticMember declaration;
        md::TypeDef type;
    };

    // Per class: the properties added to it. They take part in the key enum
    // and the DSL exactly like the real ones; only the body differs.
    std::map<md::TypeDef, std::vector<Synthetic>> synthetic;

    // A method a profile writes as a tag (see profile.h), with the class it
    // narrows the value to resolved -- empty when it keeps the parameter's own
    // type. The narrowed class is pulled into the closure the way a synthetic
    // member's type is.
    struct Setter {
        SetterMethod declaration;
        md::TypeDef type;
    };

    // Per class: the methods written as tags on it.
    std::map<md::TypeDef, std::vector<Setter>> setter_methods;

    // Per class: the attached properties its statics declare. In metadata an
    // attached property is nothing but a Set<X>(element, value) / Get<X>
    // pair on the statics interface -- Grid.SetRow, Canvas.SetLeft -- and the
    // name is what the builder syntax writes on the *child*, so it takes part
    // in the key enum like any other property name.
    std::map<md::TypeDef, std::vector<std::string>> attached;

    std::set<std::string> property_names;
    std::set<std::string> event_names;

    // Members WinRT itself marked DeprecatedAttribute. They are dropped
    // before the walk follows their signatures, so a withdrawn member does
    // not pull types into the closure that nothing else needs; reported so
    // a profile naming one is told rather than left wondering.
    std::set<std::string> deprecated;

    // Given-from-above types the walk stopped at; recorded, never emitted.
    std::set<md::TypeDef> boundary;

    // Diagnostics: profile entries the metadata doesn't back up. Both are
    // reported by main(), not thrown -- a stale name shouldn't stop a run.
    std::vector<std::string> missing_types;    // "Ns.Type" not in the metadata at all
    std::vector<std::string> unknown_members;  // "Ns.Type.Member" nothing declares
};

Closure crawl(ProfileSet const& profiles, md::cache const& db);

}  // export
