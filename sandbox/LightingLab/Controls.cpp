// Строки панели настроек: подпись, значение и контрол, привязанный к полю модели.

#include "Lab.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

// До двух знаков: шаг ползунков не мельче сотой.
hstring number(double value) {
    return hstring {core::to_u16(value, std::chars_format::fixed, 2)};
}

}  // namespace

FrameworkElement lab::sliderRow(char16_t const* title, core::observable<double>& field, double low, double high,
                                double step) {
    return Grid {
        columnDefinitions = u"*,auto",
        rowDefinitions = u"auto,auto",
        TextBlock {title},
        TextBlock {column = 1, opacity = 0.65, text = BindOutput {field, [](double value) { return number(value); }}},
        Slider {
            row = 1,
            columnSpan = 2,
            automationName = title,
            minimum = low,
            maximum = high,
            stepFrequency = step,
            smallChange = step,
            value = Bind {field},
        },
    };
}

FrameworkElement lab::choiceRow(char16_t const* title, core::observable<int>& field,
                                std::span<char16_t const* const> names) {
    auto box = ComboBox {header = title, hAlign.stretch};
    for (auto const name : names) {
        box.items().append(ComboBoxItem {content = name});
    }
    // После пунктов: выбранный номер у пустого списка не держится.
    Apply {box, selectedIndex = Bind {field}};
    return box;
}

FrameworkElement lab::toggleRow(char16_t const* title, core::observable<bool>& field) {
    return ToggleSwitch {header = title, isOn = Bind {field}};
}

FrameworkElement lab::colorRow(char16_t const* title, core::observable<Color>& field) {
    return Grid {
        columnDefinitions = u"*,auto",
        TextBlock {title, vAlign.center},
        SplitButton {
            column = 1,
            automationName = title,
            content = Border {
                width = 48,
                height = 24,
                Margin {-11, -6},
                CornerRadius {4, 0, 0, 4},
                background = BindOutput {field, [](Color value) { return SolidColorBrush {color = value}; }},
            },
            flyout = Flyout {
                placement = FlyoutPlacementMode::Bottom,
                content = ColorPicker {
                    color = Bind {field},
                    colorSpectrumShape = ColorSpectrumShape::Ring,
                    isAlphaEnabled = false,
                    isColorChannelTextInputVisible = false,
                    isHexInputVisible = true,
                    isMoreButtonVisible = false,
                },
            },
        },
    };
}

FrameworkElement lab::noteRow(char16_t const* words) {
    return TextBlock {words, textWrapping = TextWrapping::Wrap, opacity = 0.65};
}

FrameworkElement lab::statusRow(core::observable<hstring>& field) {
    return TextBlock {text = BindOutput {field}, textWrapping = TextWrapping::Wrap, isTextSelectionEnabled = true};
}

FrameworkElement lab::group(char16_t const* title, bool open, std::initializer_list<FrameworkElement> rows) {
    auto panel = StackPanel {spacing = 10.0};
    for (auto const& entry : rows) {
        panel.children().append(entry);
    }
    return Expander {
        hAlign.stretch,
        horizontalContentAlignment = HorizontalAlignment::Stretch,
        header = title,
        isExpanded = open,
        content = panel,
    };
}

FrameworkElement lab::settingsPanel(std::initializer_list<FrameworkElement> groups) {
    auto panel = StackPanel {spacing = 8.0, Margin {12}};
    for (auto const& entry : groups) {
        panel.children().append(entry);
    }
    return panel;
}

hstring lab::accepted(char16_t const* what) {
    std::u16string line {what};
    line += u": принято";
    return hstring {line};
}

hstring lab::refusal(char16_t const* what, std::int32_t code, char16_t const* why) {
    std::u16string line {what};
    line += u": отказ 0x";
    core::append_number(line, static_cast<std::uint32_t>(code), 16);
    if (*why != 0) {
        line += u", ";
        line += why;
    }
    return hstring {line};
}

hstring lab::refusal(char16_t const* what, char16_t const* why) {
    std::u16string line {what};
    line += u": отказ, ";
    line += why;
    return hstring {line};
}
