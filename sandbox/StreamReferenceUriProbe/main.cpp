// Проба RandomAccessStreamReference::CreateFromUri с адресом file://, на голой проекции cppwinrt.
//
// Журнал -- stream-reference-uri-probe.log рядом с исполняемым файлом. Без ключей проба идёт по
// четырём файлам (простой, с пробелом, с кириллицей, с кириллицей в папке с пробелом) и по
// шести написаниям адреса каждого (file:///, с %XX, с обратными слешами, file://localhost/,
// file://M:/, file:/), плюс ms-appx и ms-appdata; на каждый адрес:
//   path       три способа получить из адреса путь (PathCreateFromUrl от текста и от AbsoluteUri,
//              UnescapeComponent(Path())) и совпал ли он с настоящим файлом;
//   direct     CreateFromUri(...).OpenReadAsync() -- читается ли ссылка сама по себе;
//   clipboard  DataPackage.SetBitmap(ссылка) -> Clipboard.SetContent[WithOptions] ->
//              Clipboard.GetContent().GetBitmapAsync() -> OpenReadAsync() -- то, что делает вставка.
// Шаг, на котором отказ, и HRESULT идут в журнал; читается весь поток, сверяются размер и
// сигнатура JPEG. На файл дальше: control -- ссылка из потока StorageFile через буфер; onfile --
// ссылка из потока CreateRandomAccessStreamOnFile (повторное открытие, буфер, переименование
// файла при живой ссылке). В начале -- интерфейсы, которые объявляют платформенные ссылки, и
// есть ли у них интерфейс, о котором буфер спрашивает ссылку при чтении другим процессом.
// Clipboard.SetContent в классическом приложении требует окна переднего плана, поэтому проба
// открывает своё.
//
// Ключи: --self -- файлы рядом с exe ("файл приложения"), иначе M:\Temp\uriprobe;
// --hold и --hold-file -- положить в буфер картинку (ссылкой из потока StorageFile или
// CreateFromFile) и ждать 30 секунд, пока её прочтёт другой процесс (например,
// System.Windows.Forms.Clipboard.GetImage из pwsh -STA).

#include "platform.h"

#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.h>
#include <shcore.h>
#include <shlwapi.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace wf = winrt::Windows::Foundation;
namespace dt = winrt::Windows::ApplicationModel::DataTransfer;
namespace ss = winrt::Windows::Storage::Streams;

namespace {

FILE* g_log = nullptr;

void say(char const* format, auto... args) {
    std::fprintf(g_log, format, args...);
    std::fputc('\n', g_log);
    std::fflush(g_log);
}

std::string narrow(std::wstring_view text) {
    if (text.empty()) return {};
    std::string out(WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr), 0);
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), static_cast<int>(out.size()), nullptr,
                        nullptr);
    return out;
}

std::string describe(winrt::hresult_error const& e) {
    char head[32];
    std::snprintf(head, sizeof head, "0x%08X ", static_cast<unsigned>(e.code().value));
    return head + narrow(e.message());
}

struct Variant {
    std::wstring name;
    std::wstring uri;
};

// Читает поток целиком; возвращает размер или бросает.
wf::IAsyncOperation<uint32_t> drain(ss::IRandomAccessStreamWithContentType stream, bool& jpeg) {
    auto const size = static_cast<uint32_t>(stream.Size());
    ss::Buffer buffer {size ? size : 1};
    auto const read = co_await stream.ReadAsync(buffer, buffer.Capacity(), ss::InputStreamOptions::None);
    auto const data = read.data();
    jpeg = read.Length() >= 2 && data[0] == 0xFF && data[1] == 0xD8;
    co_return read.Length();
}

wf::IAsyncAction direct(Variant const& variant, uint64_t expected) {
    char const* step = "CreateFromUri";
    try {
        auto const reference = ss::RandomAccessStreamReference::CreateFromUri(wf::Uri {variant.uri});
        step = "OpenReadAsync";
        auto const stream = co_await reference.OpenReadAsync();
        step = "ReadAsync";
        bool jpeg = false;
        auto const got = co_await drain(stream, jpeg);
        say("  direct     OK   size=%u expected=%llu jpeg=%d type=%s", got, expected, jpeg, narrow(stream.ContentType()).c_str());
    } catch (winrt::hresult_error const& e) {
        say("  direct     FAIL at %s: %s", step, describe(e).c_str());
    }
}

wf::IAsyncAction clipboard(Variant const& variant, uint64_t expected, bool withOptions) {
    char const* step = "CreateFromUri";
    try {
        auto const reference = ss::RandomAccessStreamReference::CreateFromUri(wf::Uri {variant.uri});
        dt::DataPackage package;
        package.SetBitmap(reference);
        step = "SetContent";
        if (withOptions) {
            if (!dt::Clipboard::SetContentWithOptions(package, dt::ClipboardContentOptions {})) throw winrt::hresult_error(E_FAIL, L"SetContentWithOptions returned false");
        } else {
            dt::Clipboard::SetContent(package);
        }
        step = "GetContent";
        auto const view = dt::Clipboard::GetContent();
        if (!view.Contains(dt::StandardDataFormats::Bitmap())) throw winrt::hresult_error(E_FAIL, L"Bitmap is not among the formats");
        step = "GetBitmapAsync";
        auto const got = co_await view.GetBitmapAsync();
        step = "OpenReadAsync";
        auto const stream = co_await got.OpenReadAsync();
        step = "ReadAsync";
        bool jpeg = false;
        auto const size = co_await drain(stream, jpeg);
        say("  clipboard%s OK   size=%u expected=%llu jpeg=%d type=%s", withOptions ? "+opt" : "    ", size, expected, jpeg,
            narrow(stream.ContentType()).c_str());
    } catch (winrt::hresult_error const& e) {
        say("  clipboard%s FAIL at %s: %s", withOptions ? "+opt" : "    ", step, describe(e).c_str());
    }
}

// Тот же файл, но ссылка из CreateFromFile -- контроль, что сам буфер в процессе пробы работает.
wf::IAsyncAction control(std::filesystem::path const& file) {
    char const* step = "GetFileFromPathAsync";
    try {
        auto const reference = ss::RandomAccessStreamReference::CreateFromStream(
            co_await (co_await winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(file.wstring())).OpenAsync(winrt::Windows::Storage::FileAccessMode::Read));
        dt::DataPackage package;
        package.SetBitmap(reference);
        step = "SetContent";
        dt::Clipboard::SetContent(package);
        step = "GetBitmapAsync";
        auto const got = co_await dt::Clipboard::GetContent().GetBitmapAsync();
        step = "OpenReadAsync";
        auto const stream = co_await got.OpenReadAsync();
        bool jpeg = false;
        auto const size = co_await drain(stream, jpeg);
        say("control (createFromStream via clipboard) OK size=%u jpeg=%d", size, jpeg);
    } catch (winrt::hresult_error const& e) {
        say("control FAIL at %s: %s", step, describe(e).c_str());
    }
}

// Способ без URI: синхронно открытый дескриптор файла (shcore), ссылка CreateFromStream.
// Файл открывается сразу, читает потребитель; проверяются повторное открытие ссылки,
// переименование файла при живой ссылке и путь через буфер обмена.
ss::IRandomAccessStream streamOnFile(std::filesystem::path const& file) {
    ss::IRandomAccessStream stream;
    winrt::check_hresult(CreateRandomAccessStreamOnFile(file.c_str(), 0 /* FileAccessMode_Read */, winrt::guid_of<ss::IRandomAccessStream>(),
                                                        winrt::put_abi(stream)));
    return stream;
}

wf::IAsyncAction onFile(std::filesystem::path const& file, uint64_t expected) {
    char const* step = "CreateRandomAccessStreamOnFile";
    try {
        auto const reference = ss::RandomAccessStreamReference::CreateFromStream(streamOnFile(file));
        for (int pass = 1; pass <= 2; ++pass) {
            step = "OpenReadAsync";
            auto const stream = co_await reference.OpenReadAsync();
            bool jpeg = false;
            auto const size = co_await drain(stream, jpeg);
            say("  onfile     OpenReadAsync #%d OK size=%u expected=%llu jpeg=%d", pass, size, expected, jpeg);
        }
        dt::DataPackage package;
        package.SetBitmap(reference);
        step = "SetContent";
        dt::Clipboard::SetContent(package);
        step = "GetBitmapAsync";
        auto const got = co_await dt::Clipboard::GetContent().GetBitmapAsync();
        step = "OpenReadAsync via clipboard";
        auto const stream = co_await got.OpenReadAsync();
        bool jpeg = false;
        auto const size = co_await drain(stream, jpeg);
        say("  onfile     clipboard OK size=%u jpeg=%d", size, jpeg);
        auto moved = file;
        moved += L".moved";
        bool const renamed = MoveFileW(file.c_str(), moved.c_str()) != 0;
        say("  onfile     rename while the reference is alive: %s (error %lu)", renamed ? "allowed" : "refused", renamed ? 0ul : GetLastError());
        if (renamed) MoveFileW(moved.c_str(), file.c_str());
        step = "OpenReadAsync after the rename";
        auto const again = co_await reference.OpenReadAsync();
        bool jpegAgain = false;
        auto const sizeAgain = co_await drain(again, jpegAgain);
        say("  onfile     OpenReadAsync after the rename OK size=%u jpeg=%d", sizeAgain, jpegAgain);
    } catch (winrt::hresult_error const& e) {
        say("  onfile     FAIL at %s: %s", step, describe(e).c_str());
    }
}

// Как из адреса file:// получить путь: три способа, и совпал ли результат с настоящим файлом.
void paths(Variant const& variant, std::filesystem::path const& file) {
    wf::Uri const uri {variant.uri};
    auto const check = [&](char const* how, std::wstring const& path) {
        bool const same = std::filesystem::exists(path) && std::filesystem::equivalent(path, file);
        say("  path %-28s %s %s", how, same ? "OK  " : "FAIL", narrow(path).c_str());
    };
    wchar_t buffer[MAX_PATH];
    DWORD size = MAX_PATH;
    auto const fromUrl = [&](char const* how, std::wstring const& text) {
        size = MAX_PATH;
        HRESULT const hr = PathCreateFromUrlW(text.c_str(), buffer, &size, 0);
        check(how, SUCCEEDED(hr) ? std::wstring {buffer} : L"<PathCreateFromUrl failed>");
    };
    fromUrl("PathCreateFromUrl(text)", variant.uri);
    fromUrl("PathCreateFromUrl(AbsoluteUri)", std::wstring {uri.AbsoluteUri()});
    std::wstring path {wf::Uri::UnescapeComponent(uri.Path())};
    if (!path.empty() && path.front() == L'/') path.erase(0, 1);
    for (auto& c : path) if (c == L'/') c = L'\\';
    check("UnescapeComponent(Path())", path);
}

std::wstring percent(std::wstring_view text) {
    std::string const bytes = narrow(text);
    std::wstring out;
    for (unsigned char c : bytes) {
        if (c < 0x80 && (iswalnum(c) || c == '/' || c == ':' || c == '.' || c == '-' || c == '_')) {
            out += static_cast<wchar_t>(c);
        } else {
            wchar_t hex[4];
            swprintf(hex, 4, L"%%%02X", c);
            out += hex;
        }
    }
    return out;
}

std::wstring slashes(std::wstring text) {
    for (auto& c : text) if (c == L'\\') c = L'/';
    return text;
}

// Ссылка из потока StorageFile (или CreateFromFile) кладётся в буфер, и процесс ждёт: читает другой процесс.
winrt::fire_and_forget hold(std::filesystem::path file, bool fromFile) {
    try {
        auto const storage = co_await winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(file.wstring());
        dt::DataPackage package;
        if (fromFile) {
            package.SetBitmap(ss::RandomAccessStreamReference::CreateFromFile(storage));
        } else {
            package.SetBitmap(ss::RandomAccessStreamReference::CreateFromStream(co_await storage.OpenAsync(winrt::Windows::Storage::FileAccessMode::Read)));
        }
        dt::Clipboard::SetContent(package);
        say("hold: the clipboard is set");
    } catch (winrt::hresult_error const& e) {
        say("hold FAIL: %s", describe(e).c_str());
    }
}
// Какие интерфейсы объявляет сама платформа у своих ссылок: на них смотрит буфер обмена.
void interfaces(char const* label, ss::RandomAccessStreamReference const& reference) {
    for (auto const& id : winrt::get_interfaces(reference)) {
        say("%s: %08lX-%04hX-%04hX", label, id.Data1, id.Data2, id.Data3);
    }
    say("%s: runtime class %s", label, narrow(winrt::get_class_name(reference)).c_str());
    // The interface the clipboard asks a reference for when another process reads (seen by a stand-in
    // reference that logged every QueryInterface), and whether the platform's own references have it.
    winrt::guid const asked {0x3FE8E0E3, 0xEB50, 0x4682, {0xB9, 0x1D, 0x21, 0x02, 0x80, 0x12, 0xA0, 0x49}};
    void* found = nullptr;
    HRESULT const hr = winrt::get_unknown(reference)->QueryInterface(asked, &found);
    say("%s: QueryInterface(3FE8E0E3-EB50-4682-B91D-21028012A049) = 0x%08X", label, static_cast<unsigned>(hr));
    if (found) static_cast<IUnknown*>(found)->Release();
}

winrt::fire_and_forget run(std::filesystem::path root, bool self) {
    std::vector<std::pair<std::wstring, std::filesystem::path>> files {
        {L"plain", root / L"plain.jpg"},
        {L"space", root / L"with space.jpg"},
        {L"cyrillic", root / L"Кириллица.jpg"},
        {L"cyr-folder", root / L"папка с пробелом" / L"a.jpg"},
    };
    {
        interfaces("CreateFromUri(http)", ss::RandomAccessStreamReference::CreateFromUri(wf::Uri {L"https://example.com/a.jpg"}));
        interfaces("CreateFromUri(file)", ss::RandomAccessStreamReference::CreateFromUri(wf::Uri {L"file:///M:/Temp/uriprobe/plain.jpg"}));
        interfaces("CreateFromFile", ss::RandomAccessStreamReference::CreateFromFile(co_await winrt::Windows::Storage::StorageFile::GetFileFromPathAsync((root / L"plain.jpg").wstring())));
        interfaces("CreateFromStream", ss::RandomAccessStreamReference::CreateFromStream(streamOnFile(root / L"plain.jpg")));
    }
    {
        wf::Uri const relative {L"file:///M:/Temp/uriprobe/", L"Assets/with space.jpg"};
        say("relative to a base: RawUri=%s AbsoluteUri=%s", narrow(relative.RawUri()).c_str(), narrow(relative.AbsoluteUri()).c_str());
        wf::Uri const unc {L"file://server/share/dir/a b.jpg"};
        say("unc: RawUri=%s Host=[%s] Path=%s AbsoluteUri=%s", narrow(unc.RawUri()).c_str(), narrow(unc.Host()).c_str(), narrow(unc.Path()).c_str(), narrow(unc.AbsoluteUri()).c_str());
        wf::Uri const query {L"file:///M:/Temp/a%23b.jpg?x=1#frag"};
        say("query/fragment: RawUri=%s Path=%s Query=%s Fragment=%s", narrow(query.RawUri()).c_str(), narrow(query.Path()).c_str(), narrow(query.Query()).c_str(), narrow(query.Fragment()).c_str());
    }
    for (auto const& [label, file] : files) {
        auto const expected = std::filesystem::file_size(file);
        std::wstring const native = file.wstring();          // M:\Temp\...
        std::wstring const forward = slashes(native);        // M:/Temp/...
        std::vector<Variant> variants {
            {L"file:///M:/ raw", L"file:///" + forward},
            {L"file:///M:/ %XX", L"file:///" + percent(forward)},
            {L"file:///M:\\ backslash", L"file:///" + native},
            {L"file://localhost/M:/ raw", L"file://localhost/" + forward},
            {L"file://M:/ (2 slashes)", L"file://" + forward},
            {L"file:/M:/ (1 slash)", L"file:/" + forward},
        };
        // Схемы, которые API называет поддерживаемыми, -- у непакетированного приложения.
        if (label == L"plain") {
            variants.push_back({L"ms-appx", L"ms-appx:///plain.jpg"});
            variants.push_back({L"ms-appdata", L"ms-appdata:///local/plain.jpg"});
        }
        // Нормализованное платформой написание: что Uri отдаёт как AbsoluteUri.
        say("== %s: %s (%llu bytes)", narrow(label).c_str(), narrow(native).c_str(), expected);
        for (auto const& variant : variants) {
            say(" [%s] %s", narrow(variant.name).c_str(), narrow(variant.uri).c_str());
            try {
                wf::Uri const uri {variant.uri};
                say("  AbsoluteUri=%s SchemeName=%s RawUri=%s Host=[%s] Path=%s", narrow(uri.AbsoluteUri()).c_str(), narrow(uri.SchemeName()).c_str(), narrow(uri.RawUri()).c_str(), narrow(uri.Host()).c_str(), narrow(uri.Path()).c_str());
            } catch (winrt::hresult_error const& e) {
                say("  Uri ctor FAIL %s", describe(e).c_str());
                continue;
            }
            paths(variant, file);
            co_await direct(variant, expected);
            co_await clipboard(variant, expected, false);
            co_await clipboard(variant, expected, true);
        }
        co_await control(file);
        co_await onFile(file, expected);
    }
    say("done");
    PostQuitMessage(0);
}

LRESULT CALLBACK proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_DESTROY || message == WM_TIMER) PostQuitMessage(0);
    return DefWindowProcW(window, message, wparam, lparam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::filesystem::path const here = std::filesystem::path {exe}.parent_path();
    g_log = _wfopen((here / L"stream-reference-uri-probe.log").c_str(), L"wb");
    std::setvbuf(g_log, nullptr, _IONBF, 0);

    std::filesystem::path root = L"M:\\Temp\\uriprobe";
    std::wstring const command = GetCommandLineW();
    bool const self = command.find(L"--self") != std::wstring::npos;
    if (self) root = here;

    WNDCLASSW cls {};
    cls.lpfnWndProc = proc;
    cls.hInstance = instance;
    cls.lpszClassName = L"UriProbe";
    RegisterClassW(&cls);
    HWND const window = CreateWindowW(L"UriProbe", L"uri probe", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 400, 200, nullptr, nullptr, instance, nullptr);
    SetForegroundWindow(window);

    if (command.find(L"--hold") != std::wstring::npos) {
        hold(root / L"plain.jpg", command.find(L"--hold-file") != std::wstring::npos);
        SetTimer(window, 1, 30000, nullptr);
    } else {
        run(root, self);
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
