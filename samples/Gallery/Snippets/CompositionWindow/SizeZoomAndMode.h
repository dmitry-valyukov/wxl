// What the window knows about its size and what it lets the application say about it: the client
// area in pixels with the scale it is divided by, the largest area the screen allows, the zoom of
// the whole island, the full-screen mode, the dark frame and the colours of the caption.
struct Model {
    TextBlock sizeText {textWrapping = TextWrapping::Wrap, u"The window is closed."};
    Slider zoomSlider {header = u"zoomFactor, %", minimum = 50.0, maximum = 200.0, stepFrequency = 5.0, value = 100.0};
    ToggleSwitch fullScreenSwitch {header = u"fullScreen", isOn = false};
    ToggleSwitch darkSwitch {header = u"darkFrame", isOn = false};
    core::observable<Color> captionColour {rgb(0xf3, 0xf3, 0xf3)};
    core::observable<Color> captionText {rgb(0x20, 0x20, 0x20)};
    std::shared_ptr<CompositionWindow> window;
    std::weak_ptr<Model> self;

    static void describe(TextBlock const& target, SizeInt32 client, float scale) {
        std::u16string text = u"Client area ";
        text += core::to_u16(client.width).plain();
        text += u" x ";
        text += core::to_u16(client.height).plain();
        text += u" px, scale ";
        text += core::to_u16(scale, std::chars_format::fixed, 2).plain();
        target.text(text);
    }

    void open() {
        window = std::make_shared<CompositionWindow>(CompositionWindow {
            title = u"Size, zoom and mode",
            minSize = {480, 320},
            StackPanel {
                Margin {24},
                spacing = 8,
                TextBlock {u"The zoom enlarges the whole island, the way a browser zooms a page."},
            },
        });
        window->background(rgb(0xf3, 0xf3, 0xf3));
        // Called straight from WM_SIZE: before XAML starts to lay out, so it is the place to
        // rebuild a layout for a new width. A change of the zoom calls it too.
        window->onClientSizeChanged([weak = self](SizeInt32 client, float scale) {
            if (auto const model = weak.lock()) {
                describe(model->sizeText, client, scale);
            }
        });
        window->add_onClosed([weak = self](auto&&...) {
            if (auto const model = weak.lock()) {
                model->window.reset();
                model->sizeText.text(u"The window is closed.");
            }
        });
        window->centreWithClientSize({960, 600});
        describe(sizeText, window->clientSize(), window->rasterizationScale());
        gallery::trackWindow(*window);
        window->activate();
    }
};
auto const model = gallery::hold<Model>();
model->self = model;
model->zoomSlider.add_onValueChanged([weak = std::weak_ptr<Model>(model)](auto&&...) {
    if (auto const model = weak.lock(); model && model->window) {
        model->window->zoomFactor(model->zoomSlider.value() / 100);
    }
});
model->fullScreenSwitch.add_onToggled([weak = std::weak_ptr<Model>(model)](auto&&...) {
    if (auto const model = weak.lock(); model && model->window) {
        model->window->fullScreen(model->fullScreenSwitch.isOn());
    }
});
model->darkSwitch.add_onToggled([weak = std::weak_ptr<Model>(model)](auto&&...) {
    if (auto const model = weak.lock(); model && model->window) {
        model->window->darkFrame(model->darkSwitch.isOn());
    }
});
auto const paintCaption = [weak = std::weak_ptr<Model>(model)](auto&&...) noexcept {
    if (auto const model = weak.lock(); model && model->window) {
        model->window->captionColor(model->captionColour.get(), model->captionText.get());
    }
};
model->captionColour.on_change(paintCaption);
model->captionText.on_change(paintCaption);

auto example = StackPanel {
    spacing = 8,
    Button {content = u"Show window", onClick = [model](Button const&) { model->open(); }},
    model->sizeText,
    StackPanel {
        orientation.horizontal,
        spacing = 8,
        Button {
            content = u"Centre, 960 x 600",
            onClick = [model](Button const&) {
                if (model->window) {
                    model->window->centreWithClientSize({960, 600});
                }
            },
        },
        Button {
            content = u"The largest the screen allows",
            onClick = [model](Button const&) {
                // The answer is in the units it is given back in: no work area, no frame width to know.
                if (model->window) {
                    model->window->centreWithClientSize(model->window->maxClientSize());
                }
            },
        },
    },
};

auto options = StackPanel {
    spacing = 8,
    width = 260,
    model->zoomSlider,
    model->fullScreenSwitch,
    model->darkSwitch,
    TextBlock {u"Caption colour"},
    gallery::colorSelector(model->captionColour, u"Caption colour"),
    TextBlock {u"Caption text"},
    gallery::colorSelector(model->captionText, u"Caption text"),
};
