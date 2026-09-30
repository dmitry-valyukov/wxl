// Страница TimePicker -- TimePickerPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t simpleHeader[] = {
#include "Snippets/TimePicker/Simple.html.embed"
};
constexpr char8_t simpleCode[] = {
#include "Snippets/TimePicker/Simple.h.embed"
};

FrameworkElement simple() {
    return gallery::controlExample({
        .header = gallery::snippet(simpleHeader),
        .example =
#include "Snippets/TimePicker/Simple.h"
        ,
        .code = gallery::snippet(simpleCode),
    });
}

constexpr char8_t minuteIncrementsHeader[] = {
#include "Snippets/TimePicker/HeaderMinuteIncrements.html.embed"
};
constexpr char8_t minuteIncrementsCode[] = {
#include "Snippets/TimePicker/HeaderMinuteIncrements.h.embed"
};

FrameworkElement minuteIncrements() {
    return gallery::controlExample({
        .header = gallery::snippet(minuteIncrementsHeader),
        .example =
#include "Snippets/TimePicker/HeaderMinuteIncrements.h"
        ,
        .code = gallery::snippet(minuteIncrementsCode),
    });
}

constexpr char8_t clockIdentifiersHeader[] = {
#include "Snippets/TimePicker/ClockIdentifiers.html.embed"
};
constexpr char8_t clockIdentifiersCode[] = {
#include "Snippets/TimePicker/ClockIdentifiers.h.embed"
};

FrameworkElement clockIdentifiers() {
#include "Snippets/TimePicker/ClockIdentifiers.h"

    return gallery::controlExample({
        .header = gallery::snippet(clockIdentifiersHeader),
        .example = pickers,
        .code = gallery::snippet(clockIdentifiersCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::timePickerPage() {
    return StackPanel {simple(), minuteIncrements(), clockIdentifiers()};
}
