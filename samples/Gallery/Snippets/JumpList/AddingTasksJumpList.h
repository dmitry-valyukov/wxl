// A jump list belongs to a packaged application: unpackaged, there is nothing to load or save.
auto const addTasks = [](Button const&) -> async::detached_task {
    if (!is_packaged()) co_return;

    auto jumpList = co_await JumpList::loadCurrentAsync();

    auto composeTask = JumpListItem::createWithArguments(u"/compose", u"New Message");
    composeTask.description(u"Compose a new message");
    composeTask.logo(Uri {u"ms-appx:///Assets/Tiles/AppList.targetsize-48.png"});

    auto searchTask = JumpListItem::createWithArguments(u"/search", u"Search");
    searchTask.description(u"Search for items");
    searchTask.logo(Uri {u"ms-appx:///Assets/Tiles/AppList.targetsize-48.png"});

    jumpList.items().append(composeTask);
    jumpList.items().append(searchTask);

    co_await jumpList.saveAsync();
};

auto const clearTasks = [](Button const&) -> async::detached_task {
    if (!is_packaged()) co_return;

    auto jumpList = co_await JumpList::loadCurrentAsync();
    jumpList.items().clear();
    co_await jumpList.saveAsync();
};

auto example = StackPanel {
    spacing = 8,
    TextBlock {textWrapping = TextWrapping::Wrap,
               u"Tasks are items with an empty GroupName. They appear in the built-in 'Tasks' section at the bottom of the jump list. "
               u"Use tasks for common app-wide actions that are always relevant, such as composing a new message or opening settings. "
               u"Each task launches the app with a specific argument string that your app can handle on startup."},
    StackPanel {
        orientation.horizontal,
        spacing = 8,
        Button {styles.Button.Accent, content = u"Add sample tasks", onClick = addTasks},
        Button {content = u"Clear all items", onClick = clearTasks},
    },
};