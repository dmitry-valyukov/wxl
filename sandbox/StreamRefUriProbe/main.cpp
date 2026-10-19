// Проба: что RandomAccessStreamReference делает с адресом file:/// и что с такой
// ссылкой делает буфер обмена.
//
// Случай, из-за которого проба заведена: DataPackage::SetBitmap(
// RandomAccessStreamReference::CreateFromUri(Uri{"file:///..."})) и
// Clipboard::SetContent проходят, формат Bitmap в буфере виден, а при вставке
// DataPackageView::GetBitmapAsync отдаёт ссылку, у которой
// RandomAccessStreamReference::OpenReadAsync бросает исключение. Объяснений три:
// схему file ссылка не поддерживает вовсе; буфер обмена не может переслать
// ссылку, сделанную из адреса; ошибка в обёртке над WinRT. Здесь обёрток нет --
// только проекция cppwinrt, так что третье объяснение проверяется другой
// пробой, на обёртках, теми же шагами.
//
// Что проверяется, строка на проверку:
//   1. как Windows.Foundation.Uri разбирает каждое написание адреса;
//   2. без буфера: RandomAccessStreamReference::OpenReadAsync над ссылкой,
//      сделанной пятью способами (CreateFromUri, CreateFromStream над
//      FileRandomAccessStream -- и над таким, который закрыт сразу после
//      создания ссылки, -- CreateFromFile над StorageFile, CreateFromStream над
//      потоком в памяти); у CreateFromUri -- десять написаний адреса, включая
//      http на петле (свой маленький сервер в пробе) для сравнения; у путей --
//      родное написание с обратными слэшами и с прямыми;
//   3. через буфер: DataPackage::SetBitmap -> Clipboard::SetContent
//      (SetContentWithOptions, Flush) -> Clipboard::GetContent ->
//      DataPackageView::GetBitmapAsync -> RandomAccessStreamReference::OpenReadAsync,
//      два раза подряд -- сразу и через полторы секунды. На каждом шаге
//      печатается, где оборвалось, и HRESULT исключения.
//
// Clipboard в классическом приложении работает только у окна переднего плана,
// поэтому проба открывает простое окно, берёт передний план и гонит сценарий
// из его цикла сообщений -- как пользователь, нажавший кнопку. Сценарий
// затирает содержимое буфера обмена: картинки пробы -- синтетические.
//
//   sandbox.stream-ref-uri-probe.exe [--image файл.jpg]     вся матрица выше
//   sandbox.stream-ref-uri-probe.exe --put ВИД [--flush] [--hold-ms N]
//       положить ссылку в буфер и держать N мс (по умолчанию 20000); ВИД --
//       uri, localhost, http, stream, file или memory; пока она там, буфер
//       читает другой процесс: --paste этой же пробы или любое приложение
//   sandbox.stream-ref-uri-probe.exe --paste               одна вставка из буфера
//
// Журнал -- stream-ref-uri-probe.log (.put.log, .paste.log) рядом с
// исполняемым файлом, он же в stdout. Файлы для проверок -- в
// %TEMP%\stream-ref-uri-probe.

#include "platform.h"

#include <appmodel.h>

#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Networking.Sockets.h>
#include <winrt/Windows.Networking.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace wf = winrt::Windows::Foundation;
namespace dt = winrt::Windows::ApplicationModel::DataTransfer;
namespace wss = winrt::Windows::Storage::Streams;
namespace ws = winrt::Windows::Storage;
namespace wgi = winrt::Windows::Graphics::Imaging;
namespace wns = winrt::Windows::Networking::Sockets;

using namespace std::chrono_literals;

namespace {

std::FILE* g_log = nullptr;
std::atomic<unsigned> g_progress{0};  // растёт на каждом шаге: сторож по нему видит, что проба жива
std::atomic<char const*> g_stage{"start"};
std::vector<uint8_t> g_image;  // один и тот же JPEG: в файлах, в потоке в памяти, на сервере
std::filesystem::path g_root;
HWND g_window = nullptr;
uint16_t g_port = 0;
int g_exit = 0;

constexpr size_t column = 48;  // ширина подписи строки результата

void say(std::string const& line) {
    std::puts(line.c_str());
    std::fflush(stdout);
    if (g_log) {
        std::fputs(line.c_str(), g_log);
        std::fputc('\n', g_log);
        std::fflush(g_log);
    }
}

// Строка результата: подпись слева, итог справа; продолжения итога -- под ним, с отступом на подпись.
void row(std::string const& label, std::string const& text) {
    std::string aligned;
    for (char c : text) {
        aligned += c;
        if (c == '\n') aligned.append(column + 1, ' ');
    }
    say(std::format("{:<{}} {}", label, column, aligned));
}

void stage(char const* name) {
    g_stage = name;
    ++g_progress;
}

std::string describe(winrt::hresult_error const& error) {
    auto message = winrt::to_string(error.message());
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r' || message.back() == ' ')) {
        message.pop_back();
    }
    return std::format("0x{:08X} {}", static_cast<uint32_t>(error.code().value), message);
}

std::string hex_prefix(std::vector<uint8_t> const& bytes, size_t count = 4) {
    std::string text;
    for (size_t i = 0; i < count && i < bytes.size(); ++i) text += std::format("{:02X}", bytes[i]);
    return text.empty() ? "-" : text;
}

std::string utf8(std::filesystem::path const& path) {
    auto const text = path.generic_u8string();
    return std::string(reinterpret_cast<char const*>(text.data()), text.size());
}

// Адрес из пути, как его кодирует RFC 3986: проценты -- у каждого байта UTF-8, кроме безопасных знаков, слэша
// и двоеточия диска.
std::string percent_encode(std::filesystem::path const& path) {
    std::string result;
    for (unsigned char c : path.generic_u8string()) {
        bool const plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
                           c == '.' || c == '_' || c == '~' || c == '/' || c == ':';
        result += plain ? std::string(1, static_cast<char>(c)) : std::format("%{:02X}", c);
    }
    return result;
}

std::wstring widen(std::string const& text) {
    return winrt::to_hstring(text).c_str();
}

// ---------------------------------------------------------------- чтение и осмотр потока

wf::IAsyncAction read_all(wss::IRandomAccessStream stream, std::vector<uint8_t>& bytes) {
    bytes.clear();
    wss::DataReader reader{stream.GetInputStreamAt(0)};
    for (;;) {
        auto const loaded = co_await reader.LoadAsync(65536);
        if (loaded == 0) break;
        auto const old = bytes.size();
        bytes.resize(old + loaded);
        reader.ReadBytes(winrt::array_view<uint8_t>(bytes.data() + old, bytes.data() + old + loaded));
    }
    reader.DetachStream();
}

// Что отдала ссылка: размер, тип, прочитанные байты и разбирается ли это как картинка. Поток закрывается
// в конце -- как делает пример после BitmapImage::SetSource.
wf::IAsyncAction inspect(wss::IRandomAccessStreamWithContentType stream, std::string& text) {
    std::vector<uint8_t> bytes;
    auto const size = stream.Size();
    auto const type = winrt::to_string(stream.ContentType());
    co_await read_all(stream, bytes);
    std::string picture;
    try {
        stream.Seek(0);
        auto const decoder = co_await wgi::BitmapDecoder::CreateAsync(stream);
        picture = std::format("image {}x{}", decoder.PixelWidth(), decoder.PixelHeight());
    } catch (winrt::hresult_error const& error) {
        picture = std::format("not an image ({})", describe(error));
    }
    stream.Close();
    std::string const same = g_image.empty() ? "" : bytes == g_image ? "= original " : "!= original ";
    text = std::format("size {} type '{}' read {} bytes {}magic {} {}", size, type, bytes.size(), same, hex_prefix(bytes), picture);
}

// Ждёт, не занимая поток окна, и возвращается на него: Clipboard требует поток с очередью сообщений.
wf::IAsyncAction pause(std::chrono::milliseconds duration) {
    winrt::apartment_context context;
    co_await winrt::resume_after(duration);
    co_await context;
}

// ---------------------------------------------------------------- как делается ссылка

enum class Kind { Uri, Stream, StreamClosed, File, Memory };

struct Case {
    std::string label;
    Kind kind;
    std::wstring spec;  // текст адреса или путь к файлу
};

wf::IAsyncOperation<wss::RandomAccessStreamReference> make_reference(Case const c) {
    switch (c.kind) {
    case Kind::Uri:
        co_return wss::RandomAccessStreamReference::CreateFromUri(wf::Uri{c.spec});
    case Kind::Stream: {
        auto const stream = co_await wss::FileRandomAccessStream::OpenAsync(c.spec, ws::FileAccessMode::Read);
        co_return wss::RandomAccessStreamReference::CreateFromStream(stream);
    }
    case Kind::StreamClosed: {
        // Поток закрыт сразу после того, как из него сделана ссылка: так делает код, который поток открыл и больше не ведёт.
        auto const stream = co_await wss::FileRandomAccessStream::OpenAsync(c.spec, ws::FileAccessMode::Read);
        auto const reference = wss::RandomAccessStreamReference::CreateFromStream(stream);
        stream.Close();
        co_return reference;
    }
    case Kind::File: {
        auto const file = co_await ws::StorageFile::GetFileFromPathAsync(c.spec);
        co_return wss::RandomAccessStreamReference::CreateFromFile(file);
    }
    case Kind::Memory: {
        wss::InMemoryRandomAccessStream memory;
        wss::DataWriter writer{memory};
        writer.WriteBytes(g_image);
        co_await writer.StoreAsync();
        writer.DetachStream();
        memory.Seek(0);
        co_return wss::RandomAccessStreamReference::CreateFromStream(memory);
    }
    }
    co_return nullptr;
}

// ---------------------------------------------------------------- без буфера обмена

// Ссылка -> OpenReadAsync -> осмотр; второй раз -- отдаёт ли ссылка поток повторно.
wf::IAsyncAction direct(Case const c, std::string& text) {
    char const* where = "";
    try {
        where = "create the reference";
        auto const reference = co_await make_reference(c);
        where = "RandomAccessStreamReference::OpenReadAsync (the call itself)";
        auto const operation = reference.OpenReadAsync();
        where = "RandomAccessStreamReference::OpenReadAsync (the operation it returned)";
        auto const first = co_await operation;
        std::string detail;
        co_await inspect(first, detail);
        text = "OK   " + detail;
        where = "RandomAccessStreamReference::OpenReadAsync, second time";
        auto const second = co_await reference.OpenReadAsync();
        co_await inspect(second, detail);
        text += "\nagain OK " + detail;
    } catch (winrt::hresult_error const& error) {
        text += std::format("{}FAIL at {}: {}", text.empty() ? "" : "\n", where, describe(error));
    }
}

// То же в потоке пула (MTA): влияет ли на ответ апартамент вызывающего.
wf::IAsyncAction direct_in_mta(Case const c, std::string& text) {
    winrt::apartment_context ui;
    co_await winrt::resume_background();
    try {
        auto const reference = make_reference(c).get();
        auto const stream = reference.OpenReadAsync().get();
        text = std::format("OK   size {} type '{}'", stream.Size(), winrt::to_string(stream.ContentType()));
    } catch (winrt::hresult_error const& error) {
        text = "FAIL " + describe(error);
    }
    co_await ui;
}

// ---------------------------------------------------------------- через буфер обмена

enum class Put { SetContent, SetContentFlush, OptionsDefault, OptionsPrivate };

char const* name_of(Put put) {
    switch (put) {
    case Put::SetContent: return "Clipboard::SetContent";
    case Put::SetContentFlush: return "Clipboard::SetContent + Clipboard::Flush";
    case Put::OptionsDefault: return "Clipboard::SetContentWithOptions(default options)";
    case Put::OptionsPrivate: return "Clipboard::SetContentWithOptions(no history, no roaming)";
    }
    return "";
}

bool bring_to_front() {
    if (::GetForegroundWindow() == g_window) return true;
    // Передний план Windows отдаёт процессу, которого просит нынешний передний, или тому, кто получил
    // последний ввод: на время вызова присоединяемся к очереди ввода нынешнего переднего окна.
    HWND const current = ::GetForegroundWindow();
    DWORD const currentThread = current ? ::GetWindowThreadProcessId(current, nullptr) : 0;
    DWORD const thisThread = ::GetCurrentThreadId();
    bool const attach = currentThread && currentThread != thisThread;
    if (attach) ::AttachThreadInput(thisThread, currentThread, TRUE);
    ::ShowWindow(g_window, SW_SHOW);
    ::SetForegroundWindow(g_window);
    ::BringWindowToTop(g_window);
    if (attach) ::AttachThreadInput(thisThread, currentThread, FALSE);
    return ::GetForegroundWindow() == g_window;
}

// Ссылка -> DataPackage::SetBitmap -> буфер. В отчёте -- что вышло и, при отказе, на каком шаге и с каким HRESULT.
wf::IAsyncOperation<bool> put_on_clipboard(Case const c, Put const put, std::string& report) {
    char const* where = "";
    bool ok = false;
    try {
        where = "Clipboard::Clear";
        dt::Clipboard::Clear();
        where = "create the reference";
        auto const reference = co_await make_reference(c);
        where = "DataPackage::SetBitmap";
        dt::DataPackage package;
        package.SetBitmap(reference);
        where = name_of(put);
        if (put == Put::OptionsDefault || put == Put::OptionsPrivate) {
            dt::ClipboardContentOptions options;
            if (put == Put::OptionsPrivate) {
                options.IsAllowedInHistory(false);
                options.IsRoamable(false);
            }
            bool const accepted = dt::Clipboard::SetContentWithOptions(package, options);
            report += std::format("set {}", accepted ? "ok" : "returned false");
        } else {
            dt::Clipboard::SetContent(package);
            report += "set ok";
        }
        if (put == Put::SetContentFlush) {
            where = "Clipboard::Flush";
            dt::Clipboard::Flush();
            report += ", flush ok";
        }
        ok = true;
    } catch (winrt::hresult_error const& error) {
        report += std::format("FAIL at {}: {}", where, describe(error));
    }
    co_return ok;
}

// Одна вставка: какие форматы в буфере, что отдаёт DataPackageView::GetBitmapAsync и что -- ссылка при чтении.
wf::IAsyncAction paste_once(std::string& line) {
    char const* where = "";
    try {
        where = "Clipboard::GetContent";
        auto const view = dt::Clipboard::GetContent();
        where = "DataPackageView::AvailableFormats";
        std::string formats;
        for (auto const& format : view.AvailableFormats()) formats += (formats.empty() ? "" : ", ") + winrt::to_string(format);
        line = std::format("formats [{}]", formats);
        where = "DataPackageView::Contains";
        if (!view.Contains(dt::StandardDataFormats::Bitmap())) {
            line += "; no Bitmap format";
            co_return;
        }
        where = "DataPackageView::GetBitmapAsync";
        auto const pasted = co_await view.GetBitmapAsync();
        where = "RandomAccessStreamReference::OpenReadAsync on the pasted reference";
        auto const stream = co_await pasted.OpenReadAsync();
        where = "read the pasted stream";
        std::string detail;
        co_await inspect(stream, detail);
        line += "; " + detail;
    } catch (winrt::hresult_error const& error) {
        line += std::format("; FAIL at {}: {}", where, describe(error));
    }
}

// Положить и вставить, дважды: сразу и через полторы секунды.
wf::IAsyncAction roundtrip(Case const c, Put const put, std::string& text) {
    bool const front = bring_to_front();
    co_await pause(100ms);
    std::string report;
    bool const put_ok = co_await put_on_clipboard(c, put, report);
    for (int paste = 1; put_ok && paste <= 2; ++paste) {
        if (paste == 2) co_await pause(1500ms);
        std::string line;
        co_await paste_once(line);
        report += std::format("\npaste {}: {}", paste, line);
    }
    text = std::format("[window in front: {}] {}", front ? "yes" : "NO", report);
}

// ---------------------------------------------------------------- свой сервер на петле

// Отдаёт один и тот же JPEG на любой запрос: «доступный адрес» для сравнения с file:///, без выхода в сеть.
wf::IAsyncAction serve(wns::StreamSocketListener const& listener) {
    listener.ConnectionReceived(
        [](wns::StreamSocketListener const&, wns::StreamSocketListenerConnectionReceivedEventArgs const& args) -> winrt::fire_and_forget {
            auto const socket = args.Socket();
            try {
                wss::DataReader request{socket.InputStream()};
                request.InputStreamOptions(wss::InputStreamOptions::Partial);
                co_await request.LoadAsync(4096);  // что просили, неважно
                request.DetachStream();
                wss::DataWriter response{socket.OutputStream()};
                response.WriteString(winrt::to_hstring(std::format(
                    "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: {}\r\nConnection: close\r\n\r\n", g_image.size())));
                response.WriteBytes(g_image);
                co_await response.StoreAsync();
                co_await response.FlushAsync();
                response.DetachStream();
            } catch (...) {
            }
            socket.Close();
        });
    co_await listener.BindEndpointAsync(winrt::Windows::Networking::HostName{L"127.0.0.1"}, L"0");
    g_port = static_cast<uint16_t>(std::stoi(winrt::to_string(listener.Information().LocalPort())));
}

// ---------------------------------------------------------------- картинка и файлы

wf::IAsyncAction make_image() {
    constexpr uint32_t width = 320, height = 240;
    std::vector<uint8_t> pixels(width * height * 4);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            auto* const pixel = &pixels[(y * width + x) * 4];
            pixel[0] = static_cast<uint8_t>(x * 255 / width);
            pixel[1] = static_cast<uint8_t>(y * 255 / height);
            pixel[2] = ((x / 16 + y / 16) & 1) ? 220 : 40;
            pixel[3] = 255;
        }
    }
    wss::InMemoryRandomAccessStream memory;
    auto const encoder = co_await wgi::BitmapEncoder::CreateAsync(wgi::BitmapEncoder::JpegEncoderId(), memory);
    encoder.SetPixelData(wgi::BitmapPixelFormat::Bgra8, wgi::BitmapAlphaMode::Ignore, width, height, 96, 96, pixels);
    co_await encoder.FlushAsync();
    co_await read_all(memory, g_image);
}

// Файл с картинкой по относительному пути; путь -- с родными разделителями, как его даёт Проводник: прямые
// слэши StorageFile::GetFileFromPathAsync и FileRandomAccessStream::OpenAsync не принимают.
std::filesystem::path put_file(std::filesystem::path const& relative) {
    auto path = g_root / relative;
    path.make_preferred();
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<char const*>(g_image.data()), static_cast<std::streamsize>(g_image.size()));
    return path;
}

// ---------------------------------------------------------------- сценарий

struct Options {
    std::wstring image;   // --image файл: свой JPEG вместо сделанного пробой
    std::string put;      // --put вид: положить ссылку в буфер и держать (uri, localhost, http, stream, file, memory)
    bool flush = false;   // --flush: после Clipboard::SetContent вызвать Clipboard::Flush
    int hold_ms = 20000;  // --hold-ms N: сколько держать
    bool paste = false;   // --paste: только вставить из буфера -- для второго процесса
};

struct World {
    std::vector<Case> uris;    // CreateFromUri: десять написаний
    std::vector<Case> others;  // остальные способы
    wns::StreamSocketListener listener{nullptr};
};
World g_world;

wf::IAsyncAction prepare(std::wstring const image_argument) {
    stage("image");
    if (!image_argument.empty()) {
        std::ifstream in(image_argument, std::ios::binary);
        g_image.assign(std::istreambuf_iterator<char>(in), {});
    } else {
        co_await make_image();
    }
    say(std::format("test image: {} bytes, magic {}", g_image.size(), hex_prefix(g_image)));

    auto const plain = put_file("plain/image.jpg");
    auto const cyrillic = put_file(L"кириллица и пробел/картинка 1.jpg");
    auto const special = put_file("special/a#b+c&d.jpg");
    say("test files under " + utf8(g_root));

    stage("server");
    g_world.listener = wns::StreamSocketListener{};
    co_await serve(g_world.listener);
    auto const http = std::format("http://127.0.0.1:{}/image.jpg", g_port);
    say("loopback server: " + http);

    g_world.uris = {
        {"CreateFromUri file:/// ASCII path", Kind::Uri, widen("file:///" + utf8(plain))},
        {"CreateFromUri file://localhost/ ASCII path", Kind::Uri, widen("file://localhost/" + utf8(plain))},
        {"CreateFromUri file:/// ASCII, backslashes", Kind::Uri, L"file:///" + plain.wstring()},
        {"CreateFromUri file:/// Cyrillic+space, %XX", Kind::Uri, widen("file:///" + percent_encode(cyrillic))},
        {"CreateFromUri file:/// Cyrillic+space, raw", Kind::Uri, widen("file:///" + utf8(cyrillic))},
        {"CreateFromUri file:/// # + & in name, %XX", Kind::Uri, widen("file:///" + percent_encode(special))},
        {"CreateFromUri file:/// # + & in name, raw", Kind::Uri, widen("file:///" + utf8(special))},
        {"CreateFromUri ms-appx:///", Kind::Uri, L"ms-appx:///image.jpg"},
        {"CreateFromUri ms-appdata:///local/", Kind::Uri, L"ms-appdata:///local/image.jpg"},
        {"CreateFromUri http://127.0.0.1 (loopback)", Kind::Uri, widen(http)},
    };
    g_world.others = {
        {"CreateFromStream(FileRandomAccessStream) ASCII", Kind::Stream, plain.wstring()},
        {"CreateFromStream(FileRandomAccessStream) Cyr", Kind::Stream, cyrillic.wstring()},
        {"CreateFromStream(FileRandomAccessStream) '/'", Kind::Stream, plain.generic_wstring()},
        {"CreateFromFile(StorageFile) ASCII", Kind::File, plain.wstring()},
        {"CreateFromFile(StorageFile) Cyrillic", Kind::File, cyrillic.wstring()},
        {"CreateFromFile(StorageFile) path with '/'", Kind::File, plain.generic_wstring()},
        {"CreateFromStream(InMemoryRandomAccessStream)", Kind::Memory, L""},
        {"CreateFromStream(FileRandomAccessStream), closed", Kind::StreamClosed, plain.wstring()},
    };
}

wf::IAsyncAction matrix() {
    auto const& uris = g_world.uris;
    auto const& others = g_world.others;

    stage("uri parsing");
    say("\n== 1. Windows.Foundation.Uri: how each spelling is read ==");
    for (auto const& c : uris) {
        try {
            wf::Uri const uri{c.spec};
            row(c.label, std::format("scheme '{}' -> {}", winrt::to_string(uri.SchemeName()), winrt::to_string(uri.AbsoluteUri())));
        } catch (winrt::hresult_error const& error) {
            row(c.label, "Uri constructor FAILED " + describe(error));
        }
    }

    say("\n== 2. no clipboard: reference -> RandomAccessStreamReference::OpenReadAsync (UI thread, STA) ==");
    for (auto const* group : {&uris, &others}) {
        for (auto const& c : *group) {
            stage("direct");
            std::string text;
            co_await direct(c, text);
            row(c.label, text);
        }
    }

    say("\n== 2b. the same from a thread-pool thread (MTA), blocking .get() ==");
    for (auto const& c : {uris[0], uris[9], others[3]}) {
        stage("mta");
        std::string text;
        co_await direct_in_mta(c, text);
        row(c.label, text);
    }

    say("\n== 3. through the clipboard: SetBitmap -> put -> GetContent -> GetBitmapAsync -> OpenReadAsync, pasted twice ==");
    struct Trial {
        Case c;
        Put put;
    };
    std::vector<Trial> trials{
        {uris[0], Put::SetContent},        {uris[0], Put::SetContentFlush},   {uris[0], Put::OptionsPrivate},
        {uris[1], Put::SetContent},        {uris[3], Put::SetContent},        {uris[7], Put::SetContent},
        {uris[9], Put::SetContent},        {uris[9], Put::SetContentFlush},   {others[0], Put::SetContent},
        {others[0], Put::SetContentFlush}, {others[0], Put::OptionsPrivate},  {others[1], Put::SetContent},
        {others[3], Put::SetContent},      {others[3], Put::SetContentFlush}, {others[3], Put::OptionsPrivate},
        {others[4], Put::SetContent},      {others[6], Put::SetContent},      {others[6], Put::SetContentFlush},
        {others[7], Put::SetContent},      {others[7], Put::SetContentFlush},
    };
    if (!dt::Clipboard::IsRoamingEnabled()) {
        // Параметры по умолчанию -- как в примере; роуминг выключен, так что в облако картинка не уйдёт.
        trials.push_back({uris[0], Put::OptionsDefault});
        trials.push_back({others[0], Put::OptionsDefault});
        trials.push_back({others[3], Put::OptionsDefault});
    } else {
        say("(roaming is on: the runs with default options are skipped, so the test picture does not reach the cloud clipboard)");
    }
    for (auto const& trial : trials) {
        stage("clipboard");
        std::string text;
        co_await roundtrip(trial.c, trial.put, text);
        row(trial.c.label, std::format("via {}\n{}", name_of(trial.put), text));
    }
    dt::Clipboard::Clear();
    say("\nclipboard cleared; done");
}

// --put: ссылка в буфере, пока другой процесс читает (--paste этой же пробы или любое приложение).
wf::IAsyncAction hold(Options const options) {
    Case const* chosen = nullptr;
    if (options.put == "uri") chosen = &g_world.uris[0];
    if (options.put == "localhost") chosen = &g_world.uris[1];
    if (options.put == "http") chosen = &g_world.uris[9];
    if (options.put == "stream") chosen = &g_world.others[0];
    if (options.put == "file") chosen = &g_world.others[3];
    if (options.put == "memory") chosen = &g_world.others[6];
    if (!chosen) {
        say("--put wants one of: uri, localhost, http, stream, file, memory");
        g_exit = 1;
        co_return;
    }
    bool const front = bring_to_front();
    co_await pause(100ms);
    std::string report;
    bool const ok = co_await put_on_clipboard(*chosen, options.flush ? Put::SetContentFlush : Put::SetContent, report);
    say(std::format("[window in front: {}] {}: {}", front ? "yes" : "NO", chosen->label, report));
    if (!ok) co_return;
    say(std::format("holding the clipboard for {} ms", options.hold_ms));
    stage("hold");
    co_await pause(std::chrono::milliseconds(options.hold_ms));
    dt::Clipboard::Clear();
}

// --paste: то, что делает вставка в другом приложении, -- в чужом процессе.
wf::IAsyncAction paste_only() {
    bool const front = bring_to_front();
    co_await pause(100ms);
    std::string line;
    co_await paste_once(line);
    say(std::format("[window in front: {}] paste: {}", front ? "yes" : "NO", line));
}

winrt::fire_and_forget run(Options const options) {
    try {
        stage("environment");
        OSVERSIONINFOW version{sizeof version};
        using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
        if (auto const get = reinterpret_cast<RtlGetVersionFn>(::GetProcAddress(::GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"))) {
            get(&version);
        }
        UINT32 length = 0;
        bool const packaged = ::GetCurrentPackageFullName(&length, nullptr) != APPMODEL_ERROR_NO_PACKAGE;
        say(std::format("Windows {}.{}.{}; package identity: {}", version.dwMajorVersion, version.dwMinorVersion, version.dwBuildNumber,
                        packaged ? "yes" : "none (unpackaged)"));
        say(std::format("Clipboard::IsHistoryEnabled {}, Clipboard::IsRoamingEnabled {}", dt::Clipboard::IsHistoryEnabled(),
                        dt::Clipboard::IsRoamingEnabled()));
        if (options.paste) {
            co_await paste_only();
        } else {
            co_await prepare(options.image);
            if (options.put.empty()) {
                co_await matrix();
            } else {
                co_await hold(options);
            }
        }
    } catch (winrt::hresult_error const& error) {
        say("probe failed: " + describe(error));
        g_exit = 2;
    }
    ::DestroyWindow(g_window);
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w, LPARAM l) {
    if (message == WM_DESTROY) {
        ::PostQuitMessage(g_exit);
        return 0;
    }
    return ::DefWindowProcW(window, message, w, l);
}

}  // namespace

int main(int argc, char** argv) {
    ::SetConsoleOutputCP(CP_UTF8);
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    Options options;
    for (int i = 1; i < argc; ++i) {
        std::string_view const argument{argv[i]};
        bool const has_value = i + 1 < argc;
        if (argument == "--image" && has_value) options.image = widen(argv[++i]);
        else if (argument == "--put" && has_value) options.put = argv[++i];
        else if (argument == "--hold-ms" && has_value) options.hold_ms = std::atoi(argv[++i]);
        else if (argument == "--flush") options.flush = true;
        else if (argument == "--paste") options.paste = true;
    }

    wchar_t module[MAX_PATH];
    ::GetModuleFileNameW(nullptr, module, MAX_PATH);
    // Журнал по режимам: параллельные процессы (--put и --paste) не затирают друг друга.
    auto const log_name = options.paste ? L"stream-ref-uri-probe.paste.log" : options.put.empty() ? L"stream-ref-uri-probe.log" : L"stream-ref-uri-probe.put.log";
    g_log = _wfopen((std::filesystem::path(module).parent_path() / log_name).c_str(), L"wb");
    g_root = std::filesystem::temp_directory_path() / L"stream-ref-uri-probe";

    // Сторож: если шаг висит дольше минуты, проба сообщает, где, и выходит, а не держит буфер обмена.
    // Режим --put ждёт сам и не считается зависшим: его шаг hold длится столько, сколько попросили.
    std::thread([limit = std::chrono::milliseconds(60000 + options.hold_ms)] {
        unsigned seen = ~0u;
        for (;;) {
            std::this_thread::sleep_for(limit);
            if (g_progress == seen) {
                say(std::format("WATCHDOG: no progress for {} ms at stage '{}'", limit.count(), g_stage.load()));
                ::ExitProcess(3);
            }
            seen = g_progress;
        }
    }).detach();

    WNDCLASSW window_class{};
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = ::GetModuleHandleW(nullptr);
    window_class.lpszClassName = L"wxl.sandbox.StreamRefUriProbe";
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    ::RegisterClassW(&window_class);
    // Окно простое: нужен передний план, а не вид. Что идёт, сказано в заголовке.
    g_window = ::CreateWindowExW(0, window_class.lpszClassName, L"stream-ref-uri-probe: the clipboard checks are running, please do not touch",
                                 WS_OVERLAPPEDWINDOW, 200, 200, 720, 120, nullptr, nullptr, window_class.hInstance, nullptr);
    ::ShowWindow(g_window, SW_SHOW);
    run(options);

    MSG message;
    while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }
    return g_exit;
}
