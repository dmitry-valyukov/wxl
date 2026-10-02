// Страница RadioButton — RadioButtonPage оригинала: два примера.
//
// Строки `x:String` оригинала — элементы RadioButton: коллекция Items держит
// объекты, а строку из неё wxl не собирает.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t groupHeader[] = {
#include "Snippets/RadioButton/Group.html.embed"
};
constexpr char8_t groupCode[] = {
#include "Snippets/RadioButton/Group.h.embed"
};

FrameworkElement groupExample() {
#include "Snippets/RadioButton/Group.h"

    return gallery::controlExample({
        .header = gallery::snippet(groupHeader),
        .example = StackPanel {group},
        .output = {output},
        .code = gallery::snippet(groupCode),
    });
}

constexpr char8_t stringsHeader[] = {
#include "Snippets/RadioButton/Strings.html.embed"
};
constexpr char8_t stringsCode[] = {
#include "Snippets/RadioButton/Strings.h.embed"
};

FrameworkElement stringsExample() {
#include "Snippets/RadioButton/Strings.h"

    return gallery::controlExample({
        .header = gallery::snippet(stringsHeader),
        .example = StackPanel {background, border, sample},
        .code = gallery::snippet(stringsCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::radioButtonPage() {
    return StackPanel {groupExample(), stringsExample()};
}