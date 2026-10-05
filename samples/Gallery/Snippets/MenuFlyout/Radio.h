auto button = Button {
    content = u"Options",
    flyout = MenuFlyout {
        RadioMenuFlyoutItem {text = u"Landscape", groupName = u"OrientationGroup"},
        RadioMenuFlyoutItem {text = u"Portrait", groupName = u"OrientationGroup", isChecked = true},
        MenuFlyoutSeparator {},
        RadioMenuFlyoutItem {text = u"Small icons", groupName = u"SizeGroup"},
        RadioMenuFlyoutItem {text = u"Medium icons", isChecked = true, groupName = u"SizeGroup"},
        RadioMenuFlyoutItem {text = u"Large icons", groupName = u"SizeGroup"},
    },
};