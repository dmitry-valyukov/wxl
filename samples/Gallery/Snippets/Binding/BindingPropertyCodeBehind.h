struct Model {
    core::observable<core::u16_text> greeting {core::u16_text {u"Hello, WinUI 3!"}};
};
auto const model = gallery::hold<Model>();
// The model outlives the controls and the controls hold no copy of it: a handler takes the field by address.
auto* const greeting = &model->greeting;

auto example = StackPanel {
    spacing = 8.0,
    TextBlock {hAlign.center, vAlign.center, fontSize = 24, text = BindOutput {model->greeting}},
    Button {hAlign.center, content = u"Say it again", onClick = [greeting](auto&&...) { greeting->set(core::u16_text {u"Hello again!"}); }},
};
