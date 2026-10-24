#pragma once

// wxl::failureOf -- what a caught exception says: the HRESULT of the platform and the words it gave.
//
// A call of the platform that fails throws, and `catch (...)` is all an application that includes no
// projection header can write; what it wants of the exception -- is it "access denied", what is the message to
// show -- is read here, on the private side where the projection's exception type is known.

#include <cstdint>
#include <exception>

#include "hstring_param.h"
#include "impl/hresult.h"

namespace wxl {

struct Failure {
    int32_t hresult = 0;
    hstring message;

    /// E_ACCESSDENIED: a camera, a folder or a setting the user has not allowed.
    bool accessDenied() const noexcept { return hresult == impl::e_accessdenied; }
};

/// The failure an exception stands for; an exception that is not the platform's has the HRESULT E_FAIL and its own what().
Failure failureOf(std::exception_ptr error);

}  // namespace wxl
