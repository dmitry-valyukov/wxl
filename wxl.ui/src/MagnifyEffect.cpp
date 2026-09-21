// wxl::MagnifyEffect: the composition behind it -- see MagnifyEffect.h for
// what it does and why.
//
// The state works on the cppwinrt projection directly rather than through
// wxl's wrappers: it keeps composition objects for as long as the effect
// lives and calls them on every pointer event, and the projection is what the
// wrappers would forward to anyway.
//
// The projection comes first, and with it every standard header it needs:
// the wxl headers below carry the wxl.core import, and a standard header after
// that import is one MSVC has already seen through the std module.
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Numerics.h>

#include <algorithm>
#include <cmath>
#include <optional>

#include "MagnifyEffect.h"
#include "Object.impl.h"
#include "generated/Microsoft.UI.Xaml.impl.h"

namespace wxl {

namespace composition = winrt::Microsoft::UI::Composition;
namespace xaml = winrt::Microsoft::UI::Xaml;

namespace {
// core::duration counts 100 ns ticks, the same unit as TimeSpan.
winrt::Windows::Foundation::TimeSpan timespan(core::duration d) {
    return winrt::Windows::Foundation::TimeSpan{static_cast<int64_t>(d.ticks())};
}
} // namespace

struct MagnifyEffect::State : core::sta_refcounted {
    // ---- The settings ----
    Size scale{1.2f, 1.2f};
    std::optional<double> maximum;
    std::optional<double> minimum;
    std::optional<core::duration> duration;
    core::duration delay{};

    // ---- What every wearer shares, made on the first one ----
    //
    // The compositor is the XAML one, the same for every element on the one
    // STA thread; the property set carries the scale per axis so that a
    // changed setting reaches running expressions without restarting them.
    composition::Compositor compositor{nullptr};
    composition::CompositionPropertySet shared{nullptr};
    composition::ExpressionAnimation centre{nullptr};
    composition::ExpressionAnimation shown{nullptr};
    composition::ScalarKeyFrameAnimation grow{nullptr};
    composition::ScalarKeyFrameAnimation shrink{nullptr};
    bool stale = true;

    // The axis that moves further from 1: what maximum and minimum are
    // measured on. Below 1 on it the effect shrinks under the pointer.
    double leading() const {
        return std::abs(scale.width - 1.0f) >= std::abs(scale.height - 1.0f) ? scale.width : scale.height;
    }

    void changed() {
        stale = true;
        if (shared) {
            shared.InsertScalar(L"X", scale.width);
            shared.InsertScalar(L"Y", scale.height);
        }
    }

    // The shared objects, made on the first wearer and the motions remade
    // after a setting changed. The motions animate progress: 0 at rest, 1
    // grown, beyond either end for the swing past it.
    void prepare(composition::Compositor const& owner) {
        if (!compositor) {
            compositor = owner;
            shared = compositor.CreatePropertySet();
            shared.InsertScalar(L"X", scale.width);
            shared.InsertScalar(L"Y", scale.height);

            // The exact centre, the same point XAML scales about: two
            // uniform scales about one point compose into one.
            centre = compositor.CreateExpressionAnimation(
                L"Vector3(this.Target.Size.X * 0.5, this.Target.Size.Y * 0.5, 0)");

            // What is shown is base * visual, so the visual carries the
            // quotient. `p` is each wearer's own property set, set just before
            // the expression is started on it.
            shown = compositor.CreateExpressionAnimation(
                L"Vector3((1 + (s.X - 1) * p.MagnifyProgress) / p.MagnifyBaseX,"
                L" (1 + (s.Y - 1) * p.MagnifyProgress) / p.MagnifyBaseY, 1)");
            shown.SetReferenceParameter(L"s", shared);
        }
        if (stale) {
            remake_motions();
            stale = false;
        }
    }

    void remake_motions() {
        // An S -- handles at a third and two thirds -- is what the eye reads
        // as a thing starting and stopping rather than dragged at one speed.
        auto const curve = compositor.CreateCubicBezierEasingFunction({0.33f, 0.0f}, {0.67f, 1.0f});

        // The swing past each end in progress units: 0 at rest, 1 at the
        // scale. maximum is where the way in turns back, minimum the way out,
        // so for a shrinking effect maximum is below the scale and minimum
        // above 1. A value on the wrong side of its end gives no swing.
        double const travel = leading() - 1.0;
        double past_top = 0.0;
        double past_bottom = 0.0;
        if (travel != 0.0) {
            if (maximum) {
                past_top = std::max(0.0, (*maximum - 1.0) / travel - 1.0);
            }
            if (minimum) {
                past_bottom = std::max(0.0, -(*minimum - 1.0) / travel);
            }
        }

        // Two phases when it swings: out to the far point, and the slower
        // settle back. Without a swing the first phase is the whole motion.
        // `duration` sets the first phase; the settle keeps its proportion.
        auto const motion = [&](float to, float swing, std::chrono::milliseconds first_natural,
                                 std::chrono::milliseconds settle_natural) {
            auto const animation = compositor.CreateScalarKeyFrameAnimation();
            double const natural = static_cast<double>(first_natural.count());
            double const first = duration ? static_cast<double>(duration->ticks()) / 10'000.0 : natural;
            double const settle = swing != 0.0f ? first * static_cast<double>(settle_natural.count()) / natural : 0.0;
            double const total = first + settle;
            animation.Duration(timespan(core::duration::from_ticks(static_cast<uint64_t>(total * 10'000.0))));
            if (swing != 0.0f) {
                animation.InsertKeyFrame(static_cast<float>(first / total), to + swing, curve);
            }
            animation.InsertKeyFrame(1.0f, to, curve);
            return animation;
        };
        grow = motion(1.0f, static_cast<float>(past_top), std::chrono::milliseconds{140}, std::chrono::milliseconds{200});
        grow.DelayTime(timespan(delay));
        shrink = motion(0.0f, -static_cast<float>(past_bottom), std::chrono::milliseconds{180}, std::chrono::milliseconds{220});
    }
};

MagnifyEffect::MagnifyEffect() : state_{new State, /*add_ref=*/false} {}

MagnifyEffect::MagnifyEffect(double scale) : MagnifyEffect() {
    this->scale(scale);
}

MagnifyEffect::MagnifyEffect(MagnifyEffect const& other) noexcept = default;
MagnifyEffect& MagnifyEffect::operator=(MagnifyEffect const& other) noexcept = default;
MagnifyEffect::~MagnifyEffect() = default;

void MagnifyEffect::scale(Size value) const {
    state_->scale = value;
    state_->changed();
}

void MagnifyEffect::maximum(double value) const {
    state_->maximum = value;
    state_->changed();
}

void MagnifyEffect::minimum(double value) const {
    state_->minimum = value;
    state_->changed();
}

void MagnifyEffect::duration(core::duration value) const {
    state_->duration = value;
    state_->changed();
}

void MagnifyEffect::delayTime(core::duration value) const {
    state_->delay = value;
    state_->changed();
}

void MagnifyEffect::operator()(UIElement const& wrapper) const {
    auto const element = Object::Impl::as<xaml::UIElement>(wrapper);
    auto const visual = xaml::Hosting::ElementCompositionPreview::GetElementVisual(element);
    state_->prepare(visual.Compositor());

    // The base scale XAML draws the element at, about the element's centre.
    xaml::Media::ScaleTransform const base;
    element.RenderTransformOrigin({0.5f, 0.5f});
    element.RenderTransform(base);

    auto const own = visual.Properties();
    own.InsertScalar(L"MagnifyProgress", 0.0f);
    own.InsertScalar(L"MagnifyBaseX", 1.0f);
    own.InsertScalar(L"MagnifyBaseY", 1.0f);

    visual.StartAnimation(L"CenterPoint", state_->centre);
    state_->shown.SetReferenceParameter(L"p", own);
    visual.StartAnimation(L"Scale", state_->shown);

    // The base moves in one step, together with the divisor the visual reads,
    // so the size on screen does not jump; both are set from the same handler
    // and reach the screen in the same frame. It is set to where the motion
    // ends, which is why what the eye finally sees is XAML's own drawing at
    // that scale and nothing resampled.
    auto const rebase = [own, base](float x, float y) {
        base.ScaleX(x);
        base.ScaleY(y);
        own.InsertScalar(L"MagnifyBaseX", x);
        own.InsertScalar(L"MagnifyBaseY", y);
    };

    // The state is held by the handlers, not the element: it owns no element,
    // so nothing here is a cycle.
    element.PointerEntered([state = state_, own, rebase](auto&&, auto&&) {
        state->prepare(state->compositor);
        rebase(state->scale.width, state->scale.height);
        own.StartAnimation(L"MagnifyProgress", state->grow);
    });
    element.PointerExited([state = state_, own, rebase](auto&&, auto&&) {
        state->prepare(state->compositor);
        rebase(1.0f, 1.0f);
        own.StartAnimation(L"MagnifyProgress", state->shrink);
    });
}

}  // namespace wxl
