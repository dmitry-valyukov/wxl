// The saves under way are kept by the page's model, and go with it.
struct Model {
    async::task<> adding;
    async::task<> clearing;
};
auto const model = gallery::hold<Model>();

// A jump list belongs to a packaged application: unpackaged, there is nothing to load or save.
auto const addTasks = []() -> async::task<> {
    if (!Application::isPackaged()) co_return;

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

auto const clearTasks = []() -> async::task<> {
    if (!Application::isPackaged()) co_return;

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
        Button {styles.Button.Accent, content = u"Add sample tasks", onClick += [model, addTasks] { model->adding = addTasks(); }},
        Button {content = u"Clear all items", onClick += [model, clearTasks] { model->clearing = clearTasks(); }},
    },
};