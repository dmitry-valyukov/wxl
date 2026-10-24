#pragma once

#include "core.h"

namespace wxl {

// wxl::EventToken -- a layout-identical stand-in for winrt::event_token (a
// single int64_t plus
// explicit operator bool -- see winrt/base.h), so wxl's public DSL surface
// (wxl/core/member.h and every generated wxl::X) never needs to include
// <winrt/base.h> just to name an event subscription token. Only the
// boundary code that actually talks to cppwinrt converts between the two,
// via std::bit_cast (see wxl/impl/event_token.h) -- confined to wxl's own
// implementation, per the "no WinRT/COM types in the public surface" rule.
struct EventToken {
    std::int64_t value{};

    explicit operator bool() const noexcept {
        return value != 0;
    }
};

inline bool operator==(EventToken const& left, EventToken const& right) noexcept {
    return left.value == right.value;
}

inline bool operator!=(EventToken const& left, EventToken const& right) noexcept {
    return !(left == right);
}

namespace impl {

// A handler taken off by the token its subscription gave. A generated wrapper's remove
// answers nothing -- its WinRT event does not say -- and CompositionWindow's answers whether
// the token named a handler of the event: one that names none was taken off already or
// was given by another event or window -- a broken rule, and it stops the process rather
// than pass unnoticed.
template <typename Remove>
void checked_remove(Remove const& remove) {
    if constexpr (std::is_void_v<std::invoke_result_t<Remove const&>>) {
        remove();
    } else if (!remove()) {
        core::abort("wxl: a handler taken off by its token was not there");
    }
}

}  // namespace impl

} // namespace wxl
