// An item that changes in part while it is shown is an item with fields of its own, held by pointer. Its element binds
// to them -- here the note under each title -- and a change of the field reaches that one line: the row is not built
// again. The bindings an element makes go when the list gives the element back, so the field never keeps a row the list
// no longer shows. The selection is a field too: `selectedIndex = Bind {field}`.
struct Task : core::sta_refcounted {
    explicit Task(core::u16_view name) : title(name) {}

    core::u16_text const title;
    core::observable<core::u16_text> note {core::u16_text {u"Not started"}};
};

struct Model {
    core::observable_list<core::intrusive_ptr<Task>> tasks;
    core::observable<int> selected {0};
    core::observable<core::u16_text> draft;

    Model() {
        tasks.push_back(core::intrusive_ptr<Task> {new Task {u"Write the plan"}, false});
        tasks.push_back(core::intrusive_ptr<Task> {new Task {u"Review the code"}, false});
        tasks.push_back(core::intrusive_ptr<Task> {new Task {u"Ship it"}, false});
        draft.set(tasks[0]->note.get());

        // The box shows the note of the chosen task, and what is typed into it is that note, as it is typed.
        selected.on_change([this](int at) noexcept {
            if (Task* const task = chosen(at)) draft.set(task->note.get());
        });
        draft.on_change([this](core::u16_text const& text) noexcept {
            if (Task* const task = chosen(selected.get())) task->note.set(text);
        });
    }

    Task* chosen(int at) const {
        if (at < 0 || static_cast<uint32_t>(at) >= tasks.size()) return nullptr;
        return tasks[static_cast<uint32_t>(at)].get();
    }
};
auto const model = gallery::hold<Model>();

auto example = Grid {
    columnSpacing = 16.0,
    columnDefinitions = u"260,*",
    ListView {
        height = 220,
        borderBrush = brushes.Card.StrokeColorDefault,
        BorderThickness {1},
        cornerRadius = CornerRadius {4},
        itemsSource = BindOutput {model->tasks, [](core::intrusive_ptr<Task> const& task) {
            return StackPanel {Padding {4}, spacing = 2.0, TextBlock {FontWeight {600}, text = task->title},
                               TextBlock {foreground = brushes.Text.FillColor.Secondary, styles.TextBlock.Caption,
                                          text = BindOutput {task->note}}};
        }},
        selectedIndex = Bind {model->selected},
    },
    StackPanel {
        column = 1,
        spacing = 8.0,
        TextBlock {u"The note of the selected task:"},
        TextBox {placeholderText = u"Type a note", text = Bind {model->draft}},
    },
};
