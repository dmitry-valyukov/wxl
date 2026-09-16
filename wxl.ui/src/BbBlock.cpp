// wxl::BbBlock: разметка в parse() модуля wxl.bb, наполнение — общим
// сборщиком семейства (MarkupBlock.cpp).

// Проекция первой (см. MarkupBlock.cpp о порядке заголовков).
#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include "BbBlock.impl.h"
#include "Object.impl.h"
#include "impl/activation_factory.h"

// Импорты последними — markup_build.h несёт wxl.html, wxl.bb добавляем сами.
#include "markup_build.h"

import wxl.bb;

namespace wxl {

namespace impl {

template <>
struct runtime_class_name_of<BbBlock> {
    static constexpr wchar_t value[] = L"Microsoft.UI.Xaml.Controls.RichTextBlock";
};

}  // namespace impl

BbBlock::BbBlock(Impl* impl) noexcept : base_t(impl) {}

BbBlock::BbBlock() : base_t(new Impl{}) {
    impl::ActivationFactory<BbBlock>::activate(put_abi());
}

void BbBlock::bb(std::wstring_view markup) const {
    clear();
    append(markup);
}

void BbBlock::append(std::wstring_view markup) const {
    const html::document parsed = wxl::bb::parse(markup);
    build_markup(*this, *static_cast<Impl*>(impl()), parsed);
}

}  // namespace wxl
