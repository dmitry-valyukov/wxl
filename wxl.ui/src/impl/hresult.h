#pragma once

#include <cstdint>
#include <exception>

namespace wxl {

// A minimal HRESULT-carrying exception, so wxl::impl's raw QueryInterface
// calls (see wxl/impl/*) don't need winrt::check_hresult/hresult_error --
// checking an HRESULT and throwing on failure is a three-line problem, not
// worth pulling <winrt/base.h> in for. Used at both COM boundaries wxl::impl
// has: incoming (check_hresult, calling into the real WinUI3 ABI) and
// outgoing (exception_to_hresult, implementing an ABI method the runtime
// calls into us through -- see the UIElementOverrides nested class in
// wxl/impl/UIElement.h/.cpp).
class hresult_error : public std::exception {
public:
    explicit hresult_error(std::int32_t hr) noexcept : hr_(hr) {}

    std::int32_t code() const noexcept { return hr_; }

    char const* what() const noexcept override { return "wxl::hresult_error"; }

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
// wxl::hresult_error, otherwise reports a generic failure.
inline std::int32_t exception_to_hresult() noexcept {
    try {
        throw;
    } catch (hresult_error const& e) {
        return e.code();
    } catch (...) {
        // E_FAIL, spelled out rather than pulling in <winerror.h> for one
        // constant this header would otherwise not need.
        return static_cast<std::int32_t>(0x80004005);
    }
}

} // namespace wxl
