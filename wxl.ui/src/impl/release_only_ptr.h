#pragma once

#include <utility>

namespace wxl {

// Minimal RAII wrapper for a COM interface pointer that wxl already owns a
// single +1 reference to (e.g. the output of QueryInterface) -- its only
// job is calling Release() on destruction. Deliberately not a full COM
// smart pointer: it never calls AddRef/QueryInterface itself, and it isn't
// copyable -- sharing is handled one level up, by wxl::X wrapping
// impl::X in a wxl::com_ptr, so the raw interface
// pointer inside impl::X never needs its own independent ref-counting
// story. This is what lets wxl::impl hold bare ABI interfaces (see
// wxl/abi/*) without depending on winrt::com_ptr or any cppwinrt header.
template <typename T>
class release_only_ptr {
public:
    release_only_ptr() noexcept = default;
    explicit release_only_ptr(T* ptr) noexcept : ptr_(ptr) {}

    release_only_ptr(release_only_ptr const&) = delete;
    release_only_ptr& operator=(release_only_ptr const&) = delete;

    release_only_ptr(release_only_ptr&& other) noexcept : ptr_(std::exchange(other.ptr_, nullptr)) {}

    release_only_ptr& operator=(release_only_ptr&& other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = std::exchange(other.ptr_, nullptr);
        }
        return *this;
    }

    ~release_only_ptr() {
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
