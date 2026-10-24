#include "AppZoom.h"

namespace wxl {

namespace {

// Ступени меню масштаба Chrome и Edge от 50 до 300 %. Две трети -- дробью, а
// не 0.67: подпись «67 %» округляет, а множитель должен быть точным.
constexpr double levels[] {AppZoom::minZoomFactor, 2.0 / 3.0, 0.75, 0.8, 0.9, 1.0, 1.1, 1.25, 1.5, 1.75, 2.0,
                           2.5, AppZoom::maxZoomFactor};

// Номер ступени 100 %.
constexpr uint32_t defaultLevel = [] {
    uint32_t level = 0;
    while (levels[level] != 1.0) {
        ++level;
    }
    return level;
}();

constexpr uint32_t levelCount = static_cast<uint32_t>(std::size(levels));

// Ступень, ближайшая к множителю по отношению: из двух соседних та, во
// сколько раз до которой меньше. Поровну -- нижняя.
constexpr uint32_t nearestLevel(double factor) {
    if (std::isnan(factor)) {
        return defaultLevel;
    }
    if (factor <= levels[0]) {
        return 0;
    }
    for (uint32_t upper = 1; upper < levelCount; ++upper) {
        if (factor <= levels[upper]) {
            uint32_t const lower = upper - 1;
            return factor / levels[lower] <= levels[upper] / factor ? lower : upper;
        }
    }
    return levelCount - 1;
}

}  // namespace

AppZoom::AppZoom() { set_level(defaultLevel); }

core::intrusive_ptr<AppZoom> AppZoom::make() {
    return {new AppZoom {}, /*add_ref=*/false};
}

void AppZoom::zoomIn() {
    if (level_ + 1 < levelCount) {
        set_level(level_ + 1);
    }
}

void AppZoom::zoomOut() {
    if (level_ > 0) {
        set_level(level_ - 1);
    }
}

void AppZoom::resetZoom() { set_level(defaultLevel); }

void AppZoom::setZoomFactor(double factor) { set_level(nearestLevel(factor)); }

void AppZoom::set_level(uint32_t level) {
    level_ = level;
    zoom_factor_.set(levels[level]);
    can_zoom_in_.set(level + 1 < levelCount);
    can_zoom_out_.set(level > 0);
}

}  // namespace wxl
