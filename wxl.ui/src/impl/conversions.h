#pragma once

// The conversions a generated member body uses to cross wxl's boundary for
// types that are *not* wrappers: strings, and the WinRT types wxl projects
// onto equivalents of its own (geometry, Color, chrono).
//
// One overload set in each direction -- to_winrt() going in, from_winrt()
// coming out -- so generated code writes the same two names whatever the
// type is, and overload resolution picks the pair. Generated structs add
// their own overloads to these same sets from Structs.impl.h.
//
// Private: this names winrt:: types and is included only by generated
// .cpp files.

#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.h>

#include "../Color.h"
#include "../DateTime.h"
#include "../FontFamily.h"
#include "../ImageSource.h"
#include "../Uri.h"
#include "../generated/collections.h"
#include "../Thickness.h"
#include "../CornerRadius.h"
#include "event_token.h"
#include "../geometry.h"
#include "../string_param.h"
#include "value_box.h"

namespace wxl::impl {

// True when `T` really is the ABI struct `probe` came from: the probe is
// bit-cast across and its fields read back through `check`. What can be said
// about the types alone is asserted by name, because a conjunction that comes
// out false says nothing about which half of it did.
//
// Aggregate-ness is one of those properties, and it is required of the wxl
// type outright rather than compared: a struct of at most eight bytes comes
// back from a function in a register, but only while it stays an aggregate --
// one user-provided constructor, or one data member that is not public, and it
// returns through memory instead. cppwinrt's own Size, Point and Rect carry
// constructors and lose that, which is one of the things wxl gets back by
// declaring these types itself. Anything larger than eight bytes crosses
// through memory anyway, so there the question does not arise.
template <typename T, typename WinRT, typename Check>
consteval bool mirrors(WinRT probe, Check check) {
    static_assert(sizeof(T) == sizeof(WinRT), "the wxl type and the ABI struct differ in size");
    static_assert(alignof(T) == alignof(WinRT),
                  "the wxl type and the ABI struct differ in alignment");
    static_assert(sizeof(T) > 8 || std::is_aggregate_v<T>,
                  "the wxl type is not an aggregate and would not be returned in a register");

    return check(std::bit_cast<T>(probe));
}

// WinRT ticks are 100ns, which is what the projections below say in
// std::chrono's own terms.
using winrt_ticks = std::chrono::duration<int64_t, std::ratio<1, 10'000'000>>;
// A point in time is wxl::DateTime, declared in DateTime.h beside its sentinel.

// Strings, in the one form every generated setter takes. The text arrives as
// char16_t and crosses as wchar_t, which is the same sixteen bits under
// another name; the rename happens in string_param and nowhere else.
//
// What comes back is a *fast-pass* string: WindowsCreateStringReference
// writes a header into the temporary this returns and hands back a handle
// onto the caller's own characters. Nothing is allocated and nothing is
// copied -- which is the whole reason string_param asks for a zero after the
// text (see string_param.h). The temporary lives to the end of the full
// expression, which is exactly as long as a WinRT method may borrow a string:
// one that keeps it duplicates it, and duplicating a fast-pass string is what
// makes a real one.
//
// There is deliberately no overload over a bare std::wstring_view. A view
// makes no promise about the character after it, and an overload taking one
// would take the contract off the type that carries it.
inline winrt::param::hstring to_winrt(string_param value) { return {value.wide()}; }

// A string as the IInspectable an object-typed property takes -- `content =
// u"Click"` and its two dozen relatives.
//
// Through PropertyValue rather than through winrt::box_value, and that is not
// a detour: box_value takes a winrt::hstring, so reaching it would undo the
// fast pass above and put the copy back. CreateString takes the fast-pass
// string itself, so the characters go in as they lie and the box the property
// store keeps is made from them.
inline winrt::Windows::Foundation::IInspectable box_text(string_param value) {
    return winrt::Windows::Foundation::PropertyValue::CreateString(to_winrt(value));
}

inline wstring from_winrt(winrt::hstring const& value) {
    return wstring{reinterpret_cast<char16_t const*>(value.c_str()), value.size()};
}

// The value types wxl declares itself are the ABI structs -- the same
// fields, in the same order, of the same size -- so a value crosses by
// being reinterpreted rather than copied member by member. It is also what
// leaves nothing to do the day wxl emits its own projection.
//
// Each pair is pinned by `mirrors`, which bit-casts a probe carrying a
// distinct value per field and reads the fields back: it catches a field
// added, reordered or resized on either side at compile time, where a size
// check alone would let a reordering through.
inline winrt::Windows::Foundation::Size to_winrt(Size value) {
    return std::bit_cast<winrt::Windows::Foundation::Size>(value);
}

inline Size from_winrt(winrt::Windows::Foundation::Size const& value) {
    return std::bit_cast<Size>(value);
}

inline winrt::Windows::Foundation::Point to_winrt(Point value) {
    return std::bit_cast<winrt::Windows::Foundation::Point>(value);
}

inline Point from_winrt(winrt::Windows::Foundation::Point const& value) {
    return std::bit_cast<Point>(value);
}

inline winrt::Windows::Foundation::Rect to_winrt(Rect const& value) {
    return std::bit_cast<winrt::Windows::Foundation::Rect>(value);
}

inline Rect from_winrt(winrt::Windows::Foundation::Rect const& value) {
    return std::bit_cast<Rect>(value);
}

inline winrt::Windows::UI::Color to_winrt(Color value) {
    return std::bit_cast<winrt::Windows::UI::Color>(value);
}

inline Color from_winrt(winrt::Windows::UI::Color const& value) {
    return std::bit_cast<Color>(value);
}

// A URI. The real one parses its text, so an empty wxl::Uri must not be
// handed to it -- a property left unset stays a null Uri, which is what the
// runtime itself uses for "no link".
inline winrt::Windows::Foundation::Uri to_winrt(Uri const& value) {
    return value.empty() ? nullptr : winrt::Windows::Foundation::Uri{value.text()};
}

inline Uri from_winrt(winrt::Windows::Foundation::Uri const& value) {
    return value ? Uri{value.ToString()} : Uri{};
}

// A font family. The runtime type is built from the name and remembers
// nothing else, so an empty name is a null family -- which is what the
// framework reads as "inherit".
inline winrt::Microsoft::UI::Xaml::Media::FontFamily to_winrt(FontFamily const& value) {
    return value.empty() ? nullptr : winrt::Microsoft::UI::Xaml::Media::FontFamily{value.name()};
}

inline FontFamily from_winrt(winrt::Microsoft::UI::Xaml::Media::FontFamily const& value) {
    // The view is named rather than left to conversion: an hstring reaches
    // one of its own, and that plus string_param's would be two.
    return value ? FontFamily{std::wstring_view{value.Source()}} : FontFamily{};
}

// WinRT's nullable box and wxl's nullable. cppwinrt's IReference<T> already
// converts to and from a std::optional of the *same* T; these add the
// element's own conversion, which is why it arrives as a callable -- the
// element may cross as itself, as a cast, or through one of the overloads
// above, and only the generator knows which.
//
// The parameter is spelled compressed_optional rather than core::nullable
// because an alias template deduces nothing: nullable<T> is whatever the
// selector for T resolved to, and the compiler cannot run that backwards.
template <typename WinRT, typename T, typename S, typename Convert>
WinRT to_reference(core::compressed_optional<T, S> const& value, Convert convert) {
    return value ? WinRT{convert(*value)} : WinRT{nullptr};
}

// A boolean is handed over as one of two objects that already exist -- see
// impl/value_box.h. Every other type still gets a box of its own, because
// every other type has more values than boxes are worth making in advance.
//
// It is std::optional and not a compressed_optional because bool is the one
// type with no spare value to compress into; core::nullable<bool> is this,
// and optional_selector<bool> in compressed_optional.ixx says why.
template <typename WinRT, typename Convert>
WinRT to_reference(std::optional<bool> const& value, Convert) {
    return value ? WinRT{bool_reference(*value)} : WinRT{nullptr};
}

// Coming out, the element type is named by the caller, so the alias works
// here where it could not above.
template <typename T, typename WinRT, typename Convert>
core::nullable<T> from_reference(WinRT const& value, Convert convert) {
    return value ? core::nullable<T>{convert(value.Value())} : core::nullable<T>{};
}

// An image source. Declared here and defined in ImageSource.cpp: unlike
// every other conversion in this file it does real work -- it resolves a
// path against the application's folder and activates a BitmapImage -- and
// that belongs in one translation unit rather than in each one that sets an
// image.
winrt::Microsoft::UI::Xaml::Media::ImageSource to_winrt(ImageSource const& value);
ImageSource from_winrt(winrt::Microsoft::UI::Xaml::Media::ImageSource const& value);

// The four edges. Margin, Padding and BorderThickness are tags over this
// same type, and each of them is one, so the pair below covers them too.
inline winrt::Microsoft::UI::Xaml::Thickness to_winrt(Thickness const& value) {
    return std::bit_cast<winrt::Microsoft::UI::Xaml::Thickness>(value);
}

inline Thickness from_winrt(winrt::Microsoft::UI::Xaml::Thickness const& value) {
    return std::bit_cast<Thickness>(value);
}


// The four corners, arranged like the edges above.
inline winrt::Microsoft::UI::Xaml::CornerRadius to_winrt(CornerRadius const& value) {
    return std::bit_cast<winrt::Microsoft::UI::Xaml::CornerRadius>(value);
}

inline CornerRadius from_winrt(winrt::Microsoft::UI::Xaml::CornerRadius const& value) {
    return std::bit_cast<CornerRadius>(value);
}


// A length of time is wxl's own, because wxl already has one and the whole
// library speaks it: `core::duration` counts the same 100ns ticks WinRT does,
// takes any std::chrono duration implicitly (so `interval = 16ms` is
// unchanged), and hands the rest of wxl -- timers, waits, the loop -- exactly
// what they take. Spelling it out as the chrono type instead put an
// eighty-character type into every generated header for no gain.
//
// The ABI field is signed and core::duration is not, which costs nothing
// real: a TimeSpan in the WinRT surface is a length -- a delay, an interval,
// a time of day -- and those are never negative. wxl's signed span
// (core::time_span) is therefore not what a property is written in; it is
// what differences of two of them are.
inline winrt::Windows::Foundation::TimeSpan to_winrt(core::duration value) {
    return winrt_ticks{static_cast<int64_t>(value.ticks())};
}

inline core::duration from_winrt(winrt::Windows::Foundation::TimeSpan const& value) {
    return core::duration::from_ticks(static_cast<uint64_t>(value.count()));
}

// WinRT counts from 1601 and std::chrono's system_clock from 1970, so the
// two time points differ by more than their unit: winrt::clock is what
// knows the offset, and converting through it is what keeps a DateTime
// meaning the same instant on both sides.
inline winrt::Windows::Foundation::DateTime to_winrt(DateTime value) {
    return winrt::clock::from_sys(value);
}

inline DateTime from_winrt(winrt::Windows::Foundation::DateTime const& value) {
    return winrt::clock::to_sys(value);
}


// The numerics a composition object is written in. cppwinrt spells these
// float2 / float3 / float4x4 -- the C++ names of the metadata types
// Vector2 / Vector3 / Matrix4x4 -- which is the one place a projection
// name and a metadata name differ enough that the generator cannot derive
// one from the other, and the reason these three are projected by hand
// instead of generated.
inline winrt::Windows::Foundation::Numerics::float2 to_winrt(Vector2 value) {
    return std::bit_cast<winrt::Windows::Foundation::Numerics::float2>(value);
}

inline Vector2 from_winrt(winrt::Windows::Foundation::Numerics::float2 const& value) {
    return std::bit_cast<Vector2>(value);
}


inline winrt::Windows::Foundation::Numerics::float3 to_winrt(Vector3 value) {
    return std::bit_cast<winrt::Windows::Foundation::Numerics::float3>(value);
}

inline Vector3 from_winrt(winrt::Windows::Foundation::Numerics::float3 const& value) {
    return std::bit_cast<Vector3>(value);
}


inline winrt::Windows::Foundation::Numerics::float4x4 to_winrt(Matrix4x4 const& value) {
    return std::bit_cast<winrt::Windows::Foundation::Numerics::float4x4>(value);
}

inline Matrix4x4 from_winrt(winrt::Windows::Foundation::Numerics::float4x4 const& value) {
    return std::bit_cast<Matrix4x4>(value);
}

}  // namespace wxl::impl
