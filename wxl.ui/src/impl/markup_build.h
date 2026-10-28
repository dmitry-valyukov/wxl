#pragma once

// Внутренняя дверь общего сборщика разметки: наполнить MarkupBlock деревом
// семейства разбора HTML. Зовут её контролы-наследники из своих html()/bb()/
// rsdn(); публичной она не станет — дерево разметки, html::document из
// wxl.core, публичные заголовки библиотеки не называют.
//
// Несёт import модуля, поэтому включается в .cpp последним — после
// системных и всех остальных заголовков (правило импорта MSVC).

#include "../MarkupBlock.impl.h"

import wxl.core;

namespace wxl::impl {

// Сливает восстановления парсера в onError и обходит дерево примитивами
// FormattedBlock. Реализация — MarkupBlock.cpp.
void build_markup(MarkupBlock const& block, MarkupBlock::Impl& impl, const html::document& parsed);

}  // namespace wxl::impl
