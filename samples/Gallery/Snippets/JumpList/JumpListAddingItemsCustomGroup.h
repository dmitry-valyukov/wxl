auto const addGroup = [](Button const&) -> async::detached_task {
    if (!is_packaged()) co_return;

    auto jumpList = co_await JumpList::loadCurrentAsync();

    auto alpha = JumpListItem::createWithArguments(u"/project-alpha", u"Project Alpha");
    alpha.groupName(u"Projects");
    alpha.description(u"Open Project Alpha");
    alpha.logo(Uri {u"ms-appx:///Assets/Tiles/AppList.targetsize-48.png"});

    auto beta = JumpListItem::createWithArguments(u"/project-beta", u"Project Beta");
    beta.groupName(u"Projects");
    beta.description(u"Open Project Beta");
    beta.logo(Uri {u"ms-appx:///Assets/Tiles/AppList.targetsize-48.png"});

    jumpList.items().append(alpha);
    jumpList.items().append(beta);

    co_await jumpList.saveAsync();
};

auto example = StackPanel {
    spacing = 8,
    TextBlock {textWrapping = TextWrapping::Wrap,
               u"Custom groups let you organize jump list items into named sections. Set the GroupName property to a non-empty string and "
               u"all items sharing the same GroupName will appear together under that heading. This is useful for grouping related items "
               u"like recent projects, pinned documents, or user-defined categories."},
    Button {content = u"Add custom group items", onClick = addGroup},
};