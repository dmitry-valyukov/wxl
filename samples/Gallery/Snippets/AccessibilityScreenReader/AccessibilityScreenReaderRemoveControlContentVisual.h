// The image is not necessary for screen readers as it does not contain any information.
// Thus we remove it from the content visual tree with AccessibilityView::Raw
auto example = StackPanel {
    orientation.horizontal,
    Border {cornerRadius = CornerRadius {4},
            child = Image {height = 40, vAlign.top, automationAccessibilityView = AccessibilityView::Raw,
                           source = u"Assets/SampleMedia/treetops.jpg"}},
    TextBlock {maxWidth = 400, Margin {8, -4, 0, 0}, textWrapping = TextWrapping::WrapWholeWords,
               u"This is some demo text. The image on the right is just for decoration and serves no informational purpose. "
               u"To prevent Narrator or other screen readers from reading out the image, we set the accessibility view to \"Raw\" which "
               u"removes it from the content visual tree."},
};
