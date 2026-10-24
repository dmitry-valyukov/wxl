#include <atomic>
#include <string>

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Collections.h>

#include "ItemTemplate.h"
#include "Object.impl.h"
#include <wxl/Microsoft.UI.Xaml.Controls.impl.h>
#include <wxl/Microsoft.UI.Xaml.impl.h>
#include "impl/conversions.h"

namespace wxl {

namespace xaml = winrt::Microsoft::UI::Xaml;

namespace {

// The call of one function from XAML: the value of the binding is the item, the result is the value of the property.
struct Converter : winrt::implements<Converter, xaml::Data::IValueConverter> {
    explicit Converter(std::function<Object(Object const&)> make) : make_(std::move(make)) {}

    winrt::Windows::Foundation::IInspectable Convert(winrt::Windows::Foundation::IInspectable const& value,
                                                     winrt::Windows::UI::Xaml::Interop::TypeName const&,
                                                     winrt::Windows::Foundation::IInspectable const&,
                                                     winrt::hstring const&) const {
        Object const result = make_(Object::Impl::wrap<Object>(value));
        winrt::Windows::Foundation::IInspectable native{nullptr};
        if (result) {
            winrt::copy_from_abi(native, result.get_abi());
        }
        return native;
    }

    winrt::Windows::Foundation::IInspectable ConvertBack(winrt::Windows::Foundation::IInspectable const&,
                                                         winrt::Windows::UI::Xaml::Interop::TypeName const&,
                                                         winrt::Windows::Foundation::IInspectable const&,
                                                         winrt::hstring const&) const {
        throw winrt::hresult_not_implemented();
    }

private:
    std::function<Object(Object const&)> make_;
};

winrt::hstring text_of(std::u16string_view text) {
    return winrt::hstring{reinterpret_cast<wchar_t const*>(text.data()), static_cast<uint32_t>(text.size())};
}

}  // namespace

DataTemplate boundTemplate(hstring_param const& root, std::vector<BoundProperty> bindings) {
    // A template is loaded as text and finds its converters by key among the resources of the application; the
    // keys are numbered because every template has converters of its own.
    static std::atomic<int> counter{0};
    auto const resources = xaml::Application::Current().Resources();

    std::wstring xaml_text = L"<DataTemplate xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation'><";
    xaml_text += impl::to_winrt(root).c_str();
    for (auto& binding : bindings) {
        auto const key = L"wxl.bound." + std::to_wstring(++counter);
        resources.Insert(winrt::box_value(winrt::hstring{key}), winrt::make<Converter>(std::move(binding.make)));
        xaml_text += L" ";
        xaml_text += text_of(binding.property).c_str();
        xaml_text += L"='{Binding Converter={StaticResource " + key + L"}}'";
    }
    xaml_text += L"/></DataTemplate>";

    return Object::Impl::wrap<DataTemplate>(xaml::Markup::XamlReader::Load(xaml_text).as<xaml::DataTemplate>());
}

core::nullable<UIElement> itemElement(ListViewBase const& list, Object const& item) {
    winrt::Windows::Foundation::IInspectable native_item{nullptr};
    winrt::copy_from_abi(native_item, item.get_abi());

    // The container's content is a placeholder control (see impl/item_template.cpp) that holds the element; a miss at any step
    // leaves the element null.
    auto const container = Object::Impl::as<xaml::Controls::ListViewBase>(list).ContainerFromItem(native_item).try_as<xaml::Controls::ContentControl>();
    xaml::UIElement element{nullptr};
    if (container) {
        if (auto const place = container.ContentTemplateRoot().try_as<xaml::Controls::ContentControl>()) {
            element = place.Content().try_as<xaml::UIElement>();
        }
    }
    if (!element) {
        return {};
    }
    return Object::Impl::wrap<UIElement>(std::move(element));
}

}  // namespace wxl
