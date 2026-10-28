#pragma once

// impl::sta_com_object -- a COM object of wxl's own, for the one STA thread: what an object
// handed to XAML is when wxl writes it rather than taking cppwinrt's winrt::implements.
//
// winrt::implements is made for any apartment: an interlocked count, the CRT heap, agility
// and the free-threaded marshaler on every object. wxl's objects live on the UI thread and
// are destroyed there, like every other object of wxl, so the count here is a plain integer, the
// memory comes from the STA pool, and a debug build checks the thread on AddRef and Release
// -- the one place where a call from elsewhere would show. An object does not call itself
// agile: it is not.
//
// What it does answer besides its own interfaces is IWeakReferenceSource. The projection
// takes weak references to what it is handed -- weak_ref, an event's auto_revoke -- and
// faults on an object without one; XAML's own controls are written over that projection.
// The price is one pointer per object.
//
// Interfaces are written at the ABI, in cppwinrt's own vtable structs, as impl/value_box.h
// writes its boxes: a derived class defines the methods of each, and the members below
// override IUnknown and IInspectable in all of them at once.
//
// `own_iid` -- a constant a class may declare -- is an interface no one else asks for: it
// hands back the C++ object itself, which is how wxl recognises its own object among those
// XAML gives back (own_object below).

#include <winrt/Windows.Foundation.h>

#include "../core.h"
#include "hresult.h"

namespace wxl::impl {

// IWeakReference and IWeakReferenceSource as the Windows SDK declares them (weakreference.h),
// with their well-known identifiers.
struct weak_reference_abi : winrt::impl::unknown_abi {
    virtual int32_t __stdcall Resolve(winrt::guid const& id, void** object) noexcept = 0;
};

struct weak_reference_source_abi : winrt::impl::unknown_abi {
    virtual int32_t __stdcall GetWeakReference(weak_reference_abi** reference) noexcept = 0;
};

inline constexpr winrt::guid weak_reference_iid{
    0x00000037, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
// cppwinrt calls every one of its ABI vtables `type` -- abi<X>::type -- so deriving from two of
// them at once leaves a class with two base classes of the same name, and MSVC refuses it. One
// named layer each is enough to tell them apart; nothing else is added, and none has a member of
// its own. Shared with the property boxes of impl/value_box.h.
template <typename Interface>
struct interface_vtable : winrt::impl::abi_t<Interface> {
};

inline constexpr winrt::guid weak_reference_source_iid{
    0x00000038, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

/// The weak reference an sta_com_object hands out: a counted object of its own that names
/// the object while it lives and nothing afterwards. The object keeps one reference to it
/// and forgets it on the way out.
class weak_reference final : public weak_reference_abi
{
public:
    explicit weak_reference(winrt::impl::unknown_abi* target) noexcept : target_(target) {}

    int32_t __stdcall QueryInterface(winrt::guid const& id, void** object) noexcept final {
        if (id == weak_reference_iid ||
            id == winrt::guid_of<winrt::Windows::Foundation::IUnknown>()) {
            *object = static_cast<weak_reference_abi*>(this);
            AddRef();
            return 0;
        }
        *object = nullptr;
        return e_nointerface;
    }

    uint32_t __stdcall AddRef() noexcept final {
        core::sta_memory_pool::debug_check_thread();
        return ++count_;
    }

    uint32_t __stdcall Release() noexcept final {
        core::sta_memory_pool::debug_check_thread();
        uint32_t const left = --count_;
        if (left == 0) delete this;
        return left;
    }

    // A reference resolved after the object went is a success with nothing in it, as COM
    // has it.
    int32_t __stdcall Resolve(winrt::guid const& id, void** object) noexcept final {
        if (!target_) {
            *object = nullptr;
            return 0;
        }
        return target_->QueryInterface(id, object);
    }

    void forget() noexcept { target_ = nullptr; }

    static void* operator new(std::size_t size) { return core::sta_memory_pool::alloc(size); }
    static void operator delete(void* memory, std::size_t size) noexcept {
        core::sta_memory_pool::free(memory, size);
    }

private:
    winrt::impl::unknown_abi* target_;
    uint32_t count_ = 1;
};

/// The base of an own COM object: `Derived` is the class itself, final; `First` and `Rest`
/// are the interfaces it implements, `First` being the one that stands for its identity
/// (IUnknown, IInspectable). An object implementing IInspectable alone names it as `First`.
/// It is made with `new` at a count of one, which its first holder adopts --
/// `core::intrusive_ptr<T>{new T{...}, false}`, the `false` saying not to add a reference.
template <class Derived, class First, class... Rest>
class sta_com_object : public interface_vtable<First>,
                       public interface_vtable<Rest>...,
                       public weak_reference_source_abi
{
public:
    int32_t __stdcall QueryInterface(winrt::guid const& id, void** object) noexcept final {
        *object = interface_for(id);
        if (!*object) return e_nointerface;
        AddRef();
        return 0;
    }

    uint32_t __stdcall AddRef() noexcept final {
        core::sta_memory_pool::debug_check_thread();
        return ++count_;
    }

    uint32_t __stdcall Release() noexcept final {
        core::sta_memory_pool::debug_check_thread();
        uint32_t const left = --count_;
        if (left == 0) delete static_cast<Derived*>(this);
        return left;
    }

    /// The hooks core::intrusive_ptr finds by ADL: an own object is held by wxl as any counted
    /// object of wxl is, and the count is the COM one.
    friend void intrusive_ptr_add_ref(sta_com_object const* object) noexcept {
        const_cast<sta_com_object*>(object)->AddRef();
    }
    friend void intrusive_ptr_release(sta_com_object const* object) noexcept {
        const_cast<sta_com_object*>(object)->Release();
    }

    int32_t __stdcall GetIids(uint32_t* count, winrt::guid** ids) noexcept final try {
        // IInspectable is not listed: GetIids names what an object is beyond it.
        std::array<winrt::guid, 1 + sizeof...(Rest)> const all{winrt::guid_of<First>(),
                                                               winrt::guid_of<Rest>()...};
        std::array<winrt::guid, 1 + sizeof...(Rest)> listed{};
        uint32_t used = 0;
        for (winrt::guid const& id : all) {
            if (!(id == winrt::guid_of<winrt::Windows::Foundation::IInspectable>()))
                listed[used++] = id;
        }
        winrt::com_array<winrt::guid> implemented(listed.begin(), listed.begin() + used);
        std::tie(*count, *ids) = winrt::detach_abi(implemented);
        return 0;
    } catch (...) {
        return winrt::to_hresult();
    }

    /// Diagnostics only: the name of the first interface, as winrt::implements answers for a
    /// class that is not a runtime class.
    int32_t __stdcall GetRuntimeClassName(void** name) noexcept final try {
        *name = winrt::detach_abi(winrt::hstring{winrt::name_of<First>()});
        return 0;
    } catch (...) {
        return winrt::to_hresult();
    }

    int32_t __stdcall GetTrustLevel(winrt::Windows::Foundation::TrustLevel* level) noexcept final {
        *level = winrt::Windows::Foundation::TrustLevel::BaseTrust;
        return 0;
    }

    int32_t __stdcall GetWeakReference(weak_reference_abi** reference) noexcept final try {
        if (!weak_) weak_ = new weak_reference{identity()};
        weak_->AddRef();
        *reference = weak_;
        return 0;
    } catch (...) {
        *reference = nullptr;
        return winrt::to_hresult();
    }

    /// The object as IUnknown and IInspectable: the one pointer COM compares for identity.
    winrt::impl::inspectable_abi* identity() noexcept {
        return static_cast<interface_vtable<First>*>(this);
    }

    /// The identity with one more reference, which the caller takes over: what an out
    /// parameter of an ABI method is given.
    void* add_ref_abi() noexcept {
        AddRef();
        return identity();
    }

    /// One more reference to the object, as the IInspectable a projected call takes.
    winrt::Windows::Foundation::IInspectable as_inspectable() noexcept {
        AddRef();
        return winrt::Windows::Foundation::IInspectable{static_cast<void*>(identity()),
                                                        winrt::take_ownership_from_abi};
    }

    /// One more reference to the object, as one of its interfaces, projected.
    template <class Interface>
    Interface as_interface() noexcept {
        AddRef();
        return Interface{static_cast<void*>(static_cast<interface_vtable<Interface>*>(this)),
                         winrt::take_ownership_from_abi};
    }

    static void* operator new(std::size_t size) { return core::sta_memory_pool::alloc(size); }
    static void operator delete(void* memory, std::size_t size) noexcept {
        core::sta_memory_pool::free(memory, size);
    }

protected:
    sta_com_object() noexcept = default;
    sta_com_object(sta_com_object const&) = delete;
    sta_com_object& operator=(sta_com_object const&) = delete;

    ~sta_com_object() {
        if (weak_) {
            weak_->forget();
            weak_->Release();
        }
    }

private:
    void* interface_for(winrt::guid const& id) noexcept {
        if (id == winrt::guid_of<winrt::Windows::Foundation::IUnknown>() ||
            id == winrt::guid_of<winrt::Windows::Foundation::IInspectable>() ||
            id == winrt::guid_of<First>()) {
            return identity();
        }
        if (id == weak_reference_source_iid) return static_cast<weak_reference_source_abi*>(this);
        if constexpr (requires { Derived::own_iid; }) {
            if (id == Derived::own_iid) return static_cast<Derived*>(this);
        }
        void* found = nullptr;
        static_cast<void>(((id == winrt::guid_of<Rest>() &&
                            (found = static_cast<interface_vtable<Rest>*>(this))) ||
                           ...));
        return found;
    }

    uint32_t count_ = 1;
    weak_reference* weak_ = nullptr;
};

/// The object of class `T` behind a COM pointer, when it is one of wxl's own -- asked
/// through `T::own_iid`, which nothing else answers -- with a reference of its own; empty
/// for anything else and for null.
template <class T>
core::intrusive_ptr<T> own_object(void* object) noexcept {
    if (!object) return {};
    void* found = nullptr;
    if (static_cast<winrt::impl::unknown_abi*>(object)->QueryInterface(T::own_iid, &found) != 0)
        return {};
    return {static_cast<T*>(found), /*add_ref=*/false};
}

}  // namespace wxl::impl
