#pragma once

// Страницы, которыми образцы навигации (Samples/NavigationView, SelectorBar, TabView) заполняют свои рамки: у
// оригинала `Frame.Navigate(typeof(SamplePage1))`, здесь страница -- Page самой платформы, а её содержимое
// строит `samplePage`.

#include "Navigation.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Media.Animation.h"

namespace gallery {

/// SamplePage<number> оригинала в рамке; переход рамки по умолчанию.
void showSample(wxl::Frame const& frame, int number);

/// То же с переходом, который выбрал элемент навигации (RecommendedNavigationTransitionInfo) или образец.
void showSample(wxl::Frame const& frame, int number, wxl::NavigationTransitionInfo const& info);

/// SampleSettingsPage оригинала.
void showSettings(wxl::Frame const& frame);

/// Номер в конце тега "SamplePage3" -- то, что оригинал берёт `Substring(Length - 1)`.
int sampleNumber(wxl::Object const& tag);

/// Название страницы-образца для шапки навигации: "Sample Page 3".
std::u16string sampleHeader(int number);

/// The context menu of the tabs: "Move tab left", "Move tab right" -- Page.Resources.TabViewContextMenu of the original,
/// which fills itself in each time it opens (TabViewHelper.PopulateTabViewContextMenu).
wxl::MenuFlyout tabViewContextMenu();

/// The filling of the menu: what the tab it opens on can be moved to.
void populateTabViewContextMenu(wxl::MenuFlyout const& flyout);

}  // namespace gallery
