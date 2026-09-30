// Страница CalendarView -- CalendarViewPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include "Languages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t basicHeader[] = {
#include "Snippets/CalendarView/Basic.html.embed"
};
constexpr char8_t basicCode[] = {
#include "Snippets/CalendarView/Basic.h.embed"
};

FrameworkElement basic() {
#include "Snippets/CalendarView/Basic.h"

    return gallery::controlExample({
        .header = gallery::snippet(basicHeader),
        .example = calendar,
        .options = {options},
        .code = gallery::snippet(basicCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::calendarViewPage() {
    return StackPanel {basic()};
}
