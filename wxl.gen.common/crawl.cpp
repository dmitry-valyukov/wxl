#include <format>
#include <print>

#include "crawl.h"

import std;

using namespace winmd::reader;

namespace {

std::string full_name(TypeDef const& type) {
    return std::format("{}.{}", type.TypeNamespace(), type.TypeName());
}

// A member WinRT itself has withdrawn -- Window.CoreWindow is documented as
// "always returns null for Desktop apps" and carries DeprecatedAttribute to
// say so. Generating it would offer an API that cannot work, and worse: the
// walk is bounded by the members it generates, so a withdrawn member drags
// its whole signature into the closure (CoreWindow alone brings the
// Windows.UI.Core namespace in). Filtered here rather than at emission for
// exactly that reason.
template <typename T>
bool has_deprecated_attribute(T const& member) {
    for (auto&& attribute : member.CustomAttribute()) {
        auto const [ns, name] = attribute.TypeNamespaceAndName();
        if (ns == "Windows.Foundation.Metadata" && name == "DeprecatedAttribute") {
            return true;
        }
    }
    return false;
}

bool is_deprecated(MethodDef const& method) { return has_deprecated_attribute(method); }

// A property or an event carries the attribute on its *accessors*, not on
// itself -- the metadata for Window.CoreWindow reads
// `CoreWindow { [Deprecated(...)] get; }` -- so asking the property alone
// finds nothing.
template <typename T>
bool is_deprecated_member(T const& member) {
    if (has_deprecated_attribute(member)) {
        return true;
    }
    for (auto&& semantic : member.MethodSemantic()) {
        if (has_deprecated_attribute(semantic.Method())) {
            return true;
        }
    }
    return false;
}

// WinRT XAML controls declare their DependencyProperty registration
// tokens (e.g. Button::ContentProperty()) as ordinary static properties
// directly on the class TypeDef, alongside the "real" instance members
// reached through the class's interfaces. They return
// Microsoft.UI.Xaml.DependencyProperty and are XAML/binding plumbing,
// not something the Flutter-style builder syntax needs a key for, so
// they're filtered out by return type (robust regardless of name).
bool is_dependency_property_accessor(Property const& property) {
    PropertySig const sig = property.Type();
    auto const* ref = std::get_if<coded_index<TypeDefOrRef>>(&sig.Type().Type());
    if (!ref || ref->type() == TypeDefOrRef::TypeSpec) {
        return false;
    }
    auto const resolved = winmd::reader::find(*ref);
    return resolved && resolved.TypeNamespace() == "Microsoft.UI.Xaml" &&
           resolved.TypeName() == "DependencyProperty";
}

// Accessors (get_X/put_X/add_X/remove_X) and .ctor: reached through the
// Property/Event tables instead, so they never take part in filtering or
// in the walk as methods of their own.
bool is_plain_method(MethodDef const& method) {
    return !method.Flags().SpecialName() && !method.Flags().RTSpecialName();
}

// Resolves an interface reference to its TypeDef, or an empty TypeDef for
// a generic instantiation (IVector<Brush> and friends), which has no
// TypeDef of its own.
TypeDef resolve_interface(coded_index<TypeDefOrRef> const& ref) {
    if (!ref || ref.type() == TypeDefOrRef::TypeSpec) {
        return {};
    }
    return winmd::reader::find(ref);
}

// Every member a type declares itself, plus -- for a class -- the members of
// the interfaces it implements directly, since that's where WinRT actually
// declares a class's instance members. Handed to `visit` by kind and name, a
// name as often as it is declared.
template <typename Visit>
void visit_declared_members(TypeDef const& type, Visit&& visit) {
    auto const collect = [&visit](TypeDef const& source) {
        for (auto&& property : source.PropertyList()) {
            if (!is_dependency_property_accessor(property)) {
                visit(MemberKind::Property, property.Name());
            }
        }
        for (auto&& event : source.EventList()) {
            visit(MemberKind::Event, event.Name());
        }
        for (auto&& method : source.MethodList()) {
            if (is_plain_method(method)) {
                visit(MemberKind::Method, method.Name());
            }
        }
    };

    collect(type);
    if (get_category(type) == category::class_type) {
        for (auto&& impl : type.InterfaceImpl()) {
            if (auto const resolved = resolve_interface(impl.Interface())) {
                collect(resolved);
            }
        }
    }
}

std::set<std::string> declared_members(TypeDef const& type) {
    std::set<std::string> names;
    visit_declared_members(type, [&names](MemberKind, std::string_view name) {
        names.insert(std::string(name));
    });
    return names;
}

// The interfaces a class's *static* members live on. WinRT declares them
// nowhere near the class's own interfaces: StaticAttribute names a separate
// interface per version, implemented by the activation factory rather than
// by the object, which is exactly how they are reached at run time too.
std::vector<TypeDef> statics_interfaces(TypeDef const& type, cache const& db) {
    std::vector<TypeDef> found;
    for (auto&& attribute : type.CustomAttribute()) {
        auto const [ns, name] = attribute.TypeNamespaceAndName();
        if (ns != "Windows.Foundation.Metadata" || name != "StaticAttribute") {
            continue;
        }
        for (auto&& argument : attribute.Value().FixedArgs()) {
            auto const* element = std::get_if<ElemSig>(&argument.value);
            if (!element) {
                continue;
            }
            if (auto const* named = std::get_if<ElemSig::SystemType>(&element->value)) {
                if (auto const resolved = db.find(named->name)) {
                    found.push_back(resolved);
                }
            }
        }
    }
    return found;
}

// The interfaces a class's *parameterised* constructors come from.
//
// ActivatableAttribute is written once per constructor shape: bare, for the
// default one, and naming a factory interface for every other. So a class
// whose only ActivatableAttribute names a type has no default constructor at
// all -- CanvasCommandList is made from a resource creator and from nothing
// else -- and the factory interface is where its real constructors are
// declared, one method each.
std::vector<TypeDef> activation_factories(TypeDef const& type, cache const& db) {
    std::vector<TypeDef> found;
    for (auto&& attribute : type.CustomAttribute()) {
        auto const [ns, name] = attribute.TypeNamespaceAndName();
        if (ns != "Windows.Foundation.Metadata" || name != "ActivatableAttribute") {
            continue;
        }
        for (auto&& argument : attribute.Value().FixedArgs()) {
            auto const* element = std::get_if<ElemSig>(&argument.value);
            if (!element) {
                continue;
            }
            if (auto const* named = std::get_if<ElemSig::SystemType>(&element->value)) {
                if (auto const resolved = db.find(named->name)) {
                    found.push_back(resolved);
                }
            }
        }
    }
    return found;
}

// The walk itself. Types are (re)processed whenever their member surface
// widens -- merging is monotone over a finite set of names, so this
// terminates.
struct Crawler {
    ProfileSet const& profiles;
    cache const& db;
    Closure result;

    std::deque<TypeDef> queue;
    std::map<TypeDef, std::set<TypeDef>> dependencies;
    // Members a profile listed on a *derived* type although an ancestor
    // declares them; routed to that ancestor before the walk starts.
    std::map<TypeDef, MemberFilter> inherited;

    // An interface the profile names directly is wrapped like a class (see
    // Closure::listed_interfaces), so its own required interfaces are
    // recorded the way a class's are: they may carry fields of the wrapper.
    bool is_listed_interface(TypeDef const& type) const {
        return get_category(type) == category::interface_type &&
               profiles.types.contains(full_name(type));
    }

    MemberFilter listed_filter(TypeDef const& type) const {
        MemberFilter filter = profiles.discovered;
        if (auto const it = profiles.types.find(full_name(type)); it != profiles.types.end()) {
            filter = it->second;
        }
        if (auto const it = inherited.find(type); it != inherited.end()) {
            filter.merge(it->second);
        }
        return filter;
    }

    void depend(TypeDef const& from, TypeDef const& on) {
        if (from && on && from != on) {
            dependencies[from].insert(on);
        }
    }

    void enqueue(TypeDef const& type, MemberFilter const& filter) {
        if (!type) {
            return;
        }
        if (is_given_from_above(type)) {
            result.boundary.insert(type);
            return;
        }

        auto [it, inserted] = result.surface.try_emplace(type, filter);
        if (!inserted) {
            auto const before = it->second;
            it->second.merge(filter);
            if (it->second == before) {
                return;  // nothing new to expand
            }
        }
        queue.push_back(type);
    }

    // A type the walk found on its own carries whatever the profiles say
    // about it, or the discovered-types policy when they say nothing.
    void enqueue(TypeDef const& type) {
        if (!type) {
            return;  // an unresolved TypeRef -- metadata we weren't given
        }
        enqueue(type, listed_filter(type));
    }

    void enqueue(coded_index<TypeDefOrRef> const& ref, TypeDef const& source) {
        if (!ref) {
            return;
        }
        if (ref.type() == TypeDefOrRef::TypeSpec) {
            // A generic instantiation used directly as a type reference
            // (e.g. implementing IVector<Brush>); winmd::reader::find()
            // doesn't resolve TypeSpec, so unpack it by hand.
            visit_generic_inst(ref.TypeSpec().Signature().GenericTypeInst(), source);
            return;
        }
        auto const resolved = winmd::reader::find(ref);
        depend(source, resolved);
        enqueue(resolved);
    }

    void visit_generic_inst(GenericTypeInstSig const& inst, TypeDef const& source) {
        enqueue(inst.GenericType(), source);
        for (auto&& arg : inst.GenericArgs()) {
            visit_type_sig(arg, source);
        }
    }

    void visit_type_sig(TypeSig const& sig, TypeDef const& source) {
        std::visit(
            [&](auto&& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, coded_index<TypeDefOrRef>>) {
                    enqueue(value, source);
                } else if constexpr (std::is_same_v<T, GenericTypeInstSig>) {
                    visit_generic_inst(value, source);
                }
                // ElementType (primitives, System.Object) and generic
                // parameter placeholders name no concrete WinRT type.
            },
            sig.Type());
    }

    void visit_method_signature(MethodDefSig const& sig, TypeDef const& source) {
        for (auto&& param : sig.Params()) {
            visit_type_sig(param.Type(), source);
        }
        if (sig.ReturnType()) {
            visit_type_sig(sig.ReturnType().Type(), source);
        }
    }

    void process(TypeDef const& type) {
        auto const cat = get_category(type);
        auto const filter = result.surface.at(type);

        if (cat == category::class_type || cat == category::interface_type) {
            if (auto const base = type.Extends()) {
                enqueue(base, type);
            }

            // WinRT declares a class's instance members on the interfaces
            // it implements (Command lives on IButtonBase, not on
            // ButtonBase), while a profile names the *class*. So a class
            // hands its own member filter down to the interfaces it
            // implements directly, merged with whatever the profiles say
            // about those interfaces themselves.
            for (auto&& impl : type.InterfaceImpl()) {
                auto const resolved = resolve_interface(impl.Interface());
                if (!resolved) {
                    enqueue(impl.Interface(), type);  // generic instantiation
                    continue;
                }
                depend(type, resolved);
                if (cat == category::class_type || is_listed_interface(type)) {
                    auto& implemented = result.dropped_interfaces[type];
                    if (std::find(implemented.begin(), implemented.end(), resolved) ==
                        implemented.end()) {
                        implemented.push_back(resolved);
                    }
                }
                auto inherited_filter = filter;
                inherited_filter.merge(listed_filter(resolved));
                enqueue(resolved, inherited_filter);
            }

            // A property wxl adds where the metadata has none. Its value
            // type is named by the profile and enqueued here: nothing else
            // in the walk mentions it, and it still has to be generated.
            if (cat == category::class_type) {
                if (auto const added = profiles.synthetic.find(full_name(type));
                    added != profiles.synthetic.end()) {
                    for (auto&& member : added->second) {
                        // A value type that is a signature element rather
                        // than a TypeDef (String) resolves to nothing here
                        // and drags nothing in; the generator maps it by
                        // name, and reports the name it cannot map. Only a
                        // qualified name is a type to look up -- the cache
                        // treats an unqualified one as an error.
                        auto const value_type = member.type.contains('.')
                                                    ? db.find(member.type)
                                                    : TypeDef{};
                        if (value_type) {
                            depend(type, value_type);
                            enqueue(value_type);
                            result.synthetic[type].push_back({member, value_type});
                        } else {
                            result.synthetic[type].push_back({member, {}});
                        }
                        result.property_names.insert(member.name);
                    }
                }
                if (auto const setters = profiles.setter_methods.find(full_name(type));
                    setters != profiles.setter_methods.end()) {
                    for (auto&& setter : setters->second) {
                        TypeDef const value_type =
                            setter.type.empty() ? TypeDef{} : db.find(setter.type);
                        if (value_type) {
                            depend(type, value_type);
                            enqueue(value_type);
                        }
                        result.setter_methods[type].push_back({setter, value_type});
                        result.property_names.insert(setter.method.substr(3));
                    }
                }
            }

            // Static members are filtered by what the profile said about
            // the class, exactly like the instance ones: a profile names
            // OverlappedPresenter.Create, not IOverlappedPresenterStatics.
            if (cat == category::class_type) {
                for (auto&& iface : statics_interfaces(type, db)) {
                    depend(type, iface);
                    auto& known = result.statics[type];
                    if (std::find(known.begin(), known.end(), iface) == known.end()) {
                        known.push_back(iface);
                    }
                    auto statics_filter = filter;
                    statics_filter.merge(listed_filter(iface));
                    enqueue(iface, statics_filter);
                }

                // A constructor is not a member a profile can name -- it has
                // no name -- so the factory interface comes in whole, and
                // what it drags in with it is the types its constructors
                // take. A constructor whose signature wxl cannot map yet is
                // dropped by the generator, and reported there.
                for (auto&& iface : activation_factories(type, db)) {
                    depend(type, iface);
                    auto& known = result.factories[type];
                    if (std::find(known.begin(), known.end(), iface) == known.end()) {
                        known.push_back(iface);
                    }
                    enqueue(iface, MemberFilter::all());
                }
            }

            for (auto&& property : type.PropertyList()) {
                if (is_dependency_property_accessor(property) ||
                    !filter.allows(property.Name())) {
                    continue;
                }
                if (is_deprecated_member(property)) {
                    result.deprecated.insert(
                        std::format("{}.{}", full_name(type), property.Name()));
                    continue;
                }
                result.property_names.insert(std::string(property.Name()));
                result.members[type].insert(std::string(property.Name()));
                visit_type_sig(property.Type().Type(), type);
            }

            for (auto&& event : type.EventList()) {
                if (!filter.allows(event.Name())) {
                    continue;
                }
                if (is_deprecated_member(event)) {
                    result.deprecated.insert(std::format("{}.{}", full_name(type), event.Name()));
                    continue;
                }
                result.event_names.insert(std::string(event.Name()));
                result.members[type].insert(std::string(event.Name()));
                enqueue(event.EventType(), type);
            }

            for (auto&& method : type.MethodList()) {
                if (!is_plain_method(method) || !filter.allows(method.Name())) {
                    continue;
                }
                if (is_deprecated(method)) {
                    result.deprecated.insert(std::format("{}.{}", full_name(type), method.Name()));
                    continue;
                }
                result.members[type].insert(std::string(method.Name()));
                visit_method_signature(method.Signature(), type);
            }
        } else if (cat == category::delegate_type) {
            for (auto&& method : type.MethodList()) {
                if (method.Name() == "Invoke") {
                    visit_method_signature(method.Signature(), type);
                }
            }
        } else if (cat == category::struct_type) {
            for (auto&& field : type.FieldList()) {
                visit_type_sig(field.Signature().Type(), type);
            }
        }
        // enum_type: enumerators carry no further type references.
    }

    void run() {
        while (!queue.empty()) {
            auto const type = queue.front();
            queue.pop_front();
            process(type);
        }
    }

    // Splits the interfaces each class implements into the ones that
    // survived the filter (they still carry members, so this level needs
    // a field for them) and the ones that didn't.
    //
    // Interfaces any ancestor already implements are left out of both
    // lists: wxl's wrapper is an ordinary C++ inheritance chain where
    // every level owns its own interfaces and inherits the rest from its
    // base (unlike the WinRT projection, where each class restates the
    // full set).
    void split_interfaces() {
        std::map<TypeDef, std::vector<TypeDef>> dropped;
        for (auto&& [type, implemented] : result.dropped_interfaces) {
            auto const inherited_interfaces = interfaces_of_base_chain(type);
            for (auto&& iface : implemented) {
                if (inherited_interfaces.count(iface)) {
                    continue;  // that ancestor's level owns it
                }
                auto const members = result.members.find(iface);
                if (members != result.members.end() && !members->second.empty()) {
                    result.interfaces[type].push_back(iface);
                } else {
                    dropped[type].push_back(iface);
                }
            }
        }
        result.dropped_interfaces = std::move(dropped);

        // A statics interface nothing survived on is no interface at all
        // here: it would only mean an empty proxy and an unused runtime
        // class name.
        std::map<TypeDef, std::vector<TypeDef>> statics;
        for (auto&& [type, interfaces] : result.statics) {
            for (auto&& iface : interfaces) {
                auto const members = result.members.find(iface);
                if (members != result.members.end() && !members->second.empty()) {
                    statics[type].push_back(iface);
                }
            }
        }
        result.statics = std::move(statics);
    }

    // Attached properties, read off the statics that survived: a setter
    // taking the element and a value, with a getter of the same name beside
    // it. The getter need not be generated for the pair to say what it says,
    // which is why it is looked for in the metadata rather than in the
    // filtered surface -- a profile that lists only Grid.SetRow still means
    // an attached property.
    void collect_attached() {
        for (auto&& [type, interfaces] : result.statics) {
            for (auto&& iface : interfaces) {
                auto const surviving = result.members.find(iface);
                if (surviving == result.members.end()) {
                    continue;
                }
                for (auto&& method : iface.MethodList()) {
                    std::string_view const name = method.Name();
                    if (!name.starts_with("Set") || !surviving->second.count(std::string{name})) {
                        continue;
                    }
                    auto const parameters = method.Signature().Params();
                    if (parameters.second - parameters.first != 2) {
                        continue;
                    }
                    std::string const property{name.substr(3)};
                    if (!declares(iface, std::format("Get{}", property))) {
                        continue;
                    }
                    result.attached[type].push_back(property);
                    result.property_names.insert(property);
                }
            }
        }
    }

    static bool declares(TypeDef const& type, std::string_view method_name) {
        for (auto&& method : type.MethodList()) {
            if (method.Name() == method_name) {
                return true;
            }
        }
        return false;
    }

    // Every interface implemented anywhere above `type` in the base
    // chain, transitively through the interfaces those interfaces
    // themselves require.
    std::set<TypeDef> interfaces_of_base_chain(TypeDef const& type) const {
        std::set<TypeDef> result_set;

        std::function<void(TypeDef const&)> add = [&](TypeDef const& source) {
            for (auto&& impl : source.InterfaceImpl()) {
                auto const iface = resolve_interface(impl.Interface());
                if (iface && result_set.insert(iface).second) {
                    add(iface);
                }
            }
        };

        for (auto ancestor = base_of(type); ancestor; ancestor = base_of(ancestor)) {
            add(ancestor);
        }
        return result_set;
    }

    static TypeDef base_of(TypeDef const& type) {
        auto const extends = type.Extends();
        if (!extends || extends.type() == TypeDefOrRef::TypeSpec) {
            return {};
        }
        return winmd::reader::find(extends);
    }

    // Post-order DFS over the recorded edges: a type is appended only
    // after everything it depends on, so the result runs from dependency
    // sources to their consumers. `visiting` breaks the cycles WinRT
    // metadata genuinely contains.
    void order() {
        std::vector<TypeDef> types;
        types.reserve(result.surface.size());
        for (auto&& [type, filter] : result.surface) {
            types.push_back(type);
        }
        std::sort(types.begin(), types.end(), [](TypeDef const& a, TypeDef const& b) {
            return std::pair{a.TypeNamespace(), a.TypeName()} <
                   std::pair{b.TypeNamespace(), b.TypeName()};
        });

        std::set<TypeDef> done;
        std::set<TypeDef> visiting;

        std::function<void(TypeDef const&)> visit = [&](TypeDef const& type) {
            if (done.count(type) || !visiting.insert(type).second) {
                return;
            }
            if (auto const it = dependencies.find(type); it != dependencies.end()) {
                for (auto&& dependency : it->second) {
                    if (result.surface.count(dependency)) {
                        visit(dependency);
                    }
                }
            }
            visiting.erase(type);
            done.insert(type);
            result.rank.emplace(type, result.ordered.size());
            result.ordered.push_back(type);
        };

        for (auto&& type : types) {
            visit(type);
        }

        hoist_bases();
    }

    // The pass above breaks a cycle wherever it meets it, and metadata has
    // cycles that run through inheritance: CompositionObject.startAnimation
    // takes a CompositionAnimation, which derives from CompositionObject.
    // Broken there, the base lands in the file *after* the class deriving
    // from it, and no generated header can compile.
    //
    // A member edge is soft -- a declaration may name an incomplete type,
    // and the group headers forward-declare their own classes for exactly
    // this -- but a base edge is hard. So the order above is kept as the
    // preference it is, and this pass repairs it: walking it once and
    // emitting each type's unemitted ancestors ahead of it. Base chains
    // cannot cycle (no class is its own ancestor), so it always terminates.
    void hoist_bases() {
        std::vector<TypeDef> repaired;
        repaired.reserve(result.ordered.size());
        std::set<TypeDef> placed;

        auto place = [&](auto&& self, TypeDef const& type) -> void {
            if (placed.count(type)) {
                return;
            }
            if (auto const base = base_of(type); base && result.surface.count(base)) {
                self(self, base);
            }
            placed.insert(type);
            repaired.push_back(type);
        };

        for (auto&& type : result.ordered) {
            place(place, type);
        }

        result.ordered = std::move(repaired);
        result.rank.clear();
        for (size_t index = 0; index < result.ordered.size(); ++index) {
            result.rank.emplace(result.ordered[index], index);
        }
    }

    // A profile may name a member on any type that *has* it, inherited
    // ones included -- that's how the API reads to a user (a Button has a
    // Width), even though metadata declares Width three levels up. Such a
    // name is routed to the ancestor that actually declares it, so it is
    // generated once, at the right level. Names nothing in the chain
    // declares are reported instead.
    void route_inherited_members() {
        for (auto&& [name, filter] : profiles.types) {
            if (filter.names.empty()) {
                continue;
            }
            auto const type = db.find(name);
            if (!type) {
                continue;  // reported as a missing type
            }

            auto const own = declared_members(type);
            for (auto&& member : filter.names) {
                if (own.count(member)) {
                    continue;
                }

                TypeDef owner;
                for (auto ancestor = type; ancestor;) {
                    auto const base = base_of(ancestor);
                    if (!base || is_given_from_above(base)) {
                        break;
                    }
                    if (declared_members(base).count(member)) {
                        owner = base;
                        break;
                    }
                    ancestor = base;
                }

                if (!owner) {
                    result.unknown_members.push_back(std::format("{}.{}", name, member));
                    continue;
                }
                if (filter.kind == MemberFilter::Kind::Allow) {
                    inherited[owner].merge(MemberFilter::allow({member}));
                }
                // A deny list is deliberately *not* routed upwards:
                // hiding a member at the level that declares it would
                // hide it from every other type inheriting that level,
                // which is not what "this type doesn't use it" means.
            }
        }
    }
};

}  // namespace

bool is_given_from_above(TypeDef const& type) {
    return type && type_map().given_from_above.count(full_name(type)) != 0;
}

DeclaredMembers declared_members_of(TypeDef const& type) {
    DeclaredMembers members;
    visit_declared_members(type, [&members](MemberKind kind, std::string_view name) {
        switch (kind) {
            case MemberKind::Property: members.properties.push_back(name); break;
            case MemberKind::Method: members.methods.push_back(name); break;
            case MemberKind::Event: members.events.push_back(name); break;
        }
    });
    // A method overloaded by arity is declared once per overload, and a
    // profile names it once.
    for (auto* names : {&members.properties, &members.methods, &members.events}) {
        std::ranges::sort(*names);
        names->erase(std::ranges::unique(*names).begin(), names->end());
    }
    return members;
}

Closure crawl(ProfileSet const& raw_profiles, cache const& db) {
    ProfileSet profiles = raw_profiles;
    for (auto&& root : type_map().implicit_roots) {
        profiles.types.try_emplace(root, MemberFilter::all());
    }

    Crawler crawler{profiles, db};

    for (auto&& [name, filter] : profiles.types) {
        if (!db.find(name)) {
            crawler.result.missing_types.push_back(name);
        }
    }

    crawler.route_inherited_members();

    for (auto&& [name, filter] : profiles.types) {
        auto const type = db.find(name);
        if (!type) {
            continue;
        }
        crawler.enqueue(type, crawler.listed_filter(type));
    }

    crawler.run();

    for (auto&& [type, filter] : crawler.result.surface) {
        if (crawler.is_listed_interface(type)) {
            crawler.result.listed_interfaces.insert(type);
        }
    }

    crawler.split_interfaces();
    crawler.collect_attached();
    crawler.order();

    // The keys of the members wxl's hand-written classes add to the vocabulary
    // (types.json): the walk meets none of them, and the key enums are flat.
    for (auto&& property : type_map().hand_written_properties) {
        crawler.result.property_names.insert(property.name);
    }
    for (auto&& member : type_map().bound_members) {
        crawler.result.property_names.insert(member.name);
    }
    crawler.result.event_names.insert(type_map().hand_written_events.begin(),
                                      type_map().hand_written_events.end());

    return std::move(crawler.result);
}
