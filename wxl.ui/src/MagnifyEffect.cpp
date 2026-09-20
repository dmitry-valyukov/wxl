// wxl::MagnifyEffect: the composition behind it, kept out of the header -- see
// MagnifyEffect.h for what it does and why.
//
// The standard headers come first: the wxl headers below carry the wxl.core
// import, and a standard header after that import is one MSVC has already
// seen through the std module.
#include <chrono>

#include "MagnifyEffect.h"
#include "generated/Microsoft.UI.Composition.h"
#include "generated/Microsoft.UI.Xaml.Hosting.h"
#include "generated/Microsoft.UI.Xaml.Media.h"

namespace wxl {

void MagnifyEffect::operator()(UIElement const& element) const {
    auto const visual = ElementCompositionPreview::getElementVisual(element);
    auto const pointer = ElementCompositionPreview::getPointerPositionPropertySet(element);
    auto const compositor = visual.compositor();

    // The base scale XAML draws the element at, about the element's centre.
    ScaleTransform const base{};
    element.renderTransformOrigin({0.5, 0.5});
    element.renderTransform(base);

    auto const hover = visual.properties();
    hover.insertScalar(u"MagnifyHover", 0.0f);
    hover.insertScalar(u"MagnifyBase", 1.0f);

    // The exact centre, the same point XAML scales about: two uniform scales
    // about one point compose into one, whichever is applied first.
    auto const centre = compositor.createExpressionAnimation(
        u"Vector3(this.Target.Size.X * 0.5, this.Target.Size.Y * 0.5, 0)");
    visual.startAnimation(u"CenterPoint", centre);

    // Nearness is measured in the element's own proportions: the offset from
    // the centre divided by the half-width and half-height is 0 at the centre,
    // 0.5 on the inner oval and 1 on the inscribed ellipse, and 2 - 2d maps
    // that onto 1 .. 0. The expression language has no Vector2 divided by a
    // scalar (the animation is refused as an invalid argument), hence the
    // components one by one; the half-sizes have a floor of one so that an
    // element not laid out yet divides by something.
    hover.insertScalar(u"MagnifyFactor", 1.0f);
    auto const factor = compositor.createExpressionAnimation(
        u"1 + (m - 1) * h.MagnifyHover * Clamp(2 - 2 * Length(Vector2("
        u"(p.Position.X - v.Size.X * 0.5) / Max(v.Size.X * 0.5, 1),"
        u" (p.Position.Y - v.Size.Y * 0.5) / Max(v.Size.Y * 0.5, 1))), 0, 1)");
    factor.setScalarParameter(u"m", maxScale_);
    factor.setReferenceParameter(u"h", hover);
    factor.setReferenceParameter(u"p", pointer);
    factor.setReferenceParameter(u"v", visual);
    hover.startAnimation(u"MagnifyFactor", factor);

    // What is shown is base * visual = factor, so the visual carries the
    // quotient.
    auto const scale = compositor.createExpressionAnimation(
        u"Vector3(h.MagnifyFactor / h.MagnifyBase, h.MagnifyFactor / h.MagnifyBase, 1)");
    scale.setReferenceParameter(u"h", hover);
    visual.startAnimation(u"Scale", scale);

    auto const fade = [&compositor](float to, std::chrono::milliseconds time) {
        auto const animation = compositor.createScalarKeyFrameAnimation();
        animation.insertKeyFrame(1.0f, to);
        animation.duration(time);
        return animation;
    };
    auto const in = fade(1.0f, std::chrono::milliseconds{120});
    auto const out = fade(0.0f, std::chrono::milliseconds{220});

    // The base moves in one step, together with the divisor the visual reads,
    // so the size on screen does not jump; both are set from the same handler
    // and reach the screen in the same frame.
    auto const rebase = [hover, base](float to) {
        base.scaleX(to);
        base.scaleY(to);
        hover.insertScalar(u"MagnifyBase", to);
    };

    // An expression holds its references weakly, so the pointer set lives in
    // these closures, which live as long as the element's events do.
    element.add_onPointerEntered([hover, in, pointer, rebase, grown = maxScale_] {
        rebase(grown);
        hover.startAnimation(u"MagnifyHover", in);
    });
    element.add_onPointerExited([hover, out, pointer, rebase] {
        rebase(1.0f);
        hover.startAnimation(u"MagnifyHover", out);
    });
}

}  // namespace wxl
