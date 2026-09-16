#pragma once

#include "../event_token.h"

#include <bit>
#include <winrt/base.h>

namespace wxl::impl {

// Conversion at the boundary between wxl's public EventToken and the
// real ABI winrt::event_token. Well-defined: both are a single int64_t with
// identical layout, so std::bit_cast is the correct, standard-sanctioned tool
// -- not a raw reinterpret_cast through pointers. That the layouts really do
// match is asserted in abi_layout.cpp, once, rather than in every translation
// unit that includes this.

inline winrt::event_token to_winrt(EventToken token) noexcept {
    return std::bit_cast<winrt::event_token>(token);
}

inline EventToken from_winrt(winrt::event_token token) noexcept {
    return std::bit_cast<EventToken>(token);
}

} // namespace wxl::impl
