#pragma once

// wxl::HeaderedContentControl -- a ContentControl with a header over its content, as the Toolkit's has:
//
//     HeaderedContentControl {header = u"Name", TextBox {}}
//
//   header     the title over the content: text, or an element
//   content    what the control holds (the unnamed child)
//
// A ContentControl that wxl derives from, like SettingsCard (decisions/0465): the header lives in the object, the template
// that shows it is part of it.

#include <wxl/Members.h>
#include <wxl/Microsoft.UI.Xaml.Controls.h>

namespace wxl {

class HeaderedContentControl : public ContentControl {
public:
    HeaderedContentControl();

    template <typename... Setters>
        requires impl::setter_pack<HeaderedContentControl, Setters...>
    explicit HeaderedContentControl(Setters&&... setters) : HeaderedContentControl() {
        (impl::apply_argument(*this, std::forward<Setters>(setters)), ...);
    }

    void header(Object const& value) const;
    void header(hstring_param const& value) const;
    Object header() const;

    using ContentControl::setPositional;
    void setPositional(UIElement const& element) const { content(element); }

protected:
    explicit HeaderedContentControl(Impl* impl) noexcept : ContentControl{impl} {}

    friend class Object::Impl;
};

}  // namespace wxl
