// Nothing but the explicit instantiations the hand-written cards owe the
// linker. try_as is defined in Object.impl.h, which a public header never
// includes, so a handler that names a Card as its sender would otherwise find
// no body to call -- exactly what the generator emits for every class it
// writes.
//
// The projection headers come first, and with them the standard library they
// pull in: the wxl headers below carry the wxl.core import, and a standard
// header after that import is one MSVC has already seen through the std module.
#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include "Card.h"
#include "Object.impl.h"
#include "generated/Microsoft.UI.Xaml.Controls.impl.h"

namespace wxl {

template Card Object::try_as<Card>() const;
template OverlayCard Object::try_as<OverlayCard>() const;

}  // namespace wxl
