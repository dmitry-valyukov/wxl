#pragma once

// Заголовки, которые нужны каждому файлу образца: декларативная поверхность,
// готовые панели и карточка, два блока разметки (описание — HTML, код —
// RSDN с подсветкой), фигуры и сами эффекты.
//
// HaloEffect.h больше не несёт `import`: его композиция ушла в HaloEffect.cpp.

#include "ui.h"
#include "Card.h"
#include "Panels.h"
#include "HtmlBlock.h"
#include "RsdnBlock.h"
#include "launch.h"
#include "generated/brushes.h"
#include "generated/Microsoft.UI.Xaml.Shapes.h"
#include "MagnifyEffect.h"
#include "BevelEffect.h"
#include "HaloEffect.h"
