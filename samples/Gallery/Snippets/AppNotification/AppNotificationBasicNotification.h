// The notification is built by a chain of calls on the builder, each of which returns the builder,
// and handed to the manager of the application, which registered with the shell when the application
// started (as App.OnLaunched does in the original).
auto example = Button {
    content = u"Show notification",
    onClick = [](Button const&) {
        auto const notification = AppNotificationBuilder {}
                                      .addText(u"Welcome to WinUI 3 Gallery")
                                      .addText(u"Explore interactive samples and discover the power of modern Windows UI.")
                                      .buildNotification();

        AppNotificationManager::default_().show(notification);
    },
};