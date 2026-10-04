// TargetNullValue of XAML is the function: what the control shows while the field holds nothing.
struct Model {
    core::observable<core::u16_text> user;
};
auto const model = gallery::hold<Model>();

auto example = StackPanel {
    spacing = 8.0,
    TextBox {width = 300, header = u"User name (leave empty for none)", text = Bind {model->user}},
    TextBlock {text = BindOutput {model->user, [](core::u16_text const& name) {
                                      return name.size() == 0 ? core::u16_text {u"Anonymous User"} : name;
                                  }}},
};
