auto editor = RichEditBox {fontSize = 16, height = 80, width = 724, hAlign.left};
editor.document().setMathMode(RichEditMathMode::MathOnly);

auto description = RichTextBlock {
    Paragraph {
        Margin {0, 0, 0, 4},
        Run {u"Math mode enables users to have input automatically recognized and converted to math expressions while being received."},
    },
    Paragraph {
        Margin {0, 0, 0, 4},
        Run {u"It uses "},
        Hyperlink {
            navigateUri = u"https://www.unicode.org/notes/tn28/",
            Run {u"Unicode Nearly Plain-Text Encoding of Mathematics"},
        },
        Run {u", which allows mathematical notation to be represented in a linear format and automatically converted into proper math equations."},
    },
    Paragraph {
        Margin {0, 0, 0, 4},
        Run {u"For example, \"4^2\" is converted to \"4\u00b2\", and \"\\pi\" is converted to \"\u03c0\"."},
    },
    Paragraph {
        Run {u"Enabling math mode in a RichEditBox automatically switches the input font to Cambria Math. Additionally, toggling math mode clears any existing content and undo stack."},
    },
};