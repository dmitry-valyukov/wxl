#pragma once

// TaggedValue -- a value that carries the property it belongs to.
//
// The builder syntax routes an unnamed argument by its type alone, so a
// property whose type says nothing about it can have no such route: three
// properties share one Thickness, and two dozen share `double`. Tagging the
// value gives it one -- `Margin{20}` is a Thickness that knows which of the
// three it is -- and the setter reads the key out of the type, so the route
// costs no code on the class at all.
//
// Construction is explicit on purpose. An implicit one would let a bare
// number match every tagged route a class has at once, which is exactly the
// ambiguity the tag exists to remove.
//
// Over a class type the tag inherits it, so the value keeps that type's own
// members and its constructors reach through the braces; over a built-in
// type it holds one and converts back out. Either way it is the value where
// a value is wanted, and the setter takes it unchanged.

#include "core.h"

namespace wxl {

template <typename T, auto Tag>
class TaggedValue : public T {
public:
    using value_t = T;
    static constexpr auto tag = Tag;

    template <typename... Args>
        requires std::constructible_from<T, Args...>
                 && (sizeof...(Args) != 1
                     || !(std::same_as<std::remove_cvref_t<Args>, TaggedValue> && ...))
    explicit constexpr TaggedValue(Args&&... args) : T(std::forward<Args>(args)...) {}
};

template <typename T, auto Tag>
    requires std::is_scalar_v<T>
class TaggedValue<T, Tag> {
public:
    using value_t = T;
    static constexpr auto tag = Tag;

    explicit constexpr TaggedValue(T value) noexcept : value_(value) {}

    constexpr operator T() const noexcept { return value_; }

private:
    T value_;
};

namespace impl {

template <typename T>
inline constexpr bool is_tagged = false;

template <typename T, auto Tag>
inline constexpr bool is_tagged<TaggedValue<T, Tag>> = true;

// An argument that names its own property. What the setter does with it is
// then a matter of the key alone, which is why no class declares anything
// for such a route.
template <typename T>
concept tagged = is_tagged<std::remove_cvref_t<T>>;

}  // namespace impl
}  // namespace wxl
