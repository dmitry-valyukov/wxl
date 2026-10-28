#pragma once

#include "core.h"

namespace wxl {

// wxl::EventToken -- the token a subscription gives, wxl.core's one type for it (core::event_token):
// not copied, emptied by moving and by taking the subscription off. A generated wrapper's token is
// cppwinrt's number as it is -- the boundary (impl/conversions.h) hands winrt::event_token{value}
// across and makes the token back through core::impl::make_event_token -- so wxl's public surface
// never needs <winrt/base.h> to name one.
using EventToken = core::event_token;

namespace impl {

// The token a removal is handed, taken out of the caller's variable: the variable is empty from
// here on, so the same variable cannot take off the subscription that gets the number next. An
// empty token was taken off already or never given -- a broken rule, and it stops the process.
inline EventToken take_token(EventToken& token) {
    if (!token) {
        core::abort(
            "wxl: a handler taken off by an empty token -- taken off already, or never given");
    }
    return std::move(token);
}

// A handler taken off by the token its subscription gave. A generated wrapper's remove
// answers nothing -- its WinRT event does not say -- and CompositionWindow's answers whether
// the token named a handler of the event: one that names none was given by another event or
// window -- a broken rule, and it stops the process rather than pass unnoticed.
template <typename Remove>
void checked_remove(Remove const& remove) {
    if constexpr (std::is_void_v<std::invoke_result_t<Remove const&>>) {
        remove();
    } else if (!remove()) {
        core::abort("wxl: a handler taken off by its token was not there");
    }
}

}  // namespace impl

}  // namespace wxl
