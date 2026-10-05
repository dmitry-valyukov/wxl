// Страница AccessibilityScreenReader — AccessibilityScreenReaderPage оригинала: доступные имена, описания,
// ориентиры и заголовки; пояснения и одиннадцать примеров под ними.

#include "Pages.h"

#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Microsoft.UI.Xaml.Documents.h>
#include <wxl/Microsoft.UI.Xaml.Automation.Peers.Enums.h>

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t nameFromContentCode[] = {
#include "Snippets/AccessibilityScreenReader/AccessibilityScreenReaderNameFromContent.h.embed"
};
constexpr char8_t nameFromHeaderCode[] = {
#include "Snippets/AccessibilityScreenReader/AccessibilityScreenReaderNameFromHeader.h.embed"
};
constexpr char8_t nameOnListViewCode[] = {
#include "Snippets/AccessibilityScreenReader/AccessibilityScreenReaderNameOnListView.h.embed"
};
constexpr char8_t nameOnImageCode[] = {
#include "Snippets/AccessibilityScreenReader/AccessibilityScreenReaderNameOnImage.h.embed"
};
constexpr char8_t labeledByCode[] = {
#include "Snippets/AccessibilityScreenReader/AccessibilityScreenReaderLabeledBy.h.embed"
};
constexpr char8_t descriptionCode[] = {
#include "Snippets/AccessibilityScreenReader/AccessibilityScreenReaderDescriptionHelpTextAdd.h.embed"
};
constexpr char8_t positionCode[] = {
#include "Snippets/AccessibilityScreenReader/AccessibilityScreenReaderPositionIndicatePositionElement.h.embed"
};
constexpr char8_t rawCode[] = {
#include "Snippets/AccessibilityScreenReader/AccessibilityScreenReaderRemoveControlContentVisual.h.embed"
};
constexpr char8_t landmarksCode[] = {
#include "Snippets/AccessibilityScreenReader/AccessibilityScreenReaderLandmarks.h.embed"
};
constexpr char8_t headingsCode[] = {
#include "Snippets/AccessibilityScreenReader/AccessibilityScreenReaderHeadings.h.embed"
};
constexpr char8_t groupsCode[] = {
#include "Snippets/AccessibilityScreenReader/AccessibilityScreenReaderControlGroups.h.embed"
};

// Подзаголовки раздела; уровень сообщается экранному диктору.
TextBlock heading2(char16_t const* text, bool spaced = true) {
    return TextBlock {Margin {0, spaced ? 20 : 0, 0, 0}, automationHeadingLevel = AutomationHeadingLevel::Level2, styles.TextBlock.Subtitle, text};
}

TextBlock heading3(char16_t const* text) {
    return TextBlock {Margin {0, 20, 0, 0}, automationHeadingLevel = AutomationHeadingLevel::Level3, styles.TextBlock.BodyStrong, text};
}

TextBlock bigHeading3(char16_t const* text) {
    return TextBlock {Margin {0, 20, 0, 0}, automationHeadingLevel = AutomationHeadingLevel::Level3, styles.TextBlock.Subtitle, text};
}

Hyperlink link(char16_t const* uri, char16_t const* text) {
    return Hyperlink {navigateUri = uri, Run {text}};
}

FrameworkElement place(FrameworkElement const& shown, gallery::Snippet code) {
    return gallery::controlExample({.example = shown, .code = code});
}

FrameworkElement nameFromContent() {
#include "Snippets/AccessibilityScreenReader/AccessibilityScreenReaderNameFromContent.h"
    return place(example, gallery::snippet(nameFromContentCode));
}

FrameworkElement nameFromHeader() {
#include "Snippets/AccessibilityScreenReader/AccessibilityScreenReaderNameFromHeader.h"
    return place(example, gallery::snippet(nameFromHeaderCode));
}

FrameworkElement nameOnListView() {
#include "Snippets/AccessibilityScreenReader/AccessibilityScreenReaderNameOnListView.h"
    return place(example, gallery::snippet(nameOnListViewCode));
}

FrameworkElement nameOnImage() {
#include "Snippets/AccessibilityScreenReader/AccessibilityScreenReaderNameOnImage.h"
    return place(example, gallery::snippet(nameOnImageCode));
}

FrameworkElement labeledBy() {
#include "Snippets/AccessibilityScreenReader/AccessibilityScreenReaderLabeledBy.h"
    return place(example, gallery::snippet(labeledByCode));
}

FrameworkElement describedControls() {
#include "Snippets/AccessibilityScreenReader/AccessibilityScreenReaderDescriptionHelpTextAdd.h"
    return place(example, gallery::snippet(descriptionCode));
}

FrameworkElement positioned() {
#include "Snippets/AccessibilityScreenReader/AccessibilityScreenReaderPositionIndicatePositionElement.h"
    return place(example, gallery::snippet(positionCode));
}

FrameworkElement removedFromTree() {
#include "Snippets/AccessibilityScreenReader/AccessibilityScreenReaderRemoveControlContentVisual.h"
    return place(example, gallery::snippet(rawCode));
}

FrameworkElement landmarks() {
#include "Snippets/AccessibilityScreenReader/AccessibilityScreenReaderLandmarks.h"
    return place(example, gallery::snippet(landmarksCode));
}

FrameworkElement headings() {
#include "Snippets/AccessibilityScreenReader/AccessibilityScreenReaderHeadings.h"
    return place(example, gallery::snippet(headingsCode));
}

FrameworkElement controlGroups() {
#include "Snippets/AccessibilityScreenReader/AccessibilityScreenReaderControlGroups.h"
    return place(example, gallery::snippet(groupsCode));
}

}  // namespace

FrameworkElement gallery::accessibilityScreenReaderPage() {
    return StackPanel {
        spacing = 12.0,
        RichTextBlock {
            Paragraph {
                Run {u"Accessibility is about building experiences that make your Windows application usable by people of all abilities. "
                     u"For more information about designing accessible apps: "},
                link(u"https://learn.microsoft.com/windows/apps/design/accessibility/accessibility-overview", u"Accessibility overview"),
                Run {u"."},
                LineBreak {},
            },
            Paragraph {
                Run {u"Screen readers, such as "},
                link(u"https://support.microsoft.com/windows/complete-guide-to-narrator-e4397a0d-ef4f-b386-d8ae-c172f109bdb1", u"Narrator"),
                Run {u", convert text into spoken words to help blind or low vision users. Screen readers use the UI Automation (UIA) names "
                     u"of each control to report their name, role and content."},
            },
        },

        heading2(u"Accessible names"),
        RichTextBlock {
            Paragraph {Run {u"An "}, Bold {Run {u"accessible name"}},
                       Run {u" is a short, descriptive text string that a screen reader uses to describe a UI element."}, LineBreak {}},
            Paragraph {Run {u"Typically, the accessible name should be short and match the visual label of the control. Screen reader users "
                            u"will hear this name every time they navigate to that control."},
                       LineBreak {}},
            Paragraph {Run {u"If the control's content can be converted to a string, an accessible name is automatically determined from "
                            u"the visible text. However, elements such as images or input fields need to have a custom accessible name."},
                       LineBreak {}},
            Paragraph {
                Run {u"See "},
                link(u"https://learn.microsoft.com/windows/apps/design/accessibility/basic-accessibility-information#accessible-name",
                     u"Expose basic accessibility information#Accessible name"),
                Run {u" and "},
                link(u"https://learn.microsoft.com/windows/apps/design/accessibility/basic-accessibility-information#name-from-inner-text",
                     u"Expose basic accessibility information#Name from inner text"),
                Run {u"."},
            },
        },

        heading3(u"Getting an accessible name automatically"),
        RichTextBlock {Paragraph {Run {u"For most controls, XAML automatically sets an accessible name from the control's content (if the "
                                       u"content is a string)."}}},
        nameFromContent(),
        nameFromHeader(),

        heading3(u"Setting an accessible name manually"),
        RichTextBlock {Paragraph {Run {u"Controls without stringable content will not get an accessible name automatically."}}},
        nameOnListView(),
        nameOnImage(),

        heading3(u"Using another control to provide an accessible name"),
        RichTextBlock {Paragraph {Run {u"Controls with accessible names can be used as labels for other controls. They should be removed from "
                                       u"the UIA tree (see "},
                                  Bold {Run {u"Visual tree"}}, Run {u" below), to avoid being redundant."}}},
        labeledBy(),

        heading2(u"Common accessibility properties"),
        RichTextBlock {
            Paragraph {Run {u"Besides accessible name, common accessibility properties include:"}, LineBreak {}, LineBreak {},
                       Run {u"- Description and help text"}, LineBreak {}, Run {u"- Position in set"}, LineBreak {},
                       Run {u"- Headings and landmarks (see below)"}, LineBreak {}},
            Paragraph {
                Run {u"See "},
                link(u"https://learn.microsoft.com/windows/apps/design/accessibility/basic-accessibility-information",
                     u"Expose basic accessibility information"),
                Run {u" and "},
                link(u"https://learn.microsoft.com/accessibility-tools-docs/items/uwpxaml/control_fulldescription_describedby_helptext",
                     u"UWP XAML: Setting supplemental information on a control"),
                Run {u"."},
            },
        },
        describedControls(),
        positioned(),

        heading2(u"Visual tree"),
        RichTextBlock {
            Paragraph {Run {u"UIA exposes multiple views of the UI tree: Control, Content, and Raw."}, LineBreak {}},
            Paragraph {Run {u"Most accessibility tools use the \"Control\" or \"Content\" views, so you can effectively \"hide\" redundant or "
                            u"unhelpful controls from screen readers by putting them in the \"Raw\" view."},
                       LineBreak {}},
            Paragraph {
                Run {u"See "},
                link(u"https://learn.microsoft.com/windows/apps/design/accessibility/basic-accessibility-information#influencing-the-ui-automation-tree-views",
                     u"Expose basic accessibility information#Influencing the UI Automation tree views"),
                Run {u"."},
            },
        },
        removedFromTree(),

        heading2(u"Landmarks and headings"),
        RichTextBlock {
            Paragraph {Bold {Run {u"Landmarks and headings"}},
                       Run {u" indicate, or label, different sections of a user interface for screen readers and other Assistive Technologies "
                            u"(ATs), just like visible headings do for visual users. Marking up your content with landmarks and headings lets "
                            u"screen reader users skim content similarly to sighted users."},
                       LineBreak {}},
            Paragraph {Run {u"See "},
                       link(u"https://learn.microsoft.com/windows/apps/design/accessibility/landmarks-and-headings", u"Landmarks and headings"),
                       Run {u"."}},
        },

        bigHeading3(u"Landmarks"),
        RichTextBlock {Paragraph {Bold {Run {u"Landmarks"}},
                                  Run {u" typically identify big sections of your UI, like \"search\", \"main content\", or \"navigation.\" You "
                                       u"can also add landmarks with custom names."}}},
        landmarks(),

        bigHeading3(u"Headings"),
        RichTextBlock {
            Paragraph {Bold {Run {u"Headings"}},
                       Run {u" typically identify smaller groups of content. They usually correspond to the \"visual\" headings in your UI "
                            u"— the text that labels sections in your UI visually."},
                       LineBreak {}},
            Paragraph {Run {u"For example, all of the section headings on this page are accessible headings."}},
        },
        headings(),

        bigHeading3(u"Associating smaller groups of controls"),
        RichTextBlock {
            Paragraph {Run {u"You can also group controls together manually, even if the controls don't have a visible heading or landmark, by "
                            u"adding an accessible name to the parent container. When entering a region with a name or a landmark, Narrator "
                            u"will read it out as "},
                       Bold {Run {u"Context"}}, Run {u". Narrator users can control their "}, Bold {Run {u"Context level"}},
                       Run {u" in Settings."}, LineBreak {}},
            Paragraph {Run {u"This is helpful in complicated UI with many similar elements, where the grouping might be obvious to visual "
                            u"users, but not screen reader users."}},
        },
        controlGroups(),
    };
}
