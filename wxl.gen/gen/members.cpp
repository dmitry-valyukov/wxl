#include <format>

#include "documentation.h"

#include "wxl.gen.h"

import std;

using namespace md;

namespace gen {
namespace {

// The namespace the delegate itself lives in -- for a generic
// TypedEventHandler<S, A> that of the generic type, not of its arguments.
// Its projection header is where the delegate's constructor template is
// defined, and subscribing instantiates exactly that.
std::string delegate_namespace(coded_index<TypeDefOrRef> const& delegate) {
    if (delegate.type() == TypeDefOrRef::TypeSpec) {
        // Named, not a temporary: GenericTypeInst() refers into it.
        auto const signature = delegate.TypeSpec().Signature();
        auto const generic = signature.GenericTypeInst().GenericType();
        if (generic.type() == TypeDefOrRef::TypeSpec) {
            return {};
        }
        auto const type = md::find(generic);
        return type ? std::string{type.TypeNamespace()} : std::string{};
    }
    auto const type = md::find(delegate);
    return type ? std::string{type.TypeNamespace()} : std::string{};
}

// What a delegate hands its handler besides the sender: the second parameter
// of the delegate's Invoke, or -- for the generic TypedEventHandler<S, A>
// that most WinUI events use -- its second type argument.
struct event_args {
    enum class shape_t {
        Unsupported,  // a shape wxl has no answer for yet
        Object,       // `object` in the metadata: wxl::Object, through the bridge
        Type,         // a named type, in `type` below
    };

    shape_t shape = shape_t::Unsupported;
    TypeDef type;
};

event_args event_args_type(coded_index<TypeDefOrRef> const& delegate) {
    auto const resolve = [](TypeSig const& sig) -> event_args {
        // `object` is the metadata's IInspectable, which is what wxl::Object
        // wraps, so the args need no view type of their own.
        if (auto const* element = std::get_if<ElementType>(&sig.Type());
            element && *element == ElementType::Object) {
            return {event_args::shape_t::Object, {}};
        }
        auto const* ref = std::get_if<coded_index<TypeDefOrRef>>(&sig.Type());
        if (!ref || ref->type() == TypeDefOrRef::TypeSpec) {
            return {};
        }
        auto const type = md::find(*ref);
        return type ? event_args{event_args::shape_t::Type, type} : event_args{};
    };

    if (delegate.type() == TypeDefOrRef::TypeSpec) {
        auto const signature = delegate.TypeSpec().Signature();
        auto const& instantiation = signature.GenericTypeInst();
        std::vector<TypeSig> arguments;
        for (auto&& argument : instantiation.GenericArgs()) {
            arguments.push_back(argument);
        }
        // Two shapes, and the args are the last argument of either:
        // TypedEventHandler<Sender, Args> parameterises both, EventHandler<Args>
        // only the args and takes the sender as a plain object.
        if (arguments.size() == 1 || arguments.size() == 2) {
            return resolve(arguments.back());
        }
        return {};
    }

    auto const type = md::find(delegate);
    if (!type) {
        return {};
    }
    for (auto&& method : type.MethodList()) {
        if (method.Name() != "Invoke") {
            continue;
        }
        // Named, not a temporary: Params() hands back iterators into the
        // signature's own storage, which a temporary would have destroyed
        // by the time the loop reads them.
        auto const signature = method.Signature();
        std::vector<ParamSig> params;
        for (auto&& param : signature.Params()) {
            params.push_back(param);
        }
        return params.size() == 2 ? resolve(params[1].Type()) : event_args{};
    }
    return {};
}

void collect_event(Event const& event, std::string_view field_view, TypeDef const& owner,
                   type_index const& index, std::vector<member_info>& members,
                   std::vector<skipped_member>& skipped) {
    // The field name goes into every member_info this builds, and each of
    // those keeps its own string, so the copy is made once here.
    const std::string field(field_view);

    auto const args = event_args_type(event.EventType());
    if (args.shape == event_args::shape_t::Unsupported) {
        skipped.push_back({std::string{event.Name()},
                           "handler shape is not (sender, args)"});
        return;
    }

    // `object` args need no view type: that *is* what wxl::Object wraps, and
    // the handler gets one built by the same bridge the sender comes through.
    // A named type is either an EventArgs class, which has a view of its own,
    // or an ordinary wrapper, which goes through the bridge as well.
    std::string args_name = "Object";
    bool args_are_wrapper = true;
    std::set<std::string> public_includes{"../events.h"};
    std::set<std::string> impl_includes{"../impl/conversions.h"};

    if (args.shape == event_args::shape_t::Type) {
        auto const name = index.names.find(args.type);
        if (name == index.names.end()) {
            skipped.push_back({std::string{event.Name()},
                               std::format("no wxl type for {}", full_name(args.type))});
            return;
        }
        args_name = name->second;
        args_are_wrapper = !is_event_args_class(args.type);
        public_includes.insert(index.headers.at(args.type));
        impl_includes.insert(winrt_include(args.type.TypeNamespace()));
    }

    // add_onTextChanged / remove_onTextChanged, not an overload pair on one
    // name: subscribing and unsubscribing are different acts, and the names
    // carry the event tag (onTextChanged) inside them, so the DSL form and
    // the method form read as the same thing.
    member_info add{member_info::kind_t::EventAdd, std::format("add_on{}", event.Name()),
                    std::string{event.Name()}, field};
    std::string const doc_id = documentation_id(owner, {owner, event}, event.Name());
    add.doc_id = doc_id;
    add.returns_void = false;
    add.result.value_type = "EventToken";
    add.args_type = args_name;
    add.args_are_wrapper = args_are_wrapper;
    add.params.push_back({"handler", {}});
    add.params.back().type.param_type = std::format("EventHandler<{}> const&", args_name);
    add.params.back().type.public_includes = std::move(public_includes);

    // Subscribing constructs the delegate, so the header defining that
    // delegate has to be here -- the interface's own header only declares it.
    // Its argument type comes along for the same reason.
    add.params.back().type.impl_includes = std::move(impl_includes);
    if (auto const ns = delegate_namespace(event.EventType()); !ns.empty()) {
        add.params.back().type.impl_includes.insert(winrt_include(ns));
    }
    members.push_back(std::move(add));

    member_info remove{member_info::kind_t::EventRemove, std::format("remove_on{}", event.Name()),
                       std::string{event.Name()}, field};
    remove.doc_id = doc_id;
    remove.params.push_back({"token", {}});
    // The caller's own token, emptied by the removal (impl::take_token): a second removal through
    // the same variable would otherwise take off whatever subscription got the number next.
    remove.params.back().type.param_type = "EventToken&";
    remove.params.back().type.public_includes = {"../events.h"};
    remove.params.back().type.impl_includes = {"../impl/conversions.h"};
    members.push_back(std::move(remove));
}

void collect_property(Property const& property, std::string_view field_view, TypeDef const& owner,
                      type_index const& index, std::vector<member_info>& members,
                      std::vector<skipped_member>& skipped) {
    // The field name goes into every member_info this builds, and each of
    // those keeps its own string, so the copy is made once here.
    const std::string field(field_view);

    auto const use = map_type(property.Type().Type(), index);
    if (!use.supported) {
        skipped.push_back({std::string{property.Name()}, use.reason});
        return;
    }

    // An object-typed property additionally takes a string directly, boxing
    // it on the way in, so the DSL writes `Content = L"Click"` rather than
    // `Content = box_value(L"Click")`. Strings are the case that actually
    // appears there; the numeric primitives would multiply the overload set
    // across every object-typed property for a gain nobody needs.
    bool const boxes_strings = use.winrt_type == "winrt::Windows::Foundation::IInspectable";

    std::string const wxl_name = member_name(property.Name());
    std::string const winrt_name{property.Name()};
    std::string const doc_id = documentation_id(owner, {owner, property}, property.Name());

    for (auto&& semantic : property.MethodSemantic()) {
        if (semantic.Semantic().Getter()) {
            // A property whose type can be taken in but not handed back --
            // an interface, today -- keeps its setter and loses its getter,
            // rather than the whole property being dropped: assigning to it
            // is what such a property is for.
            if (use.parameter_only) {
                skipped.push_back({winrt_name, std::format("getter: {} can be taken in but not "
                                                           "handed back",
                                                           use.winrt_type)});
                continue;
            }
            member_info getter{member_info::kind_t::Forward, wxl_name, winrt_name, field, use,
                               /*returns_void=*/false};
            getter.is_property_getter = true;
            getter.doc_id = doc_id;
            members.push_back(std::move(getter));
        } else if (semantic.Semantic().Setter()) {
            member_info setter{member_info::kind_t::Forward, wxl_name, winrt_name, field, {},
                               /*returns_void=*/true, {{"value", use}}};
            setter.is_property_setter = true;
            setter.doc_id = doc_id;
            members.push_back(std::move(setter));
            if (boxes_strings) {
                member_info boxed{member_info::kind_t::BoxedString, wxl_name, winrt_name, field};
                boxed.params.push_back({"value", {}});
                boxed.params.back().type.param_type = "hstring_param const&";
                boxed.params.back().type.public_includes = {"hstring_param.h"};
                boxed.params.back().type.impl_includes = {"../impl/conversions.h"};
                members.push_back(std::move(boxed));
            }
        }
    }
}

void collect_method(MethodDef const& method, std::string_view field_view, TypeDef const& owner,
                    bool factory, type_index const& index, std::vector<member_info>& members,
                    std::vector<skipped_member>& skipped) {
    // The field name goes into every member_info this builds, and each of
    // those keeps its own string, so the copy is made once here.
    const std::string field(field_view);

    member_info info{member_info::kind_t::Forward, member_name(method.Name()),
                     std::string{method.Name()}, field};

    auto const signature = method.Signature();
    if (auto const& returned = signature.ReturnType()) {
        info.result = map_type(returned.Type(), index);
        info.returns_void = false;
        if (info.result.supported && info.result.parameter_only) {
            info.result.supported = false;
            info.result.reason = std::format("{} can be taken in but not handed back",
                                             info.result.value_type);
        }
        if (!info.result.supported) {
            skipped.push_back({info.name, std::format("return type: {}", info.result.reason)});
            return;
        }
    }

    auto const names = parameter_names(method);
    size_t position = 0;
    size_t count = 0;
    for (auto&& param : signature.Params()) {
        static_cast<void>(param);
        ++count;
    }
    size_t slot = 0;  // place in the signature, where `position` counts what is kept
    for (auto&& param : signature.Params()) {
        // The pair composition adds to a factory method, named so in every composable
        // factory: baseInterface in, innerInterface out. Both are left out; the
        // constructor is made through the projection's own, which supplies them.
        if (slot < names.size() && names[slot] == "baseInterface" && count >= slot + 2) {
            info.composable_constructor = true;
            ++slot;
            continue;
        }
        if (info.composable_constructor && param.ByRef()) {
            ++slot;
            continue;
        }
        ++slot;

        auto use = map_type(param.Type(), index);
        if (!use.supported) {
            skipped.push_back({info.name, std::format("parameter type: {}", use.reason)});
            return;
        }
        if (use.result_only) {
            skipped.push_back({info.name, std::format("parameter type: {} is only handed back", use.value_type)});
            return;
        }
        if (param.ByRef()) {
            // The one shape mapped: the last parameter, in a method that
            // returns nothing else. Anything more (an out beside a result, two
            // outs) is not something a return value can carry.
            if (position + 1 == count && info.returns_void && !use.parameter_only) {
                info.result = std::move(use);
                info.returns_void = false;
                info.out_result = true;
                info.out_param = slot - 1 < names.size() ? names[slot - 1] : std::string{};
                break;
            }
            skipped.push_back({info.name, "out parameters are not mapped yet"});
            return;
        }
        info.params.push_back(
            {slot - 1 < names.size() ? names[slot - 1] : std::format("arg{}", slot - 1),
             std::move(use)});
        ++position;
    }

    info.doc_id = factory ? constructor_documentation_id(owner, method, info.composable_constructor ? 2 : 0)
                          : documentation_id(owner, {owner, method}, method.Name());

    // Beside every asynchronous method, its form with a token, taken last as wxl::async's own
    // operations take one: cancelled, the operation is asked to stop and the co_await ends with
    // the cancellation (impl/operation.h). The token is a parameter of wxl's alone, so it is
    // named by wxl (cancellation_parameter_name).
    if (!info.returns_void && !info.result.from_winrt_cancelable.empty()) {
        std::string name;
        try {
            name = cancellation_parameter_name(info.params);
        } catch (std::runtime_error const& error) {
            throw std::runtime_error(std::format("{}: {}", info.winrt_name, error.what()));
        }

        member_info cancelable = info;
        cancelable.cancelable = true;
        cancelable.result.from_winrt.clear();
        for (char const c : cancelable.result.from_winrt_cancelable) {
            if (c == '@')
                cancelable.result.from_winrt += name;
            else
                cancelable.result.from_winrt += c;
        }

        type_use token;
        token.supported = true;
        token.value_type = "async::cancellation_token";
        token.param_type = token.value_type;
        token.public_includes = {"../Operation.h"};
        cancelable.params.push_back({std::move(name), std::move(token)});

        members.push_back(std::move(info));
        members.push_back(std::move(cancelable));
        return;
    }

    members.push_back(std::move(info));
}

}  // namespace

std::vector<std::string> parameter_names(MethodDef const& method) {
    std::vector<std::string> names;
    for (auto&& param : method.ParamList()) {
        auto const sequence = param.Sequence();
        if (sequence == 0) {
            continue;
        }
        if (names.size() < sequence) {
            names.resize(sequence);
        }
        names[sequence - 1] = std::string{param.Name()};
    }
    for (size_t i = 0; i < names.size(); ++i) {
        if (names[i].empty()) {
            names[i] = std::format("arg{}", i);
        }
    }
    return names;
}

// The members one interface contributes to the class implementing it,
// bounded by the names that survived the profile filter.
void collect_interface_members(TypeDef const& iface, TypeDef const& owner, bool factory,
                               std::set<std::string> const& allowed, type_index const& index,
                               std::vector<member_info>& members,
                               std::vector<skipped_member>& skipped) {
    auto const field = interface_field_name(iface.TypeName());

    for (auto&& property : iface.PropertyList()) {
        if (allowed.count(std::string{property.Name()})) {
            collect_property(property, field, owner, index, members, skipped);
        }
    }
    for (auto&& event : iface.EventList()) {
        if (allowed.count(std::string{event.Name()})) {
            collect_event(event, field, owner, index, members, skipped);
        }
    }
    for (auto&& method : iface.MethodList()) {
        if (is_plain_method(method) && allowed.count(std::string{method.Name()})) {
            collect_method(method, field, owner, factory, index, members, skipped);
        }
    }
}

std::vector<std::string_view> parameter_names(member_info const& member) {
    std::vector<std::string_view> names;
    for (auto&& param : member.params) {
        names.push_back(param.name);
    }
    return names;
}

void write_member_doc(std::ostream& out, member_info const& member) {
    write_doc(out, "    ", member.doc, parameter_names(member), !member.returns_void);
}

std::string parameter_list(member_info const& member) {
    std::string list;
    for (auto&& param : member.params) {
        if (!list.empty()) {
            list += ", ";
        }
        list += std::format("{} {}", param.type.param_type, param.name);
    }
    return list;
}

std::size_t projection_arity(member_info const& member) {
    return member.cancelable ? member.params.size() - 1 : member.params.size();
}

std::string cancellation_parameter_name(std::vector<param_info> const& params) {
    auto const taken = [&params](std::string_view name) {
        return std::any_of(params.begin(), params.end(),
                           [name](param_info const& param) { return param.name == name; });
    };
    if (!taken("token")) return "token";
    if (!taken("cancellation")) return "cancellation";
    throw std::runtime_error(
        "parameters named 'token' and 'cancellation' leave no name for the token of its form with "
        "one");
}

std::string result_type(member_info const& member) {
    return member.returns_void ? "void" : member.result.value_type;
}

}  // namespace gen
