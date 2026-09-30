RichTextBlock {
    selectionHighlightColor = SolidColorBrush {color = colors.green},
    Paragraph {
        Run {u"RichTextBlock provides a rich text display container that supports "},
        Run {u"formatted text", fontStyle = FontStyle::Italic, FontWeight {700}},
        Run {u", "},
        Hyperlink {
            navigateUri = u"https://learn.microsoft.com/windows/windows-app-sdk/api/winrt/microsoft.ui.xaml.Documents.Hyperlink",
            Run {u"hyperlinks"},
        },
        Run {u", inline images, and other rich content."},
    },
    Paragraph {Run {u"RichTextBlock also supports a built-in overflow model."}},
}