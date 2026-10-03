#include "Layouts.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

using namespace wxl;

namespace gallery {

namespace {

constexpr float infinity = std::numeric_limits<float>::infinity();

float bottomOf(Rect const& rect) { return rect.offset.y + rect.size.height; }

// ---- ActivityFeedLayout ------------------------------------------------------------------------

struct FeedState {
    ActivityFeedOptions options;
    std::vector<Rect> rects;
    int firstRealizedIndex = 0;
};

// Три прямоугольника строки: в чётной -- узкая, узкая, широкая; в нечётной -- широкая, узкая, узкая.
std::vector<Rect> rowBounds(FeedState const& state, int rowIndex, double itemWidth) {
    auto const& options = state.options;
    float const height = options.minItemSize.height;
    float const y = static_cast<float>(rowIndex * (height + options.rowSpacing));
    float const narrow = static_cast<float>(itemWidth);
    float const wide = static_cast<float>(itemWidth * 2 + options.columnSpacing);
    float const gap = static_cast<float>(options.columnSpacing);

    if (rowIndex % 2 == 0) {
        float const second = narrow + gap;
        return {{{0, y}, {narrow, height}}, {{second, y}, {narrow, height}}, {{second + narrow + gap, y}, {wide, height}}};
    }
    float const second = wide + gap;
    return {{{0, y}, {wide, height}}, {{second, y}, {narrow, height}}, {{second + narrow + gap, y}, {narrow, height}}};
}

// ---- VariedImageSizeLayout ---------------------------------------------------------------------

struct VariedState {
    double width = 150;
    int firstIndex = 0;
    int lastIndex = 0;
    double lastAvailableWidth = 0;
    std::vector<double> columnOffsets;
    std::vector<Rect> cachedBounds;
    bool cachedBoundsInvalid = false;
};

int lowestColumn(std::vector<double> const& offsets, double& lowestOffset) {
    int lowest = 0;
    lowestOffset = offsets[0];
    for (int index = 0; index < static_cast<int>(offsets.size()); ++index) {
        if (lowestOffset > offsets[static_cast<size_t>(index)]) {
            lowestOffset = offsets[static_cast<size_t>(index)];
            lowest = index;
        }
    }
    return lowest;
}

int columnCount(VariedState const& state, float availableWidth) {
    return std::max(1, static_cast<int>(availableWidth / state.width));
}

void updateCachedBounds(VariedState& state, float availableWidth) {
    state.columnOffsets.assign(static_cast<size_t>(columnCount(state, availableWidth)), 0.0);
    for (auto& bounds : state.cachedBounds) {
        double offset = 0;
        int const column = lowestColumn(state.columnOffsets, offset);
        float const oldHeight = bounds.size.height;
        bounds = {{static_cast<float>(column * state.width), static_cast<float>(offset)}, {static_cast<float>(state.width), oldHeight}};
        state.columnOffsets[static_cast<size_t>(column)] += oldHeight;
    }
    state.cachedBoundsInvalid = false;
}

int startIndex(VariedState const& state, Rect const& viewport) {
    for (int i = 0; i < static_cast<int>(state.cachedBounds.size()); ++i) {
        auto const& bounds = state.cachedBounds[static_cast<size_t>(i)];
        if (bounds.offset.y < bottomOf(viewport) && bottomOf(bounds) > viewport.offset.y) {
            return i;
        }
    }
    return 0;
}

}  // namespace

CustomVirtualizingLayout activityFeedLayout(ActivityFeedOptions const& options) {
    auto const state = std::make_shared<FeedState>();
    state->options = options;

    return CustomVirtualizingLayout {
        [state](VirtualizingLayoutContext const& context, Size available) {
            auto& options = state->options;
            if (options.minItemSize.width == 0 && options.minItemSize.height == 0) {
                auto const first = context.getOrCreateElementAt(0);
                first.measure({infinity, infinity});
                options.minItemSize = first.desiredSize();
            }

            // Строки одной высоты по три плитки: по окну показа находятся первая и последняя строки.
            Rect const view = context.realizationRect();
            float const rowHeight = static_cast<float>(options.minItemSize.height + options.rowSpacing);
            int const firstRow = std::max(static_cast<int>(view.offset.y / rowHeight) - 1, 0);
            int const lastRow = std::min(static_cast<int>(bottomOf(view) / rowHeight) + 1, context.itemCount() / 3);

            state->rects.clear();
            state->firstRealizedIndex = firstRow * 3;

            double const itemWidth = std::max<double>(options.minItemSize.width, (available.width - options.columnSpacing * 3) / 4);

            // Элемент, который за проход не запросили, WinUI после прохода сам убирает в запас.
            for (int row = firstRow; row < lastRow; ++row) {
                auto const bounds = rowBounds(*state, row, itemWidth);
                for (int column = 0; column < 3; ++column) {
                    auto const container = context.getOrCreateElementAt(row * 3 + column);
                    container.measure(bounds[static_cast<size_t>(column)].size);
                    state->rects.push_back(bounds[static_cast<size_t>(column)]);
                }
            }

            double const extentHeight = (context.itemCount() / 3 - 1) * (options.minItemSize.height + options.rowSpacing) + options.minItemSize.height;
            return Size {static_cast<float>(itemWidth * 4 + options.columnSpacing * 2), static_cast<float>(extentHeight)};
        },
        [state](VirtualizingLayoutContext const& context, Size final) {
            int index = state->firstRealizedIndex;
            for (auto const& rect : state->rects) {
                context.getOrCreateElementAt(index++).arrange(rect);
            }
            return final;
        }};
}

CustomVirtualizingLayout variedImageSizeLayout(double width) {
    auto const state = std::make_shared<VariedState>();
    state->width = width;

    return CustomVirtualizingLayout {
        [state](VirtualizingLayoutContext const& context, Size available) {
            Rect const viewport = context.realizationRect();

            if (available.width != state->lastAvailableWidth || state->cachedBoundsInvalid) {
                updateCachedBounds(*state, available.width);
                state->lastAvailableWidth = available.width;
            }

            if (state->columnOffsets.empty()) {
                state->columnOffsets.assign(static_cast<size_t>(columnCount(*state, available.width)), 0.0);
            }

            state->firstIndex = startIndex(*state, viewport);
            int current = state->firstIndex;
            double nextOffset = -1.0;

            // Плитки меряются от первой в окне показа, пока не дойдём до его низа.
            while (current < context.itemCount() && nextOffset < bottomOf(viewport)) {
                auto const child = context.getOrCreateElementAt(current);
                child.measure({static_cast<float>(state->width), available.height});

                if (current >= static_cast<int>(state->cachedBounds.size())) {
                    // Границ у этой плитки ещё нет: положить её и запомнить.
                    int const column = lowestColumn(state->columnOffsets, nextOffset);
                    float const height = child.desiredSize().height;
                    state->cachedBounds.push_back(
                        {{static_cast<float>(column * state->width), static_cast<float>(nextOffset)}, {static_cast<float>(state->width), height}});
                    state->columnOffsets[static_cast<size_t>(column)] += height;
                } else if (current + 1 == static_cast<int>(state->cachedBounds.size())) {
                    lowestColumn(state->columnOffsets, nextOffset);
                } else {
                    nextOffset = state->cachedBounds[static_cast<size_t>(current + 1)].offset.y;
                }

                state->lastIndex = current;
                ++current;
            }

            double largest = state->columnOffsets[0];
            for (double offset : state->columnOffsets) {
                largest = std::max(largest, offset);
            }
            return Size {available.width, static_cast<float>(largest)};
        },
        [state](VirtualizingLayoutContext const& context, Size final) {
            if (!state->cachedBounds.empty()) {
                for (int index = state->firstIndex; index <= state->lastIndex; ++index) {
                    context.getOrCreateElementAt(index).arrange(state->cachedBounds[static_cast<size_t>(index)]);
                }
            }
            return final;
        },
        [state](VirtualizingLayoutContext const&) {
            // Данные сменились, границы прежних плиток недействительны: пересчитать при следующем проходе.
            state->cachedBounds.clear();
            state->firstIndex = state->lastIndex = 0;
            state->cachedBoundsInvalid = true;
        }};
}

}  // namespace gallery
