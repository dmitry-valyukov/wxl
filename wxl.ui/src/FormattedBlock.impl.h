#pragma once

// The private side of wxl::FormattedBlock. A header rather than a detail of
// FormattedBlock.cpp for the same reason every generated X.impl.h is one:
// the next level down (wxl.html's HtmlBlock) inherits this Impl.

#include <cstdint>
#include <memory>
#include <vector>

#include "FormattedBlock.h"
#include "generated/Microsoft.UI.Xaml.Controls.impl.h"

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Documents.h>
#include <winrt/Microsoft.UI.Xaml.h>

namespace wxl {

// The box a WinRT-side closure shares with the Impl: it holds the current
// handler, the closures hold the box -- a Hyperlink's Click, an Image's
// ImageFailed. Reference-counted without interlocked operations and
// allocated from the STA pool, both on the same single-main-STA-thread
// guarantee as Object::Impl itself -- closures die with their elements, on
// that one thread. One template for every handler the block hands out this
// way: onLink and onError share the mechanics.
template <typename Handler>
class handler_box : public core::sta_refcounted {
public:
    // Replacing drops the old handler, and every existing closure sees the new
    // one at its next call.
    void set(Handler handler) noexcept { handler_ = std::move(handler); }

    // The current handler, or nothing while none was set -- which is the box's
    // whole starting state, and why this is nullable: a core::function is never
    // empty, so the box is what admits to having no handler.
    core::nullable<Handler> const& get() const noexcept { return handler_; }

    // Calls the handler with the args, when one is set; silence otherwise.
    template <typename... Args>
    void invoke(Args&&... args) const {
        if (handler_) (*handler_)(std::forward<Args>(args)...);
    }

private:
    core::nullable<Handler> handler_;
};

// The elements that follow the width of the text area (appendWideElement),
// shared with the block's SizeChanged closure the way the handler boxes
// are shared: the closure holds the list, never the Impl.
class wide_elements : public core::sta_refcounted {
public:
    struct entry {
        winrt::Microsoft::UI::Xaml::FrameworkElement element;
        double inset;  // the horizontal margin and indent of the paragraph around it
        WideWidth width;
    };

    std::vector<entry, core::sta_allocator<entry>> entries;
};

class FormattedBlock::Impl : public base_t::Impl {
public:
    using base_t::Impl::Impl;

    // No interface field of its own: the WinRT object behind this level is
    // the same RichTextBlock the base level already holds. What this level
    // adds is wxl-side state.

    // The style stack pushStyle()/popStyle() maintain; the effective style
    // of an append is the merge of these, innermost last. On the STA pool:
    // pushes repeat for every styled fragment of every message.
    std::vector<TextStyle, core::sta_allocator<TextStyle>> styles_;

    // The open link between pushLink() and popLink(); text appends go into
    // it instead of the paragraph. One at most -- links do not nest. The
    // target is kept beside it so an element the Hyperlink cannot hold (a
    // break, an image) can split the link and reopen it with the same
    // target.
    winrt::Microsoft::UI::Xaml::Documents::Hyperlink link_{nullptr};
    sta_wstring linkTarget_;

    // The handlers, boxed and shared: a closure on a WinRT element holds
    // the box, not the Impl -- an element that outlives the wrapper (the
    // XAML tree owns the control, not us) still finds the handler, and no
    // closure ever points back at this Impl to keep it alive in a cycle.
    // Adopted, not add-ref'd: refcounted starts at one.
    core::intrusive_ptr<handler_box<LinkHandler>> onLink_{new handler_box<LinkHandler>,
                                                          /*add_ref=*/false};
    core::intrusive_ptr<handler_box<ErrorHandler>> onError_{new handler_box<ErrorHandler>,
                                                            /*add_ref=*/false};

    // The wide elements, and whether the SizeChanged that refits them is
    // subscribed yet -- once, on the first of them.
    core::intrusive_ptr<wide_elements> wide_{new wide_elements, /*add_ref=*/false};
    bool wideHooked_ = false;

    // The folds being built, innermost last: the toggle and the block count
    // at beginFold(), which is where the hidden paragraphs start.
    struct open_fold {
        winrt::Microsoft::UI::Xaml::Controls::Expander toggle;
        std::uint32_t start;
    };

    std::vector<open_fold, core::sta_allocator<open_fold>> folds_;
};

}  // namespace wxl
