// The filling primitives behind wxl::FormattedBlock.
//
// The projection comes first, and with it every standard header it needs:
// wxl's own headers carry the wxl.core import, and a standard header
// included after that import is one the compiler has already seen through
// the std module.
#include <winrt/Microsoft.UI.Text.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Documents.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Text.h>

#include <algorithm>
#include <cassert>
#include <limits>

#include "FormattedBlock.impl.h"
#include "Object.impl.h"
#include "impl/activation_factory.h"
#include "impl/conversions.h"
#include "impl/tool_tip.h"

namespace wxl {

namespace impl {

// The wrapper is wxl's own, but the object behind it is the system's
// RichTextBlock -- FormattedBlock adds vocabulary, not a runtime class.
template <>
struct runtime_class_name_of<FormattedBlock> {
    static constexpr wchar_t value[] = L"Microsoft.UI.Xaml.Controls.RichTextBlock";
};

}  // namespace impl

namespace {

namespace docs = winrt::Microsoft::UI::Xaml::Documents;
namespace media = winrt::Microsoft::UI::Xaml::Media;

// The paragraph appends land in: the last one, made on the spot when the
// block is still empty -- bare text must work without an appendParagraph()
// first. Blocks of a RichTextBlock hold nothing but Paragraph, so the cast
// cannot meet anything else.
docs::Paragraph last_paragraph(winrt::Microsoft::UI::Xaml::Controls::RichTextBlock const& block) {
    auto blocks = block.Blocks();
    if (blocks.Size() > 0) {
        return blocks.GetAt(blocks.Size() - 1).as<docs::Paragraph>();
    }
    docs::Paragraph paragraph;
    blocks.Append(paragraph);
    return paragraph;
}

// The merge of the style stack, innermost last: a filled field overrides,
// an empty one lets the outer level through.
TextStyle merged(std::span<const TextStyle> styles) {
    TextStyle result;
    for (TextStyle const& style : styles) {
        if (style.color) result.color = style.color;
        if (style.fontSize) result.fontSize = style.fontSize;
        if (style.fontFamily) result.fontFamily = style.fontFamily;
        if (style.bold) result.bold = style.bold;
        if (style.italic) result.italic = style.italic;
        if (style.underline) result.underline = style.underline;
        if (style.strikethrough) result.strikethrough = style.strikethrough;
    }
    return result;
}

// Wears the effective style on one text element. A facet without a value
// stays untouched and inherits from the block, which is what lets the
// block-wide font settings keep working under styled fragments.
void apply(docs::TextElement const& element, TextStyle const& style) {
    if (style.bold) {
        element.FontWeight(*style.bold ? winrt::Microsoft::UI::Text::FontWeights::Bold()
                                       : winrt::Microsoft::UI::Text::FontWeights::Normal());
    }
    if (style.italic) {
        element.FontStyle(*style.italic ? winrt::Windows::UI::Text::FontStyle::Italic
                                        : winrt::Windows::UI::Text::FontStyle::Normal);
    }
    if (style.underline || style.strikethrough) {
        auto decorations = winrt::Windows::UI::Text::TextDecorations::None;
        if (style.underline.value_or(false)) {
            decorations |= winrt::Windows::UI::Text::TextDecorations::Underline;
        }
        if (style.strikethrough.value_or(false)) {
            decorations |= winrt::Windows::UI::Text::TextDecorations::Strikethrough;
        }
        element.TextDecorations(decorations);
    }
    if (style.fontSize) {
        element.FontSize(*style.fontSize);
    }
    if (style.fontFamily) {
        element.FontFamily(media::FontFamily{std::wstring_view{*style.fontFamily}});
    }
    if (style.color) {
        element.Foreground(media::SolidColorBrush{impl::to_winrt(*style.color)});
    }
}

// A source string as a Uri: one with a scheme goes as written, a bare path
// is a local file -- the only kind of image the first tier admits. A
// relative path is resolved against the working directory here and now:
// "file:///logo.png" would name the root of the drive, not the folder the
// content came from.
winrt::Windows::Foundation::Uri to_uri(std::wstring_view source) {
    if (source.find(L"://") != std::wstring_view::npos) {
        return winrt::Windows::Foundation::Uri{source};
    }
    std::wstring file{L"file:///"};
    file += std::filesystem::absolute(std::filesystem::path{source}).wstring();
    return winrt::Windows::Foundation::Uri{std::wstring_view{file}};
}

}  // namespace

FormattedBlock::FormattedBlock(Impl* impl) noexcept : base_t(impl) {}

FormattedBlock::FormattedBlock() : base_t(new Impl{}) {
    impl::ActivationFactory<FormattedBlock>::activate(put_abi());
}

void FormattedBlock::clear() const {
    get<&Impl::richTextBlock_>().Blocks().Clear();
    Impl* self = static_cast<Impl*>(impl());
    self->styles_.clear();
    self->link_ = nullptr;
    self->linkTarget_.clear();
    self->wide_->entries.clear();
    self->folds_.clear();
}

namespace {

// A link splits around what a Hyperlink cannot hold: the interrupting
// element was just appended to the paragraph, and when a link was open, a
// fresh half with the same target continues it -- so the text after the
// element stays linked *and* stays after it.
void reopen_split_link(FormattedBlock const& block, FormattedBlock::Impl* self) {
    if (!self->link_) return;
    const sta_wstring target = self->linkTarget_;
    block.pushLink(target);
}

}  // namespace

void FormattedBlock::appendParagraph() const {
    appendParagraph(BlockStyle{});
}

void FormattedBlock::appendParagraph(BlockStyle const& style) const {
    // A link belongs to one paragraph; a new paragraph closes it.
    Impl* self = static_cast<Impl*>(impl());
    self->link_ = nullptr;
    self->linkTarget_.clear();

    docs::Paragraph paragraph;
    if (style.margin) {
        paragraph.Margin(impl::to_winrt(*style.margin));
    }
    if (style.alignment) {
        paragraph.TextAlignment(
            static_cast<winrt::Microsoft::UI::Xaml::TextAlignment>(*style.alignment));
    }
    get<&Impl::richTextBlock_>().Blocks().Append(paragraph);
}

void FormattedBlock::appendText(std::wstring_view text) const {
    Impl* self = static_cast<Impl*>(impl());
    docs::Run run;
    // The view goes into WinRT as it is: param::hstring is a string
    // reference, and its contract -- a zero right after the view, or an
    // outright abort() (proven by test/param_hstring_test.cpp) -- is this
    // class's stated precondition for every text parameter. Literals,
    // basic_strings and wxl.html's arena views all satisfy it for free;
    // no copy is bought here to insure against the caller.
    run.Text(text);
    apply(run, merged(self->styles_));
    if (self->link_) {
        self->link_.Inlines().Append(run);
    } else {
        last_paragraph(get<&Impl::richTextBlock_>()).Inlines().Append(run);
    }
}

void FormattedBlock::appendLineBreak() const {
    last_paragraph(get<&Impl::richTextBlock_>()).Inlines().Append(docs::LineBreak{});
    reopen_split_link(*this, static_cast<Impl*>(impl()));
}

void FormattedBlock::appendLink(std::wstring_view text, std::wstring_view target) const {
    pushLink(target);
    appendText(text);
    popLink();
}

void FormattedBlock::pushLink(std::wstring_view target) const {
    Impl* self = static_cast<Impl*>(impl());

    docs::Hyperlink link;

    // The closure holds the handler boxes and its own copy of the target --
    // never the Impl: the XAML tree owns the control and may outlive the
    // wrapper, and a click through a dead Impl would be a crash, while a
    // box kept alive by the closure is just a handler that still works.
    link.Click([box = self->onLink_, errors = self->onError_, target = sta_wstring{target}](
                   docs::Hyperlink const&, docs::HyperlinkClickEventArgs const&) {
        if (auto const& handler = box->get()) {
            (*handler)(std::wstring_view{target});
            return;
        }
        // No handler: the spec's default policy. A web link opens in the
        // default browser; any other scheme arrived with foreign content
        // and is dropped -- silently for the reader, as a record for the
        // diagnostic sink.
        if (target.starts_with(L"http://") || target.starts_with(L"https://")) {
            winrt::Windows::System::Launcher::LaunchUriAsync(
                winrt::Windows::Foundation::Uri{std::wstring_view{target}});
        } else {
            errors->invoke(HtmlError{HtmlErrorKind::BlockedLink, std::wstring_view{target}});
        }
    });

    last_paragraph(get<&Impl::richTextBlock_>()).Inlines().Append(link);
    self->link_ = std::move(link);
    self->linkTarget_ = target;
}

void FormattedBlock::popLink() const {
    Impl* self = static_cast<Impl*>(impl());
    self->link_ = nullptr;
    self->linkTarget_.clear();
}

void FormattedBlock::appendScript(std::wstring_view text, double scale, double drop) const {
    Impl* self = static_cast<Impl*>(impl());
    const TextStyle style = merged(self->styles_);
    const double base = style.fontSize ? *style.fontSize : get<&Impl::richTextBlock_>().FontSize();

    winrt::Microsoft::UI::Xaml::Controls::TextBlock script;
    script.Text(text);
    script.FontSize(base * scale);
    if (style.bold.value_or(false)) {
        script.FontWeight(winrt::Microsoft::UI::Text::FontWeights::Bold());
    }
    if (style.italic.value_or(false)) {
        script.FontStyle(winrt::Windows::UI::Text::FontStyle::Italic);
    }
    if (style.fontFamily) {
        script.FontFamily(media::FontFamily{std::wstring_view{*style.fontFamily}});
    }
    if (style.color) {
        script.Foreground(media::SolidColorBrush{impl::to_winrt(*style.color)});
    }

    // The container's child sits with its bottom on the text baseline, and
    // there is nothing to align it with -- the shift is a transform, picked
    // in fractions of the size so it scales with the font.
    media::TranslateTransform shift;
    shift.Y(base * scale * drop);
    script.RenderTransform(shift);

    docs::InlineUIContainer container;
    container.Child(script);
    last_paragraph(get<&Impl::richTextBlock_>()).Inlines().Append(container);
    reopen_split_link(*this, self);
}

void FormattedBlock::appendImage(std::wstring_view source, Size size,
                                 std::wstring_view toolTip) const {
    Impl* self = static_cast<Impl*>(impl());

    media::Imaging::BitmapImage bitmap;
    bitmap.UriSource(to_uri(source));

    winrt::Microsoft::UI::Xaml::Controls::Image image;
    image.Source(bitmap);
    image.Width(size.width);
    image.Height(size.height);
    if (!toolTip.empty()) {
        impl::set_tool_tip(image, toolTip);
    }

    // The load is asynchronous, so a missing or unreadable file surfaces
    // long after this call returns -- as a blank box of the given size and
    // a record in the diagnostic sink. The closure holds the box and its
    // own copy of the source, never the Impl (see pushLink).
    image.ImageFailed([box = self->onError_, file = sta_wstring{source}](
                          winrt::Windows::Foundation::IInspectable const&,
                          winrt::Microsoft::UI::Xaml::ExceptionRoutedEventArgs const&) {
        box->invoke(HtmlError{HtmlErrorKind::ImageFailed, std::wstring_view{file}});
    });

    docs::InlineUIContainer container;
    container.Child(image);
    last_paragraph(get<&Impl::richTextBlock_>()).Inlines().Append(container);
    reopen_split_link(*this, self);
}

void FormattedBlock::appendElement(UIElement const& element) const {
    docs::InlineUIContainer container;
    container.Child(*Object::Impl::get_typed<UIElement>(element));
    last_paragraph(get<&Impl::richTextBlock_>()).Inlines().Append(container);
    reopen_split_link(*this, static_cast<Impl*>(impl()));
}

namespace {

namespace xaml = winrt::Microsoft::UI::Xaml;

// The width the text area offers: the block's own less its padding, and
// each element then loses the margin of the paragraph it sits in. Before
// the first layout there is no width yet -- the SizeChanged that follows
// does the fitting. Without wrapping the width goes back to "auto": an
// InlineUIContainer arranges its child at the child's desired size, and
// that natural width is exactly what an unwrapped line has.
void refit(xaml::Controls::RichTextBlock const& block, wide_elements const& wide) {
    const bool wrapping = block.TextWrapping() != xaml::TextWrapping::NoWrap;
    const xaml::Thickness padding = block.Padding();
    const double area = block.ActualWidth() - padding.Left - padding.Right;
    for (wide_elements::entry const& entry : wide.entries) {
        if (!wrapping && entry.width == WideWidth::WhileWrapping) {
            entry.element.Width(std::numeric_limits<double>::quiet_NaN());
        } else if (area > 0) {
            entry.element.Width(std::max(0.0, area - entry.inset));
        }
    }
}

// One fold's hidden paragraphs and the paragraph they return after, shared
// by the toggle's two closures. The block is held weakly: the toggle lives
// inside it, and a strong reference back would be a cycle.
class fold_state : public core::sta_refcounted {
public:
    winrt::weak_ref<xaml::Controls::RichTextBlock> block;
    docs::Block anchor{nullptr};
    std::vector<docs::Block, core::sta_allocator<docs::Block>> hidden;
};

}  // namespace

void FormattedBlock::appendWideElement(FrameworkElement const& element, WideWidth width) const {
    Impl* self = static_cast<Impl*>(impl());
    auto block = get<&Impl::richTextBlock_>();
    const docs::Paragraph paragraph = last_paragraph(block);
    appendElement(element);

    const xaml::Thickness margin = paragraph.Margin();
    self->wide_->entries.push_back({*Object::Impl::get_typed<FrameworkElement>(element),
                                    margin.Left + margin.Right + paragraph.TextIndent(), width});
    if (!self->wideHooked_) {
        self->wideHooked_ = true;
        block.SizeChanged([weak = winrt::make_weak(block), wide = self->wide_](auto const&,
                                                                                auto const&) {
            if (auto strong = weak.get()) refit(strong, *wide);
        });
    }
    refit(block, *self->wide_);
}

void FormattedBlock::textWrapping(TextWrapping value) const {
    base_t::textWrapping(value);
    refit(get<&Impl::richTextBlock_>(), *static_cast<Impl*>(impl())->wide_);
}

void FormattedBlock::beginFold(Expander const& toggle) const {
    Impl* self = static_cast<Impl*>(impl());
    self->folds_.push_back(
        {*Object::Impl::get_typed<Expander>(toggle), get<&Impl::richTextBlock_>().Blocks().Size()});
}

void FormattedBlock::endFold() const {
    Impl* self = static_cast<Impl*>(impl());
    assert(!self->folds_.empty() && "endFold without a matching beginFold");
    if (self->folds_.empty()) return;
    const Impl::open_fold fold = self->folds_.back();
    self->folds_.pop_back();

    auto block = get<&Impl::richTextBlock_>();
    auto blocks = block.Blocks();
    // The toggle's paragraph is the one before the range; without it there
    // is nothing to return after.
    if (fold.start == 0 || fold.start > blocks.Size()) return;

    core::intrusive_ptr<fold_state> state{new fold_state, /*add_ref=*/false};
    state->block = winrt::make_weak(block);
    state->anchor = blocks.GetAt(fold.start - 1);
    for (std::uint32_t i = fold.start; i < blocks.Size(); ++i) state->hidden.push_back(blocks.GetAt(i));
    if (!fold.toggle.IsExpanded()) {
        while (blocks.Size() > fold.start) blocks.RemoveAt(blocks.Size() - 1);
    }

    fold.toggle.Expanding([state](auto const&, auto const&) {
        auto strong = state->block.get();
        if (!strong) return;
        auto blocks = strong.Blocks();
        std::uint32_t at = 0;
        if (!blocks.IndexOf(state->anchor, at)) return;
        ++at;
        for (docs::Block const& paragraph : state->hidden) {
            std::uint32_t present = 0;
            if (blocks.IndexOf(paragraph, present)) {
                at = present + 1;
                continue;
            }
            blocks.InsertAt(at, paragraph);
            ++at;
        }
    });
    fold.toggle.Collapsed([state](auto const&, auto const&) {
        auto strong = state->block.get();
        if (!strong) return;
        auto blocks = strong.Blocks();
        for (docs::Block const& paragraph : state->hidden) {
            std::uint32_t present = 0;
            if (blocks.IndexOf(paragraph, present)) blocks.RemoveAt(present);
        }
    });
}

void FormattedBlock::pushStyle(TextStyle const& style) const {
    static_cast<Impl*>(impl())->styles_.push_back(style);
}

void FormattedBlock::popStyle() const {
    auto& styles = static_cast<Impl*>(impl())->styles_;
    assert(!styles.empty() && "popStyle without a matching pushStyle");
    if (!styles.empty()) {
        styles.pop_back();
    }
}

void FormattedBlock::onLink(LinkHandler handler) const {
    static_cast<Impl*>(impl())->onLink_->set(std::move(handler));
}

void FormattedBlock::onError(ErrorHandler handler) const {
    static_cast<Impl*>(impl())->onError_->set(std::move(handler));
}

}  // namespace wxl
