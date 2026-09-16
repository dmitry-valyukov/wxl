// wxl::RsdnBlock: разметка в parse() модуля wxl.rsdn, наполнение — общим
// сборщиком семейства (MarkupBlock.cpp).

// Проекция первой (см. MarkupBlock.cpp о порядке заголовков).
#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include "Object.impl.h"
#include "RsdnBlock.impl.h"
#include "impl/activation_factory.h"

// Импорты последними — markup_build.h несёт wxl.html, wxl.rsdn добавляем
// сами.
#include "markup_build.h"

import wxl.rsdn;

namespace wxl {

namespace impl {

template <>
struct runtime_class_name_of<RsdnBlock> {
    static constexpr wchar_t value[] = L"Microsoft.UI.Xaml.Controls.RichTextBlock";
};

}  // namespace impl

RsdnBlock::RsdnBlock(Impl* impl) noexcept : base_t(impl) {}

RsdnBlock::RsdnBlock() : base_t(new Impl{}) {
    impl::ActivationFactory<RsdnBlock>::activate(put_abi());
}

void RsdnBlock::rsdn(std::wstring_view markup) const {
    clear();
    append(markup);
}

void RsdnBlock::append(std::wstring_view markup) const {
    const html::document parsed = wxl::rsdn::parse(markup);
    build_markup(*this, *static_cast<Impl*>(impl()), parsed);
}

}  // namespace wxl
