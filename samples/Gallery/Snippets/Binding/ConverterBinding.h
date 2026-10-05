// A converter of XAML is a function written next to the binding: BindOutput{field, fn} shows fn of the field, any
// result the property takes.
struct Model {
    core::observable<core::u16_text> input;
};
auto const model = gallery::hold<Model>();

auto example = StackPanel {
    spacing = 8.0,
    TextBox {width = 300, header = u"Enter Text:", text = Bind {model->input}},
    TextBlock {u"The input is not empty.",
               visibility = BindOutput {model->input, [](core::u16_text const& text) {
                                            return text.size() == 0 ? Visibility::Collapsed : Visibility::Visible;
                                        }}},
};
