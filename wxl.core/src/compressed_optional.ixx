export module wxl.core:compressed_optional;

import :not_null;
import std;

export namespace wxl::core {

/// Specifies that `TSentinel` names a sentinel value of `T`: a `noexcept` static `sentinel()`
/// that returns it, and a `noexcept` static `is_sentinel()` that tests a value for it.
template <typename TSentinel, typename T>
concept sentinel = requires(const T& val) {
    { TSentinel::sentinel() } noexcept -> std::same_as<T>;
    { TSentinel::is_sentinel(val) } noexcept -> std::same_as<bool>;
};

template <typename T, sentinel<T> TSentinel>
class compressed_optional;

/// Satisfied by a specialization of `std::optional` and by a class derived from one.
template <typename X>
concept is_std_optional =
    requires { []<typename W>(const std::optional<W>&) {}(std::declval<const X&>()); };

/// std::optional or compressed_optional: a type whose default value is "nothing". Told by the
/// template rather than by members, because std::expected has the same members and a default
/// that holds a value.
template <typename X>
concept optional_like = is_std_optional<X> || requires {
    []<typename W, sentinel<W> S>(const compressed_optional<W, S>&) {}(std::declval<const X&>());
};

namespace impl {

// The default answer, for a type that has not named a spare value of its own:
// a pointer to it, empty when that pointer is null.
template <typename T>
struct raw_pointer_sentinel {
    static constexpr bool empty_is_nullptr = true;

    static constexpr T* sentinel() noexcept { return nullptr; }
    static constexpr bool is_sentinel(T* value) noexcept { return value == nullptr; }
};

}  // namespace impl

/// Selects the type of `nullable<T>`: the place where a type states what its empty state looks
/// like. A specialization, declared beside the type, names the value that stands for "no value",
/// such as the largest duration, a NaN, an empty `std::exception_ptr` or a wrapper over a null
/// `Impl`.
///
/// The primary template selects a pointer to the type: `nullable<Widget>` is one machine word
/// that holds a `Widget*` which may be null, and its value is accessed as `not_null<Widget>`. A
/// class cannot spare a value of its own, but a pointer to it can spare null.
template <typename T>
struct optional_selector {
    /// The type that represents an optional `T`: a `compressed_optional` over `T*` that is empty
    /// when the pointer is null.
    using nullable = compressed_optional<T*, impl::raw_pointer_sentinel<T>>;
};

/// The type that represents an optional `T`, as selected by `wxl::core::optional_selector`.
template <typename T>
using nullable = typename optional_selector<T>::nullable;

/// Represents an optional value whose empty state is a sentinel value of `T`, named by
/// `TSentinel`, so the object is the size of `T`.
///
/// The interface follows `std::optional`. Emptiness is decided by `TSentinel::is_sentinel`, so
/// every value it accepts reads as empty, not only the one `TSentinel::sentinel` returns; for a
/// floating-point `T`, every NaN reads as empty.
///
/// When `T` is a raw pointer, the value is accessed as a `not_null`, which states in its type
/// that the value is present; the storage remains the bare pointer. Constness carries over: a
/// const optional over `Widget*` gives access through `not_null<const Widget>`.
template <typename T, sentinel<T> TSentinel>
class compressed_optional {
    T value_;

    template <typename U, sentinel<U> USentinel>
    friend class compressed_optional;

    // A raw pointer is handed out as a not_null. The optional has already
    // answered the one question a null could have been the answer to, so what
    // comes out of it cannot be null and says so in its own type; asking it
    // again is then impossible rather than merely pointless. The storage does
    // not change -- it is still the bare T, and still one machine word.
    //
    // Constness travels with it: a const optional over `Widget*` hands out a
    // not_null<const Widget>, so what is reached through a const holder is
    // const too.
    static constexpr bool is_pointer_type = std::is_pointer_v<T>;
    using raw_element_type = std::remove_pointer_t<T>;

    using access_type = std::conditional_t<is_pointer_type, not_null<raw_element_type>, T&>;
    using const_access_type =
        std::conditional_t<is_pointer_type, not_null<const raw_element_type>, const T&>;
    using rvalue_access_type = std::conditional_t<is_pointer_type, not_null<raw_element_type>, T&&>;
    using const_rvalue_access_type =
        std::conditional_t<is_pointer_type, not_null<const raw_element_type>, const T&&>;

    using pointer_access_type = std::conditional_t<is_pointer_type, not_null<raw_element_type>, T*>;
    using const_pointer_access_type =
        std::conditional_t<is_pointer_type, not_null<const raw_element_type>, const T*>;

public:
    static_assert(!std::is_same_v<std::remove_cv_t<T>, std::nullopt_t> &&
                      !std::is_same_v<std::remove_cv_t<T>, std::in_place_t>,
                  "T in compressed_optional<T> must be a type other than nullopt_t or in_place_t "
                  "(N4950 [optional.optional.general]/3).");
    static_assert(std::is_object_v<T> && std::is_destructible_v<T> && !std::is_array_v<T>,
                  "T in compressed_optional<T> must meet the Cpp17Destructible requirements (N4950 "
                  "[optional.optional.general]/3).");

    /// The type of the contained value, `T`.
    using value_type = T;
    /// The type that names the sentinel value and tests for it, `TSentinel`.
    using sentinel_provider_type = TSentinel;

    // --- КОНСТРУКТОРЫ ---

    /// Constructs an empty object.
    constexpr compressed_optional() noexcept : value_(sentinel_provider_type::sentinel()) {}

    /// Constructs an empty object.
    constexpr compressed_optional(std::nullopt_t) noexcept
        : value_(sentinel_provider_type::sentinel()) {}

    /// Empty, spelled `nullptr` -- for the types whose empty state already had
    /// that name before it was optional.
    ///
    /// A sentinel provider asks for this by declaring `empty_is_nullptr`, and
    /// only the ones for which it reads as English do: a wrapper over a null
    /// Impl is a null thing, and `nullable<Border> root = nullptr` is how that
    /// has always been written. A `nullable<int>` gets no such constructor,
    /// because "an int that is nullptr" means nothing -- so no type ends up
    /// with two ways to say empty.
    template <typename S = TSentinel>
        requires requires { S::empty_is_nullptr; }
    constexpr compressed_optional(decltype(nullptr)) noexcept
        : value_(sentinel_provider_type::sentinel()) {}

    /// Constructs a copy of another `compressed_optional`, empty when that one is empty.
    constexpr compressed_optional(const compressed_optional&) noexcept = default;
    /// Constructs an object from a moved `compressed_optional`, empty when that one is empty.
    constexpr compressed_optional(compressed_optional&&) noexcept = default;

    /// Constructs an object whose value is initialized from \p value. The constructor is
    /// explicit when `U` does not convert implicitly to `T`.
    template <typename U = T>
        requires(!std::is_same_v<std::remove_cvref_t<U>, compressed_optional>) &&
                (!std::is_same_v<std::remove_cvref_t<U>, std::in_place_t>) &&
                (!std::is_same_v<std::remove_cvref_t<U>, std::nullopt_t>) &&
                (!optional_like<U>) && std::is_constructible_v<T, U>
    constexpr explicit(!std::is_convertible_v<U, T>)
        compressed_optional(U&& value) noexcept(std::is_nothrow_constructible_v<T, U>)
        : value_(std::forward<U>(value)) {}

    /// Constructs an object from a `compressed_optional` over another value type: its value is
    /// a `T` constructed from the value of \p other, or the object is empty when \p other is
    /// empty. The constructor is explicit when `const U&` does not convert implicitly to `T`.
    ///
    /// Both operands of the conditional expression in the initializer are of type `T`. With
    /// operands of types `U` and `T`, the conditional has to find a common type, and for a
    /// conversion from a derived wrapper to its base there is none, although every conversion it
    /// needs exists. Constructing `T` explicitly requests exactly the conversion that the
    /// constraint checks.
    template <typename U, sentinel<U> USentinel>
        requires std::is_constructible_v<T, const U&>
    constexpr explicit(!std::is_convertible_v<const U&, T>)
        compressed_optional(const compressed_optional<U, USentinel>& other) noexcept
        : value_(other.has_value() ? T{other.value_} : sentinel_provider_type::sentinel()) {}

    /// Constructs an object from an rvalue `compressed_optional` over another value type: its
    /// value is a `T` constructed from the moved value of \p other, or the object is empty when
    /// \p other is empty. The constructor is explicit when `U` does not convert implicitly to
    /// `T`.
    template <typename U, sentinel<U> USentinel>
        requires std::is_constructible_v<T, U>
    constexpr explicit(!std::is_convertible_v<U, T>)
        compressed_optional(compressed_optional<U, USentinel>&& other) noexcept
        : value_(other.has_value() ? T{std::move(other.value_)}
                                   : sentinel_provider_type::sentinel()) {}

    /// Constructs an object from a `std::optional`: its value is constructed from the value of
    /// \p other, or the object is empty when \p other is empty. The constructor is explicit
    /// when `const U&` does not convert implicitly to `T`.
    template <typename U>
        requires std::is_constructible_v<T, const U&>
    constexpr explicit(!std::is_convertible_v<const U&, T>)
        compressed_optional(const std::optional<U>& other) noexcept
        : value_(other.has_value() ? *other : sentinel_provider_type::sentinel()) {}

    /// Constructs an object from an rvalue `std::optional`: its value is constructed from the
    /// moved value of \p other, or the object is empty when \p other is empty. The constructor
    /// is explicit when `U` does not convert implicitly to `T`.
    template <typename U>
        requires std::is_constructible_v<T, U>
    constexpr explicit(!std::is_convertible_v<U, T>)
        compressed_optional(std::optional<U>&& other) noexcept
        : value_(other.has_value() ? std::move(*other) : sentinel_provider_type::sentinel()) {}

    /// Constructs the value in place from \p args.
    template <typename... Args>
        requires std::is_constructible_v<T, Args...>
    constexpr explicit compressed_optional(std::in_place_t, Args&&... args) noexcept(
        std::is_nothrow_constructible_v<T, Args...>)
        : value_(std::forward<Args>(args)...) {}

    /// Constructs the value in place from \p ilist and \p args.
    template <typename U, typename... Args>
        requires std::is_constructible_v<T, std::initializer_list<U>&, Args...>
    constexpr explicit compressed_optional(
        std::in_place_t, std::initializer_list<U> ilist,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<T, std::initializer_list<U>&,
                                                                 Args...>)
        : value_(ilist, std::forward<Args>(args)...) {}

    // --- ОПЕРАТОРЫ ПРИСВАИВАНИЯ ---

    /// Assigns a copy of another `compressed_optional`, including its empty state.
    constexpr compressed_optional& operator=(const compressed_optional&) noexcept = default;
    /// Assigns from a moved `compressed_optional`, including its empty state.
    constexpr compressed_optional& operator=(compressed_optional&&) noexcept = default;

    /// Resets the object to the empty state.
    constexpr compressed_optional& operator=(std::nullopt_t) noexcept {
        reset();
        return *this;
    }

    /// Resets the object to the empty state. Available when the sentinel provider declares
    /// `empty_is_nullptr`.
    template <typename S = TSentinel>
        requires requires { S::empty_is_nullptr; }
    constexpr compressed_optional& operator=(decltype(nullptr)) noexcept {
        reset();
        return *this;
    }

    /// Assigns \p value to the contained value.
    template <typename U = T>
        requires(!std::is_same_v<std::remove_cvref_t<U>, compressed_optional>) &&
                (!std::is_same_v<std::remove_cvref_t<U>, std::nullopt_t>) &&
                (!optional_like<U>) && std::is_constructible_v<T, U> &&
                std::is_assignable_v<T&, std::decay_t<U>>
    constexpr compressed_optional& operator=(U&& value) noexcept(
        std::is_nothrow_assignable_v<T&, U>) {
        value_ = std::forward<U>(value);
        return *this;
    }

    /// Assigns the value of \p other, a `compressed_optional` over another value type, or
    /// resets the object when \p other is empty.
    template <typename U, sentinel<U> USentinel>
        requires std::is_constructible_v<T, const U&> && std::is_assignable_v<T&, std::decay_t<const U&>>
    constexpr compressed_optional& operator=(
        const compressed_optional<U, USentinel>& other) noexcept {
        if (other.has_value()) [[likely]]
            value_ = other.value_;
        else
            reset();
        return *this;
    }

    /// Assigns the moved value of \p other, a `compressed_optional` over another value type, or
    /// resets the object when \p other is empty.
    template <typename U, sentinel<U> USentinel>
        requires std::is_constructible_v<T, U> && std::is_assignable_v<T&, std::decay_t<U>>
    constexpr compressed_optional& operator=(compressed_optional<U, USentinel>&& other) noexcept {
        if (other.has_value()) [[likely]]
            value_ = std::move(other.value_);
        else
            reset();
        return *this;
    }

    /// Assigns the value of \p other, a `std::optional`, or resets the object when \p other is
    /// empty.
    template <typename U>
        requires std::is_constructible_v<T, const U&> && std::is_assignable_v<T&, std::decay_t<const U&>>
    constexpr compressed_optional& operator=(const std::optional<U>& other) noexcept {
        if (other.has_value()) [[likely]]
            value_ = *other;
        else
            reset();
        return *this;
    }

    /// Assigns the moved value of \p other, a `std::optional`, or resets the object when
    /// \p other is empty.
    template <typename U>
        requires std::is_constructible_v<T, U> && std::is_assignable_v<T&, std::decay_t<U>>
    constexpr compressed_optional& operator=(std::optional<U>&& other) noexcept {
        if (other.has_value()) [[likely]]
            value_ = std::move(*other);
        else
            reset();
        return *this;
    }

    // --- МЕТОДЫ MODIFIERS ---

    /// Replaces the contained value with one constructed from \p args and returns a reference
    /// to it. When the construction may throw and `T` is move-constructible, the new value is
    /// built in a temporary and move-assigned, so an exception from the constructor leaves the
    /// current value unchanged.
    template <typename... Args>
        requires std::is_constructible_v<T, Args...>
    constexpr T& emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
        if constexpr (std::is_nothrow_constructible_v<T, Args...> ||
                      !std::is_move_constructible_v<T>) {
            std::destroy_at(&value_);
            std::construct_at(&value_, std::forward<Args>(args)...);
        } else {
            // Транзакционная безопасность для исключений
            T tmp(std::forward<Args>(args)...);
            value_ = std::move(tmp);
        }
        return value_;
    }

    /// Replaces the contained value with one constructed from \p ilist and \p args and returns
    /// a reference to it. When the construction may throw and `T` is move-constructible, the new
    /// value is built in a temporary and move-assigned, so an exception from the constructor
    /// leaves the current value unchanged.
    template <typename U, typename... Args>
        requires std::is_constructible_v<T, std::initializer_list<U>&, Args...>
    constexpr T& emplace(std::initializer_list<U> ilist, Args&&... args) noexcept(
        std::is_nothrow_constructible_v<T, std::initializer_list<U>&, Args...>) {
        if constexpr (std::is_nothrow_constructible_v<T, std::initializer_list<U>&, Args...> ||
                      !std::is_move_constructible_v<T>) {
            std::destroy_at(&value_);
            std::construct_at(&value_, ilist, std::forward<Args>(args)...);
        } else {
            T tmp(ilist, std::forward<Args>(args)...);
            value_ = std::move(tmp);
        }
        return value_;
    }

    /// Exchanges the stored values, empty states included, of this object and \p other.
    constexpr void swap(compressed_optional& other) noexcept(std::is_nothrow_swappable_v<T>) {
        std::swap(value_, other.value_);
    }

    /// Exchanges the stored values, empty states included, of \p lhs and \p rhs.
    friend constexpr void swap(compressed_optional& lhs,
                               compressed_optional& rhs) noexcept(noexcept(lhs.swap(rhs))) {
        lhs.swap(rhs);
    }

    // --- МЕТОДЫ НАБЛЮДАТЕЛИ (OBSERVERS) ---

    /// Tests whether the object holds a value.
    constexpr explicit operator bool() const noexcept { return has_value(); }

    /// Tests whether the object holds a value, that is, whether `TSentinel::is_sentinel` returns
    /// `false` for the stored value.
    constexpr bool has_value() const noexcept {
        return !sentinel_provider_type::is_sentinel(value_);
    }

    /// Returns a const reference to the contained value, or a `not_null` over the const element
    /// when `T` is a pointer.
    /// \throw std::bad_optional_access The object is empty.
    [[nodiscard]] constexpr const_access_type value() const& {
        if (has_value()) [[likely]] {
            if constexpr (is_pointer_type) return not_null<const raw_element_type>(value_);
            else return value_;
        }
        throw std::bad_optional_access{};
    }

    /// Returns a reference to the contained value, or a `not_null` over the element when `T` is
    /// a pointer.
    /// \throw std::bad_optional_access The object is empty.
    [[nodiscard]] constexpr access_type value() & {
        if (has_value()) [[likely]] {
            if constexpr (is_pointer_type) return not_null<raw_element_type>(value_);
            else return value_;
        }
        throw std::bad_optional_access{};
    }

    /// Returns an rvalue reference to the contained value, or a `not_null` over the element when
    /// `T` is a pointer.
    /// \throw std::bad_optional_access The object is empty.
    [[nodiscard]] constexpr rvalue_access_type value() && {
        if (has_value()) [[likely]] {
            if constexpr (is_pointer_type) return not_null<raw_element_type>(value_);
            else return std::move(value_);
        }
        throw std::bad_optional_access{};
    }

    /// Returns a const rvalue reference to the contained value, or a `not_null` over the const
    /// element when `T` is a pointer.
    /// \throw std::bad_optional_access The object is empty.
    [[nodiscard]] constexpr const_rvalue_access_type value() const&& {
        if (has_value()) [[likely]] {
            if constexpr (is_pointer_type) return not_null<const raw_element_type>(value_);
            else return std::move(value_);
        }
        throw std::bad_optional_access{};
    }

    /// Returns a copy of the contained value, or \p default_value converted to `T` when the
    /// object is empty.
    template <typename U>
        requires std::is_copy_constructible_v<T> && std::is_convertible_v<U, T>
    constexpr T value_or(U&& default_value) const& noexcept {
        if (has_value()) return value_;
        return static_cast<T>(std::forward<U>(default_value));
    }

    /// Returns the contained value moved out of the object, or \p default_value converted to
    /// `T` when the object is empty.
    template <typename U>
        requires std::is_move_constructible_v<T> && std::is_convertible_v<U, T>
    constexpr T value_or(U&& default_value) && noexcept {
        if (has_value()) return std::move(value_);
        return static_cast<T>(std::forward<U>(default_value));
    }

    // --- МОНАДИЧЕСКИЙ ИНТЕРФЕЙС (AND_THEN) ---

    /// Invokes \p f with the contained value as `T&` and returns its result, or returns a
    /// value-initialized object of the result type when the object is empty.
    template <typename F>
    constexpr auto and_then(F&& f) & -> std::invoke_result_t<F, T&> {
        using ret_t = std::invoke_result_t<F, T&>;
        if (has_value()) return std::invoke(std::forward<F>(f), value_);
        return ret_t{};
    }

    /// Invokes \p f with the contained value as `const T&` and returns its result, or returns a
    /// value-initialized object of the result type when the object is empty.
    template <typename F>
    constexpr auto and_then(F&& f) const& -> std::invoke_result_t<F, const T&> {
        using ret_t = std::invoke_result_t<F, const T&>;
        if (has_value()) return std::invoke(std::forward<F>(f), value_);
        return ret_t{};
    }

    /// Invokes \p f with the contained value as `T&&` and returns its result, or returns a
    /// value-initialized object of the result type when the object is empty.
    template <typename F>
    constexpr auto and_then(F&& f) && -> std::invoke_result_t<F, T&&> {
        using ret_t = std::invoke_result_t<F, T&&>;
        if (has_value()) return std::invoke(std::forward<F>(f), std::move(value_));
        return ret_t{};
    }

    /// Invokes \p f with the contained value as `const T&&` and returns its result, or returns
    /// a value-initialized object of the result type when the object is empty.
    template <typename F>
    constexpr auto and_then(F&& f) const&& -> std::invoke_result_t<F, const T&&> {
        using ret_t = std::invoke_result_t<F, const T&&>;
        if (has_value()) return std::invoke(std::forward<F>(f), std::move(value_));
        return ret_t{};
    }

    // --- МОНАДИЧЕСКИЙ ИНТЕРФЕЙС (TRANSFORM) ---

    /// Invokes \p f with the contained value as `T&` and returns its result in a `nullable` of
    /// the result type, or an empty `nullable` when the object is empty.
    template <typename F>
    constexpr auto transform(F&& f) & -> nullable<std::invoke_result_t<F, T&>> {
        using ret_t = nullable<std::invoke_result_t<F, T&>>;
        if (has_value()) [[likely]]
            return ret_t(std::invoke(std::forward<F>(f), value_));  // Исправлено: std::forward<F>
        return ret_t{};
    }

    /// Invokes \p f with the contained value as `const T&` and returns its result in a
    /// `nullable` of the result type, or an empty `nullable` when the object is empty.
    template <typename F>
    constexpr auto transform(F&& f) const& -> nullable<std::invoke_result_t<F, const T&>> {
        using ret_t = nullable<std::invoke_result_t<F, const T&>>;
        if (has_value()) [[likely]]
            return ret_t(std::invoke(std::forward<F>(f), value_));  // Исправлено: std::forward<F>
        return ret_t{};
    }

    /// Invokes \p f with the contained value as `T&&` and returns its result in a `nullable` of
    /// the result type, or an empty `nullable` when the object is empty.
    template <typename F>
    constexpr auto transform(F&& f) && -> nullable<std::invoke_result_t<F, T&&>> {
        using ret_t = nullable<std::invoke_result_t<F, T&&>>;
        if (has_value()) [[likely]]
            return ret_t(
                std::invoke(std::forward<F>(f), std::move(value_)));  // Исправлено: std::forward<F>
        return ret_t{};
    }

    /// Invokes \p f with the contained value as `const T&&` and returns its result in a
    /// `nullable` of the result type, or an empty `nullable` when the object is empty.
    template <typename F>
    constexpr auto transform(F&& f) const&& -> nullable<std::invoke_result_t<F, const T&&>> {
        using ret_t = nullable<std::invoke_result_t<F, const T&&>>;
        if (has_value()) [[likely]]
            return ret_t(
                std::invoke(std::forward<F>(f), std::move(value_)));  // Исправлено: std::forward<F>
        return ret_t{};
    }

    // --- МОНАДИЧЕСКИЙ ИНТЕРФЕЙС (OR_ELSE) ---

    /// Returns a copy of this object when it holds a value; otherwise returns the result of
    /// invoking \p f.
    template <typename F>
        requires std::is_copy_constructible_v<T>
    constexpr auto or_else(F&& f) const& -> compressed_optional {
        if (has_value()) return *this;
        return std::forward<F>(f)();
    }

    /// Returns this object, moved, when it holds a value; otherwise returns the result of
    /// invoking \p f.
    template <typename F>
        requires std::is_move_constructible_v<T>
    constexpr auto or_else(F&& f) && -> compressed_optional {
        if (has_value()) return std::move(*this);
        return std::forward<F>(f)();
    }

    /// Resets the object to the empty state by assigning the sentinel value.
    constexpr void reset() noexcept { value_ = sentinel_provider_type::sentinel(); }

    // --- ОПЕРАТОРЫ ДОСТУПА ПО УКАЗАТЕЛЮ ---

    /// Returns a pointer to the contained value, or a `not_null` over the element when `T` is a
    /// pointer. The object must hold a value: for a pointer `T` an empty object aborts the
    /// process, and for any other `T` no check is made.
    constexpr pointer_access_type operator->() noexcept {
        if constexpr (is_pointer_type) return not_null<raw_element_type>(value_);
        else return &value_;
    }

    /// Returns a pointer to the const contained value, or a `not_null` over the const element
    /// when `T` is a pointer. The object must hold a value: for a pointer `T` an empty object
    /// aborts the process, and for any other `T` no check is made.
    constexpr const_pointer_access_type operator->() const noexcept {
        if constexpr (is_pointer_type) return not_null<const raw_element_type>(value_);
        else return &value_;
    }

    /// Returns a reference to the contained value, or a `not_null` over the element when `T` is
    /// a pointer. The object must hold a value: for a pointer `T` an empty object aborts the
    /// process, and for any other `T` no check is made.
    constexpr access_type operator*() & noexcept {
        if constexpr (is_pointer_type) return not_null<raw_element_type>(value_);
        else return value_;
    }

    /// Returns a const reference to the contained value, or a `not_null` over the const element
    /// when `T` is a pointer. The object must hold a value: for a pointer `T` an empty object
    /// aborts the process, and for any other `T` no check is made.
    constexpr const_access_type operator*() const& noexcept {
        if constexpr (is_pointer_type) return not_null<const raw_element_type>(value_);
        else return value_;
    }

    /// Returns an rvalue reference to the contained value, or a `not_null` over the element when
    /// `T` is a pointer. The object must hold a value: for a pointer `T` an empty object aborts
    /// the process, and for any other `T` no check is made.
    constexpr rvalue_access_type operator*() && noexcept {
        if constexpr (is_pointer_type) return not_null<raw_element_type>(value_);
        else return std::move(value_);
    }

    /// Returns a const rvalue reference to the contained value, or a `not_null` over the const
    /// element when `T` is a pointer. The object must hold a value: for a pointer `T` an empty
    /// object aborts the process, and for any other `T` no check is made.
    constexpr const_rvalue_access_type operator*() const&& noexcept {
        if constexpr (is_pointer_type) return not_null<const raw_element_type>(value_);
        else return std::move(value_);
    }

    // --- СРАВНЕНИЯ (МЕЖДУ КОНТЕЙНЕРАМИ) ---

    /// Tests whether \p lhs and \p rhs are equal: both empty, or both holding equal values.
    friend constexpr bool operator==(const compressed_optional& lhs,
                                     const compressed_optional& rhs) noexcept {
        const bool lhs_has = lhs.has_value();
        if (lhs_has != rhs.has_value()) [[unlikely]]
            return false;
        if (!lhs_has) [[unlikely]]
            return true;
        return lhs.value_ == rhs.value_;
    }

    /// Compares \p lhs and \p rhs: an empty object orders before one that holds a value, two
    /// empty objects are equivalent, and two values compare with the `operator<=>` of `T`.
    [[nodiscard]] friend constexpr auto operator<=>(const compressed_optional& lhs,
                                                    const compressed_optional& rhs) noexcept
        requires requires(const T& a, const T& b) { { a <=> b }; }
    {
        using ResultType = decltype(std::declval<const T&>() <=> std::declval<const T&>());

        const bool lhs_has = lhs.has_value();

        if (lhs_has != rhs.has_value()) [[unlikely]]
            return lhs_has ? static_cast<ResultType>(std::partial_ordering::greater)
                           : static_cast<ResultType>(std::partial_ordering::less);

        if (!lhs_has) [[unlikely]]
            return static_cast<ResultType>(std::partial_ordering::equivalent);

        return lhs.value_ <=> rhs.value_;
    }

    // --- СРАВНЕНИЯ С NULLOPT ---

    /// Tests whether \p opt is empty.
    friend constexpr bool operator==(const compressed_optional& opt, std::nullopt_t) noexcept {
        return !opt.has_value();
    }

    /// Compares \p opt with `std::nullopt`: an object that holds a value is greater, and an
    /// empty object is equivalent.
    friend constexpr auto operator<=>(const compressed_optional& opt, std::nullopt_t) noexcept
        -> std::partial_ordering {
        return opt.has_value() ? std::partial_ordering::greater : std::partial_ordering::equivalent;
    }

    //  --- СРАВНЕНИЯ С ТИПОМ T ---

    /// Tests whether \p lhs holds a value equal to \p rhs.
    friend constexpr bool operator==(const compressed_optional& lhs, const T& rhs) noexcept {
        return lhs.has_value() && (lhs.value_ == rhs);
    }

    /// Compares \p lhs with \p rhs: an empty object orders before any value, and a held value
    /// compares with the `operator<=>` of `T`.
    [[nodiscard]] friend constexpr auto operator<=>(const compressed_optional& lhs,
                                                    const T& rhs) noexcept
        requires requires(const T & a, const T & b) { { a <=> b }; }
    {

        using ResultType = decltype(std::declval<const T&>() <=> std::declval<const T&>());

        if (lhs.has_value()) [[likely]]
            return lhs.value_ <=> rhs;

        return static_cast<ResultType>(std::partial_ordering::less);
    }
};

namespace impl {

// The one value a signed integer has that is not the negation of any other:
// sign bit set, everything else zero. Negating it does not fit, so arithmetic
// that reaches it is already wrong, and no counting, index or measurement ever
// lands there on purpose -- which is exactly what a sentinel has to be.
template <std::signed_integral T>
struct integral_min_sentinel {
    static constexpr T sentinel() noexcept { return std::numeric_limits<T>::min(); }

    static constexpr bool is_sentinel(T value) noexcept {
        return value == std::numeric_limits<T>::min();
    }
};

// The mirror of it for the unsigned types, and the same argument read the
// other way round: the largest representable value is where counting stops,
// so no count, size or index arrives there having meant to. core::duration
// reserved its own maximum on exactly this reasoning long before the rule was
// general; this is that rule made general.
template <std::unsigned_integral T>
struct integral_max_sentinel {
    static constexpr T sentinel() noexcept { return std::numeric_limits<T>::max(); }

    static constexpr bool is_sentinel(T value) noexcept {
        return value == std::numeric_limits<T>::max();
    }
};

// Not-a-number, for the types that have one. A NaN is what arithmetic answers
// when there is no answer, so a floating-point value that reaches one is
// already saying "nothing"; the sentinel only agrees with it.
//
// Every NaN is the sentinel, not one chosen bit pattern, and that is the
// price: a NaN carried on purpose reads back as empty. It is the right price
// because a NaN's payload does not survive the first operation performed on
// it, and a sentinel that evaporates is worse than none. Where a NaN is real
// data -- which in a measurement, a coordinate or an offset it never is -- the
// type needs a selector of its own instead of this one.
template <std::floating_point T>
struct quiet_nan_sentinel {
    static constexpr T sentinel() noexcept { return std::numeric_limits<T>::quiet_NaN(); }

    static constexpr bool is_sentinel(T value) noexcept { return value != value; }
};

// A null exception_ptr already means "no exception", so the holder's empty
// state is a value the type has of its own and there is nothing to keep
// beside it.
struct exception_ptr_sentinel {
    inline static std::exception_ptr sentinel() noexcept { return {}; }

    inline static bool is_sentinel(const std::exception_ptr& error) noexcept { return !error; }
};

}  // namespace impl

/// Selects `nullable<std::exception_ptr>`: a `compressed_optional` whose empty state is a null
/// `std::exception_ptr`, which already means "no exception", so it adds no storage to the
/// `std::exception_ptr`.
///
/// It is the one standard library type whose selector is declared here rather than beside the
/// type itself. It serves an outcome that is either a cancellation with nothing more to report or a
/// failure with its exception.
template <>
struct optional_selector<std::exception_ptr> {
    /// The type that represents an optional `std::exception_ptr`.
    using nullable = compressed_optional<std::exception_ptr, impl::exception_ptr_sentinel>;
};

/// Selects `nullable<T>` for a signed integer type: a `compressed_optional` whose sentinel is
/// `std::numeric_limits<T>::min()`, the one value whose negation is not representable. It adds
/// no storage to the number.
template <std::signed_integral T>
struct optional_selector<T> {
    /// The type that represents an optional signed integer.
    using nullable = compressed_optional<T, impl::integral_min_sentinel<T>>;
};

/// Selects `nullable<T>` for an unsigned integer type: a `compressed_optional` whose sentinel is
/// `std::numeric_limits<T>::max()`, the largest representable value. It adds no storage to the
/// number.
template <std::unsigned_integral T>
struct optional_selector<T> {
    /// The type that represents an optional unsigned integer.
    using nullable = compressed_optional<T, impl::integral_max_sentinel<T>>;
};

/// Selects `nullable<T>` for a floating-point type: a `compressed_optional` whose sentinel is a
/// quiet NaN. It adds no storage to the number.
///
/// Every NaN reads as empty, not one chosen bit pattern, so a NaN stored as a value reads back
/// as empty. A type for which a NaN is real data needs a selector of its own.
template <std::floating_point T>
struct optional_selector<T> {
    /// The type that represents an optional floating-point number.
    using nullable = compressed_optional<T, impl::quiet_nan_sentinel<T>>;
};

/// Selects `std::optional<bool>` as `nullable<bool>`, for the one type that cannot be
/// compressed.
///
/// A `bool` has two values and uses both, and reading any other bit pattern as `bool` is
/// undefined behavior, not a spare value. `nullable<bool>` is therefore `std::optional<bool>`,
/// two bytes. This explicit specialization takes precedence over the partial specialization for
/// unsigned integer types, which `bool` would otherwise match.
template <>
struct optional_selector<bool> {
    /// The type that represents an optional `bool`.
    using nullable = std::optional<bool>;
};

namespace impl {

// not_null<T> is a wrapper over a pointer and nothing else: standard
// layout, one member, no bases, no virtuals -- so its storage *is* a `T*`,
// at offset zero, and the empty state is that pointer holding null. The class
// keeps its promise untouched; what happens here happens to the bytes.
template <class T>
struct not_null_sentinel {
    // "Empty" here is a null pointer and always was, so it may be written so.
    static constexpr bool empty_is_nullptr = true;

    static not_null<T> sentinel() noexcept {
        T* nothing = nullptr;
        return *std::start_lifetime_as<not_null<T>>(&nothing);
    }

    // A copy of the bytes, not start_lifetime_as: this storage already holds
    // a live not_null, and starting a T* in it would end that one. And
    // not through get() either -- it carries an assume(), which would let the
    // optimiser fold the comparison to false.
    static bool is_sentinel(const not_null<T>& val) noexcept {
        return std::bit_cast<T*>(val) == nullptr;
    }
};

}

/// Selects `nullable<not_null<T>>`: a `compressed_optional` whose empty state is a null pointer
/// in the storage of the `not_null`, so it is the size of a pointer. The empty state may be
/// written as `nullptr`.
template <class T>
struct optional_selector<not_null<T>> {
    /// The type that represents an optional `not_null<T>`.
    using nullable = compressed_optional<not_null<T>, impl::not_null_sentinel<T>>;
};

}  // export namespace wxl::core
