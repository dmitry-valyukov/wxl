#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Interop.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>

#include <cwchar>
#include <vector>

#include "KeyedIndexList.h"

namespace wxl {

namespace {

namespace interop = winrt::Microsoft::UI::Xaml::Interop;
using winrt::Windows::Foundation::IInspectable;

// The framework's own collection types do not answer for the interfaces of Microsoft.UI.Xaml.Interop (only the
// older Windows.UI.Xaml ones), so the iterator and the view are written out.
struct Iterator : winrt::implements<Iterator, interop::IBindableIterator> {
    explicit Iterator(std::vector<IInspectable> items) : items_(std::move(items)) {}
    IInspectable Current() const {
        if (position_ >= items_.size()) {
            throw winrt::hresult_out_of_bounds{};
        }
        return items_[position_];
    }
    bool HasCurrent() const { return position_ < items_.size(); }
    bool MoveNext() {
        if (position_ < items_.size()) {
            ++position_;
        }
        return position_ < items_.size();
    }

private:
    std::vector<IInspectable> items_;
    size_t position_ = 0;
};

struct View : winrt::implements<View, interop::IBindableVectorView, interop::IBindableIterable> {
    explicit View(std::vector<IInspectable> items) : items_(std::move(items)) {}
    interop::IBindableIterator First() const { return winrt::make<Iterator>(items_); }
    IInspectable GetAt(uint32_t index) const {
        if (index >= items_.size()) {
            throw winrt::hresult_out_of_bounds{};
        }
        return items_[index];
    }
    uint32_t Size() const { return static_cast<uint32_t>(items_.size()); }
    bool IndexOf(IInspectable const& value, uint32_t& index) const {
        for (uint32_t i = 0; i < items_.size(); ++i) {
            if (items_[i] == value) {
                index = i;
                return true;
            }
        }
        return false;
    }

private:
    std::vector<IInspectable> items_;
};

// The list as the control reads it: a bindable vector that says when it was replaced and knows its keys. The items are
// kept by an ordinary vector of the library, which this delegates to.
struct KeyedList : winrt::implements<KeyedList, interop::IBindableVector, interop::INotifyCollectionChanged,
                                     winrt::Microsoft::UI::Xaml::Controls::IKeyIndexMapping> {
    // IBindableIterable
    interop::IBindableIterator First() const { return winrt::make<Iterator>(items_); }

    // IBindableVector
    IInspectable GetAt(uint32_t index) const {
        if (index >= items_.size()) {
            throw winrt::hresult_out_of_bounds{};
        }
        return items_[index];
    }
    uint32_t Size() const { return static_cast<uint32_t>(items_.size()); }
    interop::IBindableVectorView GetView() const { return winrt::make<View>(items_); }
    bool IndexOf(IInspectable const& value, uint32_t& index) const {
        for (uint32_t i = 0; i < items_.size(); ++i) {
            if (items_[i] == value) {
                index = i;
                return true;
            }
        }
        return false;
    }
    void SetAt(uint32_t index, IInspectable const& value) { items_.at(index) = value; }
    void InsertAt(uint32_t index, IInspectable const& value) { items_.insert(items_.begin() + index, value); }
    void RemoveAt(uint32_t index) { items_.erase(items_.begin() + index); }
    void Append(IInspectable const& value) { items_.push_back(value); }
    void RemoveAtEnd() { items_.pop_back(); }
    void Clear() { items_.clear(); }

    // INotifyCollectionChanged
    winrt::event_token CollectionChanged(interop::NotifyCollectionChangedEventHandler const& handler) {
        return changed_.add(handler);
    }
    void CollectionChanged(winrt::event_token token) noexcept { changed_.remove(token); }

    // IKeyIndexMapping: the key of an item is the position it holds.
    winrt::hstring KeyFromIndex(int32_t index) const {
        return winrt::to_hstring(winrt::unbox_value<int64_t>(items_.at(static_cast<size_t>(index))));
    }
    int32_t IndexFromKey(winrt::hstring const& key) const {
        int64_t const wanted = std::wcstoll(key.c_str(), nullptr, 10);
        for (size_t i = 0; i < items_.size(); ++i) {
            if (winrt::unbox_value<int64_t>(items_[i]) == wanted) {
                return static_cast<int32_t>(i);
            }
        }
        return -1;
    }

    void reset(int64_t const* positions, size_t count) {
        items_.clear();
        for (size_t i = 0; i < count; ++i) {
            items_.push_back(winrt::box_value(positions[i]));
        }
        changed_(*this, interop::NotifyCollectionChangedEventArgs{interop::NotifyCollectionChangedAction::Reset, nullptr, nullptr, -1, -1});
    }

private:
    std::vector<IInspectable> items_;
    winrt::event<interop::NotifyCollectionChangedEventHandler> changed_;
};

}  // namespace

Object keyedIndexList() {
    IInspectable list = winrt::make<KeyedList>();
    return Object::copy_from_abi(static_cast<::IInspectable*>(winrt::get_abi(list)));
}

void keyedIndexListReset(Object const& list, int64_t const* positions, size_t count) {
    IInspectable native{nullptr};
    winrt::copy_from_abi(native, list.get_abi());
    winrt::get_self<KeyedList>(native.as<interop::IBindableVector>())->reset(positions, count);
}

}  // namespace wxl
