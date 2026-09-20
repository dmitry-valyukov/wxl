#pragma once

// Заголовки, которые нужны каждому файлу образца: декларативная поверхность,
// готовые панели и карточка, два блока разметки (описание — HTML, код —
// RSDN с подсветкой), фигуры и сами эффекты.
//
// HaloEffect.h идёт последним: он единственный здесь несёт `import`, а
// заголовок после импорта должен быть уже разобран.

#include "ui.h"
#include "Card.h"
#include "Panels.h"
#include "HtmlBlock.h"
#include "RsdnBlock.h"
#include "launch.h"
#include "generated/brushes.h"
#include "generated/Microsoft.UI.Xaml.Shapes.h"
#include "MagnifyEffect.h"
#include "HaloEffect.h"
