auto const picture = PersonPicture {height = 300, vAlign.top};

auto const show = [picture](int look) {
    if (look == 0) {
        picture.profilePicture(BitmapImage {uriSource = u"https://learn.microsoft.com/windows/uwp/contacts-and-calendar/images/shoulder-tap-static-payload.png"});
        picture.displayName(u"");
        picture.initials(u"");
    } else if (look == 1) {
        picture.profilePicture(ImageSource {});
        picture.displayName(u"Jane Doe");
        picture.initials(u"");
    } else {
        picture.profilePicture(ImageSource {});
        picture.displayName(u"");
        picture.initials(u"SB");
    }
};
show(0);

auto example = picture;
auto options = RadioButtons {
    selectedIndex = 0,
    header = u"Profile type",
    RadioButton {content = u"Profile Image", isChecked = true},
    RadioButton {content = u"Display Name"},
    RadioButton {content = u"Initials"},
    onSelectionChanged = [show](RadioButtons const& self) { show(self.selectedIndex()); },
};