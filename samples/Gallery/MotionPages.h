#pragma once

// Страницы-образцы оригинала (SampleSupport/SamplePages), нужные движению: SamplePage1 и SamplePage2 --
// страницы переходов и простой связанной анимации, DetailedInfoPage -- подробная страница списка.
// У оригинала это XAML-страницы; здесь функции, которые строят их содержимое и отдают элементы, на
// которых идут связанные анимации.

#include <string>
#include <vector>

#include "CustomDataObject.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.h"

namespace gallery {

struct SamplePage1 {
    wxl::ScrollViewer root;
    wxl::Grid source;  // SourceElement: откуда летит связанная анимация вперёд
};
SamplePage1 samplePage1();

struct SamplePage2 {
    wxl::ScrollViewer root;
    wxl::Grid destination;     // DestinationElement
    wxl::StackPanel content;   // ContentPanel: появляется с EntranceThemeTransition
};
SamplePage2 samplePage2();

struct DetailedInfoPage {
    wxl::Grid root;
    wxl::Button goBack;
    wxl::Image image;            // detailedImage: связанный элемент
    wxl::StackPanel coordinated; // coordinatedPanel: идёт вместе со связанной анимацией
};
DetailedInfoPage detailedInfoPage(CustomDataObject const& object);

/// Названия всех контролов каталога по алфавиту -- ControlInfoDataSource.Instance.Groups.SelectMany(...).OrderBy(Title).
std::vector<std::u16string> sortedControlTitles();

/// Which components of a Vector3Transition animate: those checked.
inline wxl::Vector3TransitionComponents components(bool x, bool y, bool z) {
    return static_cast<wxl::Vector3TransitionComponents>((x ? 1u : 0u) | (y ? 2u : 0u) | (z ? 4u : 0u));
}

}  // namespace gallery
