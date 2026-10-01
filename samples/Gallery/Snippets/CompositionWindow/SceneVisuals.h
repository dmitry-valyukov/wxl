// The page the application draws itself: visuals of the compositor of the window, under the
// island. They are kept in the frame by the window -- resized in WM_SIZE, synchronously -- which
// XAML with its deferred layout does not give.
struct Model {
    static void fill(CompositionWindow const& window) {
        static constexpr Color palette[6] = {rgb(60, 120, 220),  rgb(90, 130, 220),  rgb(120, 140, 220),
                                             rgb(150, 150, 220), rgb(180, 160, 220), rgb(210, 170, 220)};
        auto const compositor = window.compositor();
        auto const root = window.contentVisual();
        root.children().removeAll();

        // Six squares, turning each at its own speed about its centre.
        for (int index = 0; index < 6; ++index) {
            auto const square = compositor.createSpriteVisual();
            square.size({96.0f, 96.0f});
            square.anchorPoint({0.5f, 0.5f});
            square.offset({120.0f + 150.0f * static_cast<float>(index), 220.0f, 0.0f});
            square.brush(compositor.createColorBrush(palette[index]));
            square.opacity(0.85f);

            auto const turn = compositor.createScalarKeyFrameAnimation();
            turn.insertKeyFrame(0.0f, 0.0f);
            turn.insertKeyFrame(1.0f, index % 2 == 0 ? 360.0f : -360.0f, compositor.createLinearEasingFunction());
            turn.duration(std::chrono::seconds {4 + index});
            turn.iterationBehavior(AnimationIterationBehavior::Forever);
            square.startAnimation(u"RotationAngleInDegrees", turn);

            root.children().insertAtTop(square);
        }
    }

    void open() {
        CompositionWindow window {
            title = u"Visuals of the scene",
            minSize = {640, 400},
            StackPanel {
                Margin {24},
                spacing = 8,
                TextBlock {styles.TextBlock.Title, foreground = colors.white, u"A scene under the island"},
                TextBlock {
                    foreground = colors.white,
                    u"The squares are visuals of the window's compositor, turning on its own; this text is XAML.",
                },
            },
        };
        window.background(rgb(0x14, 0x1c, 0x33));
        fill(window);
        window.centreWithClientSize({960, 480});
        gallery::trackWindow(window);
        window.activate();
    }
};
auto const model = gallery::hold<Model>();

auto example = StackPanel {
    spacing = 8,
    TextBlock {
        textWrapping = TextWrapping::Wrap,
        u"The window has two layers and says so: the scene (a background and a page of composition visuals "
        u"that the application draws) and the island of XAML above it, transparent, over the whole window.",
    },
    Button {content = u"Show window", onClick = [model](Button const&) { model->open(); }},
};
