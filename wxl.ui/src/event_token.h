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

} // namespace wxl
