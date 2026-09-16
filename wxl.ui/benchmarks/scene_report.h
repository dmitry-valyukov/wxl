#pragma once

// The stopwatch and the two tables both scene benchmarks print.
//
// Shared for the same reason the scene's shape is: two numbers are only
// comparable if they were taken and reduced the same way. The applications
// differ in how they describe a window and how they write to one; they do not
// differ in what counts as a millisecond.
//
// Only standard headers here -- this is included before either projection,
// and on the wxl side before the wxl.core import.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

namespace bench {

// steady_clock is QueryPerformanceCounter on Windows, which resolves far
// below the milliseconds a scene takes.
using clock = std::chrono::steady_clock;

inline double milliseconds(clock::duration span) {
    return std::chrono::duration<double, std::milli>(span).count();
}

// What was actually built, read back out of the framework rather than
// asserted from the source: the cards grid is asked how many children it
// received, and each card's panel how many controls it holds. A benchmark
// that does not prove its work happened is measuring whatever the optimiser
// left behind, and here it would also quietly hide the one failure that
// invalidates the whole comparison -- the two scenes drifting apart.
struct tree_count {
    int cards = 0;
    int controls = 0;  // the controls inside the cards, counted card by card
    int elements = 0;  // everything, cards and frame together
};

// The first run apart from the rest, because it is a different question: it
// is what a starting process pays, factories and cold code included, while
// the rest are what the same work costs warm.
struct summary {
    double cold = 0;
    double best = 0;
    double median = 0;
    double mean = 0;
    double worst = 0;
    bool hasRest = false;
};

inline summary summarise(std::vector<double> const& runs) {
    summary result;
    if (runs.empty()) {
        return result;
    }

    result.cold = runs.front();
    if (runs.size() < 2) {
        return result;
    }

    std::vector<double> rest(runs.begin() + 1, runs.end());
    std::sort(rest.begin(), rest.end());

    double sum = 0.0;
    for (double value : rest) {
        sum += value;
    }

    result.best = rest.front();
    result.worst = rest.back();
    result.median = rest[rest.size() / 2];
    result.mean = sum / static_cast<double>(rest.size());
    result.hasRest = true;
    return result;
}

// One table. `unitName` names what a run is divided by -- an element for a
// build, a property write for a pass -- and `unitScale` how many of them one
// run does.
inline void table(summary const& figures, char const* unitName, int units, double unitFactor,
                  char const* unitSuffix) {
    auto const per = [&](double ms) {
        return units > 0 ? ms * unitFactor / units : 0.0;
    };

    std::printf("  %-22s %10s %11s %s\n", "", "total", "per", unitName);
    std::printf("  %-22s %7.3f ms %9.1f %s\n", "first (cold)", figures.cold, per(figures.cold),
                unitSuffix);

    if (!figures.hasRest) {
        return;
    }

    std::printf("  %-22s %7.3f ms %9.1f %s\n", "best of the rest", figures.best,
                per(figures.best), unitSuffix);
    std::printf("  %-22s %7.3f ms %9.1f %s\n", "median of the rest", figures.median,
                per(figures.median), unitSuffix);
    std::printf("  %-22s %7.3f ms %9.1f %s\n", "mean of the rest", figures.mean, per(figures.mean),
                unitSuffix);
    std::printf("  %-22s %7.3f ms\n", "worst of the rest", figures.worst);
}

// The heading and the first measurement: building the scene from nothing.
inline void reportBuild(char const* which,
                        std::vector<double> const& runs,
                        double windowReady,
                        tree_count const& counted,
                        int expectedElements,
                        int expectedEvents) {
    std::printf("\n%s\n", which);
    std::printf("  built from the tree: %d cards, %d controls in them, %d elements, "
                "%d event handlers\n",
                counted.cards, counted.controls, counted.elements, expectedEvents);

    if (counted.elements != expectedElements) {
        std::printf("  !! the tree holds %d elements, the scene describes %d -- "
                    "the two benchmarks are no longer building the same scene\n",
                    counted.elements, expectedElements);
    }

    std::printf("\n  BUILDING THE SCENE -- %zu runs, each let go as its time is taken\n\n",
                runs.size());
    table(summarise(runs), "element", counted.elements, 1000.0, "us");
    std::printf("\n  %-22s %7.3f ms   scene + window, stopped on the line before activate()\n",
                "window ready", windowReady);
    std::fflush(stdout);
}

// And the second: writing to a scene that is already there.
inline void reportPass(std::vector<double> const& passes,
                       int writesPerPass,
                       int writesPerCard,
                       int checksPassed,
                       int checks) {
    std::printf("\n  WRITING TO THE BUILT SCENE -- %zu passes, %d property writes each "
                "(%d per card)\n",
                passes.size(), writesPerPass, writesPerCard);
    std::printf("  every one of them a QueryInterface for cppwinrt; none of them for wxl "
                "after the first pass\n");
    std::printf("  read back after the last pass: %d of %d sampled properties hold what "
                "it wrote\n\n",
                checksPassed, checks);

    if (checksPassed != checks) {
        std::printf("  !! a sampled property does not hold what the last pass wrote -- "
                    "the pass is not doing what this table claims\n\n");
    }

    table(summarise(passes), "write", writesPerPass, 1'000'000.0, "ns");
    std::fflush(stdout);
}

}  // namespace bench
