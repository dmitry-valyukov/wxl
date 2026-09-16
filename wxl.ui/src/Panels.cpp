// See Card.cpp: the bodies of try_as for the hand-written panels, which no
// public header can carry, and the include order the std module demands.
#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include "Object.impl.h"
#include "Panels.h"
#include "generated/Microsoft.UI.Xaml.Controls.impl.h"

namespace wxl {

template Rows Object::try_as<Rows>() const;
template Columns Object::try_as<Columns>() const;

}  // namespace wxl
