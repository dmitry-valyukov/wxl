// The two options are read when the window opens, and a change made while it is open
// goes straight to its title bar.
struct Model {
    Button showButton {content = u"Show window"};
    CheckBox extendContent {Margin {0, 0, 0, 12}, content = u"Extend content into title bar", isChecked = true};
    ComboBox heightOption {
        width = 200,
        header = u"TitleBarHeightOption",
        ComboBoxItem {content = u"Standard"},
        ComboBoxItem {content = u"Tall"},
        ComboBoxItem {content = u"Collapsed"},
        selectedIndex = 0,
    };
    std::shared_ptr<Window> window;
    std::weak_ptr<Model> self;

    TitleBarHeightOption height() const {
        static constexpr TitleBarHeightOption options[] = {
            TitleBarHeightOption::Standard, TitleBarHeightOption::Tall, TitleBarHeightOption::Collapsed};
        return options[std::max(heightOption.selectedIndex(), 0)];
    }

    void open() {
        showButton.isEnabled(false);
        window = std::make_shared<Window>(Window {
            systemBackdrop = MicaBackdrop {},
            content = Grid {
                TextBlock {
                    u"This is a sample window to demonstrate content extending into the title bar area and title bar "
                    u"height options.",
                    hAlign.center,
                    vAlign.center,
                    textWrapping = TextWrapping::Wrap,
                    Margin {20},
                    fontSize = 16,
                },
            },
        });
        auto titleBar = window->appWindow().titleBar();
        titleBar.extendsContentIntoTitleBar(extendContent.isChecked().value_or(false));
        if (titleBar.extendsContentIntoTitleBar()) {
            titleBar.preferredHeightOption(height());
        }
        window->add_onClosed([weak = self](auto&&...) {
            if (auto const model = weak.lock()) {
                model->showButton.isEnabled(true);
                model->window.reset();
            }
        });
        gallery::trackWindow(*window);
        window->activate();
    }
};
auto const model = gallery::hold<Model>();
model->self = model;
model->showButton.add_onClick([weak = model->self](auto&&...) {
    if (auto const model = weak.lock()) {
        model->open();
    }
});
model->extendContent.add_onChecked([weak = model->self](auto&&...) {
    if (auto const model = weak.lock(); model && model->window) {
        model->window->appWindow().titleBar().extendsContentIntoTitleBar(true);
    }
});
model->extendContent.add_onUnchecked([weak = model->self](auto&&...) {
    if (auto const model = weak.lock(); model && model->window) {
        model->window->appWindow().titleBar().extendsContentIntoTitleBar(false);
    }
});
model->heightOption.add_onSelectionChanged([weak = model->self](auto&&...) {
    if (auto const model = weak.lock(); model && model->window) {
        auto titleBar = model->window->appWindow().titleBar();
        if (titleBar.extendsContentIntoTitleBar()) {
            titleBar.preferredHeightOption(model->height());
        }
    }
});

auto example = model->showButton;

auto options = StackPanel {spacing = 8, model->extendContent, model->heightOption};