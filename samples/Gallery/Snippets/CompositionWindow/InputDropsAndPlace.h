// Input from the scene, a file thrown into the window, and the remembered place. A window with no
// XAML island (hideContent) is a scene alone, and its input is split: the pointer is taken from
// the island's input site, the keyboard from the window procedure; the window brings both to
// events that carry the wrappers XAML hands out.
struct Model {
    TextBlock keyText {u"Key: --"};
    TextBlock pointerText {u"Pointer: --"};
    TextBlock wheelText {u"Wheel: --"};
    TextBlock dropText {u"Dropped: --"};
    TextBlock placeText {textWrapping = TextWrapping::Wrap, u"Place: --"};
    std::shared_ptr<CompositionWindow> window;
    std::shared_ptr<std::u16string> remembered = std::make_shared<std::u16string>();
    std::weak_ptr<Model> self;

    void open() {
        window = std::make_shared<CompositionWindow>(CompositionWindow {
            title = u"Input of the scene",
            minSize = {480, 320},
        });
        window->background(rgb(0x14, 0x1c, 0x33));
        if (!remembered->empty()) {
            // The place of the last time: the monitors are checked, a string that no monitor
            // fits is not applied.
            window->placement(*remembered);
        }

        // A square that follows the pointer, takes the colour of a key and the size of the wheel.
        auto const compositor = window->compositor();
        auto const square = compositor.createSpriteVisual();
        square.size({80.0f, 80.0f});
        square.anchorPoint({0.5f, 0.5f});
        square.brush(compositor.createColorBrush(rgb(0x3b, 0x82, 0xf6)));
        window->contentVisual().children().insertAtTop(square);
        window->hideContent();

        window->onKeyDown([weak = self, square, compositor](VirtualKey key) {
            if (auto const model = weak.lock()) {
                std::u16string text = u"Key: ";
                text += core::to_u16(static_cast<int>(key)).plain();
                model->keyText.text(text);
                static constexpr Color colours[] = {rgb(0x3b, 0x82, 0xf6), rgb(0xf6, 0x82, 0x3b), rgb(0x3b, 0xf6, 0x82),
                                                    rgb(0xf6, 0x3b, 0xb0)};
                square.brush(compositor.createColorBrush(colours[static_cast<int>(key) % 4]));
            }
        });
        window->onPointerMoved([weak = self, square](PointerPoint const& point) {
            square.offset({static_cast<float>(point.position().x), static_cast<float>(point.position().y), 0.0f});
            if (auto const model = weak.lock()) {
                std::u16string text = u"Pointer: ";
                text += core::to_u16(point.position().x, std::chars_format::fixed, 0).plain();
                text += u", ";
                text += core::to_u16(point.position().y, std::chars_format::fixed, 0).plain();
                model->pointerText.text(text);
            }
        });
        window->onPointerWheel([weak = self, square](PointerPoint const& point) {
            auto const size = std::clamp(square.size().x + point.properties().mouseWheelDelta() / 12.0f, 20.0f, 400.0f);
            square.size({size, size});
            if (auto const model = weak.lock()) {
                model->wheelText.text(u"Wheel: the square is now " + std::u16string(core::to_u16(size, std::chars_format::fixed, 0).plain()) + u" px");
            }
        });
        // A file thrown into the window (DragAcceptFiles / WM_DROPFILES): its path.
        window->acceptFileDrops([weak = self](std::filesystem::path file) {
            if (auto const model = weak.lock()) {
                model->dropText.text(u"Dropped: " + file.u16string());
            }
        });
        window->add_onClosed([weak = self](auto&&...) {
            if (auto const model = weak.lock()) {
                if (model->window) {
                    // The place is kept as text: ordinary, maximized or full-screen, and the
                    // rectangle the window returns to.
                    *model->remembered = model->window->placement();
                    model->placeText.text(u"Place: " + *model->remembered);
                }
                model->window.reset();
            }
        });
        window->centreWithClientSize({960, 540});
        gallery::trackWindow(*window);
        window->activate();
    }
};
auto const model = gallery::hold<Model>();
model->self = model;

auto example = StackPanel {
    spacing = 8,
    TextBlock {
        textWrapping = TextWrapping::Wrap,
        u"The window has no XAML island: a square on the scene follows the pointer, takes a colour from a key, "
        u"changes its size with the wheel. Throw a file into it to see its path. Close it, and the place it "
        u"was in is kept: the next one opens there.",
    },
    Button {content = u"Show window", onClick = [model](Button const&) { model->open(); }},
    model->keyText,
    model->pointerText,
    model->wheelText,
    model->dropText,
    model->placeText,
};
