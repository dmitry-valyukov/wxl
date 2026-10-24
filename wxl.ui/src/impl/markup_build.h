#pragma once

// Внутренняя дверь общего сборщика разметки: наполнить MarkupBlock деревом
// семейства wxl.html. Зовут её контролы-наследники из своих html()/bb()/
// rsdn(); публичной она не станет — тип дерева принадлежит модулю wxl.html,
// которого публичные заголовки библиотеки не знают.
//
// Несёт import модуля, поэтому включается в .cpp последним — после
// системных и всех остальных заголовков (правило импорта MSVC).

#include "MarkupBlock.impl.h"

import wxl.html;

namespace wxl {

// Сливает восстановления парсера в onError и обходит дерево примитивами
// FormattedBlock. Реализация — markup_build.cpp.
void build_markup(MarkupBlock const& block, MarkupBlock::Impl& impl,
                  const html::document& parsed);

}  // namespace wxl
