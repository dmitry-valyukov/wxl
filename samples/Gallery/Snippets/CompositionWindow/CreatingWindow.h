// A CompositionWindow is a handle, like a control: the copy looks into the same window, every
// member is const, and a lambda holds the window by value. It is written in braces the way a
// control is -- the title, the least size, the title bar, the content.
struct Model {
    TextBox titleBox {header = u"Title", width = 260, text = u"wxl: CompositionWindow"};
    NumberBox minWidthBox {header = u"Least client width", minimum = 200.0, maximum = 1000.0, value = 480.0};
    NumberBox minHeightBox {header = u"Least client height", minimum = 150.0, maximum = 800.0, value = 320.0};
    ToggleSwitch extendsBox {header = u"ExtendsContentIntoTitleBar", isOn = true};
    ToggleSwitch darkFrameBox {header = u"Dark frame", isOn = false};

    void open() {
        auto const minimum = SizeInt32 {static_cast<int32_t>(minWidthBox.value()), static_cast<int32_t>(minHeightBox.value())};
        CompositionWindow window {
            title = titleBox.text(),
            minSize = minimum,
            extendsContentIntoTitleBar = extendsBox.isOn(),
        };
        if (extendsBox.isOn()) {
            // With the title bar of the window's own the buttons of the window are the window's too,
            // at the height of the bar: the bar is dragged by, and clicked through where it holds
            // something to click.
            window.titleBar(TitleBar {
                leftHeader = TextBlock {u"wxl", vAlign.center, Margin {12, 0, 0, 0}},
                content = TextBox {placeholderText = u"Search", width = 280, vAlign.center},
                rightHeader = Button {u"Sign in", vAlign.center, Margin {0, 0, 8, 0}},
            });
        }
        window.content(StackPanel {
            Margin {24},
            spacing = 12,
            TextBlock {u"The caption buttons are drawn by the window, at the height of the bar."},
            Button {u"Close", onClick = [window](Button const&) { window.close(); }},
        });
        window.background(rgb(243, 243, 243));
        window.darkFrame(darkFrameBox.isOn());
        // The place is the middle of the screen, the size is the client area's: the content lives in it.
        window.centreWithClientSize({960, 600});
        gallery::trackWindow(window);
        window.activate();
    }
};
auto const model = gallery::hold<Model>();

auto example = StackPanel {
    spacing = 8,
    TextBlock {
        textWrapping = TextWrapping::Wrap,
        u"The window is made without a redirection surface, so the white of the window class's brush cannot show "
        u"through while the corner is dragged: its pixels are laid down by the compositor.",
    },
    Button {content = u"Show window", onClick = [model](Button const&) { model->open(); }},
};

auto options = StackPanel {
    spacing = 8,
    model->titleBox,
    model->minWidthBox,
    model->minHeightBox,
    model->extendsBox,
    model->darkFrameBox,
};
