#pragma once

// What std::terminate says of a winrt::hresult_error. wxl.core's handler names a
// std::exception, an exception of a type it does not know and a terminate with nothing
// thrown; the projection's error is not a std::exception, and naming it is this layer's to
// add, through the namer wxl.core asks. wWinMain sets both first of all, on the thread the
// windows and their listeners run on.
//
// Private: the projection's error is named in terminate_reason.cpp, out of sight of the
// headers. Included by launch.cpp and the tests.

#include "../core.h"

namespace wxl::impl {

// The line for a winrt::hresult_error -- its code, and what the error says of itself or,
// failing that, what the system says of the code -- put into `report` from inside a catch
// of the exception being described; false, and nothing put, for an exception of another
// type. Nothing on the way throws, so describing it cannot bring std::terminate back.
bool name_hresult_error(core::impl::terminate_report& report) noexcept;

// Gives wxl.core's terminate handler the name of a winrt::hresult_error, and sets the handler
// on the calling thread: wWinMain's call, once in a process, since the namer is set once. A
// worker thread sets wxl.core's handler alone, and it asks the same namer.
inline void install_terminate_report() noexcept {
    core::impl::set_exception_namer(&name_hresult_error);
    static_cast<void>(core::impl::set_terminate_with_reason());
}

}  // namespace wxl::impl
