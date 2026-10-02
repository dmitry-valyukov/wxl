auto example = Button {
    content = u"Show notification with AppNotification controls",
    onClick = [](Button const&) {
        auto const notification =
            AppNotificationBuilder {}
                .addText(u"Survey")
                .addText(u"Please select your satisfaction level and leave a comment.")
                .addComboBox(AppNotificationComboBox {u"satisfaction"}
                                 .addItem(u"1", u"Very Bad")
                                 .addItem(u"2", u"Bad")
                                 .addItem(u"3", u"Neutral")
                                 .addItem(u"4", u"Good")
                                 .addItem(u"5", u"Excellent")
                                 .setSelectedItem(u"3"))
                .addTextBox(u"comment", u"Leave a comment here...", u"")
                .addButton(AppNotificationButton {u"Submit"}.addArgument(u"action", u"submit_survey"))
                .buildNotification();

        AppNotificationManager::default_().show(notification);
    },
};