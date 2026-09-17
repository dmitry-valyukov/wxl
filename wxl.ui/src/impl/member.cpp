#include "member.h"

#include "generated/Microsoft.UI.Xaml.Media.h"

namespace wxl::impl {

solid_color_brush::operator Brush() const {
    return SolidColorBrush{static_cast<Color const&>(*this)};
}

}  // namespace wxl::impl
