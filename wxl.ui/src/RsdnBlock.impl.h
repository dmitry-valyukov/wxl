#pragma once

// Приватная сторона wxl::RsdnBlock: своего состояния нет, всё общее живёт в
// MarkupBlock::Impl (см. HtmlBlock.impl.h — то же устройство).

#include "MarkupBlock.impl.h"
#include "RsdnBlock.h"

namespace wxl {

class RsdnBlock::Impl : public base_t::Impl {
public:
    using base_t::Impl::Impl;
};

}  // namespace wxl
