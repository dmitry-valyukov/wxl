auto const sheet = InkCanvas {automationName = u"Drawing canvas"};
auto const presenter = sheet.inkPresenter();

// The devices that draw, and what a new stroke looks like, are written to the presenter as the options change.
struct Model {
    CheckBox pen {automationName = u"Pen input", content = u"Pen", isChecked = true};
    CheckBox mouse {automationName = u"Mouse input", content = u"Mouse", isChecked = true};
    CheckBox touch {automationName = u"Touch input", content = u"Touch", isChecked = true};
    ComboBox color {horizontalAlignment = HorizontalAlignment::Stretch, automationName = u"Ink color", header = u"Ink color",
                    ComboBoxItem {content = u"Black"}, ComboBoxItem {content = u"Red"}, ComboBoxItem {content = u"Blue"}, selectedIndex = 0};
    Slider width {automationName = u"Stroke width", header = u"Stroke width", maximum = 12, minimum = 1, stepFrequency = 1, value = 4};
    ToggleSwitch enabled {automationName = u"Inking enabled", header = u"Inking enabled", isOn = true};
};
auto const model = gallery::hold<Model>();

auto const applyDevices = [model, presenter](auto&&...) {
    int types = 0;
    types |= model->pen.isChecked().value_or(false) ? static_cast<int>(CoreInputDeviceTypes::Pen) : 0;
    types |= model->mouse.isChecked().value_or(false) ? static_cast<int>(CoreInputDeviceTypes::Mouse) : 0;
    types |= model->touch.isChecked().value_or(false) ? static_cast<int>(CoreInputDeviceTypes::Touch) : 0;
    presenter.inputDeviceTypes(static_cast<CoreInputDeviceTypes>(types));
};
auto const applyAttributes = [model, presenter](auto&&...) {
    constexpr Color inks[] = {colors.black, colors.red, colors.blue};
    auto attributes = presenter.copyDefaultDrawingAttributes();
    attributes.color(inks[std::max(model->color.selectedIndex(), 0)]);
    auto const size = static_cast<float>(model->width.value());
    attributes.size(Size {size, size});
    presenter.updateDefaultDrawingAttributes(attributes);
};
for (auto const& box : {model->pen, model->mouse, model->touch}) {
    box.add_onChecked(applyDevices);
    box.add_onUnchecked(applyDevices);
}
model->color.add_onSelectionChanged(applyAttributes);
model->width.add_onValueChanged(applyAttributes);
model->enabled.add_onToggled([model, presenter](auto&&...) { presenter.isInputEnabled(model->enabled.isOn()); });
applyDevices();
applyAttributes();
presenter.isInputEnabled(model->enabled.isOn());

auto example = Border {height = 280, maxWidth = 600, hAlign.stretch, background = colors.white,
                       borderBrush = brushes.SystemControl.Foreground.Chrome.High, BorderThickness {1}, sheet};
auto options = StackPanel {
    width = 240,
    spacing = 8,
    TextBlock {fontWeight = FontWeight {600}, u"Input devices"},
    StackPanel {spacing = 4, model->pen, model->mouse, model->touch},
    model->color,
    model->width,
    model->enabled,
    Button {automationName = u"Clear drawing", content = u"Clear drawing", onClick = [presenter](auto&&...) { presenter.strokeContainer().clear(); }},
};