auto output = TextBlock {};

auto item = [output](char16_t const* caption) {
    return MenuFlyoutItem {
        text = caption,
        onClick = [output, caption](MenuFlyoutItem const&) { output.text(std::u16string {u"You clicked: "} + caption); },
    };
};

// A radio item belongs to the group it names; one of a group is checked at a time.
auto radio = [output](char16_t const* caption, char16_t const* group, bool checked) {
    return RadioMenuFlyoutItem {
        text = caption,
        groupName = group,
        isChecked = checked,
        onClick = [output, caption](MenuFlyoutItem const&) { output.text(std::u16string {u"You clicked: "} + caption); },
    };
};

auto bar = MenuBar {
    MenuBarItem {
        title = u"File",
        MenuFlyoutSubItem {
            text = u"New",
            item(u"Plain Text Document"),
            item(u"Rich Text Document"),
            item(u"Other Formats"),
        },
        item(u"Open"),
        item(u"Save"),
        MenuFlyoutSeparator {},
        item(u"Exit"),
    },
    MenuBarItem {title = u"Edit", item(u"Undo"), item(u"Cut"), item(u"Copy"), item(u"Paste")},
    MenuBarItem {
        title = u"View",
        item(u"Output"),
        MenuFlyoutSeparator {},
        radio(u"Landscape", u"OrientationGroup", false),
        radio(u"Portrait", u"OrientationGroup", true),
        MenuFlyoutSeparator {},
        radio(u"Small icons", u"SizeGroup", false),
        radio(u"Medium icons", u"SizeGroup", true),
        radio(u"Large icons", u"SizeGroup", false),
    },
    MenuBarItem {title = u"Help", item(u"About")},
};