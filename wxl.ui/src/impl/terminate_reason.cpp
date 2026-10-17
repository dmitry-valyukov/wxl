// The name of a winrt::hresult_error in the report std::terminate makes: its code, and what
// the error says of itself or what the system says of the code.
//
// Asked of the error's own error info and of the system, both of which answer a failure
// with a code: the error's message() would build an hstring on the heap, and with no
// memory its std::bad_alloc, meeting noexcept, would bring std::terminate back from inside
// the handler. The strings the error info hands out are the system's, made by it for the
// caller and freed here; with no memory left the call fails, and the system's text, written
// into a buffer of this frame, takes their place.
#include "platform.h"

#include <oleauto.h>
#include <restrictederrorinfo.h>

#include <winrt/base.h>

#include "terminate_reason.h"

namespace wxl::impl {
namespace {

// A string the error info makes for its caller.
struct system_string {
    BSTR text = nullptr;

    system_string() noexcept = default;
    system_string(system_string const&) = delete;
    system_string& operator=(system_string const&) = delete;
    ~system_string() { ::SysFreeString(text); }

    std::wstring_view view() const noexcept { return {text, ::SysStringLen(text)}; }
};

// "0x80004005", the code as Windows writes one.
void put_code(core::impl::terminate_report& report, std::uint32_t const code) noexcept {
    char text[10] = {'0', 'x'};
    for (int digit = 0; digit != 8; ++digit) {
        text[2 + digit] = "0123456789ABCDEF"[(code >> (28 - 4 * digit)) & 0xFu];
    }
    report.put(std::string_view{text, sizeof text});
}

// What the error says of itself: the details it was originated with, when GetErrorDetails
// succeeds -- any code of success -- and names the error's own code. The restricted
// description, which is all an error of XAML or of the composition says, when it holds more
// than ASCII whitespace, and the plain one otherwise; ASCII whitespace at the end is cut off.
bool put_details(core::impl::terminate_report& report, winrt::hresult_error const& error) noexcept {
    winrt::com_ptr<::IRestrictedErrorInfo> const info = error.try_as<::IRestrictedErrorInfo>();
    if (!info) return false;

    system_string description;
    system_string restricted;
    system_string capability;
    HRESULT code = S_OK;
    if (FAILED(info->GetErrorDetails(&description.text, &code, &restricted.text, &capability.text)) ||
        code != static_cast<std::int32_t>(error.code())) {
        return false;
    }

    std::wstring_view said = core::trim_back(restricted.view());
    if (said.empty()) said = core::trim_back(description.view());
    if (said.empty()) return false;

    report.put(": ");
    report.put(said);
    return true;
}

// What the system says of the code. Language 0 is the system's own order of search:
// language-neutral first, then the thread's language, the user's, the system's, US English.
// A text longer than the buffer is refused rather than cut, and the code then stands alone.
void put_system_text(core::impl::terminate_report& report, std::uint32_t const code) noexcept {
    wchar_t text[512];
    DWORD const size = ::FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                        nullptr, code, 0, text, static_cast<DWORD>(std::size(text)),
                                        nullptr);

    std::wstring_view const said = core::trim_back(std::wstring_view{text, size});
    if (said.empty()) return;

    report.put(": ");
    report.put(said);
}

}  // namespace

bool name_hresult_error(core::impl::terminate_report& report) noexcept {
    try {
        throw;
    } catch (winrt::hresult_error const& error) {
        auto const code = static_cast<std::uint32_t>(static_cast<std::int32_t>(error.code()));
        report.put("wxl: unhandled exception: winrt::hresult_error ");
        put_code(report, code);
        if (!put_details(report, error)) put_system_text(report, code);
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace wxl::impl
