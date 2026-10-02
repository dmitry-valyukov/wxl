struct Model {
    ComboBox soundBox {
        header = u"AppNotificationSoundEvent",
        width = 180,
        ComboBoxItem {content = u"Default"},
        ComboBoxItem {content = u"IM"},
        ComboBoxItem {content = u"Reminder"},
        ComboBoxItem {content = u"SMS"},
        ComboBoxItem {content = u"Alarm"},
        ComboBoxItem {content = u"Call"},
        selectedIndex = 0,
    };

    AppNotificationSoundEvent sound() const {
        static constexpr AppNotificationSoundEvent sounds[] = {
            AppNotificationSoundEvent::Default, AppNotificationSoundEvent::IM,    AppNotificationSoundEvent::Reminder,
            AppNotificationSoundEvent::SMS,     AppNotificationSoundEvent::Alarm, AppNotificationSoundEvent::Call};
        return sounds[std::clamp(soundBox.selectedIndex(), 0, 5)];
    }
};
auto const model = gallery::hold<Model>();

// The shell reads a picture of a file: address.
std::u16string logo = u"file:///" + (applicationFolder() / L"Assets" / L"ControlImages" / L"PersonPicture.png").generic_u16string();

auto example = Button {
    content = u"Show informational notification with logo and custom audio",
    onClick = [model, logo](Button const&) {
        auto const notification = AppNotificationBuilder {}
                                      .addText(u"Control Highlight: PersonPicture")
                                      .addText(u"Use the PersonPicture control to display user avatars with initials or images.")
                                      .setAppLogoOverride(Uri {logo}, AppNotificationImageCrop::Circle)
                                      .setAudioEvent(model->sound())
                                      .setTimeStamp(std::chrono::time_point_cast<DateTime::duration>(DateTime::clock::now()))
                                      .buildNotification();

        AppNotificationManager::default_().show(notification);
    },
};

auto options = model->soundBox;