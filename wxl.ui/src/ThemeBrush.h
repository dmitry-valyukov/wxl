#pragma once

// wxl::ThemeBrush -- one of the framework's own brushes, replaced for one
// element and everything under it.
//
// What a control looks like while something is happening to it -- the pointer
// is over it, it is pressed, it is disabled -- is its template's business,
// and the template does not read the properties the application set. For the
// duration of the state it puts a brush of its own in place of Background,
// and it finds that brush *by name*, in the theme's dictionary. So a key
// painted almost black turns pale grey under the pointer whatever its
// Background says: the name resolved to the theme's brush and not to ours.
//
// This is the name resolving to ours. A resource written on an element is
// found before the theme's own by the templates of that element and of
// everything under it -- what the framework calls lightweight styling. The
// states stay the framework's to drive; only the colour becomes the
// application's.
//
//     Button {
//         background = face,
//         ThemeBrush {L"ButtonBackgroundPointerOver", litFace},
//         ThemeBrush {L"ButtonBackgroundPressed", dimFace},
//     }
//
// The names are the framework's, spelled the way it spells them --
// `ButtonBackgroundPointerOver`, `TextControlBorderBrushFocused` -- and wxl
// does not enumerate them: which of them exist belongs to the version of the
// framework the application runs against, and they are documented under
// "lightweight styling". A name nothing ever looks up is a brush that does
// nothing, not an error.
//
// Which element to write it on is a choice with a cost either way. On a
// common ancestor one dictionary serves every control under it, and they all
// get the same colour; on the control itself each gets its own, which is what
// a keypad of three kinds of key needs.

#include <string_view>

#include "core.h"
#include "generated/Microsoft.UI.Xaml.Media.h"
#include "generated/Microsoft.UI.Xaml.h"
#include "impl/member.h"
#include "string_param.h"

namespace wxl {

class ThemeBrush {
public:
    // The key still kept in wchar_t, because the dictionary lookup in
    // ThemeBrush.cpp is written that way; what changed is the door, so a
    // caller may spell the name in either unit.
    ThemeBrush(string_param key, Brush brush) : key_(key.wide()), brush_(std::move(brush)) {}

    template <typename Obj>
        requires std::derived_from<Obj, FrameworkElement>
    void operator()(Obj const& object) const {
        write(object);
    }

private:
    // The winrt side, in ThemeBrush.cpp: the dictionary is reached through
    // the projection, which no public header may name. A member rather than
    // a free function next door -- there is a class here that owns the
    // operation, and the two fields it needs are its own.
    void write(FrameworkElement const& element) const;

    // The name is kept rather than the caller's view of it: this is written
    // in the braces of a description, and a description outlives the
    // expression that built it -- a preset is worn long after.
    core::sta_wstring key_;

    Brush brush_;
};

namespace impl {

// A description token like any other, so the comma between two of them is
// the deleted one -- see impl/member.h.
template <>
inline constexpr bool describes<ThemeBrush> = true;

}  // namespace impl

}  // namespace wxl
