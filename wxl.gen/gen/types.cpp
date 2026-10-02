#include <format>

#include "wxl.gen.h"

import std;
import wxl.core;

using namespace md;

namespace gen {
namespace {

// The hand-written private header carrying the conversions that aren't
// generated: strings, and the WinRT types projected onto wxl's own
// (geometry, Color, chrono).
constexpr std::string_view conversions_include = "../impl/conversions.h";

type_use unsupported(std::string reason) {
    type_use use;
    use.reason = std::move(reason);
    return use;
}

// A primitive crosses the boundary unchanged: the same C++ type on both
// sides, so both conversions are the identity.
type_use primitive(std::string_view cpp_type) {
    type_use use;
    use.supported = true;
    use.value_type = cpp_type;
    use.param_type = cpp_type;
    use.winrt_type = cpp_type;
    use.to_winrt = "$";
    use.from_winrt = "$";
    return use;
}

// WinRT strings are HSTRINGs. A string read off a member is an hstring -- a
// reference to the very HSTRING the object holds, so reading a Text is a count
// and not a copy of its text -- and one written is an hstring_param, which
// borrows: an hstring's handle as it is, or text with a terminator under a
// fast-pass header. Both are wxl.core's, named in hstring_param.h; the unit is
// char16_t, and a wchar_t literal still goes in through the seam in
// zstring_view. See hstring_param.h for why the move away from wchar_t is
// happening at all.
type_use string_type() {
    type_use use;
    use.supported = true;
    use.value_type = "hstring";
    use.param_type = "hstring_param const&";
    use.winrt_type = "winrt::hstring";
    use.to_winrt = "impl::to_winrt($)";
    use.from_winrt = "impl::from_winrt($)";
    use.public_includes = {"hstring_param.h"};
    use.impl_includes = {std::string{conversions_include}};
    return use;
}
// Any wrapped object -- wxl::Object itself, or a generated wrapper.
//
// Crossing in hands the call the argument's own Impl: each level declares a
// conversion operator per interface it holds, so the argument binds to the
// one the call wants with no field named at the call site, and the single
// QueryInterface behind that field is paid once per object by the lazy cache
// rather than once per call. Crossing out builds a fresh wrapper around the
// returned object.
type_use wrapper_type(std::string_view name, std::string_view winrt_name,
                      std::string_view public_include) {
    type_use use;
    use.supported = true;
    use.is_wrapper = true;
    use.value_type = std::string{name};
    use.param_type = std::format("{} const&", use.value_type);
    use.winrt_type = winrt_name;
    use.to_winrt = std::format("*Object::Impl::get_typed<{}>($)", use.value_type);
    use.from_winrt = std::format("Object::Impl::wrap<{}>($)", use.value_type);
    use.public_includes = {std::string{public_include}};

    // Building a wrapper means allocating its Impl, so the private header
    // defining that Impl chain is needed -- every public header has one
    // beside it under the same name. It is also all the source needs: that
    // header includes the projection of every type its Impl fields hold, and
    // the source names none of them itself.
    std::string_view const stem = public_include.substr(0, public_include.size() - 2);
    use.impl_includes = {std::format("{}.impl.h", stem)};
    return use;
}

bool mirrorable_field(TypeSig const& sig, type_map const& types);

// Whether a struct is the ABI struct field for field, which is what wxl
// generating a struct at all rests on: the wxl copy is written from the same
// metadata and crossed with a bit_cast.
//
// Not every WinRT struct is. Windows.UI.Xaml.Interop.TypeName carries a
// String, which the ABI holds as an HSTRING and the projection as a
// winrt::hstring -- an owning type, and one no cast produces. A struct like
// that has no wxl form yet, and is dropped rather than generated as
// something that claims to mirror an ABI it does not.
bool struct_mirrors_abi(TypeDef const& type, type_map const& types) {
    for (auto&& field : type.FieldList()) {
        if (field.Flags().Literal()) {
            continue;
        }
        if (!mirrorable_field(field.Signature().Type(), types)) {
            return false;
        }
    }
    return true;
}

bool mirrorable_field(TypeSig const& sig, type_map const& types) {
    if (sig.is_szarray()) {
        return false;
    }
    if (primitive_name(sig.element_type())) {
        return true;
    }

    auto const* ref = std::get_if<coded_index<TypeDefOrRef>>(&sig.Type());
    if (!ref || ref->type() == TypeDefOrRef::TypeSpec) {
        return false;
    }

    auto const resolved = md::find(*ref);
    if (!resolved) {
        return false;
    }
    if (project_type(resolved, types)) {
        // A type wxl has its own of -- Point, Size, Rect -- and those are
        // the ABI's own layout too.
        return true;
    }
    switch (get_category(resolved)) {
        case category::enum_type:
            return true;
        case category::struct_type:
            return struct_mirrors_abi(resolved, types);
        default:
            return false;
    }
}

// A parameter declared as a WinRT interface: ICanvasImage, IGraphicsEffectSource,
// ICanvasResourceCreator. wxl wraps classes, not interfaces, so there is no wxl
// type that means "whatever implements this" -- and inventing one per interface
// would be a wrapper hierarchy beside the real one, since a class implements
// many.
//
// What such a parameter is *for* is passing an object the callee will ask for
// that interface, so the parameter is wxl::Object and the ask happens at the
// call: the same QueryInterface a collection's argument goes through. The
// trade is deliberate and it is a real one -- an object that does not
// implement the interface is refused by the runtime rather than by the
// compiler.
//
// Only a parameter -- unless the profile lists the interface by name, and
// then it is wrapped like a class and handed back as itself (see the
// interface branch in map_type_def). The unlisted interface *returned*
// would have to become an owning wrapper of some class, and which class it
// is is exactly what the signature does not say.
type_use interface_parameter(std::string_view winrt_name, std::string_view winrt_header) {
    type_use use;
    use.supported = true;
    use.parameter_only = true;
    use.value_type = "Object";
    use.param_type = "Object const&";
    use.winrt_type = winrt_name;
    use.to_winrt = std::format("Object::Impl::as<{}>($)", winrt_name);
    use.public_includes = {"../Object.h"};
    use.impl_includes = {"../Object.impl.h", std::string{winrt_header}};
    return use;
}

// The WinRT interfaces wxl reads as "a vector of T". Only these two, because
// wxl::Collection holds an IVector<T> and both of them answer to that IID --
// IObservableVector<T> requires IVector<T>. A read-only IVectorView<T> or a
// bare IIterable<T> does not, and would need a collection type of its own.
constexpr std::string_view vector_interfaces[] = {
    "Windows.Foundation.Collections.IVector`1",
    "Windows.Foundation.Collections.IObservableVector`1",
};

// WinRT's nullable box, which is core::nullable in everything but name.
constexpr std::string_view reference_interface = "Windows.Foundation.IReference`1";

// The generic type of an instantiation of one of `names` with a single
// argument -- IVector`1, IObservableVector`1, IReference`1 -- or an empty
// TypeDef for any other. The argument is single_argument(instantiation).
TypeDef generic_of(GenericTypeInstSig const& instantiation, std::span<std::string_view const> names) {
    auto const generic = instantiation.GenericType();
    if (generic.type() == TypeDefOrRef::TypeSpec) {
        return {};
    }
    auto const type = md::find(generic);
    if (!type) {
        return {};
    }
    if (std::find(names.begin(), names.end(), full_name(type)) == names.end()) {
        return {};
    }

    auto const arguments = instantiation.GenericArgs();
    if (arguments.second - arguments.first != 1) {
        return {};
    }
    return type;
}

// The one argument of an instantiation generic_of() accepted. A reference into
// the instantiation, so good for as long as it is.
TypeSig const& single_argument(GenericTypeInstSig const& instantiation) {
    return *instantiation.GenericArgs().first;
}

// The same, for a *class* that implements one of the vector interfaces --
// UIElementCollection, ItemCollection and their kind, which exist in metadata
// only to give an IVector<T> a name. wxl represents them as the collection
// itself rather than as a wrapper of their own.
//
// Whether the class is one, and if it is, `use` is called with the
// instantiation it implements. A call rather than a result: the
// instantiation lives in a signature that does not outlive this function.
template <typename Use>
bool with_class_vector(TypeDef const& type, Use use) {
    if (!type || get_category(type) != category::class_type) {
        return false;
    }
    for (auto&& implemented : type.InterfaceImpl()) {
        auto const iface = implemented.Interface();
        if (iface.type() != TypeDefOrRef::TypeSpec) {
            continue;
        }
        // Named, not a temporary: GenericTypeInst() hands back a reference
        // into the signature object, which a temporary would have destroyed
        // before the instantiation could be read out of it.
        auto const signature = iface.TypeSpec().Signature();
        auto const& instantiation = signature.GenericTypeInst();
        if (generic_of(instantiation, vector_interfaces)) {
            use(instantiation);
            return true;
        }
    }
    return false;
}

// "Windows.Foundation.Collections.IVector`1" applied to one argument, as the
// projection spells it: winrt::Windows::Foundation::Collections::IVector<X>.
// The arity suffix metadata carries is not part of the C++ name.
std::string winrt_generic_name(TypeDef const& generic, std::string_view argument) {
    std::string_view name = generic.TypeName();
    name = name.substr(0, name.find('`'));
    return std::format("{}::{}<{}>", winrt_namespace(generic.TypeNamespace()), name, argument);
}

// wxl::Collection<E> -- one hand-written template for every vector-shaped
// collection, rather than a generated wrapper per instantiation. `winrt_name`
// is whatever the member's signature actually declares (the concrete
// collection class, or the parameterized interface itself), because that is
// the IID the QueryInterface on the way in has to ask for.
type_use collection_of(type_use const& item, std::string_view winrt_name,
                       std::string_view winrt_header) {
    if (!item.supported) {
        return unsupported(std::format("collection element: {}", item.reason));
    }
    if (!item.is_wrapper) {
        return unsupported(std::format("collection of {}, which is not a wrapped class",
                                       item.value_type));
    }

    type_use use;
    use.supported = true;
    use.is_collection = true;
    use.element_type = item.value_type;
    use.value_type = std::format("Collection<{}>", item.value_type);
    use.param_type = std::format("{} const&", use.value_type);
    use.winrt_type = winrt_name;
    use.to_winrt = std::format("Object::Impl::as<{}>($)", winrt_name);
    use.from_winrt = std::format("Object::Impl::wrap<{}>($)", use.value_type);
    use.public_includes = item.public_includes;
    use.public_includes.insert("../Collection.h");
    use.impl_includes = item.impl_includes;
    use.impl_includes.insert("../Collection.impl.h");
    use.impl_includes.insert(std::string{winrt_header});
    return use;
}

// core::nullable<T> -- what WinRT's nullable box is in everything but name.
// The element keeps its own conversion, applied to the value inside the
// nullable, which is why both helpers take it as a lambda: the element may
// be a primitive that crosses as itself, an enum that is cast, or a chrono
// type that goes through impl::to_winrt.
type_use reference_of(type_use const& item, std::string_view winrt_name,
                      std::string_view winrt_header) {
    if (!item.supported) {
        return unsupported(std::format("optional element: {}", item.reason));
    }
    if (item.is_wrapper) {
        // A wrapper is nullable already -- adding an optional on top would
        // invent a second empty state saying the same thing.
        return unsupported(std::format("IReference of the wrapper {}", item.value_type));
    }

    type_use use;
    use.supported = true;
    use.value_type = std::format("core::nullable<{}>", item.value_type);
    use.param_type = std::format("{} const&", use.value_type);
    use.winrt_type = std::string{winrt_name};
    use.to_winrt = std::format("impl::to_reference<{}>($, [](auto const& v) {{ return {}; }})",
                               winrt_name, substitute(item.to_winrt, "v"));
    use.from_winrt = std::format("impl::from_reference<{}>($, [](auto const& v) {{ return {}; }})",
                                 item.value_type, substitute(item.from_winrt, "v"));
    use.public_includes = item.public_includes;
    use.public_includes.insert("../core.h");
    use.impl_includes = item.impl_includes;
    use.impl_includes.insert(std::string{conversions_include});
    use.impl_includes.insert(std::string{winrt_header});
    return use;
}

// A parameterized type named directly in a signature -- IVector<MenuFlyoutItemBase>,
// IObservableVector<Object>, IReference<int32_t>.
type_use map_generic(GenericTypeInstSig const& instantiation, type_index const& index) {
    if (auto const generic = generic_of(instantiation, vector_interfaces)) {
        auto const item = map_type(single_argument(instantiation), index);
        return collection_of(item, winrt_generic_name(generic, item.winrt_type),
                             winrt_include(generic.TypeNamespace()));
    }
    if (auto const generic = generic_of(instantiation, {&reference_interface, 1})) {
        auto const item = map_type(single_argument(instantiation), index);
        return reference_of(item, winrt_generic_name(generic, item.winrt_type),
                            winrt_include(generic.TypeNamespace()));
    }
    return unsupported("generic instantiation");
}

// A delegate parameter becomes a callable of the delegate's own signature.
// The projection's delegate constructor takes any invocable and keeps a copy
// of it, which is exactly what the callee needs: work handed to
// DispatcherQueue.TryEnqueue runs after the call has returned, so nothing may
// be held by reference.
type_use map_delegate(TypeDef const& type, type_index const& index, std::string_view winrt_name,
                      std::string_view winrt_header) {
    for (auto&& method : type.MethodList()) {
        if (method.Name() != "Invoke") {
            continue;
        }

        // Named, not a temporary: Params() and ReturnType() refer into the
        // signature's own storage.
        auto const signature = method.Signature();

        type_use use;
        use.public_includes = {"<functional>"};
        use.impl_includes = {std::string(winrt_header)};

        std::string result = "void";
        if (auto const& returned = signature.ReturnType()) {
            auto const returns = map_type(returned.Type(), index);
            if (!returns.supported) {
                return unsupported(std::format("delegate result: {}", returns.reason));
            }
            result = returns.value_type;
            use.public_includes.insert(returns.public_includes.begin(),
                                       returns.public_includes.end());
        }

        std::vector<std::string> parameters;
        for (auto&& param : signature.Params()) {
            auto const taken = map_type(param.Type(), index);
            if (!taken.supported) {
                return unsupported(std::format("delegate parameter: {}", taken.reason));
            }
            parameters.push_back(taken.param_type);
            use.public_includes.insert(taken.public_includes.begin(), taken.public_includes.end());
        }

        use.supported = true;
        use.parameter_only = true;
        use.value_type =
            std::format("std::function<{}({})>", result, wxl::core::join(parameters, ", "));
        use.param_type = std::format("{} const&", use.value_type);
        use.winrt_type = winrt_name;
        use.to_winrt = std::format("{}{{$}}", winrt_name);
        return use;
    }
    return unsupported(std::format("{} declares no Invoke", full_name(type)));
}

type_use map_type_def(TypeDef const& type, type_index const& index) {
    if (!type) {
        return unsupported("unresolved type reference");
    }

    std::string const winrt_name =
        std::format("{}::{}", winrt_namespace(type.TypeNamespace()), type.TypeName());
    std::string const winrt_header = winrt_include(type.TypeNamespace());

    // A projected type has a wxl equivalent that is not a wrapper at all,
    // so it converts through the hand-written overloads rather than
    // through the interface cache.
    if (auto const* projection = project_type(type, index.types)) {
        if (projection->cpp_name == "core::nullable") {
            return unsupported("IReference<T> is not mapped yet");
        }
        type_use use;
        use.supported = true;
        use.value_type = std::string{projection->cpp_name};
        use.param_type = std::format("{} const&", use.value_type);
        use.winrt_type = winrt_name;
        use.from_string = projection->from_string;
        use.to_winrt = "impl::to_winrt($)";
        use.from_winrt = "impl::from_winrt($)";
        use.public_includes = {std::string{projection->include}};
        use.impl_includes = {std::string{conversions_include}, winrt_header};
        return use;
    }

    // A class that is only a name for an IVector<T> becomes the collection
    // itself. Checked before the name registry, since no wrapper is
    // generated for such a class to be found under.
    type_use collection;
    if (with_class_vector(type, [&](GenericTypeInstSig const& instantiation) {
            collection = collection_of(map_type(single_argument(instantiation), index), winrt_name,
                                       winrt_header);
        })) {
        return collection;
    }

    if (is_event_args_class(type)) {
        // An args wrapper is a view that only exists for the duration of a
        // callback, so it can be a handler's parameter and nothing else --
        // never a property's type or a method's result.
        return unsupported(std::format("{} is an event-args view", full_name(type)));
    }

    if (get_category(type) == category::delegate_type) {
        return map_delegate(type, index, winrt_name, winrt_header);
    }

    if (get_category(type) == category::interface_type) {
        // An interface a profile listed by name is a wrapper like any class
        // (see write_classes): the object *is* the interface, so handing one
        // back is as meaningful as handing back a class. Every other
        // interface stays the parameter-only Object described above.
        auto const name = index.names.find(type);
        auto const header = index.headers.find(type);
        if (name != index.names.end() && header != index.headers.end()) {
            return wrapper_type(name->second, winrt_name, header->second);
        }
        return interface_parameter(winrt_name, winrt_header);
    }

    // DependencyObject is given from above: it is written by hand (Object.h)
    // and has no entry among the generated names, but a member handing one over
    // or taking one in -- FrameworkElement.Parent, FlyoutBase.ShowAt -- is as
    // wrappable as any class.
    if (get_category(type) == category::class_type &&
        full_name(type) == "Microsoft.UI.Xaml.DependencyObject") {
        return wrapper_type("DependencyObject", winrt_name, "../Object.h");
    }

    auto const name = index.names.find(type);
    auto const header = index.headers.find(type);
    if (name == index.names.end() || header == index.headers.end()) {
        return unsupported(std::format("no wxl type for {}", full_name(type)));
    }

    switch (get_category(type)) {
        case category::enum_type: {
            type_use use;
            use.supported = true;
            use.value_type = name->second;
            use.param_type = use.value_type;
            use.winrt_type = winrt_name;
            use.to_winrt = std::format("static_cast<{}>($)", winrt_name);
            use.from_winrt = std::format("static_cast<{}>($)", use.value_type);
            use.public_includes = {header->second};
            use.impl_includes = {std::string(winrt_header)};
            return use;
        }
        case category::struct_type: {
            type_use use;
            use.supported = true;
            use.value_type = name->second;
            use.param_type = std::format("{} const&", use.value_type);
            use.winrt_type = winrt_name;
            use.to_winrt = "impl::to_winrt($)";
            use.from_winrt = "impl::from_winrt($)";
            use.public_includes = {header->second};
            use.impl_includes = {"Structs.impl.h", winrt_header};
            return use;
        }
        case category::class_type: {
            if (index.without_default_interface.contains(type)) {
                return unsupported(
                    std::format("{} declares no default interface", full_name(type)));
            }
            return wrapper_type(name->second, winrt_name, header->second);
        }
        default:
            // An interface and a delegate are answered above, before the
            // name registry is asked.
            break;
    }
    return unsupported("unknown type category");
}

}  // namespace

char const* primitive_name(ElementType element) {
    switch (element) {
        case ElementType::Boolean:
            return "bool";
        case ElementType::Char:
            return "char16_t";
        case ElementType::I1:
            return "int8_t";
        case ElementType::U1:
            return "uint8_t";
        case ElementType::I2:
            return "int16_t";
        case ElementType::U2:
            return "uint16_t";
        case ElementType::I4:
            return "int32_t";
        case ElementType::U4:
            return "uint32_t";
        case ElementType::I8:
            return "int64_t";
        case ElementType::U8:
            return "uint64_t";
        case ElementType::R4:
            return "float";
        case ElementType::R8:
            return "double";
        default:
            return nullptr;
    }
}

type_use map_type(TypeDef const& type, type_index const& index) {
    return map_type_def(type, index);
}

type_use map_element_type(std::string_view metadata_name) {
    if (metadata_name == "String") {
        return string_type();
    }
    return unsupported(std::format("{} is not a metadata type, and not String", metadata_name));
}

std::string substitute(std::string_view expression, std::string_view argument) {
    std::string result;
    result.reserve(expression.size() + argument.size());
    for (auto&& c : expression) {
        if (c == '$') {
            result.append(argument);
        } else {
            result.push_back(c);
        }
    }
    return result;
}

type_use map_type(TypeSig const& sig, type_index const& index) {
    if (sig.is_szarray()) {
        return unsupported("arrays are not mapped yet");
    }

    auto const element = sig.element_type();
    if (auto const* name = primitive_name(element)) {
        return primitive(name);
    }
    if (element == ElementType::String) {
        return string_type();
    }
    if (element == ElementType::Object) {
        // System.Object in metadata: the wrapped IInspectable itself, which
        // is exactly what wxl::Object is.
        return wrapper_type("Object", "winrt::Windows::Foundation::IInspectable", "../Object.h");
    }

    if (auto const* ref = std::get_if<coded_index<TypeDefOrRef>>(&sig.Type())) {
        if (ref->type() == TypeDefOrRef::TypeSpec) {
            auto const signature = ref->TypeSpec().Signature();
            return map_generic(signature.GenericTypeInst(), index);
        }
        return map_type_def(md::find(*ref), index);
    }
    if (auto const* instantiation = std::get_if<GenericTypeInstSig>(&sig.Type())) {
        return map_generic(*instantiation, index);
    }
    return unsupported("unmapped signature shape");
}

bool is_collection_class(TypeDef const& type) {
    return with_class_vector(type, [](GenericTypeInstSig const&) {});
}

bool mirrors_abi(TypeDef const& type, type_map const& types) {
    return struct_mirrors_abi(type, types);
}

}  // namespace gen
