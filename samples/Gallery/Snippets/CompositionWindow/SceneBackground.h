// The scene is the layer under the island: its only background is a colour, a picture or a surface
// the application draws. It is swapped while the window is open, and it is the same one background
// -- never one under another.
struct Model {
    // The kinds of background; the picture has its own fit.
    enum class Kind { Colour, Picture, Drawing };

    ComboBox kindBox {
        header = u"Background",
        width = 200,
        ComboBoxItem {content = u"Colour"},
        ComboBoxItem {content = u"Picture"},
        ComboBoxItem {content = u"Drawn by the application"},
        selectedIndex = 1,
    };
    ComboBox fillBox {
        header = u"Fill of the picture",
        width = 200,
        ComboBoxItem {content = u"UniformToFill"},
        ComboBoxItem {content = u"Uniform"},
        ComboBoxItem {content = u"Fill"},
        ComboBoxItem {content = u"None"},
        ComboBoxItem {content = u"Tile"},
        ComboBoxItem {content = u"TileMirrored"},
        selectedIndex = 0,
    };
    core::observable<Color> colour {rgb(0x20, 0x4a, 0x87)};
    std::shared_ptr<CompositionWindow> window;
    std::shared_ptr<DrawingSurface> surface;

    BackgroundFill fill() const {
        static constexpr BackgroundFill fills[] = {BackgroundFill::UniformToFill, BackgroundFill::Uniform,
                                                   BackgroundFill::Fill,          BackgroundFill::None,
                                                   BackgroundFill::Tile,          BackgroundFill::TileMirrored};
        return fills[std::clamp(fillBox.selectedIndex(), 0, 5)];
    }

    Kind kind() const {
        static constexpr Kind kinds[] = {Kind::Colour, Kind::Picture, Kind::Drawing};
        return kinds[std::clamp(kindBox.selectedIndex(), 0, 2)];
    }

    // Puts the chosen background into the open window.
    void apply() {
        if (!window) {
            return;
        }
        switch (kind()) {
            case Kind::Colour:
                window->background(colour.get());
                break;
            case Kind::Picture:
                // A picture is decoded off the interface thread and appears when it is ready.
                window->backgroundAsync(BackgroundImage {
                    .path = applicationFolder() / L"Assets" / L"SampleMedia" / L"LandscapeImage3.jpg",
                    .fill = fill(),
                    .color = rgb(0x20, 0x20, 0x20),
                });
                break;
            case Kind::Drawing: {
                // A surface of the compositor of the window, drawn with Direct2D.
                surface = std::make_shared<DrawingSurface>(window->compositor(), SizeInt32 {640, 400});
                surface->draw([](ID2D1DeviceContext* context) {
                    context->Clear(D2D1::ColorF(0.07f, 0.1f, 0.2f));
                    ID2D1SolidColorBrush* brush = nullptr;
                    context->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f), &brush);
                    for (int ring = 0; ring < 8; ++ring) {
                        brush->SetColor(D2D1::ColorF(0.2f + 0.1f * ring, 0.5f, 1.0f - 0.1f * ring, 0.6f));
                        context->FillEllipse(
                            D2D1::Ellipse(D2D1::Point2F(320, 200), 300.0f - 36.0f * ring, 180.0f - 21.0f * ring), brush);
                    }
                    brush->Release();
                });
                window->background(*surface);
                break;
            }
        }
    }

    void open() {
        window = std::make_shared<CompositionWindow>(CompositionWindow {
            title = u"Background of the scene",
            minSize = {480, 320},
            StackPanel {
                Margin {24},
                TextBlock {
                    foreground = colors.white,
                    u"The text is XAML, on the island; what is behind it is the scene.",
                },
            },
        });
        window->centreWithClientSize({960, 600});
        apply();
        window->add_onClosed([weak = std::weak_ptr<Model>(self)](auto&&...) {
            if (auto const model = weak.lock()) {
                model->window.reset();
            }
        });
        gallery::trackWindow(*window);
        window->activate();
    }

    std::weak_ptr<Model> self;
};
auto const model = gallery::hold<Model>();
model->self = model;
model->kindBox.add_onSelectionChanged([weak = std::weak_ptr<Model>(model)](auto&&...) {
    if (auto const model = weak.lock()) {
        model->apply();
    }
});
model->fillBox.add_onSelectionChanged([weak = std::weak_ptr<Model>(model)](auto&&...) {
    if (auto const model = weak.lock()) {
        model->apply();
    }
});
model->colour.on_change([weak = std::weak_ptr<Model>(model)](auto&&...) noexcept {
    if (auto const model = weak.lock()) {
        model->apply();
    }
});

auto example = Button {content = u"Show window", onClick = [model](Button const&) { model->open(); }};

auto options = StackPanel {
    spacing = 8,
    model->kindBox,
    model->fillBox,
    TextBlock {u"Colour"},
    gallery::colorSelector(model->colour, u"Background colour"),
};
