// Проба: доступная ширина контейнера как источник привязки (вместо AdaptiveTrigger по ширине окна).
// LayoutPanel с CustomLayout в функции измерения пишет available.width в поле модели и только потом измеряет ребёнка.
// Поле-ступень `wide` следует за шириной (порог 641), свойства страницы привязаны к нему через BindOutput.
//
// Вопросы:
//   1. можно ли из функции измерения менять свойства детей (через поле и привязки) до их измерения -- без ошибки цикла
//      раскладки, и встают ли значения в том же проходе (сколько раз зовётся измерение на одну смену размера);
//   2. то же для присоединённого свойства (PROBE_OPTS=attached: column у Border);
//   3. то же для структуры (PROBE_OPTS=struct: child у Border заменяется новым поддеревом).
//
// Ответы (2026-10-05): да на все три. На каждую смену размера функция измерения зовётся один раз, значения стоят в том же
// проходе, ошибки цикла раскладки и падений нет: свойство (margin), присоединённое свойство (column: Border встаёт во вторую
// колонку) и структура (child заменяется новым TextBlock) меняются из функции измерения до измерения ребёнка.
// Не проверено: бесконечная доступная ширина, перетаскивание границы окна мышью, панель внутри NavigationView.
//
// Окно ставится в 1000, 500, 900, 500; через 1,2 с после каждого шага в PROBE_LOG пишется, что видно.

#include "ui.h"
#include "Bind.h"
#include "CustomLayout.h"
#include "launch.h"
#include <wxl/Microsoft.UI.Xaml.Media.h>
#include <wxl/Microsoft.UI.Dispatching.h>

import std;

using namespace wxl;
using namespace wxl::dsl;

namespace {

std::set<std::string> opts;
bool has(char const* name) { return opts.contains(name); }

std::FILE* logFile;

struct Model {
    core::observable<double> width {0};
    core::observable<bool> wide {false};
    Model() {
        wide.follow(width, [](double w) { return w >= 641; });
    }
};

std::optional<Model> model;
std::optional<Window> mainWindow;
std::optional<DispatcherQueueTimer> timer;
std::optional<Grid> page;
std::optional<TextBlock> headerText;
std::optional<Border> options;
std::optional<Border> host;
int measures = 0;

void report(char const* when) {
    auto const x = options->transformToVisual(*page).transformPoint({0, 0}).x;
    std::fprintf(logFile, "%-30s measures %2d  page %4.0f  wide %d  header margin %2.0f  options x %4.0f", when, measures, page->actualWidth(),
                 int(model->wide.get()), headerText->margin().left, x);
    if (has("struct")) {
        auto const child = host->child();
        std::fprintf(logFile, "  host child: %s", child && child.is<TextBlock>() ? (child.try_as<TextBlock>().text() == u"wide tree" ? "wide tree" : "narrow tree") : "(none)");
    }
    std::fprintf(logFile, "\n");
    std::fflush(logFile);
    measures = 0;
}

}  // namespace

wxl::Teardown wxl_launched() {
    if (char const* const o = std::getenv("PROBE_OPTS")) {
        for (auto const word : std::string_view {o} | std::views::split(',')) opts.insert(std::string(std::string_view(word)));
    }
    char const* const path = std::getenv("PROBE_LOG");
    logFile = std::fopen(path ? path : "available-width.log", "w");
    std::fprintf(logFile, "opts: %s\n", std::getenv("PROBE_OPTS") ? std::getenv("PROBE_OPTS") : "");
    std::fflush(logFile);

    model.emplace();

    headerText = TextBlock {
        u"header",
        columnSpan = 2,
        margin = BindOutput {model->wide, [](bool wide) { return wide ? Thickness {36, 24, 36, 24} : Thickness {16, 16, 16, 16}; }},
    };
    options = Border {row = 1, width = 100, height = 20, hAlign.left, background = colors.red};
    if (has("attached")) {
        Apply {*options, column = BindOutput {model->wide, [](bool wide) { return wide ? 1 : 0; }}};
    }
    host = Border {row = 2, columnSpan = 2};
    if (has("struct")) {
        Apply {*host, child = BindOutput {model->wide, [](bool wide) { return TextBlock {wide ? u"wide tree" : u"narrow tree"}; }}};
    }
    page = Grid {columnDefinitions = u"*,*", rowDefinitions = u"auto,auto,*", *headerText, *options, *host};

    auto const widthLayout = CustomLayout {
        [](Collection<UIElement> const& children, Size available) {
            ++measures;
            // Источник: ширина, которую дал родитель. Привязки срабатывают здесь, до измерения ребёнка.
            model->width.set(available.width);
            children[0].measure(available);
            return children[0].desiredSize();
        },
        [](Collection<UIElement> const& children, Size final) {
            children[0].arrange({{0, 0}, {final.width, final.height}});
            return final;
        },
    };

    mainWindow = Window {title = u"Available width probe", content = LayoutPanel {layout = widthLayout, *page}};
    mainWindow->appWindow().resize({1000, 600});
    mainWindow->activate();

    timer = DispatcherQueue::getForCurrentThread().createTimer();
    timer->interval(std::chrono::milliseconds {1200});
    timer->add_onTick([step = 0](auto&&...) mutable {
        switch (step++) {
        case 0: report("start, window 1000"); mainWindow->appWindow().resize({500, 600}); break;
        case 1: report("window 500 (expect narrow)"); mainWindow->appWindow().resize({900, 600}); break;
        case 2: report("window 900 (expect wide)"); mainWindow->appWindow().resize({500, 600}); break;
        case 3: report("window 500 again (narrow)"); break;
        case 4:
            // Таймер повторяется: после закрытия окна и файла он больше не должен ничего трогать.
            timer->stop();
            std::fclose(logFile);
            mainWindow->close();
            break;
        default: break;
        }
    });
    timer->isRepeating(true);
    timer->start();
    return {};
}
