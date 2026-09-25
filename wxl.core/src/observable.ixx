export module wxl.core:observable;

import :event;
import std;

export namespace wxl::core {

template <class T>
class observable;

/**
 * The read-only face of a field: `observable<T const>`. A model hands it out
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
 * bindings' watches. So BindOutput takes the face -- the control follows the
 * value -- while Bind and BindInput, which write into the field, do not.
 *
 * Only an `observable<T>` is ever made: the constructors and the destructor
 * are protected, so there is no face without the field behind it, and no way
 * to destroy a field through its face.
 */
template <class T>
class observable<T const> {
public:
    // Spelled noexcept: a watcher is called from inside fire(), which walks the
    // whole list and has nobody to hand an exception to, so a throwing watcher
    // would break the chain and abandon the watchers after it. The event enforces
    // this on every callback regardless; spelling it here makes the contract read
    // off the type, which is the reason event accepts the noexcept signature.
    using changed_event = event<void(T const&) noexcept>;

    // A field, not a handle: copying one would make a second state that no
    // binding to the first would ever hear of. Share the model instead.
    observable(observable const&) = delete;
    observable& operator=(observable const&) = delete;

    /// The value now.
    T const& get() const noexcept { return value_; }

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

protected:
    observable() = default;
    explicit observable(T value) : value_(std::move(value)) {}
    ~observable() = default;

    T value_{};
    // Two lists rather than one with a mark: the bindings' watches are the ones
    // unbind() drops, the application's are the ones it must not touch. Declared
    // after the value, so that they are destroyed first -- a binding's watch
    // removes the handler it put on its control as it goes, and does so while
    // the field it named is still there.
    changed_event bound_;
    changed_event changed_;
};

namespace impl {

// The face a field extends, named apart from the field itself: Doxygen reads a
// base of the same template as a class deriving from itself, whatever the
// arguments.
template <class T>
using observable_face = observable<T const>;

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
 * cookie `on_change` returns. A binding's watches are kept apart (see
 * watch_for_binding), and all of them go with the field: destroying the model
 * destroys every watch, and what a watch owned -- the control it wrote to --
 * is let go with it.
 *
 * What it adds to its read-only face (`observable<T const>`, above) is setting:
 * set() and follow().
 */
template <class T>
class observable : public impl::observable_face<T> {
    using face = impl::observable_face<T>;

public:
    observable() = default;
    explicit observable(T value) : face(std::move(value)) {}

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
        this->bound_.fire(this->value_);
        this->changed_.fire(this->value_);
    }

    /// Follows other fields: whenever any of the sources changes, `fn` -- the
    /// last argument -- is called with the current values of all of them and
    /// the result becomes this field's value. It is computed once right here
    /// too, so the field is in step from the moment it follows rather than
    /// from the first change after. A source may be a read-only face.
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
