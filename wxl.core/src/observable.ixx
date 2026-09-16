export module wxl.core:observable;

import :event;
import std;

export namespace wxl::core {

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
 * cookie `on_change` returns. A binding's watches are kept apart (see
 * watch_for_binding), and all of them go with the field: destroying the model
 * destroys every watch, and what a watch owned -- the control it wrote to --
 * is let go with it.
 */
template <class T>
class observable {
public:
    // Spelled noexcept: a watcher is called from inside fire(), which walks the
    // whole list and has nobody to hand an exception to, so a throwing watcher
    // would break the chain and abandon the watchers after it. The event enforces
    // this on every callback regardless; spelling it here makes the contract read
    // off the type, which is the reason event accepts the noexcept signature.
    using changed_event = event<void(T const&) noexcept>;

    observable() = default;
    explicit observable(T value) : value_(std::move(value)) {}

    // A field, not a handle: copying one would make a second state that no
    // binding to the first would ever hear of. Share the model instead.
    observable(observable const&) = delete;
    observable& operator=(observable const&) = delete;

    /// The value now.
    T const& get() const noexcept { return value_; }

    /// Sets the value and, if it actually changed, tells the watchers. Setting
    /// it to what it already holds says nothing: a binding writing back the
    /// value it has just read must not set off an echo between the two sides.
    ///
    /// The bindings hear first, the application after: by the time a watcher
    /// added through on_change runs -- to save the model, say -- the controls
    /// already show what it is saving.
    void set(T value) {
        if constexpr (std::equality_comparable<T>) {
            if (value_ == value) return;
        }
        value_ = std::move(value);
        bound_.fire(value_);
        changed_.fire(value_);
    }

    /// Watches for changes; the returned cookie removes the watch. The callback
    /// is `event`'s, so it has to be noexcept -- add() below says so.
    ///
    /// The cookie is the caller's: an application watching the model to react
    /// removes the watch when it likes, or lets it go with the model. A
    /// binding's own watch is a different thing -- see watch_for_binding.
    template <class F>
    cookie_t on_change(F&& callback) {
        return changed_.add(std::forward<F>(callback));
    }

    bool remove_change(cookie_t cookie) { return changed_.remove(cookie); }

    /// Watches for changes on behalf of a binding, the field keeping the watch
    /// rather than handing a cookie back. This is not a convenience over
    /// on_change: a binding's model->control watch *owns the control* -- it
    /// must, to write to it, and a borrowed pointer would dangle, the control
    /// being a temporary in the description tree. Kept on a list of their own
    /// so that unbind() can cut them without touching the application's.
    template <class F>
    void watch_for_binding(F&& callback) {
        static_cast<void>(bound_.add(std::forward<F>(callback)));
    }

    /// Cuts every binding watch (see watch_for_binding), letting go of whatever
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

private:
    T value_{};
    // Two lists rather than one with a mark: the bindings' watches are the ones
    // unbind() drops, the application's are the ones it must not touch. Declared
    // after the value, so that they are destroyed first -- a binding's watch
    // removes the handler it put on its control as it goes, and does so while
    // the field it named is still there.
    changed_event bound_;
    changed_event changed_;
};

/**
 * Cuts the binding watches of every model named -- the teardown a window does
 * from its Closed handler when the models it bound to will outlive it:
 *
 *     unsubscribe_all(settings.minimizeOnClose, settings.theme);
 *
 * Each model lets go of the controls its watches held, so the controls, and
 * the window that held them, are freed. Naming the models is the whole of it:
 * a binding leaves no cookie for the caller to keep, on purpose -- the model
 * remembers it, and this is where it is spent.
 */
template <class... Ts>
void unsubscribe_all(observable<Ts>&... models) {
    (models.unbind(), ...);
}

}  // export namespace wxl::core
