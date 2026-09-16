module wxl.xml;

import wxl.core;
import std;

namespace wxl::xml {

const node* node::child(const std::string_view name, const std::size_t index) const noexcept {
    std::size_t seen = 0;

    for (const node& child : children()) {
        if (child.is_element() && child.name() == name && seen++ == index)
            return &child;
    }

    return nullptr;
}

std::optional<core::u8_view> node::attribute(const std::string_view name) const noexcept {
    const auto found = std::ranges::find_if(
        attributes_, [name](const xml::attribute& a) { return a.name() == name; });

    if (found == attributes_.end())
        return std::nullopt;

    return found->value();
}

std::optional<core::u8_view> node::attribute(const std::string_view namespace_uri,
                                                const std::string_view name) const noexcept {
    const auto found = std::ranges::find_if(
        attributes_, [&](const xml::attribute& a) { return a.full_name().is(namespace_uri, name); });

    if (found == attributes_.end())
        return std::nullopt;

    return found->value();
}

core::u8_view node::required_attribute(const std::string_view name) const {
    if (const auto value = attribute(name))
        return *value;

    throw exception(
        std::format("({},{}): the element '{}' has no attribute '{}'", line_, column_, qualified_name().chars(), name));
}

const node* node::next_in_subtree(const node* at, const node* const root) noexcept {
    if (const node* const first = at->children_.front())
        return first;

    while (at != root) {
        if (at->next_)
            return at->next_;

        at = at->parent_;
    }

    return nullptr;
}

/// Walked by the links a node already carries -- first child, next sibling,
/// parent -- rather than by recursion or by a stack of levels. A document may
/// nest as deeply as it likes, and both of those pay for depth: one in the
/// process stack, the one bound that cannot be caught when it is reached, and
/// one in an allocation on every call.
const node* node::find(const std::string_view name) const noexcept {
    if (is_element() && this->name() == name)
        return this;

    for (const node* at = next_in_subtree(this, this); at; at = next_in_subtree(at, this)) {
        if (at->is_element() && at->name() == name)
            return at;
    }

    return nullptr;
}

}  // namespace wxl::xml
