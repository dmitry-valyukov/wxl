// See Card.cpp: the bodies of try_as for the hand-written panels, which no
// public header can carry.

#include "Object.impl.h"
#include "Panels.h"
#include <wxl/Microsoft.UI.Xaml.Controls.impl.h>

namespace wxl {

template Rows Object::try_as<Rows>() const;
template Columns Object::try_as<Columns>() const;

}  // namespace wxl
