#pragma once

#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "gen/types.h"
#include "md.h"
#include "xml_input.h"

// One member of a wrapped type, read off the interface that declares it.
//
// Both hierarchies wxl generates are built from this: an ordinary class
// wrapper reaches its interface through the lazily cached field on its
// `Impl`, an EventArgs view queries the interface off the raw ABI pointer
// it was handed -- but what a member *is*, how its signature crosses the
// public boundary and which members a profile let through are the same
// question in both, so it is answered once, here.

namespace gen {

struct param_info {
    std::string name;
    type_use type;
};

// One forwarded property accessor, method, or event accessor. A property
// contributes up to two of these -- a getter (no parameters, a result) and
// a setter (one parameter, no result) -- and an event exactly two, an add
// taking a handler and a remove taking the token the add returned. All of
// them read as C++ overloads of one name, which is how the projection
// itself spells them.
struct member_info {
    enum class kind_t { Forward, EventAdd, EventRemove, BoxedString };

    kind_t kind = kind_t::Forward;
    std::string name;        // as wxl spells it: camelCase
    std::string winrt_name;  // as the metadata and the projection spell it
    std::string field;       // the Impl field holding the interface declaring it
    type_use result;     // unused when `returns_void`
    bool returns_void = true;
    std::vector<param_info> params;

    // A method whose last parameter is an out value and which returns nothing
    // else: the wxl member returns that value (`GetText(options, out string)`
    // becomes `getText(options)`), and the call passes a local for it.
    // `out_param` is that parameter's name, which documents the result.
    bool out_result = false;
    std::string out_param;

    // A constructor of a composable class: its factory method carries the two parameters of
    // composition after its own (the object being composed into, and the inner one handed
    // back). They are dropped here, and the constructor is made through the projection's
    // own, which supplies them.
    bool composable_constructor = false;
    std::string args_type;  // events only: what the handler is handed besides the sender

    // Whether that args type is an ordinary wrapper rather than an EventArgs
    // view: an event whose args are `object` (or any class of the wrapper
    // hierarchy) hands over a real wxl object, built by the same bridge the
    // sender comes through, instead of a non-owning view of an ABI pointer.
    bool args_are_wrapper = false;

    // A property setter, as opposed to a one-argument void method, which
    // looks identical from the signature alone. Only a property gets a DSL
    // tag: `ScrollIntoView = item` would read as an assignment to something
    // that is not a property.
    bool is_property_setter = false;

    // And its other half, which the signature hides just as well: a getter
    // takes nothing and returns something, and so does a method like
    // GetCurrentPoint. The two are told apart where const-ness is decided --
    // an EventArgs view keeps honest const-ness, and reading a property is
    // the one thing that does not change it.
    bool is_property_getter = false;

    // A member of the class rather than of an object. It has no Impl field
    // to reach through: the interface declaring it belongs to the activation
    // factory, and `statics_interface` names it as the projection spells it.
    bool is_static = false;
    std::string statics_interface;

    // A property wxl adds where WinRT declares none: the body is a call to
    // this hand-written function, taking the object and the value, instead
    // of a hop into the projection.
    std::string synthetic_call;

    // A method whose call a profile replaces (see ReplacedCall in profile.h): the body
    // calls this hand-written function with the arguments the projection call would
    // get, and the header declaring it belongs to the .cpp side.
    std::string replaced_call;
    std::string replaced_include;

    // A property setter made out of a method a profile names (see profile.h):
    // the member is spelled as the property, `titleBar`, and its body calls
    // this method of the projection, `SetTitleBar`.
    std::string method_call;

    // Its value is a class narrower than the method's parameter, named by
    // the profile -- which is what lets the tag build one from braces.
    bool braced = false;

    // The form with a token of an asynchronous method: its last parameter, `stop`, is wxl's and
    // not the projection's -- the call is made without it, and the operation stands under it
    // (result.from_winrt is the operation's from_winrt_under).
    bool under_token = false;

    // The member's .NET documentation ID, on the type the documentation files
    // key it on (see documentation.h); empty for a member the metadata does not
    // declare. And what the documentation says under it, when a run writes it.
    std::string doc_id;
    member_documentation doc;
};

// The parameters the projection's call is handed: all of them, but the token of a form with
// one.
std::size_t projection_arity(member_info const& member);

// What a member cannot be generated for, so the trimming stays visible
// instead of a member simply not being there.
struct skipped_member {
    std::string name;
    std::string reason;
};

// Parameter names as the metadata spells them, indexed by position.
std::vector<std::string> parameter_names(md::MethodDef const& method);

// The members one interface contributes to the type implementing it,
// bounded by the names that survived the profile filter. Each one records
// in member_info::field the Impl field named after the interface, which is
// how a class wrapper's body reaches it; an args view leaves it unused and
// queries off its ABI pointer.
//
// `owner` is the type the documentation keys the members on -- the class,
// for its interfaces, statics and factories alike, or the interface itself;
// the methods of a `factory` are documented as the owner's constructors.
void collect_interface_members(md::TypeDef const& iface, md::TypeDef const& owner, bool factory,
                               std::set<std::string> const& allowed, type_index const& index,
                               std::vector<member_info>& members,
                               std::vector<skipped_member>& skipped);

// The parameters a member declares in wxl, by name: what a @param may name.
std::vector<std::string_view> parameter_names(member_info const& member);

// The documentation of a member's declaration in a class body (see write_doc).
void write_member_doc(std::ostream& out, member_info const& member);

// "Object const& value, double width" -- the parameter list as it appears
// in both the declaration and the definition.
std::string parameter_list(member_info const& member);

std::string result_type(member_info const& member);

}  // namespace gen
