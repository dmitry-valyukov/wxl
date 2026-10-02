// VectorView<T> and Collection<T> over a real WinRT list.
//
// The list is cppwinrt's single_threaded_vector, and its GetView hands back
// the vector itself as an IVectorView: a view that follows its source, which
// is the case a copy of the elements would have broken. Nothing is activated,
// so no WinUI runtime is needed -- the vector is cppwinrt's own object.

#include <gtest/gtest.h>

#include <iterator>
#include <string_view>
#include <vector>

#include <winrt/Windows.Foundation.Collections.h>

#include "Collection.impl.h"
#include "VectorView.impl.h"

// What a generated .cpp does for every specialization a profile hands out.
template class wxl::VectorView<wxl::hstring>;
template class wxl::VectorView<wxl::Object>;
template class wxl::Collection<wxl::Object>;

namespace {

using winrt::Windows::Foundation::IInspectable;
using winrt::Windows::Foundation::Collections::IVector;
using winrt::Windows::Foundation::Collections::IVectorView;

static_assert(std::forward_iterator<wxl::impl::index_iterator<wxl::VectorView<wxl::hstring>>>);
static_assert(std::forward_iterator<wxl::impl::index_iterator<wxl::Collection<wxl::Object>>>);

IVector<winrt::hstring> letters() {
    return winrt::single_threaded_vector<winrt::hstring>({L"a", L"b", L"c"});
}

// How a generated member turns the view a call returned into the wrapper.
template <typename T, typename WinRT>
T wrap(WinRT const& object) {
    return wxl::Object::Impl::wrap<T>(object);
}

std::u16string_view text(wxl::hstring const& value) {
    return std::u16string_view{value};
}

// An object of its own for every element: a vector is an IInspectable like any
// other, and needs no activation.
std::vector<IInspectable> objects(size_t count) {
    std::vector<IInspectable> made;
    for (size_t i = 0; i != count; ++i) {
        made.push_back(winrt::single_threaded_vector<int32_t>());
    }
    return made;
}

TEST(vector_view, reads_size_and_elements_by_index) {
    auto const view = wrap<wxl::VectorView<wxl::hstring>>(letters().GetView());

    ASSERT_TRUE(view);
    EXPECT_EQ(view.size(), 3u);
    EXPECT_FALSE(view.empty());
    EXPECT_EQ(text(view.getAt(0)), u"a");
    EXPECT_EQ(text(view[2]), u"c");
}

TEST(vector_view, finds_where_an_element_is) {
    auto const view = wrap<wxl::VectorView<wxl::hstring>>(letters().GetView());

    auto const found = view.indexOf(wxl::hstring{u"b"});
    ASSERT_TRUE(found);
    EXPECT_EQ(*found, 1u);
    EXPECT_FALSE(view.indexOf(wxl::hstring{u"z"}));
}

TEST(vector_view, is_walked_in_order) {
    auto const view = wrap<wxl::VectorView<wxl::hstring>>(letters().GetView());

    std::u16string walked;
    for (auto const& letter : view) {
        walked += text(letter);
    }
    EXPECT_EQ(walked, u"abc");
    EXPECT_EQ(std::distance(view.begin(), view.end()), 3);
}

TEST(vector_view, an_empty_view_is_walked_not_at_all) {
    auto const view =
        wrap<wxl::VectorView<wxl::hstring>>(winrt::single_threaded_vector<winrt::hstring>().GetView());

    EXPECT_TRUE(view.empty());
    EXPECT_TRUE(view.begin() == view.end());
}

// What the whole type is for: the source changes, and the view says so.
TEST(vector_view, follows_a_source_that_changes) {
    auto const source = letters();
    auto const view = wrap<wxl::VectorView<wxl::hstring>>(source.GetView());

    source.Append(L"d");
    EXPECT_EQ(view.size(), 4u);
    EXPECT_EQ(text(view[3]), u"d");

    source.SetAt(0, L"z");
    EXPECT_EQ(text(view[0]), u"z");

    source.RemoveAt(1);
    std::u16string walked;
    for (auto const& letter : view) {
        walked += text(letter);
    }
    EXPECT_EQ(walked, u"zcd");

    source.Clear();
    EXPECT_TRUE(view.empty());
}

// Handed back to a call, a view is the very object it holds: no copy, no
// second list.
TEST(vector_view, hands_over_the_view_it_holds) {
    auto const source = letters();
    auto const view = wrap<wxl::VectorView<wxl::hstring>>(source.GetView());

    IVectorView<winrt::hstring> const& handed = *wxl::Object::Impl::get_typed(view);
    EXPECT_EQ(winrt::get_abi(handed), winrt::get_abi(source.GetView()));
}

// A runtime that hands back no list hands back the empty wrapper, as for any
// other object.
TEST(vector_view, no_view_is_the_empty_wrapper) {
    auto const view = wrap<wxl::VectorView<wxl::hstring>>(IVectorView<winrt::hstring>{nullptr});

    EXPECT_FALSE(view);
}

// Elements that are objects come out as wrappers around those very objects.
TEST(vector_view, wraps_each_object_element_around_the_object_itself) {
    auto const made = objects(2);
    auto const view = wrap<wxl::VectorView<wxl::Object>>(
        winrt::single_threaded_vector<IInspectable>(std::vector<IInspectable>{made}).GetView());

    ASSERT_EQ(view.size(), 2u);
    EXPECT_EQ(static_cast<void*>(view[1].get_abi()), winrt::get_abi(made[1]));
    auto const found = view.indexOf(view[1]);
    ASSERT_TRUE(found);
    EXPECT_EQ(*found, 1u);
}

// The collection walks the same way, and what it is given is the object
// itself.
TEST(collection, is_walked_and_takes_the_object_itself) {
    auto const source = winrt::single_threaded_vector<IInspectable>();
    auto const collection = wrap<wxl::Collection<wxl::Object>>(source);
    auto const made = objects(2);

    for (auto const& object : made) {
        collection.append(wxl::Object::copy_from_abi(static_cast<::IInspectable*>(winrt::get_abi(object))));
    }

    ASSERT_EQ(source.Size(), 2u);
    EXPECT_EQ(winrt::get_abi(source.GetAt(0)), winrt::get_abi(made[0]));

    size_t walked = 0;
    for (auto const& object : collection) {
        EXPECT_EQ(static_cast<void*>(object.get_abi()), winrt::get_abi(made[walked]));
        ++walked;
    }
    EXPECT_EQ(walked, 2u);
}

}  // namespace
