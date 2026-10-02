std::u16string hero = u"file:///" + (applicationFolder() / L"Assets" / L"SampleMedia" / L"LandscapeImage5.jpg").generic_u16string();

auto example = Button {
    content = u"Show visual notification with hero image and attribution",
    onClick = [hero](Button const&) {
        auto const notification = AppNotificationBuilder {}
                                      .addText(u"Harbor Scene with Boats")
                                      .addText(u"A quiet harbor with boats gently anchored in view.")
                                      .setHeroImage(Uri {hero})
                                      .setAttributionText(u"WinUI gallery assets")
                                      .buildNotification();

        AppNotificationManager::default_().show(notification);
    },
};