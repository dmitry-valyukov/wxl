#include "ZoomEffect.h"

#include "CompositionWindow.h"

namespace wxl {

ZoomEffect::ZoomEffect() : ZoomEffect(zoomLevels125) {}

ZoomEffect::ZoomEffect(ZoomLevels levels) : model_(AppZoom::make(levels)) {}

void ZoomEffect::operator()(CompositionWindow const& window) const {
    window.attachZoom(model_);
}

}  // namespace wxl
