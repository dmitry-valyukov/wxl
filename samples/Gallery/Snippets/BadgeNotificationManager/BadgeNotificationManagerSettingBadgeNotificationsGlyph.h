struct Model {
    ComboBox glyphBox {
        header = u"BadgeNotificationGlyph",
        width = 160,
        ComboBoxItem {content = u"None"},
        ComboBoxItem {content = u"Activity"},
        ComboBoxItem {content = u"Alarm"},
        ComboBoxItem {content = u"Alert"},
        ComboBoxItem {content = u"Attention"},
        ComboBoxItem {content = u"Available"},
        ComboBoxItem {content = u"Away"},
        ComboBoxItem {content = u"Busy"},
        ComboBoxItem {content = u"Error"},
        ComboBoxItem {content = u"NewMessage"},
        ComboBoxItem {content = u"Paused"},
        ComboBoxItem {content = u"Playing"},
        ComboBoxItem {content = u"Unavailable"},
        selectedIndex = 1,
    };
    bool badgeSet = false;

    // The glyphs are numbered in the order of the list: None is 0, Unavailable is 12.
    BadgeNotificationGlyph selectedGlyph() const {
        return static_cast<BadgeNotificationGlyph>(std::clamp(glyphBox.selectedIndex(), 0, 12));
    }
};
auto const model = gallery::hold<Model>();
model->glyphBox.add_onSelectionChanged([weak = std::weak_ptr<Model>(model)](auto&&...) {
    if (auto const model = weak.lock(); model && model->badgeSet && is_packaged()) {
        BadgeNotificationManager::current().setBadgeAsGlyph(model->selectedGlyph());
    }
});

auto example = StackPanel {
    spacing = 8,
    Button {
        content = u"Set badge glyph",
        width = 160,
        onClick = [model](Button const&) {
            if (is_packaged()) {
                BadgeNotificationManager::current().setBadgeAsGlyph(model->selectedGlyph());
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

auto options = model->glyphBox;