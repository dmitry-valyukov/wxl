#pragma once

// Приватная сторона wxl::BbBlock: своего состояния нет, всё общее живёт в
// MarkupBlock::Impl (см. HtmlBlock.impl.h — то же устройство).

#include "BbBlock.h"
#include "MarkupBlock.impl.h"

namespace wxl {

class BbBlock::Impl : public base_t::Impl {
public:
    using base_t::Impl::Impl;
};

}  // namespace wxl
