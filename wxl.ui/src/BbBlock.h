#pragma once

// wxl::BbBlock -- BB-код форумов, брат HtmlBlock над тем же MarkupBlock:
// парсер — модуль wxl.bb, дерево и сборщик — общие у семейства. Перевод
// строки в BB значим ([b]…[/b] в плоском тексте поста), незнакомый тег
// остаётся буквальным текстом — как показывают его сами форумы.
//
// Наполнение — как у брата: bb(markup) заменяет всё, append(markup)
// дописывает самодостаточным куском; незакрытый [b] в следующий кусок не
// перетекает. Тема, база картинок и onError — от MarkupBlock.

#include <string_view>

#include "MarkupBlock.h"

namespace wxl {

class BbBlock : public MarkupBlock {
    using base_t = MarkupBlock;

public:
    class Impl;

    BbBlock();

    template <typename... Setters>
        requires impl::setter_pack<BbBlock, Setters...>
    explicit BbBlock(Setters&&... setters) : BbBlock() {
        (impl::apply_argument(*this, std::forward<Setters>(setters)), ...);
    }

    // Заменяет всё содержимое разобранной разметкой.
    void bb(hstring_param const& markup) const;

    // Дописывает кусок после уже показанного, ничего не перечитывая.
    void append(hstring_param const& markup) const;

    // Голая строка в декларативной записи — разметка:
    // BbBlock{ u"Привет, [b]мир[/b]!" }.
    using base_t::setPositional;
    void setPositional(hstring_param const& markup) const { bb(markup); }

protected:
    explicit BbBlock(Impl* impl) noexcept;

    friend class Object::Impl;
};

}  // namespace wxl
