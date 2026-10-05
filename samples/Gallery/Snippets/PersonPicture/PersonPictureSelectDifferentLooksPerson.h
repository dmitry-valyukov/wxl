struct Model {
    core::observable<int> look {0};
};
auto const model = gallery::hold<Model>();

auto const picture = PersonPicture {
    height = 300,
    vAlign.top,
    profilePicture = BindOutput {model->look, [](int look) -> ImageSource {
        if (look == 0) {
            return BitmapImage {uriSource = u"https://learn.microsoft.com/windows/uwp/contacts-and-calendar/images/shoulder-tap-static-payload.png"};
        }
        return ImageSource {};
    }},
    displayName = BindOutput {model->look, [](int look) { return look == 1 ? u"Jane Doe" : u""; }},
    initials = BindOutput {model->look, [](int look) { return look == 2 ? u"SB" : u""; }},
};

auto example = picture;
auto options = RadioButtons {
    selectedIndex = Bind {model->look},
    header = u"Profile type",
    RadioButton {content = u"Profile Image", isChecked = true},
    RadioButton {content = u"Display Name"},
    RadioButton {content = u"Initials"},
};
