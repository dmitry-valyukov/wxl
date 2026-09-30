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
