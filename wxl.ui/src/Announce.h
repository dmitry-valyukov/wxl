#pragma once

// wxl::announce -- words for a screen reader, about something that happened on screen.
//
// A change the eyes see and the ears do not -- a color that was set, a list that was filtered -- is told to a
// screen reader through the automation peer of an element: the peer raises a notification, and the reader says
// the text, the way the setting of the user asks (the newest word first, others dropped). This is that, for the
// element the change belongs to; an element with no peer (nothing is listening) is a no-op.
//
// The activity id names the kind of notification, so that a reader can tell a new one of the same kind from
// another one.

#include <wxl/Microsoft.UI.Xaml.h>
#include "hstring_param.h"

namespace wxl {

/// An action was completed: "Rectangle color set to Blue".
void announce(UIElement const& element, hstring_param const& text, hstring_param const& activityId);

/// Something else happened: "Filtered recipes, 12 results".
void announceOther(UIElement const& element, hstring_param const& text, hstring_param const& activityId);

}  // namespace wxl
