// The theme is read when the window opens, and a change made while it is open goes
// straight to its title bar.
struct Model {
    Button showButton {content = u"Show window"};
    ComboBox themeOption {
        width = 200,
        header = u"TitleBarTheme",
        ComboBoxItem {content = u"Legacy"},
        ComboBoxItem {content = u"UseDefaultAppMode"},
        ComboBoxItem {content = u"Light"},
        ComboBoxItem {content = u"Dark"},
        selectedIndex = 1,
    };
    std::shared_ptr<Window> window;
    std::weak_ptr<Model> self;

    TitleBarTheme theme() const {
        static constexpr TitleBarTheme themes[] = {
            TitleBarTheme::Legacy, TitleBarTheme::UseDefaultAppMode, TitleBarTheme::Light, TitleBarTheme::Dark};
        return themes[std::max(themeOption.selectedIndex(), 0)];
    }

    void open() {
        showButton.isEnabled(false);
        window = std::make_shared<Window>(Window {
            systemBackdrop = MicaBackdrop {},
            content = Grid {
                TextBlock {
                    u"This is a sample window to demonstrate AppWindowTitleBar theme customization.",
                    hAlign.center,
                    vAlign.center,
                    textWrapping = TextWrapping::Wrap,
                    Margin {20},
                    fontSize = 16,
                },
            },
        });
        window->appWindow().titleBar().preferredTheme(theme());
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
model->themeOption.add_onSelectionChanged([weak = model->self](auto&&...) {
    if (auto const model = weak.lock(); model && model->window) {
        model->window->appWindow().titleBar().preferredTheme(model->theme());
    }
});

auto example = model->showButton;

auto options = model->themeOption;