// Проба на обёртках wxl: те же шаги, что у sandbox/StreamRefUriProbe (голый cppwinrt), но звать их приходится
// через wxl::RandomAccessStreamReference, wxl::Operation, wxl::DataPackage и wxl::Clipboard -- так, как их зовёт
// пример Gallery. Если голый cppwinrt читает ссылку, а обёртка нет, дефект в обёртке; если ответы совпадают
// до HRESULT, обёртка ничего не добавляет.
//
// Проверяется:
//   2w. без буфера обмена: createFromUri / createFromFile -> openReadAsync -> BitmapImage::setSourceAsync
//       (поток разбирается как картинка);
//   3w. через буфер обмена: DataPackage::setBitmap -> Clipboard::setContent (setContentWithOptions с параметрами
//       по умолчанию, как в примере; setContent + flush) -> Clipboard::getContent -> DataPackageView::getBitmapAsync
//       -> RandomAccessStreamReference::openReadAsync -> BitmapImage::setSourceAsync, два раза подряд.
// На каждом шаге -- где оборвалось и HRESULT исключения, которое Operation отдаёт на co_await.
//
// Clipboard в классическом приложении работает у окна переднего плана: проба берёт его (поиск окна по
// заголовку, AttachThreadInput) перед каждой проверкой. Сценарий затирает содержимое буфера обмена.
//
// Журнал -- stream-ref-uri-wxl-probe.log рядом с исполняемым файлом. Картинка, файлы и сервер на петле --
// на cppwinrt, в server.cpp: это не предмет пробы.

#include "platform.h"

#include <cstdio>
#include <fstream>
#include <thread>

#include "pch.h"

#include "Packaged.h"
#include "event_awaitable.h"
#include "generated/Microsoft.UI.Dispatching.h"
#include "generated/Microsoft.UI.Xaml.Media.Imaging.h"
#include "generated/Windows.ApplicationModel.DataTransfer.h"
#include "generated/Windows.Storage.Streams.h"
#include "generated/Windows.Storage.h"
#include "server.h"

import wxl.async;

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr wchar_t windowTitle[] = L"stream-ref-uri-wxl-probe";
constexpr std::size_t labelWidth = 48;  // ширина подписи строки результата

std::FILE* logFile = nullptr;

void say(std::string const& line) {
    if (!logFile) return;
    std::fputs(line.c_str(), logFile);
    std::fputc('\n', logFile);
    std::fflush(logFile);
}

// Строка результата: подпись слева, итог справа; продолжения итога -- под ним, с отступом на подпись.
void printRow(std::string const& label, std::string const& text) {
    std::string aligned;
    for (char c : text) {
        aligned += c;
        if (c == '\n') aligned.append(labelWidth + 1, ' ');
    }
    say(std::format("{:<{}} {}", label, labelWidth, aligned));
}

std::string utf8(std::u16string_view text) {
    if (text.empty()) return {};
    int const size = ::WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<wchar_t const*>(text.data()), static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(size), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<wchar_t const*>(text.data()), static_cast<int>(text.size()), result.data(), size,
                          nullptr, nullptr);
    return result;
}

// Передний план: окно берётся по заголовку, а право на него -- присоединением к очереди ввода нынешнего переднего окна.
bool bringToFront() {
    HWND const window = ::FindWindowW(nullptr, windowTitle);
    if (!window) return false;
    if (::GetForegroundWindow() == window) return true;
    HWND const current = ::GetForegroundWindow();
    DWORD const currentThread = current ? ::GetWindowThreadProcessId(current, nullptr) : 0;
    DWORD const thisThread = ::GetCurrentThreadId();
    bool const attach = currentThread && currentThread != thisThread;
    if (attach) ::AttachThreadInput(thisThread, currentThread, TRUE);
    ::ShowWindow(window, SW_SHOW);
    ::SetForegroundWindow(window);
    ::BringWindowToTop(window);
    if (attach) ::AttachThreadInput(thisThread, currentThread, FALSE);
    return ::GetForegroundWindow() == window;
}

enum class Put { SetContent, SetContentFlush, OptionsDefault };

constexpr char const* nameOf(Put put) {
    switch (put) {
    case Put::SetContent: return "Clipboard::setContent";
    case Put::SetContentFlush: return "Clipboard::setContent + Clipboard::flush";
    case Put::OptionsDefault: return "Clipboard::setContentWithOptions(ClipboardContentOptions {})";
    }
    return "";
}

struct Case {
    std::string label;
    bool file;            // createFromFile(StorageFile::getFileFromPathAsync(spec)); иначе createFromUri(Uri {spec})
    std::u16string spec;  // адрес или путь
};

struct Trial {
    Case what;
    Put put;
};

async::detached_task run(Window window, std::vector<Case> cases, std::vector<Trial> trials) {
    auto const timer = DispatcherQueue::getForCurrentThread().createTimer();
    timer.isRepeating(false);

    say("\n== 2w. no clipboard: wxl::RandomAccessStreamReference -> openReadAsync -> BitmapImage::setSourceAsync ==");
    for (auto const& c : cases) {
        std::string text;
        char const* where = "";
        try {
            where = "create the reference";
            auto const reference = c.file ? RandomAccessStreamReference::createFromFile(co_await StorageFile::getFileFromPathAsync(c.spec))
                                          : RandomAccessStreamReference::createFromUri(Uri {c.spec});
            where = "RandomAccessStreamReference::openReadAsync";
            auto const stream = co_await reference.openReadAsync();
            where = "BitmapImage::setSourceAsync";
            auto const picture = BitmapImage {};
            co_await picture.setSourceAsync(stream);
            stream.close();
            text = "OK   opened and decoded as an image";
        } catch (...) {
            text = std::format("FAIL at {}: {}", where, probe::describe(std::current_exception()));
        }
        printRow(c.label, text);
    }

    say("\n== 3w. through the clipboard: setBitmap -> put -> getContent -> getBitmapAsync -> openReadAsync -> setSourceAsync, pasted twice ==");
    for (auto const& trial : trials) {
        std::string report = std::format("via {}\n[window in front: {}] ", nameOf(trial.put), bringToFront() ? "yes" : "NO");
        char const* where = "";
        bool put = false;
        try {
            where = "Clipboard::clear";
            Clipboard::clear();
            where = "create the reference";
            auto const reference = trial.what.file
                                       ? RandomAccessStreamReference::createFromFile(co_await StorageFile::getFileFromPathAsync(trial.what.spec))
                                       : RandomAccessStreamReference::createFromUri(Uri {trial.what.spec});
            where = "DataPackage::setBitmap";
            auto const package = DataPackage {};
            package.setBitmap(reference);
            where = nameOf(trial.put);
            if (trial.put == Put::OptionsDefault) {
                report += Clipboard::setContentWithOptions(package, ClipboardContentOptions {}) ? "set ok" : "set returned false";
            } else {
                Clipboard::setContent(package);
                report += "set ok";
            }
            if (trial.put == Put::SetContentFlush) {
                where = "Clipboard::flush";
                Clipboard::flush();
                report += ", flush ok";
            }
            put = true;
        } catch (...) {
            report += std::format("FAIL at {}: {}", where, probe::describe(std::current_exception()));
        }

        for (int paste = 1; put && paste <= 2; ++paste) {
            if (paste == 2) {
                timer.interval(std::chrono::milliseconds {1500});
                timer.start();
                co_await onTick(timer);
            }
            std::string line;
            try {
                where = "Clipboard::getContent";
                auto const view = Clipboard::getContent();
                where = "DataPackageView::availableFormats";
                std::string formats;
                for (auto const& format : view.availableFormats()) formats += (formats.empty() ? "" : ", ") + utf8(format);
                line = std::format("formats [{}]", formats);
                where = "DataPackageView::contains";
                if (!view.contains(StandardDataFormats::bitmap())) {
                    line += "; no Bitmap format";
                } else {
                    where = "DataPackageView::getBitmapAsync";
                    auto const pasted = co_await view.getBitmapAsync();
                    where = "RandomAccessStreamReference::openReadAsync on the pasted reference";
                    auto const stream = co_await pasted.openReadAsync();
                    where = "BitmapImage::setSourceAsync";
                    auto const picture = BitmapImage {};
                    co_await picture.setSourceAsync(stream);
                    stream.close();
                    line += "; opened and decoded as an image";
                }
            } catch (...) {
                line += std::format("; FAIL at {}: {}", where, probe::describe(std::current_exception()));
            }
            report += std::format("\npaste {}: {}", paste, line);
        }
        printRow(trial.what.label, report);
    }

    Clipboard::clear();
    say("\nclipboard cleared; done");
    window.close();
}

}  // namespace

wxl::Teardown wxl_launched() {
    wchar_t module[MAX_PATH];
    ::GetModuleFileNameW(nullptr, module, MAX_PATH);
    logFile = _wfopen((std::filesystem::path(module).parent_path() / L"stream-ref-uri-wxl-probe.log").c_str(), L"wb");

    // Сторож: проба, которая не кончила за три минуты, зависла -- говорит об этом и выходит.
    std::thread([] {
        std::this_thread::sleep_for(std::chrono::minutes(3));
        say("WATCHDOG: the probe has not finished in 3 minutes");
        ::ExitProcess(3);
    }).detach();

    say(std::format("wxl::is_packaged(): {}", is_packaged() ? "yes" : "no"));
    say(std::format("Clipboard::isHistoryEnabled {}, Clipboard::isRoamingEnabled {}", Clipboard::isHistoryEnabled(),
                    Clipboard::isRoamingEnabled()));

    auto const picture = probe::makeImage();
    auto const port = probe::serve(picture);
    auto const root = std::filesystem::temp_directory_path() / L"stream-ref-uri-wxl-probe";
    auto path = root / "plain/image.jpg";
    path.make_preferred();
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<char const*>(picture.data()), static_cast<std::streamsize>(picture.size()));
    auto const generic = path.generic_u16string();
    auto const native = path.u16string();
    auto const portText = std::to_string(port);
    auto const http = u"http://127.0.0.1:" + std::u16string(portText.begin(), portText.end()) + u"/image.jpg";
    say(std::format("test image {} bytes; file {}; loopback server port {}", picture.size(), utf8(generic), port));

    // Подписи те же, что у голой пробы: строки сравниваются глазами.
    std::vector<Case> const cases{
        {"CreateFromUri file:/// ASCII path", false, u"file:///" + generic},
        {"CreateFromUri file://localhost/ ASCII path", false, u"file://localhost/" + generic},
        {"CreateFromUri ms-appx:///", false, u"ms-appx:///image.jpg"},
        {"CreateFromUri http://127.0.0.1 (loopback)", false, http},
        {"CreateFromFile(StorageFile) ASCII", true, native},
        {"CreateFromFile(StorageFile) path with '/'", true, generic},
    };
    std::vector<Trial> const trials{
        {cases[0], Put::OptionsDefault}, {cases[0], Put::SetContent}, {cases[0], Put::SetContentFlush},
        {cases[2], Put::SetContent},     {cases[3], Put::SetContent}, {cases[3], Put::SetContentFlush},
        {cases[4], Put::OptionsDefault}, {cases[4], Put::SetContent}, {cases[4], Put::SetContentFlush},
    };

    auto window = Window {
        title = L"stream-ref-uri-wxl-probe",
        TextBlock {
            L"stream-ref-uri-wxl-probe: the clipboard checks are running, please do not touch this window",
            Margin {16},
            textWrapping.wrap,
        },
    };
    window.appWindow().resize({720, 160});
    window.activate();
    run(window, cases, trials);
    return {};
}
