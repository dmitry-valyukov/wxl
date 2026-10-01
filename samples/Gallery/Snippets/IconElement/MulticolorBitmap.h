struct Model {
    core::observable<bool> monochrome{false};
};
auto const model = gallery::hold<Model>();

auto example = StackPanel {
    TextBlock {
        Margin {0, 0, 0, 12},
        u"The ShowAsMonochrome property (true by default) will result in a solid block of the foreground color "
        u"if the property is set to true and the icon is more than one color. This behavior can be ignored by "
        u"setting the ShowAsMonochrome property to false.",
        styles.TextBlock.Body,
    },
    BitmapIcon {
        width = 50,
        hAlign.left,
        showAsMonochrome = BindOutput {model->monochrome},
        uriSource = u"Assets/SampleMedia/Slices.png",
    },
};

auto options = CheckBox {content = u"Monochrome", isChecked = Bind {model->monochrome}};