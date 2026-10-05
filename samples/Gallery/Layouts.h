#pragma once

// Два виртуализирующих layout'а оригинала (WinUIGallery/Layouts): ActivityFeedLayout и
// VariedImageSizeLayout, написанные на wxl::CustomVirtualizingLayout. Всё, что layout помнит от
// прохода к проходу, живёт в том, что захватили функции.

#include "CustomVirtualizingLayout.h"

namespace gallery {

struct ActivityFeedOptions {
    double columnSpacing = 0;
    double rowSpacing = 0;
    /// Нулевой размер -- взять размер первого элемента, как у оригинала.
    wxl::Size minItemSize = {0, 0};
};

/// Лента из строк по три плитки: в чётных строках две узкие и одна широкая, в нечётных наоборот.
wxl::CustomVirtualizingLayout activityFeedLayout(ActivityFeedOptions const& options);

/// Колонки одной ширины; плитка встаёт в самую низкую колонку, высота плитки -- её собственная.
wxl::CustomVirtualizingLayout variedImageSizeLayout(double width);

}  // namespace gallery
