// Страница CalendarDatePicker -- CalendarDatePickerPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t headerPlaceholderHeader[] = {
#include "Snippets/CalendarDatePicker/HeaderPlaceholder.html.embed"
};
constexpr char8_t headerPlaceholderCode[] = {
#include "Snippets/CalendarDatePicker/HeaderPlaceholder.h.embed"
};

FrameworkElement headerPlaceholder() {
    return gallery::controlExample({
        .header = gallery::snippet(headerPlaceholderHeader),
        .example =
#include "Snippets/CalendarDatePicker/HeaderPlaceholder.h"
        ,
        .code = gallery::snippet(headerPlaceholderCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::calendarDatePickerPage() {
    return StackPanel {headerPlaceholder()};
}
