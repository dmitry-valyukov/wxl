#pragma once

#include <cstdint>
#include <exception>

namespace wxl::impl {

// The HRESULTs wxl answers or checks for itself, named as <winerror.h> names them, in lower
// case: that header has no business in wxl's headers for five constants.
inline constexpr std::int32_t e_nointerface = static_cast<std::int32_t>(0x80004002);
inline constexpr std::int32_t e_notimpl = static_cast<std::int32_t>(0x80004001);
inline constexpr std::int32_t e_bounds = static_cast<std::int32_t>(0x8000000B);
inline constexpr std::int32_t e_fail = static_cast<std::int32_t>(0x80004005);
inline constexpr std::int32_t e_accessdenied = static_cast<std::int32_t>(0x80070005);

// A minimal HRESULT-carrying exception, so wxl::impl's raw ABI calls
// (impl/winrt_helpers.h, impl/activator.h) don't need winrt::check_hresult or
// winrt::hresult_error -- checking an HRESULT and throwing on failure is a
// three-line problem, not worth pulling <winrt/base.h> in for. For both COM
// boundaries wxl::impl has: incoming (check_hresult, calling into the WinUI3
// ABI) and outgoing (exception_to_hresult, an ABI method of an object of
// wxl's own -- impl/sta_com.h -- answering what it threw).
class hresult_error : public std::exception
{
public:
    explicit hresult_error(std::int32_t hr) noexcept : hr_(hr) {}

    std::int32_t code() const noexcept { return hr_; }

    char const* what() const noexcept override { return "wxl::impl::hresult_error"; }

private:
    std::int32_t hr_;
};

// Out of line, because constructing and throwing the exception is by far the
// bulkiest part of check_hresult and check_hresult sits in generated code at
// every activation, property and method call.
[[noreturn]] void throw_hresult(std::int32_t hr);

inline void check_hresult(std::int32_t hr) {
    if (hr < 0) {
        throw_hresult(hr);
    }
}

// Call from inside a `catch (...)` block at an ABI boundary (a noexcept
// __stdcall method wxl implements) to convert whatever's in flight into an
// HRESULT to return -- preserves the original code for a propagating
// hresult_error, otherwise reports a generic failure.
inline std::int32_t exception_to_hresult() noexcept {
    try {
        throw;
    } catch (hresult_error const& e) {
        return e.code();
    } catch (...) {
        return e_fail;
    }
}

}  // namespace wxl::impl
