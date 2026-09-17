module;

#include <set>
#include <string>
#include <vector>

// One member of a wrapped type, read off the interface that declares it.
//
// Both hierarchies wxl generates are built from this: an ordinary class
// wrapper reaches its interface through the lazily cached field on its
// `Impl`, an EventArgs view queries the interface off the raw ABI pointer
// it was handed -- but what a member *is*, how its signature crosses the
// public boundary and which members a profile let through are the same
// question in both, so it is answered once, here.

export module wxl.gen:members;

import :types;
import :md;
export namespace gen {

struct param_info {
    std::string name;
    TypeUse type;
};

// One forwarded property accessor, method, or event accessor. A property
// contributes up to two of these -- a getter (no parameters, a result) and
// a setter (one parameter, no result) -- and an event exactly two, an add
// taking a handler and a remove taking the token the add returned. All of
// them read as C++ overloads of one name, which is how the projection
// itself spells them.
struct member_info {
    enum class Kind { Forward, EventAdd, EventRemove, BoxedString };

    Kind kind = Kind::Forward;
    std::string name;        // as wxl spells it: camelCase
    std::string winrt_name;  // as the metadata and the projection spell it
    std::string field;       // the Impl field holding the interface declaring it
    TypeUse result;     // unused when `returns_void`
    bool returns_void = true;
    std::vector<param_info> params;
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

    // A property setter made out of a method a profile names (see profile.h):
    // the member is spelled as the property, `titleBar`, and its body calls
    // this method of the projection, `SetTitleBar`.
    std::string method_call;

    // Its value is a class narrower than the method's parameter, named by
    // the profile -- which is what lets the tag build one from braces.
    bool braced = false;
};

// What a member cannot be generated for, so the trimming stays visible
// instead of a member simply not being there.
struct skipped_member {
    std::string name;
    std::string reason;
};

// Accessors (get_X/put_X/add_X/remove_X) and .ctor reach the wrapper
// through the Property and Event tables instead, never as methods of their
// own.
bool is_plain_method(md::MethodDef const& method);

// Parameter names as the metadata spells them, indexed by position.
std::vector<std::string> parameter_names(md::MethodDef const& method);

// The members one interface contributes to the type implementing it,
// bounded by the names that survived the profile filter. `field` is what a
// generated body names to reach the interface -- the Impl field for a class
// wrapper, unused by an args view, which queries off its ABI pointer.
void collect_interface_members(md::TypeDef const& iface,
                               std::set<std::string> const& allowed, TypeIndex const& index,
                               std::vector<member_info>& members,
                               std::vector<skipped_member>& skipped);

// "Object const& value, double width" -- the parameter list as it appears
// in both the declaration and the definition.
std::string parameter_list(member_info const& member);

std::string result_type(member_info const& member);

}  // namespace gen
