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

class SteppedZoom final : public AppZoom {
public:
    SteppedZoom() { setLevel(defaultLevel); }

    core::observable<double const>& zoomFactor() override { return zoomFactor_; }
    core::observable<bool const>& canZoomIn() override { return canZoomIn_; }
    core::observable<bool const>& canZoomOut() override { return canZoomOut_; }

    void zoomIn() override {
        if (level_ + 1 < levelCount) {
            setLevel(level_ + 1);
        }
    }

    void zoomOut() override {
        if (level_ > 0) {
            setLevel(level_ - 1);
        }
    }

    void resetZoom() override { setLevel(defaultLevel); }

    void setZoomFactor(double factor) override { setLevel(nearestLevel(factor)); }

private:
    void setLevel(uint32_t level) {
        level_ = level;
        zoomFactor_.set(levels[level]);
        canZoomIn_.set(level + 1 < levelCount);
        canZoomOut_.set(level > 0);
    }

    uint32_t level_ = defaultLevel;
    core::observable<double> zoomFactor_;
    core::observable<bool> canZoomIn_;
    core::observable<bool> canZoomOut_;
};

}  // namespace

core::intrusive_ptr<AppZoom> AppZoom::make() {
    return {new SteppedZoom {}, /*add_ref=*/false};
}

}  // namespace wxl
