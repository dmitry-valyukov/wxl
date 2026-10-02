auto example = Button {
    content = u"Show notification with progress bar",
    onClick = [](Button const&) {
        auto const notification = AppNotificationBuilder {}
                                      .addText(u"Progress Bar Example")
                                      .addText(u"This is a sample notification showing how to use a progress bar.")
                                      .addProgressBar(AppNotificationProgressBar {
                                          title = u"Demo Progress",
                                          value = 0.6,  // 60%
                                          valueStringOverride = u"60%",
                                          status = u"In progress...",
                                      })
                                      .buildNotification();

        AppNotificationManager::default_().show(notification);
    },
};