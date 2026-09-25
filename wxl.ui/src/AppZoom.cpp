#include "AppZoom.h"

namespace wxl {

namespace {

constexpr double levels120[] {1 / 1.728, 1 / 1.44, 1 / 1.2, 1.0, 1.2, 1.44, 1.728, 2.0736};
constexpr double levels125[] {0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0};

// Масштаб по уровням: шаг -- переход на соседний уровень.
template <auto const& levels>
class SteppedZoom final : public AppZoom {
public:
    SteppedZoom() { setLevel(defaultLevel); }

    core::observable<double const>& zoomFactor() override { return zoomFactor_; }
    core::observable<bool const>& canZoomIn() override { return canZoomIn_; }
    core::observable<bool const>& canZoomOut() override { return canZoomOut_; }

    void zoomIn() override {
        if (level_ + 1 < std::size(levels)) {
            setLevel(level_ + 1);
        }
    }

    void zoomOut() override {
        if (level_ > 0) {
            setLevel(level_ - 1);
        }
    }

    void resetZoom() override { setLevel(defaultLevel); }

private:
    // Номер уровня 100 %.
    static constexpr uint32_t defaultLevel = [] {
        uint32_t level = 0;
        while (levels[level] != 1.0) {
            ++level;
        }
        return level;
    }();

    void setLevel(uint32_t level) {
        level_ = level;
        zoomFactor_.set(levels[level]);
        canZoomIn_.set(level + 1 < std::size(levels));
        canZoomOut_.set(level > 0);
    }

    uint32_t level_ = defaultLevel;
    core::observable<double> zoomFactor_;
    core::observable<bool> canZoomIn_;
    core::observable<bool> canZoomOut_;
};

using AppZoom120 = SteppedZoom<levels120>;
using AppZoom125 = SteppedZoom<levels125>;

}  // namespace

core::intrusive_ptr<AppZoom> AppZoom::make(ZoomLevels levels) {
    switch (levels) {
        case zoomLevels120:
            return {new AppZoom120 {}, /*add_ref=*/false};
        case zoomLevels125:
            break;
    }
    return {new AppZoom125 {}, /*add_ref=*/false};
}

}  // namespace wxl
