#include "ZoomEffect.h"

#include "CompositionWindow.h"

namespace wxl {

ZoomEffect::ZoomEffect() : model_(AppZoom::make()) {}

void ZoomEffect::operator()(CompositionWindow const& window) const {
    window.attachZoom(model_);
}

}  // namespace wxl
