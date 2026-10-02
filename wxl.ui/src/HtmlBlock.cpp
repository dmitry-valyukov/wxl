// wxl::HtmlBlock: разметка в parse() модуля wxl.html, наполнение — общим
// сборщиком семейства (MarkupBlock.cpp).

// Проекция первой (см. MarkupBlock.cpp о порядке заголовков).
#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include "HtmlBlock.impl.h"
#include "Object.impl.h"
#include "impl/activation_factory.h"

// Импорт последним — его несёт markup_build.h.
#include "markup_build.h"

namespace wxl {

namespace impl {

// Обёртка своя, объект за ней — системный RichTextBlock, как у всей семьи.
template <>
struct runtime_class_name_of<HtmlBlock> {
    static constexpr wchar_t value[] = L"Microsoft.UI.Xaml.Controls.RichTextBlock";
};

}  // namespace impl

HtmlBlock::HtmlBlock(Impl* impl) noexcept : base_t(impl) {}

HtmlBlock::HtmlBlock() : base_t(new Impl{}) {
    impl::ActivationFactory<HtmlBlock>::activate(put_abi());
}

void HtmlBlock::html(hstring_param const& markup) const {
    clear();
    append(markup);
}

void HtmlBlock::append(hstring_param const& markup) const {
    const html::document parsed = html::parse(markup.text().view());
    build_markup(*this, *static_cast<Impl*>(impl()), parsed);
}

void HtmlBlock::registerStyle(hstring_param const& name, HtmlStyle const& style) const {
    auto& styles = static_cast<Impl*>(impl())->namedStyles_;
    for (auto& existing : styles) {
        if (existing.name == name.text().view()) {
            existing.style = style;
            return;
        }
    }
    styles.push_back({hstring{name}, style});
}

void HtmlBlock::registerStyles(
    std::initializer_list<std::pair<zstring_view, HtmlStyle>> styles) const {
    for (const auto& [name, style] : styles) registerStyle(name, style);
}

}  // namespace wxl
