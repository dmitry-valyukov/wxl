#pragma once

// wxl::HaloEffect -- a glow around the glyphs, worn by the element that
// carries it and written in that element's own braces:
//
//     TextBlock {
//         u"0",
//         fontSize = 48,
//         HaloEffect { blurRadius = 14.0f, color = rgb(92, 138, 32) },
//     }
//
// A shadow whose offset is zero is a halo, and the shape it takes is whatever
// its mask is -- here the alpha of the laid-out glyphs, which getAlphaMask()
// hands over. The framework's own ThemeShadow is no use for it: no colour, no
// radius and no offset to zero, being a shadow of height rather than of light.
// So this is composition rather than XAML, and it is the one thing in a window
// that a declarative tree could not say.
//
// **No compositor is asked for.** There is none to be had where this is
// written -- the element is being constructed, and it belongs to no window
// yet. What the effect does instead is wait: XAML draws an element into a
// child of its visual, made on the first frames, and everything the halo needs
// is reachable from that visual once it exists. The waiting is a coroutine
// nobody holds, so writing the effect is the whole of using it.
//
// Its settings are a preset for the DropShadow, which is why they are spelled
// with the same tags a DropShadow's own properties are: whatever is written
// in the braces is applied to the shadow when there is one to apply it to.
// A preset's arguments are its type, so the effect is a template on them,
// deduced from the braces; nothing of that shows at the point of writing.
// The element it is worn by is another matter, and constrained separately:
// anything that hands over the alpha of its glyphs.

#include "core.h"

#include "event_awaitable.h"
#include "generated/Members.h"
#include "generated/Microsoft.UI.Composition.h"
#include "generated/Microsoft.UI.Xaml.Hosting.h"
#include "generated/Microsoft.UI.Xaml.Media.h"
#include "impl/member.h"

import wxl.async;

namespace wxl {

template <typename... Settings>
class HaloEffect
{
    using shape_t = Preset<Settings...>;

public:
    /// The shadow's own settings, in the shadow's own vocabulary: blurRadius,
    /// color, opacity, offset. Nothing here is wxl's invention, and nothing
    /// has to be listed twice.
    explicit HaloEffect(Settings... settings) : shape_{std::move(settings)...} {}

    /// Worn by anything that can hand over the alpha of its own glyphs, which
    /// is what the halo is cut out of.
    template <typename Obj>
        requires requires(Obj const& element) { element.getAlphaMask(); }
    void operator()(Obj const& element) const {
        wear(element, shape_);
    }

private:
    // By value, both of them, and that is the rule rather than a choice here:
    // a coroutine's parameters are copied into its frame while a reference is
    // not, and this one outlives the expression that started it. Both are
    // handles -- a reference count each.
    template <typename Obj>
    static async::task wear(Obj element, shape_t shape) {
        auto const visual = ElementCompositionPreview::getElementVisual(element);
        auto const children = visual.try_as<ContainerVisual>().children();

        // An element draws itself into a child of its visual rather than into
        // the visual, so the glyphs are a sibling of the halo and being under
        // them is a matter of order in this collection. XAML makes that child
        // lazily, on its first frames, and puts it *below* whatever it finds
        // already there -- so the halo waits for it instead of racing it.
        // Added at the bottom afterwards, it lands under the glyphs with
        // nothing to reorder.
        //
        // Waited for by the frame rather than by the element: XAML raises
        // nothing when it gets round to building that child, and its own
        // events stop coming once the window has settled -- so a wait on
        // LayoutUpdated here comes true only if something else provokes a
        // layout, and on a window nobody has touched yet, nothing does.
        auto frames = on_event(&CompositionTarget::add_onRendering,
                               &CompositionTarget::remove_onRendering);
        while (children.count() == 0) {
            co_await frames;
        }

        // And by now there is one to be had: every composition object knows
        // the compositor that made it, and the element's visual was made by
        // the one its window runs on.
        auto const compositor = visual.compositor();

        auto const glow = compositor.createDropShadow();
        shape(glow);
        glow.mask(element.getAlphaMask());

        auto const halo = compositor.createSpriteVisual();
        halo.shadow(glow);

        // No size of its own and nothing to subscribe to: the halo's parent
        // is the element's own visual, XAML keeps that at the element's
        // layout size, and a relative adjustment of one says "the same".
        halo.relativeSizeAdjustment({1.0f, 1.0f});

        children.insertAtBottom(halo);
    }

    shape_t shape_;
};

template <typename... Settings>
    requires impl::setter_pack<DropShadow, Settings...>
HaloEffect(Settings&&...) -> HaloEffect<std::decay_t<Settings>...>;

}  // namespace wxl
