module;

#include <format>
#include <print>

module wxl.gen;

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
    enum class Shape {
        Unsupported,  // a shape wxl has no answer for yet
        Object,       // `object` in the metadata: wxl::Object, through the bridge
        Type,         // a named type, in `type` below
    };

    Shape shape = Shape::Unsupported;
    TypeDef type;
};

event_args event_args_type(coded_index<TypeDefOrRef> const& delegate) {
    auto const resolve = [](TypeSig const& sig) -> event_args {
        // `object` is the metadata's IInspectable, which is what wxl::Object
        // wraps, so the args need no view type of their own.
        if (auto const* element = std::get_if<ElementType>(&sig.Type());
            element && *element == ElementType::Object) {
            return {event_args::Shape::Object, {}};
        }
        auto const* ref = std::get_if<coded_index<TypeDefOrRef>>(&sig.Type());
        if (!ref || ref->type() == TypeDefOrRef::TypeSpec) {
            return {};
        }
        auto const type = md::find(*ref);
        return type ? event_args{event_args::Shape::Type, type} : event_args{};
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

void collect_event(Event const& event, std::string_view field_view, TypeIndex const& index,
                   std::vector<member_info>& members, std::vector<skipped_member>& skipped) {
    // The field name goes into every member_info this builds, and each of
    // those keeps its own string, so the copy is made once here.
    const std::string field(field_view);

    auto const args = event_args_type(event.EventType());
    if (args.shape == event_args::Shape::Unsupported) {
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

    if (args.shape == event_args::Shape::Type) {
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
    member_info add{member_info::Kind::EventAdd, std::format("add_on{}", event.Name()),
                    std::string{event.Name()}, field};
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

    member_info remove{member_info::Kind::EventRemove, std::format("remove_on{}", event.Name()),
                       std::string{event.Name()}, field};
    remove.params.push_back({"token", {}});
    remove.params.back().type.param_type = "EventToken";
    remove.params.back().type.public_includes = {"../events.h"};
    remove.params.back().type.impl_includes = {"../impl/conversions.h"};
    members.push_back(std::move(remove));
}

void collect_property(Property const& property, std::string_view field_view, TypeIndex const& index,
                      std::vector<member_info>& members, std::vector<skipped_member>& skipped) {
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
            member_info getter{member_info::Kind::Forward, wxl_name, winrt_name, field, use,
                               /*returns_void=*/false};
            getter.is_property_getter = true;
            members.push_back(std::move(getter));
        } else if (semantic.Semantic().Setter()) {
            member_info setter{member_info::Kind::Forward, wxl_name, winrt_name, field, {},
                               /*returns_void=*/true, {{"value", use}}};
            setter.is_property_setter = true;
            members.push_back(std::move(setter));
            if (boxes_strings) {
                member_info boxed{member_info::Kind::BoxedString, wxl_name, winrt_name, field};
                boxed.params.push_back({"value", {}});
                boxed.params.back().type.param_type = "string_param";
                boxed.params.back().type.public_includes = {"string_param.h", "<string_view>"};
                boxed.params.back().type.impl_includes = {"../impl/conversions.h"};
                members.push_back(std::move(boxed));
            }
        }
    }
}

void collect_method(MethodDef const& method, std::string_view field_view, TypeIndex const& index,
                    std::vector<member_info>& members, std::vector<skipped_member>& skipped) {
    // The field name goes into every member_info this builds, and each of
    // those keeps its own string, so the copy is made once here.
    const std::string field(field_view);

    member_info info{member_info::Kind::Forward, member_name(method.Name()),
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
    for (auto&& param : signature.Params()) {
        auto use = map_type(param.Type(), index);
        if (!use.supported) {
            skipped.push_back({info.name, std::format("parameter type: {}", use.reason)});
            return;
        }
        if (param.ByRef()) {
            skipped.push_back({info.name, "out parameters are not mapped yet"});
            return;
        }
        info.params.push_back(
            {position < names.size() ? names[position] : std::format("arg{}", position),
             std::move(use)});
        ++position;
    }

    members.push_back(std::move(info));
}

}  // namespace

bool is_plain_method(MethodDef const& method) {
    return !method.Flags().SpecialName() && !method.Flags().RTSpecialName();
}

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
void collect_interface_members(TypeDef const& iface, std::set<std::string> const& allowed,
                               TypeIndex const& index, std::vector<member_info>& members,
                               std::vector<skipped_member>& skipped) {
    auto const field = interface_field_name(iface.TypeName());

    for (auto&& property : iface.PropertyList()) {
        if (allowed.count(std::string{property.Name()})) {
            collect_property(property, field, index, members, skipped);
        }
    }
    for (auto&& event : iface.EventList()) {
        if (allowed.count(std::string{event.Name()})) {
            collect_event(event, field, index, members, skipped);
        }
    }
    for (auto&& method : iface.MethodList()) {
        if (is_plain_method(method) && allowed.count(std::string{method.Name()})) {
            collect_method(method, field, index, members, skipped);
        }
    }
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

std::string result_type(member_info const& member) {
    return member.returns_void ? "void" : member.result.value_type;
}

}  // namespace gen
