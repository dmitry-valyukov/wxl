TextBlock {
    Run {
        u"Text in a TextBlock doesn't have to be a simple string.",
        fontFamily = u"Times New Roman",
        foreground = rgb(169, 169, 169),
    },
    LineBreak {},
    Span {
        Run {u"Text can be "},
        Bold {Run {u"bold"}},
        Run {u", "},
        Italic {Run {u"italic"}},
        Run {u", or "},
        Underline {Run {u"underlined"}},
        Run {u"."},
    },
}