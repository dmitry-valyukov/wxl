struct Model {
    TextBlock status {textWrapping = TextWrapping::Wrap};
    ToggleSwitch monitor {header = u"Monitor ContentChanged"};
    EventToken token {};

    static std::u16string formats(DataPackageView const& package, std::u16string heading, std::u16string empty) {
        if (!package) return empty;
        auto const names = package.availableFormats();
        if (names.empty()) return empty;
        for (auto const& name : names) {
            heading += u"\n  • ";
            heading += std::u16string_view {name};
        }
        return heading;
    }
};
auto const model = gallery::hold<Model>();

model->monitor.add_onToggled([weak = std::weak_ptr<Model>(model)](auto&&...) {
    auto const model = weak.lock();
    if (!model) return;
    if (model->monitor.isOn()) {
        // The event arrives on a thread of the system's choice; the text block is written from the interface thread.
        auto const queue = DispatcherQueue::getForCurrentThread();
        model->token = Clipboard::add_onContentChanged([weak, queue](Object const&, Object const&) {
            queue.tryEnqueue([weak] {
                if (auto const model = weak.lock()) {
                    model->status.text(Model::formats(Clipboard::getContent(), u"Clipboard content changed!\nNew formats:",
                                                      u"Clipboard content changed!\nClipboard is now empty."));
                }
            });
        });
        model->status.text(u"Monitoring clipboard changes...");
    } else {
        Clipboard::remove_onContentChanged(model->token);
        model->status.text(u"Stopped monitoring clipboard changes.");
    }
});

auto example = StackPanel {
    verticalAlignment = VerticalAlignment::Top,
    spacing = 10,
    StackPanel {
        orientation.horizontal,
        spacing = 8,
        Button {
            content = u"Show Clipboard Formats",
            onClick = [model](Button const&) {
                model->status.text(Model::formats(Clipboard::getContent(), u"Available formats on the clipboard:", u"The clipboard is empty."));
            },
        },
        Button {
            content = u"Clear Clipboard",
            onClick = [model](Button const&) {
                try {
                    Clipboard::clear();
                    model->status.text(u"Clipboard has been cleared.");
                } catch (...) {
                    model->status.text(u"Error clearing clipboard.");
                }
            },
        },
    },
    model->monitor,
    model->status,
};