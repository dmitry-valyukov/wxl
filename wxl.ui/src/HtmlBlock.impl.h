#pragma once

// Приватная сторона wxl::HtmlBlock. Своего состояния у этого этажа нет:
// реестр стилей, тема и база картинок живут уровнем ниже, в
// MarkupBlock::Impl, где их читает общий сборщик; класс существует, чтобы
// у контрола был свой тип Impl в цепочке.

#include "HtmlBlock.h"
#include "MarkupBlock.impl.h"

namespace wxl {

class HtmlBlock::Impl : public base_t::Impl {
public:
    using base_t::Impl::Impl;
};

}  // namespace wxl
