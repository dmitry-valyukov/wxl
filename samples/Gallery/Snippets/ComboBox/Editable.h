static constexpr int sizes[] = {8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 36, 48, 72};

auto output = TextBlock {
    u"You can set the font size used for this text.",
    fontFamily = u"Segoe UI",
    fontSize = 12,
    Margin {0, 8, 0, 0},
};

auto combo = ComboBox {
    width = 200,
    header = u"Font Size",
    isEditable = true,
    [](iterate<sizes> size) { return ComboBoxItem {content = core::to_u16(size)}; },
    selectedIndex = 2,
    onSelectionChanged = [output](ComboBox const& self) {
        int const index = self.selectedIndex();
        if (index >= 0) {
            output.fontSize(sizes[index]);
        }
    },
    onTextSubmitted = [output](ComboBox const& self, ComboBoxTextSubmittedEventArgs& args) {
        auto const text = gallery::wide(args.text());
        wchar_t* end = nullptr;
        double const size = std::wcstod(text.c_str(), &end);
        bool const isNumber = !text.empty() && end == text.c_str() + text.size();
        bool const inList = std::ranges::find(sizes, static_cast<int>(size)) != std::end(sizes);

        // Accept a number from the list, or a custom one between 8 and 100.
        if (isNumber && (inList || (size > 8 && size < 100))) {
            output.fontSize(size);
            args.handled(true);
            return;
        }

        // Anything else is rejected: the text goes back and a dialog says why.
        self.text(core::to_u16(sizes[std::max(self.selectedIndex(), 0)]));
        args.handled(true);
        showDialog(ContentDialog {
                       content = u"The font size must be a number between 8 and 100.",
                       closeButtonText = u"Close",
                       defaultButton = ContentDialogButton::Close,
                   },
                   self);
    },
};