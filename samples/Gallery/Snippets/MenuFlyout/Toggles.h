// The two items flip themselves when chosen; the fields keep what they show.
struct Model {
    core::observable<bool> repeat;
    core::observable<bool> shuffle;
};
auto const model = gallery::hold<Model>();

auto button = Button {
    content = u"Options",
    flyout = MenuFlyout {
        MenuFlyoutItem {text = u"Reset"},
        MenuFlyoutSeparator {},
        ToggleMenuFlyoutItem {text = u"Repeat", isChecked = Bind {model->repeat}},
        ToggleMenuFlyoutItem {text = u"Shuffle", isChecked = Bind {model->shuffle}},
    },
};