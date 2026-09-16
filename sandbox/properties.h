#pragma once

#include <type_traits>
#include <utility>

#include "pch.h"

template <typename Obj>
inline Obj* cell_ = {};

template <typename Obj, typename T, auto F>
struct Property {
  void operator=(T v) const { F(cell_<Obj>, v); }

  template<class F>
  void operator<<(T f) {
    *this = f();
  }
};

template <typename Obj, typename T>
struct CollectionProperty {
  using value_type = std::decay_t<T>;
  template <typename... U>
    requires(std::convertible_to<U, value_type> && ...)
  void operator[](U&&... values) const {
    if (cell_<Obj>) {
      auto collection = cell_<Obj>->Children();
      (collection.Append(std::forward<U>(values)), ...);
    }
  }
};

template <class Obj, class Props>
struct ObjectBuilder : public Props 
{
  ObjectBuilder(Obj& obj) 
      : obj_(obj)
      , prev_(cell_<Obj>) { 
      cell_<Obj> = &obj_; 
  }

  ObjectBuilder(ObjectBuilder<Obj, Props>&& other)
      : obj_(other.obj_)
      , prev_(other.prev_) {
    assert(other.obj_);
    other.obj_ = nullptr;
  }

  ~ObjectBuilder() { 
      if(obj_)
          cell_<Obj> = prev_;
  }

  Obj& target() { 
      assert(obj_);
      return *obj_; 
  }

 private:
  Obj* obj_;
  Obj* prev_;
};

#define SETTER(P, S, T) \
  inline static Property<Obj, T, [](Obj* o, T v) { o->S(v); }> P {}

#define PROPERTY(P, T) SETTER(P, P, T)

#define COLLECTION(P, T) \
  inline static CollectionProperty<Obj, T> P {}

namespace Properties {
template <typename Obj>
struct Window {
  PROPERTY(Content, const winrt::Microsoft::UI::Xaml::UIElement&);
  PROPERTY(ExtendsContentIntoTitleBar, bool);
  PROPERTY(Title, const winrt::hstring&);
  PROPERTY(SystemBackdrop, const winrt::Microsoft::UI::Xaml::Media::SystemBackdrop&);
  SETTER(TitleBar, SetTitleBar, const winrt::Microsoft::UI::Xaml::UIElement&);
};

template <typename Obj>
struct UIElement {
  PROPERTY(Visibility, winrt::Microsoft::UI::Xaml::Visibility);
  PROPERTY(Opacity, double);
  PROPERTY(Width, double);
  PROPERTY(Height, double);
  PROPERTY(Margin, winrt::Microsoft::UI::Xaml::Thickness);
  PROPERTY(HorizontalAlignment, winrt::Microsoft::UI::Xaml::HorizontalAlignment);
  PROPERTY(VerticalAlignment, winrt::Microsoft::UI::Xaml::VerticalAlignment);
};

template <typename Obj>
struct FrameworkElement : UIElement<Obj> {
  PROPERTY(Name, const winrt::hstring&);
  PROPERTY(Style, const winrt::Microsoft::UI::Xaml::Style&);
  PROPERTY(Tag, const winrt::Windows::Foundation::IInspectable&);
};

template <typename Obj>
struct Control : FrameworkElement<Obj> {
  PROPERTY(Content, const winrt::Microsoft::UI::Xaml::UIElement&);
  PROPERTY(Background, const winrt::Microsoft::UI::Xaml::Media::Brush&);
  PROPERTY(Foreground, const winrt::Microsoft::UI::Xaml::Media::Brush&);
  PROPERTY(BorderBrush, const winrt::Microsoft::UI::Xaml::Media::Brush&);
  PROPERTY(BorderThickness, const winrt::Microsoft::UI::Xaml::Thickness&);
  PROPERTY(Padding, const winrt::Microsoft::UI::Xaml::Thickness&);
};

template <typename Obj>
struct ContentControl : Control<Obj> {
  PROPERTY(Content, const winrt::Windows::Foundation::IInspectable&);
};

template <typename Obj>
struct ButtonBase : ContentControl<Obj> {
  PROPERTY(IsEnabled, bool);
  PROPERTY(IsTabStop, bool);
};

template <typename Obj>
struct Button : ButtonBase<Obj> {
  PROPERTY(Content, const winrt::Windows::Foundation::IInspectable&);
};

template <typename Obj>
struct HyperlinkButton : ButtonBase<Obj> {
  PROPERTY(NavigateUri, const winrt::Windows::Foundation::Uri&);
};

template <typename Obj>
struct ToggleButton : ButtonBase<Obj> {
  PROPERTY(IsChecked, bool);
  PROPERTY(IsThreeState, bool);
};

template <typename Obj>
struct CheckBox : ToggleButton<Obj> {
  PROPERTY(IsThreeState, bool);
};

template <typename Obj>
struct RadioButton : ToggleButton<Obj> {
  PROPERTY(GroupName, const winrt::hstring&);
};

template <typename Obj>
struct AppBarToggleButton : ToggleButton<Obj> {
  PROPERTY(IsCompact, bool);
};

template <typename Obj>
struct Panel : FrameworkElement<Obj> {
  PROPERTY(ChildrenTransitions, const winrt::Microsoft::UI::Xaml::Media::Animation::TransitionCollection&);
  COLLECTION(Children, const winrt::Microsoft::UI::Xaml::UIElement&);
};

template <typename Obj>
struct StackPanel : Panel<Obj> {
  PROPERTY(Orientation, winrt::Microsoft::UI::Xaml::Controls::Orientation);
};

template <typename Obj>
struct Grid : Panel<Obj> {
  PROPERTY(RowSpacing, double);
  PROPERTY(ColumnSpacing, double);
};

template <typename Obj>
struct TextBlock : FrameworkElement<Obj> {
  PROPERTY(Text, const winrt::hstring&);
  PROPERTY(FontSize, double);
  PROPERTY(TextWrapping, winrt::Microsoft::UI::Xaml::TextWrapping);
  PROPERTY(FontFamily, const winrt::Microsoft::UI::Xaml::Media::FontFamily&);
  PROPERTY(FontWeight, const winrt::Windows::UI::Text::FontWeight&);
  PROPERTY(FontStyle, const winrt::Windows::UI::Text::FontStyle&);
  PROPERTY(FontStretch, const winrt::Windows::UI::Text::FontStretch&);
  PROPERTY(CharacterSpacing, std::int32_t);
  PROPERTY(Foreground, const winrt::Microsoft::UI::Xaml::Media::Brush&);
  PROPERTY(TextTrimming, const winrt::Microsoft::UI::Xaml::TextTrimming&);
  PROPERTY(TextAlignment, const winrt::Microsoft::UI::Xaml::TextAlignment&);
  PROPERTY(Padding, const winrt::Microsoft::UI::Xaml::Thickness&);
  PROPERTY(LineHeight, double);
  PROPERTY(LineStackingStrategy, const winrt::Microsoft::UI::Xaml::LineStackingStrategy&);
  PROPERTY(SelectionHighlightColor, const winrt::Microsoft::UI::Xaml::Media::SolidColorBrush&);
  PROPERTY(MaxLines, std::int32_t);
  PROPERTY(TextLineBounds, const winrt::Microsoft::UI::Xaml::TextLineBounds&);
  PROPERTY(TextDecorations, const winrt::Windows::UI::Text::TextDecorations&);
  PROPERTY(SelectionFlyout, const winrt::Microsoft::UI::Xaml::Controls::Primitives::FlyoutBase&);
};

}
