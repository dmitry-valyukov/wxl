module;

#include "abi.h"

export module wxl.core:binding_scope;

import :function;
import :intrusive_list;
import :intrusive_slist;
import :noncopyable;
import :not_null;
import std;

export namespace wxl::core {

class binding_scope;

namespace impl {

/**
 * A binding's watch as the scope it was made in sees it. The link lives inside the watch,
 * in the node of the field's event, so the two go together: when the field lets the watch
 * go -- dying, unbind(), a remove -- the link leaves its scope on the way out, and a scope
 * never holds anything dead. From the scope's side, cut() takes the watch off its field,
 * which destroys both.
 */
class binding_link : public intrusive_list_node<binding_link>
{
public:
    /// Takes the watch off its field. The link leaves its scope first: a watch that is
    /// being called right now outlives its removal until it returns, and the scope must
    /// not meet it again meanwhile.
    inline void cut() noexcept {
        intrusive_list<binding_link>::remove(as_not_null(this));
        cut_(*this);
    }

protected:
    // How the watch around this link takes itself off its field, whatever the field's
    // event: the watch is typed on it, the scope is not.
    using cut_fn = void (*)(binding_link&) noexcept;

    inline binding_link(binding_scope& scope, cut_fn cut) noexcept;

    inline ~binding_link() { intrusive_list<binding_link>::remove(as_not_null(this)); }

private:
    cut_fn cut_;
};

/// A binding's watch made while a scope was open: the callback and its link in one node
/// of the field's event, so collecting it costs no allocation of its own. `Body` is the
/// event's callback node, from which the call signature is read.
template <class Event, class Fn, class Body = typename Event::func_t>
class linked_watch;

template <class Event, class Fn, class... Args, template <class> class Base>
class linked_watch<Event, Fn, func_body<void(Args...) noexcept, Base>> final
    : public func_body<void(Args...) noexcept, Base>,
      public binding_link
{
    using body_t = func_body<void(Args...) noexcept, Base>;

    static_assert(invocable<Fn, void(Args...) noexcept>,
                  "wxl: a binding's watch is called from inside fire(), which has nobody to "
                  "hand an exception to -- it has to be noexcept");

public:
    template <class F>
    linked_watch(Event& source, binding_scope& scope, F&& fn)
        : binding_link(scope, &cut_from_source), source_(source), fn_(std::forward<F>(fn)) {}

    void operator()(Args... args) noexcept override { fn_(std::forward<Args>(args)...); }

private:
    static void cut_from_source(binding_link& link) noexcept {
        auto& self = static_cast<linked_watch&>(link);
        static_cast<void>(self.source_.remove(cookie_t{static_cast<body_t*>(&self)}));
    }

    Event& source_;
    Fn fn_;
};

}  // namespace impl

/**
 * The bindings made while something is built, cut at once when that something goes.
 *
 * A binding's watch is kept by its field, and it owns the control it writes to; it goes
 * with the field or with unbind(). That is right for a window, which lives as long as its
 * models or names them on closing. It is not for an element a list builds each time it
 * comes into view: every build would leave watches on the fields it bound to, each holding
 * an element the list has long given back. A scope is how such a builder's watches are
 * counted. Opened around the build with collect(), it gathers every binding watch made
 * meanwhile, on any field; kept by whatever knows when the element goes, it takes them
 * all off their fields with cut(), or with itself.
 *
 *     binding_scope scope;
 *     auto element = scope.collect([&] { return build(item); });
 *     ...
 *     scope.cut();   // the element was given back: its bindings go, the fields stay
 *
 * The field may go first -- an item model dropped from the list -- and then its watches
 * leave the scope as they die, so neither side outlives the other. Only binding watches
 * are collected; what the application adds with on_change stays its own.
 *
 * Which scope is open is one pointer for the process, not one per thread: bindings are
 * made on the STA thread alone, whose pool their watches come from. A scope is not moved
 * or copied -- its links point at it -- so it lives where its owner puts it.
 */
class binding_scope : noncopyable
{
public:
    binding_scope() noexcept = default;

    /// Cuts every watch still collected.
    inline ~binding_scope() { cut(); }

    /// Runs `build` with this scope open and returns what it returns. Every binding watch
    /// made meanwhile is collected here as well as kept by its field. Scopes nest: one
    /// opened inside another collects alone until `build` returns, and then the outer one
    /// collects again -- an element built inside an element belongs to its own scope.
    template <class F>
    decltype(auto) collect(F&& build) {
        struct reopen {
            binding_scope* outer;
            ~reopen() { s_current = outer; }
        } const restore{std::exchange(s_current, this)};
        return std::forward<F>(build)();
    }

    /// Takes every collected watch off its field, and with it whatever the watch held --
    /// the controls it wrote to, the handlers it put on them. The scope stays, empty, and
    /// can collect again. A field firing right now may be among them: the watch it is
    /// calling is called no more and goes when its call returns.
    inline void cut() noexcept {
        while (impl::binding_link* const link = links_.front()) {
            link->cut();
        }
    }

    /// Nothing collected, or nothing left: every watch went with its field or was cut.
    inline bool empty() const noexcept { return links_.empty(); }

    /// The scope open now, or null when no scope is.
    inline static binding_scope* current() noexcept { return s_current; }

private:
    friend class impl::binding_link;

    intrusive_list<impl::binding_link> links_;

    inline static binding_scope* s_current = nullptr;
};

namespace impl {

/// What watch_for_binding does, for observable and observable_list alike: the plain watch
/// when no scope is open, the watch with its link when one is.
template <class Event, class F>
void add_binding_watch(Event& bound, F&& callback) {
    if (binding_scope* const scope = binding_scope::current()) {
        using watch = linked_watch<Event, std::decay_t<F>>;
        static_cast<void>(bound.add(as_not_null<typename Event::func_t>(
            new watch(bound, *scope, std::forward<F>(callback)))));
    } else {
        static_cast<void>(bound.add(std::forward<F>(callback)));
    }
}

}  // namespace impl

}  // export namespace wxl::core

namespace wxl::core::impl {

// Out of the class, because the link joins the list of a scope declared after it.
inline binding_link::binding_link(binding_scope& scope, cut_fn cut) noexcept : cut_(cut) {
    scope.links_.push_back(as_not_null(this));
}

}  // namespace wxl::core::impl
