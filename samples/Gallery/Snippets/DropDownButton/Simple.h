StackPanel {
    orientation.horizontal,
    DropDownButton {
        content = u"Email",
        flyout = MenuFlyout {
            placement = FlyoutPlacementMode::BottomEdgeAlignedLeft,
            MenuFlyoutItem {text = u"Send"},
            MenuFlyoutItem {text = u"Reply"},
            MenuFlyoutItem {text = u"Reply All"},
        },
    },
}