// The badge is the shell's, on the taskbar button of a packaged application: this page changes it
// only when the application is packaged, and the bar above says why it is not otherwise.
struct Model {
    NumberBox countBox {
        header = u"Badge count",
        width = 160,
        value = 5.0,
        minimum = 1.0,
        spinButtonPlacementMode = NumberBoxSpinButtonPlacementMode::Inline,
        smallChange = 1.0,
        largeChange = 10.0,
    };
    bool badgeSet = false;

    uint32_t count() const { return static_cast<uint32_t>(countBox.value()); }
};
auto const model = gallery::hold<Model>();
// A change of the count is shown at once, once a badge has been set.
model->countBox.add_onValueChanged([weak = std::weak_ptr<Model>(model)](auto&&...) {
    if (auto const model = weak.lock(); model && model->badgeSet && is_packaged()) {
        BadgeNotificationManager::current().setBadgeAsCount(model->count());
    }
});

auto example = StackPanel {
    spacing = 8,
    Button {
        content = u"Set badge as count",
        width = 160,
        onClick = [model](Button const&) {
            if (is_packaged()) {
                BadgeNotificationManager::current().setBadgeAsCount(model->count());
                model->badgeSet = true;
            }
        },
    },
    Button {
        content = u"Clear badge",
        width = 160,
        onClick = [](Button const&) {
            if (is_packaged()) {
                BadgeNotificationManager::current().clearBadge();
            }
        },
    },
};

auto options = model->countBox;