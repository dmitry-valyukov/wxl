StackPanel {
    Button {
        u"This is some text that is too long and will get cut off",
        hAlign.stretch,
        Margin {0, 0, 0, 5},
    },
    Button {
        u"This is another text that would result in being cut off",
        hAlign.stretch,
    },
    TextBlock {u"Another option is to explicitly wrap the Button's content", Margin {0, 8}},
    StackPanel {
        orientation.horizontal,
        hAlign.center,
        Button {
            maxWidth = 240,
            Margin {0, 0, 8, 0},
            content = TextBlock {
                u"This is some text that is too long and will get cut off without wrapping",
                textWrapping.wrapWholeWords,
            },
        },
        Button {
            maxWidth = 240,
            content = TextBlock {
                u"This is another text that would result in being cut off without wrapping",
                textWrapping.wrapWholeWords,
            },
        },
    },
}
