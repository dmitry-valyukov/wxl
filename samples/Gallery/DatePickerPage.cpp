// Страница DatePicker -- DatePickerPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t withHeaderHeader[] = {
#include "Snippets/DatePicker/Header.html.embed"
};
constexpr char8_t withHeaderCode[] = {
#include "Snippets/DatePicker/Header.h.embed"
};

FrameworkElement withHeader() {
    return gallery::controlExample({
        .header = gallery::snippet(withHeaderHeader),
        .example =
#include "Snippets/DatePicker/Header.h"
        ,
        .code = gallery::snippet(withHeaderCode),
    });
}

constexpr char8_t dayFormattedYearHeader[] = {
#include "Snippets/DatePicker/DayFormattedYear.html.embed"
};
constexpr char8_t dayFormattedYearCode[] = {
#include "Snippets/DatePicker/DayFormattedYear.h.embed"
};

FrameworkElement dayFormattedYear() {
#include "Snippets/DatePicker/DayFormattedYear.h"

    return gallery::controlExample({
        .header = gallery::snippet(dayFormattedYearHeader),
        .example = picker,
        .code = gallery::snippet(dayFormattedYearCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::datePickerPage() {
    return StackPanel {withHeader(), dayFormattedYear()};
}
