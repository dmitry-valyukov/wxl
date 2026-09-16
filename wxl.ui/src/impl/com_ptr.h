#pragma once

#include <utility>

namespace wxl {

// Reference-counted smart pointer with real copy semantics (AddRef on
// copy, Release on destruction) -- unlike release_only_ptr (move-only,
// Release on destruction only, no AddRef ever), this is for objects meant
// to be freely copied, sharing the same underlying identity. wxl::UIElement
// owns its impl::UIElement through this specifically so wxl::UIElement
// itself has *reference* semantics: copying a wxl::UIElement copies the
// reference, not the object -- needed to capture a wxl::X by value in
// event-handler lambdas.
//
// Constructing from a raw pointer *adopts* an existing +1 reference (e.g.
// straight out of `new` or QueryInterface) without an extra AddRef --
// matching how every wxl COM-shaped object already hands back a pointer at
// +1 from construction.
template <typename T>
class com_ptr {
public:
    com_ptr() noexcept = default;
    explicit com_ptr(T* ptr) noexcept : ptr_(ptr) {}

    com_ptr(com_ptr const& other) noexcept : ptr_(other.ptr_) {
        if (ptr_) {
            ptr_->AddRef();
        }
    }

    com_ptr& operator=(com_ptr const& other) noexcept {
        if (this != &other) {
            if (other.ptr_) {
                other.ptr_->AddRef();
            }
            reset();
            ptr_ = other.ptr_;
        }
        return *this;
    }

    com_ptr(com_ptr&& other) noexcept : ptr_(std::exchange(other.ptr_, nullptr)) {}

    com_ptr& operator=(com_ptr&& other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = std::exchange(other.ptr_, nullptr);
        }
        return *this;
    }

    ~com_ptr() {
        reset();
    }

    void reset() noexcept {
        if (ptr_) {
            ptr_->Release();
            ptr_ = nullptr;
        }
    }

    T* get() const noexcept { return ptr_; }
    T* operator->() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

private:
    T* ptr_{};
};

} // namespace wxl
