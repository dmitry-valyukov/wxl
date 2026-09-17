// The eyeball viewer for the markup family: the sample markup from
// wxl.html/sample, shown by an HtmlBlock -- and, by the BB/RSDN buttons,
// its siblings BbBlock and RsdnBlock over the same MarkupBlock. A button
// appends a chunk -- the chat-feed motion the spec is built around -- and
// any .html dropped on the window is loaded in place of the samples.

// Standard headers the wxl headers do not carry come first: pch.h imports
// wxl.core, and a header after that import must already have been seen.
#include <fstream>

#include "pch.h"

import wxl.core;

using namespace wxl;
using namespace wxl::dsl;

namespace {

// A sample file as wide text: bytes in, checked UTF-8, transcoded once.
// A file that is not UTF-8 answers with the reason on screen instead of a
// page -- this is a viewer, showing is what it does.
std::wstring load(std::wstring const& path) {
    std::ifstream file(std::filesystem::path(path), std::ios::binary);
    if (!file) return L"Не открылся файл: " + path;

    std::string bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    std::string_view view = bytes;
    if (view.starts_with("\xEF\xBB\xBF")) view.remove_prefix(3);

    const std::optional<core::u8_view> checked = core::unicode::checked(view);
    if (!checked) return L"Файл не в UTF-8: " + path;
    return std::wstring{checked->to_utf16().wchars()};
}

// Что случилось, по-русски: подпись рода ошибки для строки состояния.
std::wstring_view error_name(HtmlErrorKind kind) {
    switch (kind) {
    case HtmlErrorKind::UnknownTag: return L"незнакомый тег";
    case HtmlErrorKind::UnpairedClose: return L"непарный закрывающий";
    case HtmlErrorKind::MisnestedTags: return L"перепутанная вложенность";
    case HtmlErrorKind::BadEntity: return L"кривая сущность";
    case HtmlErrorKind::MisplacedTag: return L"тег не на своём месте";
    case HtmlErrorKind::UnknownStyle: return L"незнакомый стиль";
    case HtmlErrorKind::BadColor: return L"кривой цвет";
    case HtmlErrorKind::BadFontSize: return L"кривой кегль";
    case HtmlErrorKind::BadDimension: return L"кривой размер";
    case HtmlErrorKind::MissingImage: return L"картинка без источника";
    case HtmlErrorKind::RemoteImage: return L"удалённая картинка отброшена";
    case HtmlErrorKind::ImageFailed: return L"картинка не загрузилась";
    case HtmlErrorKind::BlockedLink: return L"клик по ссылке погашен";
    }
    return L"?";
}

}  // namespace

wxl::Teardown wxl_launched() {
    auto view = HtmlBlock {
        isTextSelectionEnabled = true,
        Margin {16, 8},
    };
    auto forum = BbBlock {
        isTextSelectionEnabled = true,
        Margin {16, 8},
    };
    auto post = RsdnBlock {
        isTextSelectionEnabled = true,
        Margin {16, 8},
    };

    // The named styles the appended chunk below exercises; foreign markup
    // whose style="..." holds real CSS finds nothing here and stays plain.
    view.registerStyles({
        {L"warn", {.text = {.color = ARGB{0xFFC0392B}, .bold = true}}},
        {L"note", {.text = {.color = ARGB{0xFF808080}, .italic = true}}},
    });

    auto status = TextBlock {
        L"Ссылки кликабельны; файл .html можно бросить в окно.",
        row = 1,
        Margin {16, 4},
        styles.TextBlock.Caption,
    };

    // Обвязка одна на всё семейство: ссылки и диагностика — в строку
    // состояния (последнее восстановление видно; их может быть несколько).
    const std::wstring assets = std::filesystem::absolute(L"Assets").wstring();
    const auto wire = [status, &assets](auto const& block) {
        block.onLink([status](std::wstring_view target) {
            status.text(L"Клик по ссылке: " + std::wstring{target});
        });
        block.onError([status](HtmlError const& error) {
            std::wstring line{L"Ошибка разметки: "};
            line += error_name(error.kind);
            if (!error.detail.empty()) {
                line += L" («";
                line += error.detail;
                line += L"»)";
            }
            status.text(line);
        });
        // Относительные картинки разметки — из папки её файла.
        block.baseDirectory(assets);
    };
    wire(view);
    wire(forum);
    wire(post);

    view.html(load(L"Assets/first-tier.html"));

    auto scroll = ScrollViewer {
        row = 2,
        content = view,
    };

    // Перенос текста — свойство самого RichTextBlock; без него линейка,
    // таблица и раскрывашка держат естественную ширину, как и строки.
    const auto set_wrapping = [view, forum, post](TextWrapping mode) {
        view.textWrapping(mode);
        forum.textWrapping(mode);
        post.textWrapping(mode);
    };
    auto wrap = CheckBox {
        content = L"Перенос текста",
        toolTip = L"Без переноса линейка, таблица и раскрывашка держат естественную ширину",
        onChecked = [set_wrapping](Object const&, RoutedEventArgs&) {
            set_wrapping(TextWrapping::Wrap);
        },
        onUnchecked = [set_wrapping](Object const&, RoutedEventArgs&) {
            set_wrapping(TextWrapping::NoWrap);
        },
    };
    wrap.isChecked(true);

    auto bar = StackPanel {
        row = 0,
        orientation.horizontal,
        Margin {12, 8},
        spacing = 8.0,
        Button {
            content = L"Первая очередь",
            onClick = [view, scroll, assets](Object const&, RoutedEventArgs&) {
                view.baseDirectory(assets);
                view.html(load(L"Assets/first-tier.html"));
                scroll.content(view);
            },
        },
        Button {
            content = L"Вторая очередь",
            toolTip = L"Линейка, таблица и раскрывающийся блок",
            onClick = [view, scroll, assets](Object const&, RoutedEventArgs&) {
                view.baseDirectory(assets);
                view.html(load(L"Assets/second-tier.html"));
                scroll.content(view);
            },
        },
        Button {
            content = L"Мусор",
            onClick = [view, scroll, assets](Object const&, RoutedEventArgs&) {
                view.baseDirectory(assets);
                view.html(load(L"Assets/garbage.html"));
                scroll.content(view);
            },
        },
        Button {
            content = L"BB",
            toolTip = L"Тот же образец BB-кодом: BbBlock над общим MarkupBlock",
            onClick = [forum, scroll](Object const&, RoutedEventArgs&) {
                forum.bb(load(L"Assets/forum.bb.txt"));
                scroll.content(forum);
            },
        },
        Button {
            content = L"RSDN",
            toolTip = L"Разметка RSDN: построчные цитаты, смайлы, подсветка кода, таблицы",
            onClick = [post, scroll](Object const&, RoutedEventArgs&) {
                post.rsdn(load(L"Assets/forum.rsdn.txt"));
                scroll.content(post);
            },
        },
        Button {
            content = L"Дописать",
            toolTip = L"Дописывание куском, как в ленте чата",
            onClick = [view, scroll](Object const&, RoutedEventArgs&) {
                view.append(L"<p style=\"warn\">Дописанный кусок.</p>"
                            L"<p style=\"note\">И ещё один, <b>стилем по имени"
                            L"</b> из реестра. Незакрытый <i>курсив");
                scroll.content(view);
            },
        },
        wrap,
    };

    auto window = Window {
        title = L"wxl.html — смотрелка",
        minSize = {480, 360},
        Grid {
            rowDefinitions = L"auto,auto,*",
            background = brushes.SolidBackgroundFillColor.Base,
            bar,
            status,
            scroll,
        },
    };

    accept_file_drops(window, [view, scroll, status](std::vector<std::wstring> const& paths) {
        if (paths.empty()) return;
        // Картинки брошенного файла — из его же папки.
        view.baseDirectory(std::filesystem::path(paths.front()).parent_path().wstring());
        view.html(load(paths.front()));
        scroll.content(view);
        status.text(L"Загружен: " + paths.front());
    });

    auto appWindow = window.appWindow();
    appWindow.resize({860, 720});
    window.activate();

    return [window](TeardownReason) {};
}
