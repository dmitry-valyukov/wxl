#pragma once

// wxl::ZoomEffect -- масштаб окна с клавиатуры и колеса, присоединяемый к
// CompositionWindow, как MagnifyEffect на элемент:
//
//     auto const zoom = ZoomEffect {};
//     CompositionWindow {
//         zoom,
//         titleBar = {rightHeader = Button {
//             onClick = [zoom] { zoom.model().zoomIn(); },
//             isEnabled = BindOutput {zoom.model().canZoomIn()},
//         }},
//     };
//
// Ctrl и «+» или «-» (основной ряд и цифровой блок), Ctrl+0 и Ctrl+колесо
// двигают модель (AppZoom) на шаг, а окно следует за её масштабом -- весь
// остров разом, как CompositionWindow::zoomFactor. Клавиши окно ловит у корня
// острова раньше элемента в фокусе, колесо -- и тогда, когда его уже взял
// ScrollViewer. Кнопки масштаба видят модель -- model(): зовут её методы и
// привязываются к её полям.
//
// Хендл: копии -- один эффект и одна модель. Ступени масштаба у всех
// приложений одни (AppZoom) и не настраиваются.

#include "AppZoom.h"

namespace wxl {

class CompositionWindow;

class ZoomEffect {
public:
    ZoomEffect();

    AppZoom& model() const { return *model_; }

    /// Присоединяется к окну, когда применяются его скобки.
    void operator()(CompositionWindow const& window) const;

private:
    core::intrusive_ptr<AppZoom> model_;
};

}  // namespace wxl
