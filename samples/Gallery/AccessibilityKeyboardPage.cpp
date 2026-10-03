// Страница AccessibilityKeyboard — AccessibilityKeyboardPage оригинала: порядок табуляции, стрелки, горячие клавиши
// и клавиши доступа; между разделами пояснения, под ними шесть примеров.

#include "Pages.h"

#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Documents.h"
#include "generated/Microsoft.UI.Xaml.Input.h"
#include "generated/Microsoft.UI.Xaml.Automation.Peers.Enums.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t automaticTabOrderCode[] = {
#include "Snippets/AccessibilityKeyboard/AccessibilityKeyboardAutomaticTabOrder.h.embed"
};
constexpr char8_t manualTabOrderCode[] = {
#include "Snippets/AccessibilityKeyboard/AccessibilityKeyboardManualTabOrder.h.embed"
};
constexpr char8_t automaticArrowKeysCode[] = {
#include "Snippets/AccessibilityKeyboard/AccessibilityKeyboardAutomaticArrowKeys.h.embed"
};
constexpr char8_t manualArrowKeysCode[] = {
#include "Snippets/AccessibilityKeyboard/AccessibilityKeyboardManualArrowKeys.h.embed"
};
constexpr char8_t acceleratorsCode[] = {
#include "Snippets/AccessibilityKeyboard/AccessibilityKeyboardAccelerators.h.embed"
};
constexpr char8_t accessKeysCode[] = {
#include "Snippets/AccessibilityKeyboard/AccessibilityKeyboardAccessKeys.h.embed"
};

// Заголовок раздела: уровень сообщается экранному диктору.
TextBlock section(char16_t const* text, bool second) {
    return TextBlock {Margin {0, second ? 20 : 0, 0, 0}, automationHeadingLevel = AutomationHeadingLevel::Level2, styles.TextBlock.Subtitle, text};
}

TextBlock subsection(char16_t const* text) {
    return TextBlock {Margin {0, 20, 0, 0}, automationHeadingLevel = AutomationHeadingLevel::Level3, styles.TextBlock.BodyStrong, text};
}

Hyperlink link(char16_t const* uri, char16_t const* text) {
    return Hyperlink {navigateUri = uri, Run {text}};
}

FrameworkElement place(wxl::FrameworkElement const& shown, gallery::Snippet code) {
    return gallery::controlExample({.example = shown, .code = code});
}

FrameworkElement automaticTabOrder() {
#include "Snippets/AccessibilityKeyboard/AccessibilityKeyboardAutomaticTabOrder.h"
    return place(example, gallery::snippet(automaticTabOrderCode));
}

FrameworkElement manualTabOrder() {
#include "Snippets/AccessibilityKeyboard/AccessibilityKeyboardManualTabOrder.h"
    return place(example, gallery::snippet(manualTabOrderCode));
}

FrameworkElement automaticArrowKeys() {
#include "Snippets/AccessibilityKeyboard/AccessibilityKeyboardAutomaticArrowKeys.h"
    return place(example, gallery::snippet(automaticArrowKeysCode));
}

FrameworkElement manualArrowKeys() {
#include "Snippets/AccessibilityKeyboard/AccessibilityKeyboardManualArrowKeys.h"
    return place(example, gallery::snippet(manualArrowKeysCode));
}

FrameworkElement accelerators() {
#include "Snippets/AccessibilityKeyboard/AccessibilityKeyboardAccelerators.h"
    return place(example, gallery::snippet(acceleratorsCode));
}

FrameworkElement accessKeys() {
#include "Snippets/AccessibilityKeyboard/AccessibilityKeyboardAccessKeys.h"
    return place(example, gallery::snippet(accessKeysCode));
}

}  // namespace

FrameworkElement gallery::accessibilityKeyboardPage() {
    auto references = StackPanel {automationName = u"Arrow keys references"};
    for (auto const& [title, anchor] : std::initializer_list<std::pair<char16_t const*, char16_t const*>> {
             {u"Keyboard interactions: Navigation", u"#navigation"},
             {u"Keyboard interactions: Home and End keys", u"#home-and-end-keys"},
             {u"Keyboard interactions: Page up and Page down keys", u"#page-up-and-page-down-keys"},
             {u"Keyboard interactions: Control group", u"#control-group"}}) {
        references.children().append(HyperlinkButton {
            content = title, navigateUri = std::u16string {u"https://learn.microsoft.com/windows/apps/design/input/keyboard-interactions"} + anchor});
    }

    return StackPanel {
        spacing = 12.0,
        RichTextBlock {
            Paragraph {
                Run {u"Accessibility is about building experiences that make your Windows application usable by people of all abilities. "
                     u"For more information about designing accessible apps: "},
                link(u"https://learn.microsoft.com/windows/apps/design/accessibility/accessibility-overview", u"Accessibility overview"),
                Run {u"."},
                LineBreak {},
                LineBreak {},
                Run {u"If your app does not provide good keyboard access, users who are blind or have mobility issues can have difficulty "
                     u"using your app or may not be able to use it at all."},
                LineBreak {},
            },
        },

        section(u"Tab order", false),
        RichTextBlock {
            Paragraph {
                Run {u"To use the keyboard with a control, the control must have focus. The most common way to receive focus is via "},
                Bold {Run {u"Tab navigation"}},
                Run {u", which cycles through controls that are "},
                Bold {Run {u"tab stops"}},
                Run {u". The order of these tab stops is called the "},
                Bold {Run {u"tab order"}},
                Run {u"."},
                LineBreak {},
            },
            Paragraph {
                Run {u"All interactive controls, like buttons, should be tab stops (unless they are in a group that's accessible in some other "
                     u"way), but non-interactive controls, like labels, should not. Try to put initial focus on the most useful or logical "
                     u"element."},
                LineBreak {},
            },
            Paragraph {
                Run {u"See "},
                link(u"https://learn.microsoft.com/windows/apps/design/input/keyboard-interactions", u"Keyboard interactions"),
                Run {u" and "},
                link(u"https://learn.microsoft.com/windows/apps/design/accessibility/keyboard-accessibility", u"Keyboard accessibility"),
                Run {u"."},
            },
        },

        TextBlock {automationHeadingLevel = AutomationHeadingLevel::Level3, styles.TextBlock.BodyStrong, u"Automatic tab order"},
        RichTextBlock {Paragraph {Run {u"By default, tab order matches the order elements are defined in XAML. This is usually the best order:"}}},
        automaticTabOrder(),

        TextBlock {automationHeadingLevel = AutomationHeadingLevel::Level3, styles.TextBlock.BodyStrong, u"Manual tab order"},
        RichTextBlock {Paragraph {Run {u"When the XAML order doesn't match the \"logical\" tab order, though, you can specify tab order manually:"}}},
        manualTabOrder(),

        section(u"Arrow keys", true),
        RichTextBlock {
            Paragraph {
                Run {u"Users expect groups of similar, related controls to be navigable via "},
                Bold {Run {u"Arrow keys"}},
                Run {u", too. This can be instead of "},
                Italic {Run {u"or"}},
                Run {u" in addition to tab navigation, depending on the situation."},
                LineBreak {},
            },
            Paragraph {Run {u"Groups of controls that support arrow key navigation typically support Home/End and PgUp/PgDn, too."}},
        },
        TextBlock {Margin {0, 8, 0, 0}, u"See also:"},
        references,

        subsection(u"Automatically supporting arrow keys"),
        RichTextBlock {Paragraph {Run {u"Most controls that group elements support arrow keys (and Home/End and PgUp/PgDn) by default:"}}},
        automaticArrowKeys(),

        subsection(u"Manually supporting arrow keys with XYFocusKeyboardNavigation"),
        RichTextBlock {
            Paragraph {Run {u"You can enable arrow key navigation between items manually, too."}, LineBreak {}},
            Paragraph {
                Run {u"Note that if you're implementing a list of items, users may expect additional affordances like support for "
                     u"Home/End, PgUp/PgDn, and additional accessibility properties like PositionInSet and SizeOfSet. Keyboard navigation can "
                     u"get complicated, but getting it right can make your app a lot easier to use — for everyone."},
                LineBreak {},
            },
            Paragraph {
                Run {u"See "},
                link(u"https://learn.microsoft.com/windows/apps/design/input/focus-navigation",
                     u"Focus navigation for keyboard, gamepad, remote control, and accessibility tools"),
                Run {u" and "},
                link(u"https://learn.microsoft.com/windows/apps/design/accessibility/keyboard-accessibility", u"Keyboard accessibility"),
                Run {u"."},
            },
        },
        manualArrowKeys(),

        section(u"Keyboard shortcuts", true),
        RichTextBlock {
            Paragraph {
                Run {u"Keyboard shortcuts are extremely helpful for Narrator users, keyboard users, and power users. Since keyboard shortcuts "
                     u"generally lack the ability to quickly switch between sections of UI like a mouse can, adding a few keyboard shortcuts for "
                     u"common actions can make your app much easier to use."},
                LineBreak {},
            },
            Paragraph {
                Run {u"WinUI 3 offers 2 types of keyboard shortcuts: "},
                Bold {Run {u"Accelerators"}},
                Run {u" and "},
                Bold {Run {u"Access keys"}},
                Run {u". Accelerators invoke specific app commands, while access keys set focus to specific parts of your UI."},
            },
        },

        subsection(u"Accelerators"),
        RichTextBlock {
            Paragraph {Run {u"Accelerators are hotkeys (typically starting with the Ctrl key) that invoke specific app commands."}, LineBreak {}},
            Paragraph {
                Run {u"It's important to provide an easy way of discovering keyboard accelerators. For example, with tooltips, visible labels, "
                     u"AutomationProperties.AcceleratorKey, accessible descriptions, etc. By default, WinUI adds a tooltip with the hotkey, but "
                     u"consider including the accelerator manually if you use a custom tooltip."},
                LineBreak {},
            },
            Paragraph {
                Run {u"See "},
                link(u"https://learn.microsoft.com/windows/apps/design/input/keyboard-accelerators", u"Keyboard accelerators"),
                Run {u"."},
            },
        },
        accelerators(),

        subsection(u"Access keys"),
        RichTextBlock {
            Paragraph {Run {u"Access keys are keyboard shortcuts, starting with Alt, that move system focus around your UI."}, LineBreak {}},
            Paragraph {
                Run {u"When users press the Alt key, WinUI shows "},
                Bold {Run {u"Key Tips"}},
                Run {u" next to each control with an access key, so users can discover them. WinUI also populates AccessKey UIA property, so "
                     u"screen reader users can learn access keys, too."},
                LineBreak {},
            },
            Paragraph {Run {u"See "}, link(u"https://learn.microsoft.com/windows/apps/design/input/access-keys", u"Access keys"), Run {u"."}},
        },
        accessKeys(),
    };
}
