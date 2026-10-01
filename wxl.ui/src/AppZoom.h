#pragma once

// wxl::AppZoom -- масштаб приложения ступенями: что показывать (масштаб, можно
// ли ещё крупнее или мельче) и что делать (крупнее, мельче, обратно к 100 %,
// на ближайшую ступень к числу). Поля -- только на чтение: ступень меняют
// методы, а вид привязывается к полям (BindOutput) и зовёт методы.
//
// Ступени одни на все приложения и не настраиваются -- те же, что в меню
// масштаба Chrome и Edge, от 50 до 300 %:
//
//   50, 67, 75, 80, 90, 100, 110, 125, 150, 175, 200, 250, 300 %,
//
// где 67 % -- ровно две трети. Модель всегда стоит на ступени: значения между
// ступенями у неё не бывает, поэтому «крупнее» и «мельче» всегда ведут на
// соседнюю.
//
// Модель ничего не увеличивает сама: к окну её присоединяет ZoomEffect, и
// окно следует за её масштабом.

#include "core.h"

namespace wxl {

class AppZoom : public core::sta_refcounted {
public:
    /// Нижняя и верхняя ступени -- пределы и для жеста масштабирования.
    static constexpr double minZoomFactor = 0.5;
    static constexpr double maxZoomFactor = 3.0;

    static core::intrusive_ptr<AppZoom> make();

    /// Множитель масштаба: 1 -- это 100 %.
    virtual core::observable<double const>& zoomFactor() = 0;
    virtual core::observable<bool const>& canZoomIn() = 0;
    virtual core::observable<bool const>& canZoomOut() = 0;

    virtual void zoomIn() = 0;
    virtual void zoomOut() = 0;

    /// Обратно к 100 %.
    virtual void resetZoom() = 0;

    /// На ступень, ближайшую к множителю, -- так восстанавливают сохранённый
    /// масштаб и так прилипает к ступени жест масштабирования. Ближайшая --
    /// по отношению, а не по разности: масштаб мультипликативный. Ниже
    /// нижней ступени -- нижняя, выше верхней -- верхняя, не число -- 100 %.
    virtual void setZoomFactor(double factor) = 0;
};

}  // namespace wxl
