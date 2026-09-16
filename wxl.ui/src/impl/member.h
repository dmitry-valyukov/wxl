#pragma once

// The construction-time DSL: what `Content = L"Click"` and `OnClick = ...`
// actually are, and how a constructor applies them.
//
// Construction is the only mechanism -- there is no ambient "current
// object" a statement could land on. Each named-argument-style assignment
// inside the braces evaluates to a small deferred op, and the class's
// variadic constructor applies the whole pack to the freshly activated
// object in order. That is what makes `Button { Content = ..., Margin = ... }`
// one expression rather than a sequence of statements, and it is why the
// same syntax cannot silently write to the wrong object when hand-written
// and declarative code are mixed.
//
// Nothing here names a property or an event: the per-key dispatch lives in
// the generated Members.h, which specialises PropertySetter / EventAdder
// below. This header therefore stays the same size whatever the profile
// generates.

#include "../Object.h"
#include "../TaggedValue.h"
#include "../event_token.h"
#include "EventKey.h"
#include "PropertyKey.h"

namespace wxl {

namespace impl {

// The args an event carries, read off the signature of the object's own
// add_onX. Which is where they have to be read from: the same EventKey means
// different args on different classes -- Click is RoutedEventArgs on Button
// and SplitButtonClickEventArgs on SplitButton -- so the type belongs to the
// pair, not to the key. A member pointer names that pair exactly, and the
// only thing it can be decomposed into is the handler type, which carries
// the args as a member alias of its own.
template <typename Member>
struct event_args_of;

template <typename Obj, typename Handler>
struct event_args_of<EventToken (Obj::*)(Handler const&) const> {
    using type = typename Handler::args_t;
};

// And the same for an event that belongs to the type rather than to an
// object: its add is a plain function, not a member.
template <typename Handler>
struct event_args_of<EventToken (*)(Handler const&)> {
    using type = typename Handler::args_t;
};

template <typename Member>
using event_args_of_t = typename event_args_of<Member>::type;

// Specialised once per property and per event in the generated Members.h.
// Both take the object as a template parameter, so one specialisation
// serves every class declaring that member -- there is no per-class code.
template <PropertyKey key>
struct PropertySetter;

// A property the control writes itself, paired with the event that says so.
// Specialised in impl/binding.h per (property, control) with a static
// `bind(control, field)`. The primary is empty on purpose: `text = Bind{...}`
// on a TextBlock, which has no such pair, finds no bind() here and is bound
// one way by bind_property below.
template <PropertyKey key, typename Obj>
struct TwoWayBinder {};

template <EventKey key>
struct EventAdder;

// Specialised for each collection-valued property. It takes the whole run of
// items, not one at a time, so the specialisation is free to fill the
// collection in a single call rather than appending in a loop.
template <PropertyKey key>
struct CollectionSetter;

// The class a member was named through, checked against the object it is
// being written on.
//
// `Owner` is `void` for the tags of namespace `dsl`, which name a member and
// not a class: `content = ...` is written on whatever the braces are
// building, and whether that object has a Content is answered by the setter
// call itself. The schema names the class too -- `schema::Button::content` --
// and then the mistake worth catching is writing one class's member on
// another, which is caught here, where both types are known, instead of
// several template expansions further in.
template <typename Owner, typename Obj>
constexpr void check_owner() {
    static_assert(std::is_void_v<Owner> || std::derived_from<Obj, Owner>,
                  "wxl: this member was named through a class the object does not derive from. "
                  "The schema path says which class declares it -- write that one, or reach the "
                  "member by its bare name in namespace dsl.");
}

}  // namespace impl

// A binding written on the right of the `=`: `text = Bind{field}`. Defined in
// Bind.h; named here so that SetterOp can tell one from a value.
template <class T>
struct Bind;

namespace impl {

template <typename T>
inline constexpr bool is_bind = false;

template <typename T>
inline constexpr bool is_bind<Bind<T>> = true;

// `property = Bind{field}`: the field's value now, and every value after.
//
// Two ways where a pair says the control writes this property itself (see
// TwoWayBinder), one way otherwise -- the setter the generator emits, called
// once here and again from a watch the field keeps. The watch owns the
// control, as it must to write to it; the control holds nothing back, so the
// ownership runs one way and ends with the field. Nothing is generated for a
// one-way binding: every settable property already has the one thing it needs.
template <PropertyKey key, typename Obj, typename T>
void bind_property(Obj const& object, core::observable<T>& model) {
    if constexpr (requires { TwoWayBinder<key, Obj>::bind(object, model); }) {
        TwoWayBinder<key, Obj>::bind(object, model);
    } else {
        PropertySetter<key>::set(object, model.get());
        model.watch_for_binding(
            [object](T const& value) noexcept { PropertySetter<key>::set(object, value); });
    }
}

}  // namespace impl

// A pending assignment: the value, plus the key saying where it goes. The
// object it goes to is not known yet -- it is whatever the constructor
// applies the pack to.
//
// The value may be a Bind, and then the assignment is a binding: the property
// follows the field from here on rather than taking one value now.
template <PropertyKey key, typename T, typename Owner = void>
struct SetterOp {
    T value_;

    template <typename Obj>
    void operator()(Obj const& object) const {
        impl::check_owner<Owner, Obj>();
        if constexpr (impl::is_bind<T>) {
            impl::bind_property<key>(object, *value_.model);
        } else {
            impl::PropertySetter<key>::set(object, value_);
        }
    }
};

// A pending subscription. Returns the token, so the same op serves the
// procedural form (`element[OnClick] += handler`) where the caller wants it.
template <EventKey key, typename Fn, typename Owner = void>
struct AddEventOp {
    Fn handler_;

    template <typename Obj>
    EventToken operator()(Obj const& object) const {
        impl::check_owner<Owner, Obj>();
        return impl::EventAdder<key>::add(object, handler_);
    }
};

template <EventKey key, typename Owner = void>
struct RemoveEventOp {
    EventToken token_;

    template <typename Obj>
    void operator()(Obj const& object) const {
        impl::check_owner<Owner, Obj>();
        impl::EventAdder<key>::remove(object, token_);
    }
};

// The shorthand form `OnClick = {Content = L"Thank You!"}`: a handler whose
// whole body is assignments back to the object the event was attached to.
template <EventKey key, typename Setter, typename Owner = void>
struct SettersEventOp {
    Setter setter_;

    template <typename Obj>
    EventToken operator()(Obj const& object) const {
        impl::check_owner<Owner, Obj>();
        return impl::EventAdder<key>::add(
            object, [object, setter = setter_](Object const&, auto&) { setter(object); });
    }
};

namespace impl {

// The generic half of a property tag: assignment from anything the
// property's setter accepts, with the value's type deduced.
//
// constexpr, here and on every tag below: an assignment makes a value and
// nothing else, so a preset written out of these is a constant the compiler
// lays out, at namespace scope included.
template <PropertyKey key, typename Owner = void>
struct PropertyTag {
    template <typename T>
    constexpr SetterOp<key, std::decay_t<T>, Owner> operator=(T&& value) const {
        return {std::forward<T>(value)};
    }
};

}  // namespace impl

// A property tag -- the object the DSL writes on the left of the `=`.
//
// `Value` is the property's own type, and the reason for the extra
// overload: `Margin = {20}` initialises a parameter of a known type from a
// braced list, which template deduction alone can never do. Where a
// property name is declared with different types by different classes there
// is no single such type, and the tag is generated with `void`, leaving only
// the deduced form.
// `Owner` is the class the tag was named through, or `void` for the plain
// vocabulary of namespace `dsl`; see impl::check_owner above. It is the last
// parameter because almost nothing writes it: only the generated schema does.
template <PropertyKey key, typename Value = void, typename Owner = void>
struct Property : impl::PropertyTag<key, Owner> {
    using impl::PropertyTag<key, Owner>::operator=;

    constexpr SetterOp<key, Value, Owner> operator=(Value value) const { return {std::move(value)}; }
};

template <PropertyKey key, typename Owner>
struct Property<key, void, Owner> : impl::PropertyTag<key, Owner> {
    using impl::PropertyTag<key, Owner>::operator=;
};

// An event tag, written `On` + the metadata name -- OnClick, OnPointerPressed
// -- so that an event is visibly not a property inside the same braces.
template <EventKey key, typename Owner = void>
struct Event {
    // An ordinary callable: (Object const& sender, Args& args).
    template <typename Fn>
    constexpr AddEventOp<key, std::decay_t<Fn>, Owner> operator=(Fn&& handler) const {
        return {std::forward<Fn>(handler)};
    }

    // The braced shorthand. std::initializer_list is what gives a braced
    // list a type at all here; it deduces from the elements, so the list has
    // to be homogeneous -- one setter today.
    template <PropertyKey k, typename T, typename O>
    constexpr SettersEventOp<key, SetterOp<k, T, O>, Owner> operator=(
        std::initializer_list<SetterOp<k, T, O>> setters) const {
        return {*setters.begin()};
    }

    template <typename Fn>
    constexpr AddEventOp<key, std::decay_t<Fn>, Owner> operator+=(Fn&& handler) const {
        return {std::forward<Fn>(handler)};
    }

    constexpr RemoveEventOp<key, Owner> operator-=(EventToken token) const { return {token}; }
};

// A repeated child: `[](repeat<20> i) { return Button { ... }; }` inside a
// parent's braces builds twenty of them.
//
// The count is part of the *type*, so the loop is a fold over an integer
// sequence and the index a compile-time constant at every step -- there is no
// runtime bound anywhere. The index converts to int, which is all a body ever
// wants it for: `row = i / 4 + 1`.
template <int Count>
struct repeat {
    static constexpr int count = Count;

    int index{};

    constexpr operator int() const noexcept { return index; }
};

namespace impl {

// The types whose runs are text, and therefore the only ones a terminator
// means anything for.
template <typename T>
inline constexpr bool is_character =
    std::is_same_v<T, char> || std::is_same_v<T, wchar_t> || std::is_same_v<T, char8_t> ||
    std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t>;

// One argument's worth of compile-time values, and the reason a literal, an
// array and a lone value can all be written in the same place: a string
// literal is its characters without the terminator, an array is its elements
// as they are, a single value is a run of one.
//
// Which of those an argument is cannot be decided by the deduction guide: a
// guide sees the parameter's *type*, and whether an array ends in a
// terminator is a question about its value -- a function parameter is not a
// constant expression, so `-> values<T, length_of(source)>` cannot be
// written. So the length stays what the array has and `count` is decided in
// the constructor, where the values are there to be read: a character array
// is one element shorter than it looks only when its last element really is
// a zero. That keeps `char raw[]{'a', 'b', 'c'}` three elements and
// `int marks[]{1, 4, 0}` three as well, while `L"abc"` is three of four.
//
// Storage holds one element more than a character run needs, and the
// constructor sees to it that the run is terminated whether its source was
// or not -- so `items` is a valid string of `count` characters in every
// case. How much room to leave *is* decidable from the type alone, which is
// why that half stays in the type while the count stays in the constructor.
//
// The type is structural -- public members, all of structural type -- which
// is what makes it usable as a template argument at all.
template <typename T, std::size_t Length>
struct values {
    using element_t = T;

    static constexpr std::size_t capacity = Length + (is_character<T> ? 1 : 0);

    T items[capacity]{};
    std::size_t count = Length;

    consteval values(T const (&source)[Length]) {
        for (std::size_t i = 0; i < Length; ++i) {
            items[i] = source[i];
        }
        // A terminator is a thing only for characters, so nothing else ever
        // needs to be comparable to zero. A run that arrived without one is
        // given it in the spare slot; a run that arrived with one simply
        // does not count it.
        if constexpr (is_character<T>) {
            if (Length == 0 || items[Length - 1] != T{}) {
                items[Length] = T{};
            } else {
                count = Length - 1;
            }
        }
    }

    consteval values(T value)
        requires(Length == 1)
        : items{value}, count(1) {}
};

template <typename T, std::size_t Length>
values(T const (&)[Length]) -> values<T, Length>;

template <typename T>
values(T value) -> values<T, 1>;

}  // namespace impl

// The same, over given values rather than a count:
//
//     [](iterate<L"789456123"> key) { return Button { key.text(), ... }; }
//     [](iterate<1, 4, 17> span)    { return ColumnDefinition { ... }; }
//     [](iterate<rows> height)      { ... }
//
// The body gets the element itself, not just a position -- which is what the
// index was being used for anyway -- and `index` is still there for the
// layout arithmetic. The element converts implicitly, so a handler taking one
// takes `key` as it is.
//
// One parameter pack serves all three spellings, and mixtures of them: each
// argument deduces its own `values<T, N>`, and the elements are flattened
// into one array here. There is no specialisation of this template anywhere
// -- what varies is which deduction guide claims the argument.
template <impl::values... Parts>
struct iterate {
    using element_t = std::common_type_t<typename decltype(Parts)::element_t...>;

    static constexpr int count = (static_cast<int>(Parts.count) + ... + 0);

    // The pieces, run together. Building it once here is what lets the step
    // be an index rather than a search through the pack.
    static constexpr std::array<element_t, count> elements = [] {
        std::array<element_t, count> all{};
        int at = 0;
        auto const add = [&](auto const& part) {
            for (std::size_t i = 0; i < part.count; ++i) {
                all[at++] = part.items[i];
            }
        };
        (add(Parts), ...);
        return all;
    }();

    int index{};
    element_t value{};

    constexpr operator element_t() const noexcept { return value; }

    // Text only where the elements are text: the property that takes a
    // string is the one this exists for. It refers into this object, which
    // lives as long as the full expression building the child -- the
    // property has copied it long before that ends.
    constexpr std::basic_string_view<element_t> text() const noexcept
        requires impl::is_character<element_t>
    {
        return {&value, 1};
    }
};

namespace impl {

// What an iteration argument is: how many steps, and what to hand the body on
// step i. One specialisation per form, which is what lets a body ask for a
// position or for an element and get exactly that.
//
// The primary is defined and empty on purpose: asking it for `count` has to
// be an ordinary substitution failure, so that an argument which is merely
// some other one-parameter callable is rejected rather than hard-errored.
template <typename T>
struct iteration {};

template <int Count>
struct iteration<repeat<Count>> {
    static constexpr int count = Count;

    static constexpr repeat<Count> at(int index) noexcept { return {index}; }
};

template <values... Parts>
struct iteration<iterate<Parts...>> {
    using step_t = iterate<Parts...>;

    static constexpr int count = step_t::count;

    static constexpr step_t at(int index) noexcept { return {index, step_t::elements[index]}; }
};

template <typename F>
struct iteration_of {};

template <typename R, typename C, typename A>
struct iteration_of<R (C::*)(A) const> : iteration<std::decay_t<A>> {};

template <typename R, typename C, typename A>
struct iteration_of<R (C::*)(A)> : iteration<std::decay_t<A>> {};

template <typename F>
using iteration_over = iteration_of<decltype(&std::decay_t<F>::operator())>;

template <typename F>
concept iterating = requires { iteration_over<F>::count; };

}  // namespace impl

// The contents of a collection-valued property, pending like every other
// argument: `Children[first, second, third]`.
//
// It holds references, not copies. The items are temporaries of the very
// expression that is building the parent, so they outlive this op by
// definition, and copying a wrapper would touch a reference count for
// nothing.
//
// C++23's multi-argument subscript is what lets the items arrive as a
// parameter pack. An initializer_list would force them through one common
// type, which for a list of unrelated controls means slicing every one of
// them to a base before the collection ever sees it.
// `Owner` sits before the pack because a pack has to come last; everywhere
// else it is the trailing parameter. See impl::check_owner.
template <PropertyKey key, typename Owner, typename... Items>
struct CollectionArg {
    std::tuple<Items const&...> items_;

    template <typename Obj>
    void operator()(Obj const& object) const {
        impl::check_owner<Owner, Obj>();
        std::apply([&object](Items const&... items) {
            impl::CollectionSetter<key>::set(object, items...);
        }, items_);
    }
};

namespace impl {

// Whether an argument merely borrows what it will apply. A collection list
// refers to the items written inside it, which is right inside a
// constructor's braces -- they are temporaries of that very expression -- and
// wrong in a style, which outlives the expression that built it.
template <typename Arg>
inline constexpr bool borrows_items = false;

template <PropertyKey key, typename Owner, typename... Items>
inline constexpr bool borrows_items<CollectionArg<key, Owner, Items...>> = true;

}  // namespace impl

// The tag a collection-valued property is written as. Unlike a property tag
// it is subscripted rather than assigned to, which is what makes the
// contents of a collection read as a list in the source.
template <PropertyKey key, typename Owner = void>
struct CollectionProperty {
    template <typename... Items>
    CollectionArg<key, Owner, Items...> operator[](Items const&... items) const {
        return {std::tie(items...)};
    }
};

namespace impl {

// What it means for one argument to be a constructor argument at all:
// either it is one of the ops above, which knows itself what to do with the
// object, or it is a bare value the object has a route for, matched on its
// type alone -- `hAlign.center`, `L"..."`, `Margin{20}` -- which is what
// keeps the DSL short enough to be worth writing.
//
// This is a diagnostic predicate, not a constraint: the constructor stays
// viable for an argument that fails it, so that the error names the mistake
// rather than the absence of an overload (see apply_argument below).
template <typename Arg, typename Obj>
concept applicable_to =
    requires(Obj const& object, Arg&& argument) { argument(object); }
    // A tagged value names its own property, so the route is the key it
    // carries and no class declares anything for it.
    || (tagged<Arg> && requires(Obj const& object, Arg&& argument) {
           PropertySetter<std::remove_cvref_t<Arg>::tag>::set(object, argument);
       })
    || requires(Obj const& object, Arg&& argument) {
           object.setPositional(std::forward<Arg>(argument));
       }
    // A repeated child: what it builds is what has to have a route, and the
    // parent takes it the same way a child written out by hand is taken.
    || (iterating<Arg> && requires(Obj const& object, Arg&& argument) {
           object.setPositional(argument(iteration_over<Arg>::at(0)));
       });

// The whole condition on a wrapper's setter-pack constructor, stated once
// so that no generated class carries it.
//
// It says only what overload resolution has to know, and deliberately not
// whether the object has a route for each argument. An argument the object
// cannot take is a mistake to report, not a reason for the constructor to
// disappear: a removed constructor leaves the compiler saying only that no
// overload matched, while a viable one fails inside apply_argument below,
// where the message can say what is actually wrong.
//
// What it does exclude is the two constructors this template would
// otherwise take arguments away from. The protected constructor takes an
// Impl*, which a derived level hands down as its *own* Impl* -- so at the
// base that pointer needs a conversion while this template would match it
// exactly. A pointer is therefore not a builder argument, with the one
// exception the syntax writes constantly: a string literal is a pointer too.
//
// And a single argument derived from the class belongs to the copy
// constructor, for the same reason: it matches this template exactly and
// the copy constructor only after a qualification conversion. Public
// inheritance runs the length of the hierarchy, so a user-authored type
// counts too.
template <typename Arg>
inline constexpr bool impl_pointer =
    std::is_pointer_v<std::decay_t<Arg>>
    && !is_character<std::remove_cv_t<std::remove_pointer_t<std::decay_t<Arg>>>>;

template <typename Obj, typename... Args>
concept setter_pack =
    (sizeof...(Args) != 1 || !(std::derived_from<std::decay_t<Args>, Obj> && ...))
    && !(impl_pointer<Args> || ...);

// One constructor argument, applied.
//
// The last branch is the diagnostic one: it is reached only by an argument
// none of the routes above claimed, and says so instead of letting the
// error surface as a failed call to setPositional.
template <typename Obj, typename Arg>
void apply_argument(Obj const& object, Arg&& argument) {
    if constexpr (requires { argument(object); }) {
        argument(object);
    } else if constexpr (tagged<Arg>) {
        PropertySetter<std::remove_cvref_t<Arg>::tag>::set(object, argument);
    } else if constexpr (iterating<Arg>) {
        // A fold over the steps rather than a loop: how many, and what each
        // one is, both come from the parameter's type, so every step is a
        // constant.
        using loop = iteration_over<Arg>;
        [&]<int... index>(std::integer_sequence<int, index...>) {
            (object.setPositional(argument(loop::at(index))), ...);
        }(std::make_integer_sequence<int, loop::count>{});
    } else if constexpr (requires { object.setPositional(std::forward<Arg>(argument)); }) {
        object.setPositional(std::forward<Arg>(argument));
    } else {
        static_assert(applicable_to<Arg, Obj>,
                      "wxl: no unnamed-argument route for this type on this object. Write "
                      "the property name (`text = ...`), or wrap the value in the "
                      "property's own tag (`Margin{20}`).");
    }
}

}  // namespace impl

// A preset: the constructor arguments themselves, kept in a variable and worn
// by as many objects as you like.
//
//     inline constexpr Preset centered{hAlign.center, vAlign.center};
//
//     Button { L"7", centered };
//     centered(button);
//
// Written unnamed inside braces, a preset dresses the object being built --
// which is what an unnamed argument does, so nothing is added for it -- and
// called, it dresses one that already exists. Those are its two forms, and
// building is neither: the object a property asks for is what a Template
// builds, and a preset assigned to a property is refused with a message that
// says so.
//
// No target type. A preset fits whatever has the members it names, and its
// arguments are checked where it is worn, against the object wearing it: font
// settings written once fit a TextBlock and a Control alike, though the two
// share no base that has a font. What that costs is where a mistake is
// reported -- at the wearer, with the property and the class named and the
// preset in the trace.
//
// The setters are kept as they were written, each in its own type, and
// nothing is allocated for them: a preset is a constant whenever its setters
// are, so one at namespace scope is laid out by the compiler and nothing
// about it runs at start-up. That is also why the pack is the type and a
// preset is only ever `auto` or deduced: a class that has to keep one takes
// the pack as its own parameter, the way HaloEffect does.
//
// Presets nest: a preset inside another's braces is an argument like any
// other, and so is a template.
//
// It is not a Style and does not become one. A Style is a framework resource,
// applied to a FrameworkElement by the framework itself, and most of what an
// application wants to give a name to -- a brush, a gradient, a transform --
// has no Style property to apply one to. A preset is the arguments, so
// wearing one is the same thing as having been built with them: it is the
// braces, given a name. Which is also why it holds more than properties --
// unnamed values, event handlers and children go in the same braces.
//
// Nor is it a bag: there is nothing in it to look up, read back or iterate.
// The arguments are deferred operations and their order is the order they
// were written in.
//
// One thing a preset may not carry is a live object: a wrapper written inside
// the braces is made on the spot, and at namespace scope the spot is static
// initialization, before the runtime is up -- and a child made once and worn
// twice is handed to the framework twice, which refuses it a second parent.
// Both are written as a Template, which builds afresh where it is applied.
template <typename... Setters>
class Preset {
public:
    constexpr explicit Preset(Setters... setters) : setters_{std::move(setters)...} {
        static_assert(!(impl::borrows_items<Setters> || ...),
                      "wxl: a collection list refers to the objects written inside it, and they "
                      "live only as long as the expression that built them -- a preset outlives "
                      "it. Write the items as unnamed children, or keep the list in the braces "
                      "of the object that gets it.");
    }

    // Dressing an object: the arguments, applied to one that already exists.
    // Public, and the only way a preset is ever worn -- the unnamed form
    // inside braces is this call, made by the constructor.
    template <typename Obj>
    void operator()(Obj const& object) const {
        std::apply([&object](auto const&... setter) { (impl::apply_argument(object, setter), ...); },
                   setters_);
    }

private:
    [[no_unique_address]] std::tuple<Setters...> setters_;
};

template <typename... Setters>
Preset(Setters&&...) -> Preset<std::decay_t<Setters>...>;

// A template: the description of an object, built where it is applied.
//
//     background = Template<SolidColorBrush>{ARGB{0xFF101215}}
//     Grid { Template<Border>{cardLook, TextBlock{L"inside"}} }
//     auto const brush = ink.build();
//
// Writing `SolidColorBrush{...}` constructs the object on the spot, and in a
// namespace-scope constant "on the spot" is static initialization -- before
// wxl has brought the runtime up, which no WinRT object survives. A template
// keeps the arguments and builds the object at the moment it is applied, so
// it stands wherever an object of its target is expected: assigned to a
// property, written unnamed inside a parent's braces, or asked for the object
// itself. Every application builds afresh, which is what a child needs, the
// framework refusing a second parent, and what a resource in a shared preset
// gets.
//
// The target is written out because it is what checks the arguments -- every
// one of them is verified against it here, where the template is written --
// and because it is what gets built. The arguments themselves leave the type:
// a template is named as a type, kept in a member, passed and returned, and a
// type carrying its arguments could be none of those. core::function erases
// them, its body in the STA pool, which stands before the first object of the
// program and after the last -- so a template at namespace scope is an
// ordinary line.
template <typename Target>
class Template {
    // The plain spelling, not `noexcept`: a setter's work goes through WinRT
    // and can fail, and whoever applies the template is there to hear about it.
    using func_t = core::function<void(Target const&)>;

public:
    using target_type = Target;

    template <typename... Setters>
        requires(!(sizeof...(Setters) == 1
                   && (std::same_as<std::decay_t<Setters>, Template> && ...)))
    Template(Setters&&... setters)
        : setters_{[pack = std::tuple<std::decay_t<Setters>...>{std::forward<Setters>(setters)...}](
                       Target const& object) {
              std::apply(
                  [&object](auto const&... setter) {
                      (impl::apply_argument(object, setter), ...);
                  },
                  pack);
          }} {
        static_assert(!(impl::borrows_items<std::decay_t<Setters>> || ...),
                      "wxl: a collection list refers to the objects written inside it, and they "
                      "live only as long as the expression that built them -- a template "
                      "outlives it. Write the items as unnamed children, or keep the list in "
                      "the braces of the object that gets it.");
    }

    // The object this template describes, built and dressed. Available for a
    // target that can be default-constructed, which is every concrete class
    // and no abstract one -- a wxl base such as Brush or Panel keeps its
    // constructor protected, and a template is written for the class that
    // gets built.
    Target build() const
        requires std::default_initializable<Target>
    {
        Target object;
        setters_(object);
        return object;
    }

    // The conversion is the construction, the way converting a brush path is
    // the lookup: a template written but never applied builds nothing.
    operator Target() const
        requires std::default_initializable<Target>
    {
        return build();
    }

private:
    func_t setters_;
};

// The verb to Preset's noun: the same arguments, applied once, to an object
// that already exists.
//
//     Apply {themeButton, content = icon, isEnabled = false};
//
// Everything a description can hold goes in -- assignments, subscriptions,
// tagged values, resource paths, a preset, an unnamed child -- because the
// arguments take the very routes the constructor's do. The first one is the
// target, and nothing else may be: a control there is what tells this form
// from a description, and a description with a stray control at the front
// would otherwise dress the wrong thing in silence.
//
// It is a statement, not a value: no members, no result, the whole of it in
// the constructor, so the temporary costs nothing at all. Written this way
// the runtime form reads like the declarative one -- same braces, same
// commas, same order -- which is the entire point of it. The older spelling,
// `(content = icon)(button)`, remains what a *preset* does when it is worn,
// and is no longer how a pack of setters is written by hand.
class Apply {
public:
    template <typename Obj, typename... Setters>
        requires std::derived_from<std::remove_cvref_t<Obj>, Object>
    explicit Apply(Obj const& object, Setters&&... setters) {
        (impl::apply_argument(object, std::forward<Setters>(setters)), ...);
    }
};

template<typename Preset>
class BuildPreset : public Preset::target_type
{
public:
    template <typename... Setters>
    BuildPreset(Setters&&... setters) : Preset::target_type {
        Preset(),
        std::forward<Setters>(setters)...
    } {}
};

// A preset is worn, never assigned: the object a property asks for is what a
// Template builds. Refused here by name rather than left to fail inside the
// property's setter, where the message would be about a conversion nobody
// wrote.
template <PropertyKey key, typename Owner, typename... Setters>
struct SetterOp<key, Preset<Setters...>, Owner> {
    Preset<Setters...> value_;

    template <typename Obj>
    void operator()(Obj const&) const {
        static_assert(sizeof...(Setters) < 0,
                      "wxl: a preset is worn, not assigned -- written unnamed inside the braces "
                      "it dresses the object being built. The object a property asks for is "
                      "built by a template: write `property = Template<T>{...}`, with the "
                      "preset inside if it is the look.");
    }
};

namespace impl {

// Everything that takes part in a braced description: a pending assignment, a
// pending subscription, a property or event tag, a tagged value, a preset, a
// template. The list is a variable template rather than a base class or a concept
// over structure, for two reasons: a marker base would turn these aggregates
// into non-aggregates, and a family that arrives later -- a resource path, or a
// description of something that is not a control at all -- joins by
// specialising this one name, without touching the guard below.
template <typename T>
inline constexpr bool describes = tagged<T>;

template <PropertyKey key, typename T, typename Owner>
inline constexpr bool describes<SetterOp<key, T, Owner>> = true;
template <EventKey key, typename Fn, typename Owner>
inline constexpr bool describes<AddEventOp<key, Fn, Owner>> = true;
template <EventKey key, typename Owner>
inline constexpr bool describes<RemoveEventOp<key, Owner>> = true;
template <EventKey key, typename Setter, typename Owner>
inline constexpr bool describes<SettersEventOp<key, Setter, Owner>> = true;
template <PropertyKey key, typename Value, typename Owner>
inline constexpr bool describes<Property<key, Value, Owner>> = true;
template <EventKey key, typename Owner>
inline constexpr bool describes<Event<key, Owner>> = true;
template <typename... Setters>
inline constexpr bool describes<Preset<Setters...>> = true;
template <typename Target>
inline constexpr bool describes<Template<Target>> = true;

}  // namespace impl

// The comma is not a way to write two of them.
//
// `(text = a, isEnabled = b)` is the built-in comma operator: it evaluates the
// first assignment, throws the result away and yields the second. A pack
// written that way loses everything but its last element, and loses it
// silently -- the code compiles and does a fraction of what it says. Deleting
// the operator turns that into an error where it is written.
//
// The description is a list of *arguments*, and an argument list is separated
// by commas the language already understands -- inside braces, where each one
// reaches its own destination. Any parenthesised comma between two description
// tokens is therefore a mistake, and this says so.
//
// ADL finds this because the operands live in wxl. Two controls are not
// covered: a wrapper is an ordinary object, and poisoning the comma for it
// would reach far outside the DSL.
template <typename A, typename B>
    requires(impl::describes<std::remove_cvref_t<A>> || impl::describes<std::remove_cvref_t<B>>)
void operator,(A&&, B&&) = delete;

}  // namespace wxl
