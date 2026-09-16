#pragma once

// wxl::RsdnBlock -- разметка RSDN (rsdn.org, клиент Janus), брат HtmlBlock
// над тем же MarkupBlock: парсер — модуль wxl.rsdn, дерево и сборщик —
// общие у семейства. Сверх BB здесь построчные цитаты («AVK> так и было»
// складывается в blockquote с видимым префиксом, стрелки — вложенность),
// языковые теги кода ([c#], [nemerle]…) с подсветкой через wxl.highlight
// (цвета — HtmlTheme, или стили kw/str/com из реестра), смайлы форума
// картинками smiles/*.gif от baseDirectory и адреса прямо в тексте.
//
// Наполнение — как у братьев: rsdn(markup) заменяет всё, append(markup)
// дописывает самодостаточным куском. Тема, база картинок и onError — от
// MarkupBlock.

#include <string_view>

#include "MarkupBlock.h"

namespace wxl {

class RsdnBlock : public MarkupBlock {
    using base_t = MarkupBlock;

public:
    class Impl;

    RsdnBlock();

    template <typename... Setters>
        requires impl::setter_pack<RsdnBlock, Setters...>
    explicit RsdnBlock(Setters&&... setters) : RsdnBlock() {
        (impl::apply_argument(*this, std::forward<Setters>(setters)), ...);
    }

    // Заменяет всё содержимое разобранной разметкой.
    void rsdn(std::wstring_view markup) const;

    // Дописывает кусок после уже показанного, ничего не перечитывая.
    void append(std::wstring_view markup) const;

    // Голая строка в декларативной записи — разметка:
    // RsdnBlock{ L"[q]цитата[/q] ответ" }.
    using base_t::setPositional;
    void setPositional(std::wstring_view markup) const { rsdn(markup); }

protected:
    explicit RsdnBlock(Impl* impl) noexcept;

    friend class Object::Impl;
};

}  // namespace wxl
