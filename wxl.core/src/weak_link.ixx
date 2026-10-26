module;

#include "abi.h"

export module wxl.core:weak_link;

import :intrusive_ptr;
import :refcounted;

export namespace wxl::core {

template <class T, class Count>
class weak_linked;

/**
 * A reference to an owner that does not keep it: `get()` is the owner while it lives and null
 * once it is gone. It is held by whoever must reach the owner without keeping it -- a handler
 * the platform keeps, a turn in a queue -- and outlives the owner as long as they hold it. Only
 * the owner's base, `weak_linked`, makes the link and clears it.
 *
 * The link synchronizes neither the pointer nor the owner. `get()` is right where and when the
 * scenario rules out the owner's destruction -- as a rule, on the owner's own thread and outside
 * its destructor; it is the scenario that makes the read correct, not a lock. The owner is not
 * reached through its link from inside its own destructor.
 *
 * `Count` counts the link itself, chosen by the threads its copies die on: `sta_refcounted`
 * (`sta_weak_link<T>`), `refcounted`, or `refcounted_mt` (`weak_link_mt<T>`) for a copy let go of
 * on another thread -- a delegate handed to WinRT, a turn posted to another queue. The atomic
 * count guards the link's count and nothing else: not the read of `get()`.
 */
template <class T, class Count>
class weak_link final : public Count
{
    static_assert(impl::counts_own_references<Count>,
                  "wxl: a weak_link is counted by sta_refcounted, refcounted or refcounted_mt");

public:
    /// The owner while it lives, null once it is gone.
    [[nodiscard]] T* get() const noexcept { return owner_; }

private:
    friend class weak_linked<T, Count>;

    explicit weak_link(T* owner) noexcept : owner_(owner) {}

    T* owner_;
};

/**
 * The base of an owner that hands out a `weak_link` to itself (`T` is the owner): the link is
 * made with the owner and cleared as it goes, so it is null exactly when the owner is gone.
 * `Count` is the link's, as `weak_link` says: `sta_weak_linked<T>`, `weak_linked_mt<T>`, or
 * `weak_linked<T, refcounted>` written out. An owner that counts its own references takes this
 * beside its count:
 *
 * ```cpp
 * struct Attachment : core::sta_refcounted, core::sta_weak_linked<Attachment> { ... };
 *
 * sized = fe.SizeChanged(winrt::auto_revoke, [link = weak_link()](auto&&, auto&&) {
 *     if (core::intrusive_ptr<Attachment> const self = link->get()) self->fit();
 * });
 * ```
 *
 * The link is cleared by this base's destructor, which runs after the owner's own and after its
 * fields are gone; until then `get()` still answers the owner. Nothing synchronizes that, as
 * nothing synchronizes `get()` at all: the scenario makes a read correct -- on the owner's own
 * thread, outside its destructor -- and the owner is not reached through its link from inside
 * its destructor. A `refcounted_mt` link guards its count when a copy dies on another thread,
 * not the read of `get()`.
 */
template <class T, class Count>
class weak_linked
{
    using link_t = core::weak_link<T, Count>;

public:
    weak_linked(const weak_linked&) = delete;
    weak_linked& operator=(const weak_linked&) = delete;

    /// The link to this owner, for a holder that must not keep it.
    [[nodiscard]] intrusive_ptr<link_t> const& weak_link() const noexcept { return link_; }

protected:
    // The link's constructor is this base's alone, so it is made by `new` and the reference it
    // is born with adopted, rather than through make_refcounted.
    weak_linked() : link_(new link_t(static_cast<T*>(this)), /*add_ref=*/false) {}

    ~weak_linked() { link_->owner_ = nullptr; }

private:
    intrusive_ptr<link_t> const link_;
};

/// A link counted by `sta_refcounted`, and the base of its owner: one thread, the STA pool.
template <class T>
using sta_weak_link = weak_link<T, sta_refcounted>;

/// The type of the base of an owner whose `weak_link` is counted by `sta_refcounted`.
template <class T>
using sta_weak_linked = weak_linked<T, sta_refcounted>;

/// A link counted by `refcounted_mt`, and the base of its owner: a copy may die on another thread.
template <class T>
using weak_link_mt = weak_link<T, refcounted_mt>;

/// The type of the base of an owner whose `weak_link` is counted by `refcounted_mt`.
template <class T>
using weak_linked_mt = weak_linked<T, refcounted_mt>;

}  // export namespace wxl::core
