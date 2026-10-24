// wxl::FocusEffect -- the composition behind it; see FocusEffect.h.
//
// The projection comes first, and with it every standard header it needs: the
// wxl headers below carry the wxl.core import.
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.UI.ViewManagement.h>

#include <algorithm>
#include <vector>

#include "FocusEffect.h"
#include "Object.impl.h"
#include <wxl/Microsoft.UI.Xaml.impl.h>
#include "impl/effect_helpers.h"

namespace wxl {

namespace composition = winrt::Microsoft::UI::Composition;
namespace xaml = winrt::Microsoft::UI::Xaml;

namespace {

// How far the element rises: the scale it grows to and the pixels it moves up.
constexpr float liftScale = 1.03f;

// Corner radius of the plate under a focused element, the controls' own
// (ControlCornerRadius).
constexpr float plateRadius = 4.0f;

bool has(composition::CompositionPropertySet const& set, wchar_t const* name) {
    float value = 0.0f;
    return set.TryGetScalar(name, value) == composition::CompositionGetValueStatus::Succeeded;
}

}  // namespace

struct FocusEffect::Impl : core::sta_refcounted {
    FocusLook look = FocusLook::Lift;

    winrt::event_token got{};
    winrt::event_token lost{};

    // What every fitted element shares, made with the first one. Expressions
    // read `p`, the element's own property set, set just before each start.
    composition::Compositor compositor{nullptr};
    composition::ExpressionAnimation scale{nullptr};
    composition::ExpressionAnimation centre{nullptr};
    composition::ExpressionAnimation rise{nullptr};
    composition::ExpressionAnimation level{nullptr};
    composition::ExpressionAnimation liftOffset{nullptr};
    composition::ExpressionAnimation liftBlur{nullptr};
    composition::ExpressionAnimation liftOpacity{nullptr};
    composition::ExpressionAnimation glowBlur{nullptr};
    composition::ExpressionAnimation glowOpacity{nullptr};
    composition::ScalarKeyFrameAnimation levelUp{nullptr};
    composition::ScalarKeyFrameAnimation levelDown{nullptr};
    composition::ScalarKeyFrameAnimation scaleUp{nullptr};
    composition::ScalarKeyFrameAnimation scaleDown{nullptr};

    // An element fitted once, held weakly -- the element, since a composition
    // visual gives no weak reference -- and the shadow whose colour and motion
    // follow the look of each focus.
    struct Attachment {
        winrt::weak_ref<xaml::UIElement> element;
        composition::DropShadow shadow{nullptr};
    };
    std::vector<Attachment> attachments;

    ~Impl() {
        if (got) {
            xaml::Input::FocusManager::GotFocus(got);
        }
        if (lost) {
            xaml::Input::FocusManager::LostFocus(lost);
        }
    }

    void prepare(composition::Compositor const& owner) {
        if (compositor) {
            return;
        }
        compositor = owner;
        scale = compositor.CreateExpressionAnimation(L"Vector3(p.FocusScale, p.FocusScale, 1)");
        centre = compositor.CreateExpressionAnimation(
            L"Vector3(this.Target.Size.X * 0.5, this.Target.Size.Y * 0.5, 0)");
        rise = compositor.CreateExpressionAnimation(L"Vector3(0, -p.FocusRise * p.FocusLevel, 0)");
        level = compositor.CreateExpressionAnimation(L"p.FocusLevel");
        liftOffset = compositor.CreateExpressionAnimation(L"Vector3(0, 1 + p.FocusLevel, 0)");
        liftBlur = compositor.CreateExpressionAnimation(L"2 + 4 * p.FocusLevel");
        liftOpacity = compositor.CreateExpressionAnimation(L"0.25 * p.FocusLevel");
        glowBlur = compositor.CreateExpressionAnimation(L"4 + 8 * p.FocusLevel");
        glowOpacity = compositor.CreateExpressionAnimation(L"0.9 * p.FocusLevel");

        // Up on an S with a small spring past the end, the way a lifted thing
        // settles; down on the plain S, a little faster.
        auto const curve = compositor.CreateCubicBezierEasingFunction({0.33f, 0.0f}, {0.67f, 1.0f});
        auto const spring = composition::CompositionEasingFunction::CreateBackEasingFunction(
            compositor, composition::CompositionEasingFunctionMode::Out, 0.4f);
        auto const motion = [this](float to, composition::CompositionEasingFunction const& easing, int ms) {
            auto const animation = compositor.CreateScalarKeyFrameAnimation();
            animation.Duration(std::chrono::milliseconds{ms});
            animation.InsertKeyFrame(1.0f, to, easing);
            return animation;
        };
        levelUp = motion(1.0f, curve, 220);
        levelDown = motion(0.0f, curve, 160);
        scaleUp = motion(liftScale, spring, 300);
        scaleDown = motion(1.0f, curve, 160);
    }

    Attachment* find(xaml::UIElement const& element) {
        std::erase_if(attachments, [](Attachment const& each) { return !each.element.get(); });
        auto const found = std::ranges::find_if(attachments, [&element](Attachment const& each) { return each.element.get() == element; });
        return found == attachments.end() ? nullptr : &*found;
    }

    // The first focus from the keyboard: the element's own level and scale,
    // the plate and the shadow under it.
    Attachment& fit(xaml::UIElement const& element, composition::Visual const& visual) {
        auto const own = visual.Properties();
        own.InsertScalar(L"FocusLevel", 0.0f);
        own.InsertScalar(L"FocusRise", 0.0f);
        // An element that grows under the pointer already reads FocusScale in
        // its Scale; any other gets the expression here.
        bool const magnified = has(own, L"MagnifyProgress");
        if (!magnified) {
            own.InsertScalar(L"FocusScale", 1.0f);
            visual.StartAnimation(L"CenterPoint", centre);
            scale.SetReferenceParameter(L"p", own);
            visual.StartAnimation(L"Scale", scale);
        }
        xaml::Hosting::ElementCompositionPreview::SetIsTranslationEnabled(element, true);
        rise.SetReferenceParameter(L"p", own);
        visual.StartAnimation(L"Translation", rise);

        // The plate: a light rounded card under the element's own pixels, so
        // a transparent button rises as a card rather than as its glyph.
        auto const geometry = compositor.CreateRoundedRectangleGeometry();
        geometry.CornerRadius({plateRadius, plateRadius});
        auto const size = compositor.CreateExpressionAnimation(L"host.Size");
        size.SetReferenceParameter(L"host", visual);
        geometry.StartAnimation(L"Size", size);
        auto const shape = compositor.CreateSpriteShape(geometry);
        shape.FillBrush(compositor.CreateColorBrush(winrt::Windows::UI::Color{255, 255, 255, 255}));
        auto const plate = compositor.CreateShapeVisual();
        plate.Shapes().Append(shape);
        plate.RelativeSizeAdjustment({1.0f, 1.0f});
        level.SetReferenceParameter(L"p", own);
        plate.StartAnimation(L"Opacity", level);

        // The shadow is cast by the element's rectangle; its look is set on
        // each focus.
        auto const shadow = compositor.CreateDropShadow();
        shadow.Mask(compositor.CreateColorBrush(winrt::Windows::UI::Color{255, 255, 255, 255}));
        shadow.Opacity(0.0f);
        auto const cast = compositor.CreateSpriteVisual();
        cast.Shadow(shadow);
        cast.RelativeSizeAdjustment({1.0f, 1.0f});

        impl::when_drawn(visual.as<composition::ContainerVisual>(), [cast, plate](auto const& children) {
            impl::insert_layer(children, cast, -2);
            impl::insert_layer(children, plate, -1);
        });

        return attachments.emplace_back(Attachment{winrt::make_weak(element), shadow});
    }

    void focused(xaml::UIElement const& element) {
        if (element.FocusState() != xaml::FocusState::Keyboard) {
            return;
        }
        auto const visual = xaml::Hosting::ElementCompositionPreview::GetElementVisual(element);
        auto const own = visual.Properties();
        if (has(own, L"FocusExcluded")) {
            return;
        }
        // The system frame is off on an element while the effect shows its
        // focus, and back at its own thickness when the look is the system's.
        bool const system = look == FocusLook::System;
        if (system && (!compositor || !find(element))) {
            return;
        }
        if (auto const framed = element.try_as<xaml::FrameworkElement>()) {
            double const primary = system ? 2.0 : 0.0;
            double const secondary = system ? 1.0 : 0.0;
            framed.FocusVisualPrimaryThickness({primary, primary, primary, primary});
            framed.FocusVisualSecondaryThickness({secondary, secondary, secondary, secondary});
        }
        if (system) {
            return;
        }
        prepare(visual.Compositor());
        Attachment* entry = find(element);
        if (!entry) {
            entry = &fit(element, visual);
        }
        own.InsertScalar(L"FocusRise", look == FocusLook::Lift ? 1.0f : 0.0f);

        auto const& shadow = entry->shadow;
        for (auto const& expression : {liftOffset, liftBlur, liftOpacity, glowBlur, glowOpacity}) {
            expression.SetReferenceParameter(L"p", own);
        }
        if (look == FocusLook::Lift) {
            shadow.Color(winrt::Windows::UI::Color{255, 0, 0, 0});
            shadow.StartAnimation(L"Offset", liftOffset);
            shadow.StartAnimation(L"BlurRadius", liftBlur);
            shadow.StartAnimation(L"Opacity", liftOpacity);
            own.StartAnimation(L"FocusScale", scaleUp);
        } else {
            winrt::Windows::UI::ViewManagement::UISettings const settings;
            shadow.Color(settings.GetColorValue(winrt::Windows::UI::ViewManagement::UIColorType::Accent));
            shadow.StopAnimation(L"Offset");
            shadow.Offset({0.0f, 0.0f, 0.0f});
            shadow.StartAnimation(L"BlurRadius", glowBlur);
            shadow.StartAnimation(L"Opacity", glowOpacity);
            own.InsertScalar(L"FocusScale", 1.0f);
        }
        own.StartAnimation(L"FocusLevel", levelUp);
    }

    void unfocused(xaml::UIElement const& element) {
        auto const visual = xaml::Hosting::ElementCompositionPreview::GetElementVisual(element);
        if (!compositor || !find(element)) {
            return;
        }
        auto const own = visual.Properties();
        own.StartAnimation(L"FocusLevel", levelDown);
        own.StartAnimation(L"FocusScale", scaleDown);
    }
};

FocusEffect::FocusEffect() : impl_{new Impl, /*add_ref=*/false} {}

FocusEffect::FocusEffect(FocusLook look) : FocusEffect() {
    this->look(look);
}

FocusEffect::FocusEffect(FocusEffect const& other) noexcept = default;
FocusEffect& FocusEffect::operator=(FocusEffect const& other) noexcept = default;
FocusEffect::~FocusEffect() = default;

void FocusEffect::look(FocusLook value) const {
    impl_->look = value;
}

void FocusEffect::operator()(CompositionWindow const&) const {
    if (impl_->got) {
        return;
    }
    // The handlers hold the state by address: the state revokes them when the
    // last copy of the effect goes.
    impl_->got = xaml::Input::FocusManager::GotFocus([impl = impl_.get()](auto&&, xaml::Input::FocusManagerGotFocusEventArgs const& args) {
        if (auto const element = args.NewFocusedElement().try_as<xaml::UIElement>()) {
            impl->focused(element);
        }
    });
    impl_->lost = xaml::Input::FocusManager::LostFocus([impl = impl_.get()](auto&&, xaml::Input::FocusManagerLostFocusEventArgs const& args) {
        if (auto const element = args.OldFocusedElement().try_as<xaml::UIElement>()) {
            impl->unfocused(element);
        }
    });
}

void FocusEffect::exclude(UIElement const& wrapper) {
    auto const element = Object::Impl::as<xaml::UIElement>(wrapper);
    xaml::Hosting::ElementCompositionPreview::GetElementVisual(element).Properties().InsertScalar(L"FocusExcluded", 1.0f);
    if (auto const framed = element.try_as<xaml::FrameworkElement>()) {
        framed.FocusVisualPrimaryThickness({0, 0, 0, 0});
        framed.FocusVisualSecondaryThickness({0, 0, 0, 0});
    }
}

}  // namespace wxl
