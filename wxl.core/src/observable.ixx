export module wxl.core:observable;

import :binding_scope;
import :checks;
import :event;
import :function;
import :not_null;
import std;

export namespace wxl::core {

template <class T>
class observable;

/**
 * A field seen read-only: `observable<T const>`. A model hands it out
 * where a view may watch a value and bind to it but only the model sets it --
 * the model keeps an `observable<T>` and returns a reference to it as this:
 *
 *     struct Zoom {
 *         observable<double const>& factor() { return factor_; }
 *     private:
 *         observable<double> factor_{1.0};
 *     };
 *
 * Everything but setting is here: the value, the application's watches, the
 * bindings' watches. So BindOutput takes a read-only observable -- the control
 * follows the value -- while Bind and BindInput, which write into the field,
 * do not.
 *
 * Only an `observable<T>` is ever made: the constructors and the destructor
 * are protected, so there is no read-only observable without the field behind
 * it, and no way to destroy a field through it.
 */
template <class T>
class observable<T const> {
public:
    /// The type of the event that notifies the watchers of a change. The signature is
    /// `noexcept`: a watcher is called from inside the call of the event, which walks the whole
    /// list and has no caller to propagate an exception to, so a throwing watcher would abort
    /// the walk and skip the watchers after it. The event requires `noexcept` of every callback
    /// in either case; the spelling here makes the contract visible in the type, which is the
    /// reason `event` accepts the `noexcept` signature.
    using changed_event = event<void(T const&) noexcept>;

    // A field, not a handle: copying one would make a second state that no
    // binding to the first would ever hear of. Share the model instead.
    observable(observable const&) = delete;
    observable& operator=(observable const&) = delete;

    /// The value now.
    T const& get() const noexcept { return value_; }

    /// Watches for changes; the returned token removes the watch. The callback
    /// is `event`'s, so it has to be noexcept -- add() below says so.
    ///
    /// The token is the caller's: an application watching the model to react
    /// removes the watch when it likes, or lets it go with the model. A watch
    /// that a member keeps for its owner, and that comes off with it, is a
    /// change_watch and needs no token. A binding's own watch is a different
    /// thing -- see watch_for_binding.
    template <class F>
    event_token on_change(F&& callback) {
        return changed_.add(std::forward<F>(callback));
    }

    /// Takes the watch off and empties the token (event::remove).
    bool remove_change(event_token& token) { return changed_.remove(token); }

    /// The same for a watch built into its subscriber, by its node: a change_watch.
    bool remove_change(typename changed_event::func_t& node) { return changed_.remove(node); }

    /// Watches for changes on behalf of a binding, the field keeping the watch
    /// rather than handing a token back. This is not a convenience over
    /// on_change: a binding's model->control watch *owns the control* -- it
    /// must, to write to it, and a borrowed pointer would dangle, the control
    /// being a temporary in the description tree. Kept on a list of their own
    /// so that unbind() can drop them without touching the application's.
    ///
    /// Made while a binding_scope is open, the watch is collected by it as
    /// well, and the scope can take it off again -- the way a list drops the
    /// bindings of an element it has given back.
    template <class F>
    void watch_for_binding(F&& callback) {
        impl::add_binding_watch(bound_, std::forward<F>(callback));
    }

    /// Drops every binding watch (see watch_for_binding), letting go of whatever
    /// they held -- the bound controls. Watches added through on_change are the
    /// caller's and are left where they are.
    ///
    /// Needed only where the model outlives the window that bound to it: a
    /// settings model kept for the run, whose dialog opens and closes. Then the
    /// dialog's Closed handler names the models, through wxl::unsubscribe_all,
    /// so their controls are freed with the window rather than with the model.
    /// A model that dies with its window -- a calculator held by its keys --
    /// needs nothing of the kind: its watches go with it.
    void unbind() { bound_.clear(); }

protected:
    observable() = default;
    /// Constructs an observable that holds \p value.
    explicit observable(T value) : value_(std::move(value)) {}
    ~observable() = default;

    /// The current value.
    T value_{};
    /// The watches of the bindings, which `unbind()` removes. The watches of the bindings and
    /// those of the application are kept in two lists rather than in one list with a mark:
    /// `unbind()` removes the former and must not touch the latter. Both lists are declared
    /// after the value so that they are destroyed first: a binding's watch removes the handler
    /// it put on its control as it is destroyed, and does so while the field it refers to
    /// still exists.
    changed_event bound_;
    /// The watches of the application, added through `on_change`; `unbind()` leaves them in
    /// place.
    changed_event changed_;
};

namespace impl {

// The read-only observable a field extends, named apart from the field itself:
// Doxygen reads a base of the same template as a class deriving from itself,
// whatever the arguments.
template <class T>
using readonly_observable = observable<T const>;

}  // namespace impl

/**
 * A field that announces when it changes -- the model side of a binding.
 *
 * Declarative UI needs something to bind a control to that is neither the
 * control nor a bare field. A bare field cannot say it was set from elsewhere,
 * so a control bound straight to one would never learn of a change it did not
 * make itself. An observable is the field plus that announcement.
 *
 * It is a *field*, held by value inside the model that owns it, and not a
 * handle: it has no reference count of its own and cannot be copied. The
 * identity is the model's -- one refcounted object, made in the STA pool, whose
 * observables are its members, the way a `QProperty` is a member of its
 * `QObject`. Two controls bound to the same switch share its state because
 * they look at the same field of the same model, not because the field was
 * copied and the copies happen to agree. This is what keeps a model one
 * allocation, however many fields it announces.
 *
 * The watchers are an `event`, so each is `noexcept` and each is removed by the
 * token `on_change` returns. A binding's watches are kept apart (see
 * watch_for_binding), and all of them go with the field: destroying the model
 * destroys every watch, and what a watch owned -- the control it wrote to --
 * is let go with it.
 *
 * What it adds to its read-only base (`observable<T const>`, above) is setting:
 * set() and follow().
 */
template <class T>
class observable : public impl::readonly_observable<T> {
    using readonly = impl::readonly_observable<T>;

public:
    observable() = default;
    /// Constructs a field that holds \p value.
    explicit observable(T value) : readonly(std::move(value)) {}

    /// Sets the value and, if it actually changed, tells the watchers. Setting
    /// it to what it already holds says nothing: a binding writing back the
    /// value it has just read must not set off an echo between the two sides.
    ///
    /// The bindings hear first, the application after: by the time a watcher
    /// added through on_change runs -- to save the model, say -- the controls
    /// already show what it is saving.
    void set(T value) {
        if constexpr (std::equality_comparable<T>) {
            if (this->value_ == value) return;
        }
        this->value_ = std::move(value);
        this->bound_(this->value_);
        this->changed_(this->value_);
    }

    /// Follows other fields: whenever any of the sources changes, `fn` -- the
    /// last argument -- is called with the current values of all of them and
    /// the result becomes this field's value. It is computed once right here
    /// too, so the field is in step from the moment it follows rather than
    /// from the first change after. A source may be a read-only observable.
    ///
    /// The watches live on the sources and point back at this field, so it has
    /// to be there whenever a source changes -- as it is when all of them are
    /// fields of one model. They are application watches, which unbind()
    /// leaves alone. `fn` runs inside a watcher and so must not throw.
    ///
    /// Returns the field itself, so its own watch can follow in one expression:
    ///
    ///     answer.follow(va, vb, vc, solve).on_change(show);
    template <class... Args>
        requires (sizeof...(Args) >= 2)
    observable& follow(Args&&... args) {
        auto wire = [this]<std::size_t... I>(std::index_sequence<I...>, auto sources_and_fn) {
            auto recompute = [this, fn = std::get<sizeof...(I)>(std::move(sources_and_fn)),
                              ...sources = &std::get<I>(sources_and_fn)]() noexcept {
                set(fn(sources->get()...));
            };
            recompute();
            (std::get<I>(sources_and_fn).on_change([recompute](auto const&) noexcept { recompute(); }), ...);
        };
        wire(std::make_index_sequence<sizeof...(Args) - 1>{},
             std::forward_as_tuple(std::forward<Args>(args)...));
        return *this;
    }
};

namespace impl {

// A field a binding can be unbound from: an observable, an observable_list.
template <class Model>
concept unbindable = requires(Model& model) { model.unbind(); };

}  // namespace impl

/**
 * Unbinds the binding watches of every model named -- the teardown a window does
 * from its Closed handler when the models it bound to will outlive it:
 *
 *     unsubscribe_all(settings.minimizeOnClose, settings.theme);
 *
 * Each model lets go of the controls its watches held, so the controls, and
 * the window that held them, are freed. Naming the models is the whole of it:
 * a binding leaves no token for the caller to keep, on purpose -- the model
 * remembers it, and this is where it is spent. A model is any field a binding
 * watches: an observable or an observable_list.
 */
template <impl::unbindable... Models>
void unsubscribe_all(Models&... models) {
    (models.unbind(), ...);
}

namespace impl {

// The class a pointer to member names: the owner whose member a change_watch calls, the Impl
// level whose interface field Object::get() reads (wxl.ui). And the member's own type.
template <class Member>
struct member_owner {};

template <class T, class Owner>
struct member_owner<T Owner::*> {
    using type = Owner;
    using member_t = T;
};

// What a change event hands each of its watchers -- the value of an observable, the
// list_change of an observable_list -- read off the event's callback node, as
// linked_watch reads it, whichever way the event's signature was spelled.
template <class Body>
struct change_argument {};

template <class A, template <class> class Base>
struct change_argument<func_body<void(A) noexcept, Base>> {
    using type = A;
};

// A member a change_watch can call: it takes the change, returns nothing, and neither it
// nor handing it the change throws -- the event's call has nobody to hand an exception to.
template <auto Listener, class Owner, class Argument>
concept change_listener = requires(Owner& owner, Argument change) {
    { (owner.*Listener)(change) } noexcept -> std::same_as<void>;
};

}  // namespace impl

/**
 * An application's watch on a field -- an observable or an observable_list -- kept by
 * a member of its owner and taken off with it. Every change calls `Listener`, a
 * noexcept member of the owner named in the type, so a class declares its watches
 * the way it declares its other members:
 *
 *     class Panel {
 *         void fontSizeChanged(double size) noexcept;
 *         ...
 *         change_watch<observable<double const>, &Panel::fontSizeChanged> fontSizeWatch_;
 *     };
 *
 *     Panel::Panel(Settings& settings) : fontSizeWatch_{settings.fontSize, *this} {}
 *
 * The watch is itself the node in the field's event, so standing there allocates
 * nothing and leaves no token to keep: the destructor takes the node off by its own
 * address. One made empty stands nowhere until attach() puts it on a field, once.
 *
 * The rules are those of any node built into its subscriber, and each one broken
 * stops the process rather than leave the event holding freed memory: the field
 * outlives the watch -- declared after what it watches, the watch goes first -- and
 * the watch is not destroyed from inside its own call. Neither copied nor moved: the
 * node stands in the field's list by its address. Nor declared `const`: the field's
 * list writes the node's link.
 */
template <class Observed, auto Listener>
class change_watch final : public Observed::changed_event::func_t
{
    using event_t = typename Observed::changed_event;
    using func_t = typename event_t::func_t;
    using argument_t = typename impl::change_argument<func_t>::type;
    using owner_t = typename impl::member_owner<decltype(Listener)>::type;

public:
    /// A watch on no field yet: see attach().
    change_watch() noexcept = default;

    /// A watch on `field` from the start, calling `owner`.
    change_watch(Observed& field, owner_t& owner) noexcept { attach(field, owner); }

    /// Takes the watch off its field, if it is on one.
    ~change_watch() override {
        Observed* const field = std::exchange(field_, nullptr);
        if (!field) return;

        if (!field->remove_change(static_cast<func_t&>(*this)))
            abort("wxl: a change_watch is no longer on its field");
        // The event lets go of a removed node at once, unless that node's call is under
        // way: then only once the call returns -- into a watch that is gone by then.
        if (owner_) abort("wxl: a change_watch is destroyed from inside its own call");
    }

    /// Puts the watch on `field`: from now on each change calls the listener on
    /// `owner`. Once -- a watch names one field for its life, and a second attach
    /// stops the process rather than leave the first field calling the owner or
    /// move the watch behind the back of whoever put it there.
    void attach(Observed& field, owner_t& owner) noexcept {
        // Checked here and not in the class: the watch is declared inside its owner,
        // which is not complete there, and a member cannot be called on it yet.
        static_assert(impl::change_listener<Listener, owner_t, argument_t>,
                      "wxl: a change_watch calls a member of its owner with the change, from "
                      "inside the field's event, which has nobody to hand an exception to -- name "
                      "a noexcept member that takes the change and returns nothing");

        if (field_) abort("wxl: a change_watch is attached twice");
        field_ = &field;
        owner_ = &owner;
        static_cast<void>(field.on_change(as_not_null<func_t>(this)));
    }

    /// Whether the watch is on a field.
    bool attached() const noexcept { return field_ != nullptr; }

private:
    void operator()(argument_t change) noexcept override { (owner_->*Listener)(change); }

    /// The field lets go of the node. Only the destructor takes it off, and clears
    /// field_ first: let go of while still on its field, the watch has outlived the
    /// field, or somebody else took it off.
    void release() noexcept override {
        if (field_)
            abort("wxl: a change_watch outlived its field, or was taken off it by someone else");
        owner_ = nullptr;
    }

    Observed* field_ = nullptr;
    // Cleared when the field lets go of the node: still set after the destructor has
    // taken the node off, it says the release was put off for a call under way.
    owner_t* owner_ = nullptr;
};

}  // export namespace wxl::core
