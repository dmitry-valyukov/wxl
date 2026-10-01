// Какие контролы перенесены: идентификатор из каталога и функция, строящая
// страницу примеров. Строка сюда — единственное, что нужно добавить, когда
// перенесена очередная страница; в навигации и на плитках контрол
// включается сам.

#include "Pages.h"

#include <iterator>

namespace {

struct Entry {
    std::wstring_view id;
    gallery::ControlPage page;
};

constexpr Entry ported[] = {
    {L"Button", &gallery::buttonPage},
    {L"DropDownButton", &gallery::dropDownButtonPage},
    {L"HyperlinkButton", &gallery::hyperlinkButtonPage},
    {L"RepeatButton", &gallery::repeatButtonPage},
    {L"ToggleButton", &gallery::toggleButtonPage},
    {L"SplitButton", &gallery::splitButtonPage},
    {L"ToggleSplitButton", &gallery::toggleSplitButtonPage},
    {L"CheckBox", &gallery::checkBoxPage},
    {L"ColorPicker", &gallery::colorPickerPage},
    {L"ComboBox", &gallery::comboBoxPage},
    {L"RadioButton", &gallery::radioButtonPage},
    {L"RatingControl", &gallery::ratingControlPage},
    {L"Slider", &gallery::sliderPage},
    {L"ToggleSwitch", &gallery::toggleSwitchPage},
    {L"TextBlock", &gallery::textBlockPage},
    {L"TextBox", &gallery::textBoxPage},
    {L"PasswordBox", &gallery::passwordBoxPage},
    {L"NumberBox", &gallery::numberBoxPage},
    {L"AutoSuggestBox", &gallery::autoSuggestBoxPage},
    {L"RichTextBlock", &gallery::richTextBlockPage},
    {L"RichEditBox", &gallery::richEditBoxPage},
    {L"Border", &gallery::borderPage},
    {L"Canvas", &gallery::canvasPage},
    {L"Expander", &gallery::expanderPage},
    {L"Grid", &gallery::gridPage},
    {L"RelativePanel", &gallery::relativePanelPage},
    {L"SplitView", &gallery::splitViewPage},
    {L"StackPanel", &gallery::stackPanelPage},
    {L"VariableSizedWrapGrid", &gallery::variableSizedWrapGridPage},
    {L"Viewbox", &gallery::viewboxPage},
    {L"ScrollView", &gallery::scrollViewPage},
    {L"AnnotatedScrollBar", &gallery::annotatedScrollBarPage},
    {L"PipsPager", &gallery::pipsPagerPage},
    // PagerControl не включён: уже пустой `PagerControl {}` роняет XAML (0xC000027B) на
    // установленном рантайме 2.5.1. Причина не установлена; контрол числится
    // экспериментальным. Страница написана (PagerControlPage.cpp) и собирается; строка
    // {L"PagerControl", &gallery::pagerControlPage} включается, когда экземпляр заработает.
    {L"ScrollViewer", &gallery::scrollViewerPage},
    {L"CalendarView", &gallery::calendarViewPage},
    {L"TimePicker", &gallery::timePickerPage},
    {L"DatePicker", &gallery::datePickerPage},
    {L"CalendarDatePicker", &gallery::calendarDatePickerPage},
    {L"StandardUICommand", &gallery::standardUICommandPage},
    {L"XamlUICommand", &gallery::xamlUICommandPage},
    {L"SwipeControl", &gallery::swipeControlPage},
    {L"CommandBarFlyout", &gallery::commandBarFlyoutPage},
    {L"MenuFlyout", &gallery::menuFlyoutPage},
    {L"MenuBar", &gallery::menuBarPage},
    {L"CommandBar", &gallery::commandBarPage},
    {L"LayoutPanel", &gallery::layoutPanelPage},
    {L"WrapPanel", &gallery::wrapPanelPage},
    {L"AppBarSeparator", &gallery::appBarSeparatorPage},
    {L"AppBarToggleButton", &gallery::appBarToggleButtonPage},
    {L"AppBarButton", &gallery::appBarButtonPage},
    {L"ToolTip", &gallery::toolTipPage},
    {L"ProgressRing", &gallery::progressRingPage},
    {L"ProgressBar", &gallery::progressBarPage},
    {L"InfoBar", &gallery::infoBarPage},
    {L"InfoBadge", &gallery::infoBadgePage},
};

}  // namespace

gallery::ControlPage gallery::pageFor(std::wstring_view uniqueId) {
    for (auto const& entry : ported) {
        if (entry.id == uniqueId) {
            return entry.page;
        }
    }
    return nullptr;
}
