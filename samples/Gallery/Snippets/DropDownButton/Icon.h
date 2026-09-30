StackPanel {
    orientation.horizontal,
    DropDownButton {
        content = FontIcon {glyph = u"\uE715"},
        flyout = MenuFlyout {
            placement = FlyoutPlacementMode::BottomEdgeAlignedLeft,
            MenuFlyoutItem {text = u"Send", icon = FontIcon {glyph = u"\uE725"}},
            MenuFlyoutItem {text = u"Reply", icon = FontIcon {glyph = u"\uE8CA"}},
            MenuFlyoutItem {text = u"Reply All", icon = FontIcon {glyph = u"\uE8C2"}},
        },
    },
}