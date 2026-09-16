module;

namespace winrt::Windows::Foundation {
class IInspectable;
}

namespace winrt::Microsoft::UI::Xaml {
struct IDependencyObject;
}

namespace wxl {

//
class Object
{
public:
    operator winrt::Windows::Foundation::IInspectable const &() const noexcept;

protected:
    Object(std::nullptr_t) noexcept {}

    struct Impl;
    explicit Object(Impl* impl) noexcept : impl_(impl) {}

    template <auto member>
    auto& get_for_init() const noexcept;

    template <auto member>
    auto& get() const;

private:
    Impl* const impl_{};
};

//
class DependencyObject : public Object
{
public:
    operator winrt::Microsoft::UI::Xaml::IDependencyObject const &() const noexcept;
protected:
    struct Impl;
    explicit DependencyObject(Impl* impl) noexcept;
};

}  // namespace wxl