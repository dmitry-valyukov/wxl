#pragma once

// Приватная сторона wxl::MarkupBlock: состояние, которое читает общий
// сборщик разметки (markup_build.cpp). Заголовок, а не деталь .cpp, по той
// же причине, что все *.impl.h: наследники (HtmlBlock, BbBlock, RsdnBlock)
// наследуют и этот Impl.

#include <vector>

#include "FormattedBlock.impl.h"
#include "MarkupBlock.h"

namespace wxl {

class MarkupBlock::Impl : public base_t::Impl {
public:
    using base_t::Impl::Impl;

    // Реестр style="имя": плоский список с линейным поиском — приложение
    // регистрирует горсть имён, а не таблицу стилей. На STA-пуле:
    // регистрация повторяется на каждый блок.
    struct named_style {
        sta_wstring name;
        HtmlStyle style;
    };

    std::vector<named_style, core::sta_allocator<named_style>> namedStyles_;

    const HtmlStyle* findStyle(std::wstring_view name) const noexcept {
        for (const named_style& candidate : namedStyles_) {
            if (candidate.name == name) return &candidate.style;
        }
        return nullptr;
    }

    // Вид тегов; сборщик читает при каждом наполнении.
    HtmlTheme theme_;

    // База голых относительных <img src>; пусто — рабочий каталог процесса.
    sta_wstring baseDirectory_;
};

}  // namespace wxl
