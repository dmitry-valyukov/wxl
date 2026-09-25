#pragma once

// wxl::AppZoom -- масштаб приложения шагами: что показывать (масштаб, можно ли
// ещё крупнее или мельче) и что делать (крупнее, мельче, обратно к 100 %).
// Поля -- только на чтение: шаг меняют методы, а вид привязывается к полям
// (BindOutput) и зовёт методы.
//
// Реализаций две, по уровням масштаба:
//
//   zoomLevels120 -- каждый шаг крупнее или мельче в 1,2 раза, как уровень
//                    масштаба в Electron: 1,2 в степени от -3 до 4, то есть
//                    58, 69, 83, 100, 120, 144, 173, 207 %;
//   zoomLevels125 -- 50, 75, 100, 125, 150, 175, 200 %.
//
// Модель ничего не увеличивает сама: к окну её присоединяет ZoomEffect, и
// окно следует за её масштабом.

#include <cstdint>

#include "core.h"

namespace wxl {

// Не enum class: значения -- слова в скобках, ZoomEffect {zoomLevels125}.
enum ZoomLevels : uint8_t { zoomLevels120, zoomLevels125 };

class AppZoom : public core::sta_refcounted {
public:
    static core::intrusive_ptr<AppZoom> make(ZoomLevels levels);

    /// Множитель масштаба: 1 -- это 100 %.
    virtual core::observable<double const>& zoomFactor() = 0;
    virtual core::observable<bool const>& canZoomIn() = 0;
    virtual core::observable<bool const>& canZoomOut() = 0;

    virtual void zoomIn() = 0;
    virtual void zoomOut() = 0;

    /// Обратно к 100 %.
    virtual void resetZoom() = 0;
};

}  // namespace wxl
