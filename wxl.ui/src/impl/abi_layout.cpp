// The layout wxl's own types share with the ABI structs they stand for,
// checked in one translation unit.
//
// Every one of these is a self-check: it says nothing to a caller and cannot
// be provoked by one, it only has to hold for the std::bit_cast conversions in
// conversions.h and event_token.h to be what they claim. So it belongs in a
// test -- and a test is what this is, in the only place that can hold it. The
// cppwinrt projection is private to this library on purpose (a consumer must
// never need a winrt header), so a test target outside cannot name
// winrt::Windows::Foundation::Size to compare wxl::Size against it. What it
// can be is compiled once, which is the whole point: conversions.h is included
// by every generated .cpp, and an assertion at its namespace scope is paid for
// in each of them.
//
// Nothing here runs. If this file compiles, the casts hold.

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.h>
#include <winrt/base.h>

#include "conversions.h"
#include "event_token.h"

namespace wxl::impl {
namespace {

// Geometry and colour: same size, same alignment, same field order -- read by
// the probe values these lambdas check after the cast.
static_assert(mirrors<Size>(winrt::Windows::Foundation::Size{1, 2},
                            [](Size v) { return v.width == 1 && v.height == 2; }));

static_assert(mirrors<Point>(winrt::Windows::Foundation::Point{1, 2},
                             [](Point v) { return v.x == 1 && v.y == 2; }));

static_assert(mirrors<Rect>(winrt::Windows::Foundation::Rect{1, 2, 3, 4}, [](Rect v) {
    return v.offset.x == 1 && v.offset.y == 2 && v.size.width == 3 && v.size.height == 4;
}));

static_assert(mirrors<Color>(winrt::Windows::UI::Color{1, 2, 3, 4}, [](Color v) {
    return v.A == 1 && v.R == 2 && v.G == 3 && v.B == 4;
}));

// The four edges and the four corners, in the order the framework writes them.
static_assert(mirrors<Thickness>(winrt::Microsoft::UI::Xaml::Thickness{1, 2, 3, 4}, [](Thickness v) {
    return v.left == 1 && v.top == 2 && v.right == 3 && v.bottom == 4;
}));

static_assert(mirrors<CornerRadius>(winrt::Microsoft::UI::Xaml::CornerRadius{1, 2, 3, 4},
                                    [](CornerRadius v) {
                                        return v.topLeft == 1 && v.topRight == 2
                                            && v.bottomRight == 3 && v.bottomLeft == 4;
                                    }));

// The numerics a composition object is written in: cppwinrt spells them
// float2 / float3 / float4x4, and the whole of the projection is that they are
// the same bytes under another name.
static_assert(mirrors<Vector2>(winrt::Windows::Foundation::Numerics::float2{1, 2},
                               [](Vector2 v) { return v.x == 1 && v.y == 2; }));

static_assert(mirrors<Vector3>(winrt::Windows::Foundation::Numerics::float3{1, 2, 3},
                               [](Vector3 v) { return v.x == 1 && v.y == 2 && v.z == 3; }));

static_assert(mirrors<Matrix4x4>(
    winrt::Windows::Foundation::Numerics::float4x4{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                                                   15, 16},
    [](Matrix4x4 v) { return v.m11 == 1 && v.m14 == 4 && v.m41 == 13 && v.m44 == 16; }));

// And the subscription token: one int64_t on both sides, which is what makes
// the bit_cast in event_token.h the right tool rather than a reinterpret_cast
// through pointers.
static_assert(sizeof(EventToken) == sizeof(winrt::event_token));
static_assert(alignof(EventToken) == alignof(winrt::event_token));

}  // namespace
}  // namespace wxl::impl
