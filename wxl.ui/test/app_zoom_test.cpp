// wxl::AppZoom -- ступени масштаба и выбор ближайшей.
//
// Обычная программа, а не gtest, по той же причине, что teardown_test:
// AppZoom.h доходит до wxl.core через core.h (include, затем import), а
// стандартные заголовки gtest с этим import в одной единице трансляции не
// уживаются. Итог -- в коде выхода.
//
// Что проверяется: модель начинает со 100 %; лестница -- те самые тринадцать
// ступеней и две трети в ней точные; «крупнее» и «мельче» идут по соседним
// ступеням и стоят на концах; setZoomFactor ставит на ступень, ближайшую по
// отношению, в том числе сохранённое прежде значение ровно на ту же ступень.

#include <cmath>
#include <cstdio>
#include <limits>

#include "AppZoom.h"

namespace {

int failures = 0;

void check(bool ok, char const* what) {
    if (!ok) {
        std::fprintf(stderr, "app_zoom_test: FAILED -- %s\n", what);
        ++failures;
    }
}

double factorAfter(double wanted) {
    auto const zoom = wxl::AppZoom::make();
    zoom->setZoomFactor(wanted);
    return zoom->zoomFactor().get();
}

}  // namespace

int main() {
    {
        auto const zoom = wxl::AppZoom::make();
        check(zoom->zoomFactor().get() == 1.0, "a new model is at 100 %");
        check(zoom->canZoomIn().get() && zoom->canZoomOut().get(), "at 100 % both ways are open");
    }

    {
        double constexpr expected[] {0.5, 2.0 / 3.0, 0.75, 0.8, 0.9, 1.0, 1.1, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0};
        auto const zoom = wxl::AppZoom::make();
        zoom->setZoomFactor(0.0);
        bool ladder = true;
        for (double const level : expected) {
            ladder = ladder && zoom->zoomFactor().get() == level;
            zoom->zoomIn();
        }
        check(ladder, "zoomIn walks the thirteen steps from 50 % to 300 %, two thirds exact");
        check(zoom->zoomFactor().get() == 3.0, "zoomIn stays at 300 %");
        check(!zoom->canZoomIn().get() && zoom->canZoomOut().get(), "at 300 % only zoomOut is open");

        zoom->resetZoom();
        check(zoom->zoomFactor().get() == 1.0, "resetZoom goes back to 100 %");

        for (int i = 0; i < 20; ++i) zoom->zoomOut();
        check(zoom->zoomFactor().get() == 0.5, "zoomOut stays at 50 %");
        check(zoom->canZoomIn().get() && !zoom->canZoomOut().get(), "at 50 % only zoomIn is open");
    }

    {
        check(factorAfter(1.1) == 1.1, "a saved step comes back as the same step");
        check(factorAfter(0.67) == 2.0 / 3.0, "the old 0.67 lands on two thirds");
        check(factorAfter(1.04) == 1.0 && factorAfter(1.06) == 1.1, "between 100 and 110 % -- the nearer one");
        // 1.37 is nearer 1.25 by difference (0.12 against 0.13) but nearer 1.5
        // by ratio (1.096 against 1.095): the ratio decides.
        check(factorAfter(1.37) == 1.5, "nearest by ratio, not by difference");
        check(factorAfter(0.1) == 0.5 && factorAfter(-2.0) == 0.5, "below 50 % -- 50 %");
        check(factorAfter(10.0) == 3.0 && factorAfter(std::numeric_limits<double>::infinity()) == 3.0,
              "above 300 % -- 300 %");
        check(factorAfter(std::nan("")) == 1.0, "not a number -- 100 %");
    }

    {
        auto const zoom = wxl::AppZoom::make();
        int changes = 0;
        double seen = 0.0;
        zoom->zoomFactor().on_change([&](double factor) noexcept {
            ++changes;
            seen = factor;
        });
        zoom->setZoomFactor(1.25);
        check(changes == 1 && seen == 1.25, "a watcher sees the new step once");
    }

    return failures == 0 ? 0 : 1;
}
