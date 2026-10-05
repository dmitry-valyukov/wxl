static constexpr const char16_t* fonts[] = {
    u"Arial", u"Comic Sans MS", u"Courier New", u"Segoe UI", u"Times New Roman",
};

struct Model {
    core::observable<int> font {2};
};
auto const model = gallery::hold<Model>();

auto output = TextBlock {
    u"You can set the font used for this text.",
    fontFamily = BindOutput {model->font, [](int index) { return fonts[std::max(index, 0)]; }},
    styles.TextBlock.Body,
    Margin {0, 8, 0, 0},
};

auto combo = ComboBox {
    minWidth = 200,
    header = u"Font",
    ComboBoxItem {content = fonts[0]},
    ComboBoxItem {content = fonts[1]},
    ComboBoxItem {content = fonts[2]},
    ComboBoxItem {content = fonts[3]},
    ComboBoxItem {content = fonts[4]},
    selectedIndex = Bind {model->font},
};