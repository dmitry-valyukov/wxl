// The name of a winrt::hresult_error in the report std::terminate makes is asked of the error's
// own error info and of the system, both of which answer a failure with a code: the error's
// message() would build an hstring on the heap, and with no memory its std::bad_alloc, meeting
// noexcept, would bring std::terminate back from inside the handler. The strings the error info
// hands out are the system's, made by it for the caller and freed here; with no memory left the
// call fails, and the system's text, written into a buffer of this frame, takes their place.
#include "platform.h"

#include <appmodel.h>
#include <oleauto.h>
#include <restrictederrorinfo.h>

// rpcndr.h, which these bring in, defines `small` as `char`, and Icons below has a field of that
// name. Nothing here uses the macro, so it goes for the rest of this file.
#undef small

#include <winrt/base.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include "Application.h"

namespace wxl {

namespace {

// Resource id 1: the icon the shell shows for the executable, the one
// wxl_target_icon() writes into the generated script, and the one
// wxl::TrayIcon falls back to. Asked for at the size it will be shown at --
// the title bar takes the small one and Alt+Tab the large one, and an .ico
// carries both -- so nothing is scaled from the wrong size.
HICON iconOfThisModule(int width, int height) noexcept {
    return static_cast<HICON>(::LoadImageW(::GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1),
                                           IMAGE_ICON, width, height, LR_SHARED));
}

// Both places a window's icon can come from. WM_GETICON answers for the window
// itself and says nothing about the class behind it, and a window drawn with
// the class icon is a window with an icon -- ours are exactly that.
bool hasIcon(HWND window) noexcept {
    if (::SendMessageW(window, WM_GETICON, ICON_SMALL, 0)) return true;
    if (::SendMessageW(window, WM_GETICON, ICON_BIG, 0)) return true;

    return ::GetClassLongPtrW(window, GCLP_HICONSM) || ::GetClassLongPtrW(window, GCLP_HICON);
}

struct Icons {
    HICON large;
    HICON small;
};

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
// than ASCII whitespace, and the plain one otherwise; ASCII whitespace at the end is trimmed.
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
// A text longer than the buffer is refused rather than truncated, and the code then stands alone.
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

bool Application::isPackaged() noexcept {
    UINT32 length = 0;
    return ::GetCurrentPackageFullName(&length, nullptr) != APPMODEL_ERROR_NO_PACKAGE;
}

std::filesystem::path Application::makeAbsolute(std::filesystem::path const& path) {
    return path.is_absolute() ? path : core::environment::application_folder() / path;
}

void Application::exit() {
    if (loopRunning()) winrt::Microsoft::UI::Xaml::Application::Current().Exit();
}

void Application::install_terminate_report() noexcept {
    core::impl::set_exception_namer(&name_hresult_error);
    static_cast<void>(core::impl::set_terminate_with_reason());
}

bool Application::name_hresult_error(core::impl::terminate_report& report) noexcept {
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

void Application::apply_icon() noexcept {
    Icons const icons{
        iconOfThisModule(::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON)),
        iconOfThisModule(::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON)),
    };

    // No icon resource in this executable: the application either ships none or
    // sets its own, and either way there is nothing here to hand out.
    if (!icons.large && !icons.small) return;

    // Only the thread's own top-level windows, which is every window an
    // application put up from wxl_launched: EnumThreadWindows skips child
    // windows, and with them the XAML island's own hosts. Invisible ones are
    // skipped too -- a window nobody has shown has nowhere to show an icon,
    // and the ones WinUI keeps for itself are exactly that.
    ::EnumThreadWindows(
        ::GetCurrentThreadId(),
        [](HWND window, LPARAM state) noexcept -> BOOL {
            Icons const& icons = *reinterpret_cast<Icons const*>(state);

            if (::IsWindowVisible(window) && !hasIcon(window)) {
                if (icons.large)
                    ::SendMessageW(window, WM_SETICON, ICON_BIG,
                                   reinterpret_cast<LPARAM>(icons.large));
                if (icons.small)
                    ::SendMessageW(window, WM_SETICON, ICON_SMALL,
                                   reinterpret_cast<LPARAM>(icons.small));
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&icons));
}

}  // namespace wxl
