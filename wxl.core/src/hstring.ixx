module;

#include "abi.h"

// hstring: the WinRT string as a value, without WinRT.
//
// An HSTRING is a pointer to a header -- flags, length, two words of padding,
// a pointer to the characters -- and a string with a count in it is that header
// followed by an atomic reference count and the characters, a zero after them.
// The layout is not in the SDK (only HSTRING_HEADER is, as an opaque blob), but
// it is shared: the operating system, cppwinrt and every WinRT component read
// and write it the same way, cppwinrt without a single call into a DLL, and this
// file does what cppwinrt does. So a string can be read, copied (one increment)
// and released (one decrement, and the heap when it was the last) here, with no
// call into combase and no header from winrt/.
//
// Two rules the layout imposes. The count is atomic, because an HSTRING is not
// tied to a thread: any side that holds one may drop the last reference, on any
// thread. And the memory comes from the process heap, because that side may be
// the operating system or another component, which frees it with HeapFree. The
// STA pool, sta_allocator and malloc are all wrong here, for that reason and no
// other.
//
// Two types. hstring owns a reference. hstring_param is what a function takes
// instead: a borrowed reference that is either somebody's hstring or a fast-pass
// string -- a header on the stack over characters that live somewhere else --
// and it is why a zstring_view goes into WinRT without a copy.

export module wxl.core:hstring;

import :checks;
import :unicode;
import :zstring_view;
import std;

namespace wxl::core {

namespace impl {

struct hstring_header {
    std::uint32_t flags;
    std::uint32_t length;
    std::uint32_t padding1;
    std::uint32_t padding2;
    const char16_t* ptr;
};

/// Set on a string that only points at characters somebody else keeps, and has
/// no count. Copying one makes a real string.
inline constexpr std::uint32_t hstring_reference_flag = 1;

struct shared_hstring_header : hstring_header {
    std::atomic<std::int32_t> count;
    char16_t buffer[1];
};

// The heap side, out of line: it wants <windows.h> and nobody else does.
// create_hstring answers null for nothing at all, the way an empty HSTRING is a
// null handle.
hstring_header* create_hstring(const char16_t* text, std::uint32_t length);
void free_hstring(hstring_header* header) noexcept;

inline hstring_header* duplicate_hstring(hstring_header* header) {
    if (header == nullptr) return nullptr;

    if ((header->flags & hstring_reference_flag) == 0) {
        ++static_cast<shared_hstring_header*>(header)->count;
        return header;
    }

    return create_hstring(header->ptr, header->length);
}

inline void release_hstring(hstring_header* header) noexcept {
    if (header == nullptr) return;

    assume((header->flags & hstring_reference_flag) == 0);

    if (--static_cast<shared_hstring_header*>(header)->count == 0) free_hstring(header);
}

inline zstring_view text_of(const hstring_header* header) noexcept {
    return header == nullptr
               ? zstring_view{}
               : zstring_view{terminated{}, header->ptr, header->length};
}

}  // namespace impl

export {

class hstring_param;

/// A WinRT string, owned: a reference to an HSTRING. Copying it is an increment,
/// not a copy of the text; the text is copied once, when it is made.
///
/// It is a string of char16_t and says nothing about being well-formed UTF-16:
/// an HSTRING is a sequence of sixteen-bit units and neither the system nor any
/// component checks it. Checked text out of one is a door of its own --
/// unicode::checked() or repaired() -- not a conversion.
class hstring {
public:
    using value_type = char16_t;
    using const_iterator = const char16_t*;

    hstring() noexcept = default;

    hstring(const hstring& other) : header_(impl::duplicate_hstring(other.header_)) {}

    hstring(hstring&& other) noexcept : header_(std::exchange(other.header_, nullptr)) {}

    hstring& operator=(const hstring& other) {
        if (this != &other) {
            hstring copy(other);
            swap(copy);
        }
        return *this;
    }

    hstring& operator=(hstring&& other) noexcept {
        if (this != &other) {
            impl::release_hstring(header_);
            header_ = std::exchange(other.header_, nullptr);
        }
        return *this;
    }

    ~hstring() { impl::release_hstring(header_); }

    /// A string of its own, made from a copy of the text. Explicit: the copy is
    /// the cost of this type, and it is written where it is paid.
    explicit hstring(std::u16string_view text) : header_(create(text)) {}

    /// A C string, which is also what a literal is: a literal would otherwise
    /// reach the view, the checked view and the checked string through one
    /// conversion each, and none of them would be the better one.
    explicit hstring(const char16_t* text) : header_(create(std::u16string_view(text))) {}

    // The seam of wchar_t, the same units under the other name.
    explicit hstring(const wchar_t* text)
        : header_(create(std::u16string_view(reinterpret_cast<const char16_t*>(text)))) {}

    explicit hstring(u16_view text) : header_(create(text.plain())) {}

    explicit hstring(const u16_text& text) : header_(create(text.plain())) {}

    /// Takes over a reference the caller already owns: what a method of WinRT
    /// hands out in an out parameter. The handle must be a real string, not a
    /// fast-pass one.
    static hstring attach(void* handle) noexcept {
        hstring result;
        result.header_ = static_cast<impl::hstring_header*>(handle);
        return result;
    }

    /// The handle, still owned here: for passing to a call that borrows it.
    void* get_abi() const noexcept { return header_; }

    /// The handle, no longer owned here: for a call that takes the reference.
    void* detach() noexcept { return std::exchange(header_, nullptr); }

    /// Where an out parameter writes the handle: what is held is released first.
    void** put_abi() noexcept {
        impl::release_hstring(header_);
        header_ = nullptr;
        return reinterpret_cast<void**>(&header_);
    }

    void swap(hstring& other) noexcept { std::swap(header_, other.header_); }

    const char16_t* data() const noexcept { return text().data(); }
    const char16_t* c_str() const noexcept { return text().data(); }
    std::size_t size() const noexcept { return header_ == nullptr ? 0 : header_->length; }
    std::size_t length() const noexcept { return size(); }
    bool empty() const noexcept { return header_ == nullptr; }

    const_iterator begin() const noexcept { return data(); }
    const_iterator end() const noexcept { return data() + size(); }
    char16_t operator[](std::size_t at) const noexcept { return data()[at]; }

    /// Terminated, and known to be so without a look: an HSTRING always is. This
    /// is why the text of an hstring reaches anything that wants the zero for
    /// nothing.
    operator zstring_view() const noexcept { return text(); }

    operator std::u16string_view() const noexcept { return text().view(); }

    friend bool operator==(const hstring& left, const hstring& right) noexcept {
        return left.text().view() == right.text().view();
    }

    friend bool operator==(const hstring& left, std::u16string_view right) noexcept {
        return left.text().view() == right;
    }

    friend bool operator==(const hstring& left, const char16_t* right) noexcept {
        return left.text().view() == std::u16string_view(right);
    }

private:
    friend class hstring_param;

    static impl::hstring_header* create(std::u16string_view text) {
        ensure(text.size() <= std::numeric_limits<std::uint32_t>::max() &&
               "a string of WinRT holds at most 4 GiB of units");
        return impl::create_hstring(text.data(), static_cast<std::uint32_t>(text.size()));
    }

    zstring_view text() const noexcept { return impl::text_of(header_); }

    impl::hstring_header* header_ = nullptr;
};

/// What a function takes when it wants a WinRT string and will not keep it: a
/// borrowed reference. Whoever calls it has one of three things and hands over
/// any of them for nothing --
///
///   - an hstring: the handle is lent, the count does not move;
///   - text with a terminator known by its type (a zstring_view, a literal, a
///     std::u16string, checked text that owns its buffer): a header is written
///     on the stack over these very characters -- a fast-pass string -- with no
///     copy and without a look at the zero, since the type already promised it;
///   - nothing: the empty string.
///
/// A plain view is not one of the three, and that is deliberate: it says nothing
/// about the unit after its end, and this type would have to either copy or
/// trust. It goes through assume_terminated(), where the trust is written out.
///
/// It is not copyable and not movable: the handle of a fast-pass string points
/// into the object itself, so the object has to stay where it was made. That is
/// what a parameter does -- the argument is built in place -- and it lives until
/// the end of the call, which is exactly how long a method of WinRT may borrow
/// the string. One that keeps it must duplicate it, and duplicating a fast-pass
/// string is what makes a real one.
class hstring_param {
public:
    /// Written out rather than defaulted: the header is left as it is until a
    /// text arrives, and a const object may only be default-initialized when the
    /// constructor is not the implicit one.
    hstring_param() noexcept {}

    hstring_param(const hstring_param&) = delete;
    hstring_param& operator=(const hstring_param&) = delete;

    hstring_param(const hstring& value) noexcept : handle_(value.header_) {}

    hstring_param(zstring_view text) noexcept { reference(text); }

    /// Anything that is a zstring_view on its own, in one step. A template
    /// because a literal or a string would otherwise need two conversions -- to
    /// zstring_view and from it -- and an implicit sequence has one.
    template <typename Text>
        requires(!std::same_as<std::remove_cvref_t<Text>, hstring> &&
                 !std::same_as<std::remove_cvref_t<Text>, zstring_view> &&
                 std::convertible_to<const Text&, zstring_view>)
    hstring_param(const Text& text) noexcept : hstring_param(zstring_view(text)) {}

    /// The text, with its terminator, for the code that also wants to read it.
    zstring_view text() const noexcept { return impl::text_of(handle_); }

    std::size_t size() const noexcept { return handle_ == nullptr ? 0 : handle_->length; }
    bool empty() const noexcept { return handle_ == nullptr; }

    /// The handle to lend to a call, for the layer that speaks WinRT: this is the
    /// one value that layer reinterprets as its own hstring const&.
    void* get_abi() const noexcept { return handle_; }

private:
    void reference(zstring_view text) noexcept {
        if (text.empty()) {
            handle_ = nullptr;
            return;
        }

        ensure(text.size() <= std::numeric_limits<std::uint32_t>::max() &&
               "a string of WinRT holds at most 4 GiB of units");

        header_.flags = impl::hstring_reference_flag;
        header_.length = static_cast<std::uint32_t>(text.size());
        header_.padding1 = 0;
        header_.padding2 = 0;
        header_.ptr = text.data();
        handle_ = &header_;
    }

    impl::hstring_header* handle_ = nullptr;
    impl::hstring_header header_;
};

}  // export

}  // namespace wxl::core
