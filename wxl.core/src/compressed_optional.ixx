export module wxl.core:compressed_optional;

import :not_null;
import std;

export namespace wxl::core {

template <typename TSentinel, typename T>
concept sentinel = requires(const T& val) {
    { TSentinel::sentinel() } noexcept -> std::same_as<T>;
    { TSentinel::is_sentinel(val) } noexcept -> std::same_as<bool>;
};

template <typename T, sentinel<T> TSentinel>
class compressed_optional;

template <typename X>
concept is_std_optional =
    requires { []<typename W>(const std::optional<W>&) {}(std::declval<const X&>()); };

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

// Where a type says what its empty state looks like. Specialise it beside the
// type, naming the value that stands for "nothing" -- the largest duration, a
// NaN, an empty exception_ptr, a wrapper over a null Impl.
//
// The default is a pointer to the type: `nullable<Widget>` is one machine word
// holding a Widget* that may be null, and what comes out of it is a
// not_null<Widget>. That is what an ordinary class has to offer -- a class
// cannot spare a value of itself, but the pointer to it can spare its null.
template <typename T>
struct optional_selector {
    using nullable = compressed_optional<T*, impl::raw_pointer_sentinel<T>>;
};

template <typename T>
using nullable = typename optional_selector<T>::nullable;

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

    using value_type = T;
    using sentinel_provider_type = TSentinel;

    // --- КОНСТРУКТОРЫ ---

    constexpr compressed_optional() noexcept : value_(sentinel_provider_type::sentinel()) {}

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

    constexpr compressed_optional(const compressed_optional&) noexcept = default;
    constexpr compressed_optional(compressed_optional&&) noexcept = default;

    template <typename U = T>
        requires(!std::is_same_v<std::remove_cvref_t<U>, compressed_optional>) &&
                (!std::is_same_v<std::remove_cvref_t<U>, std::in_place_t>) &&
                (!std::is_same_v<std::remove_cvref_t<U>, std::nullopt_t>) &&
                (!requires { typename std::remove_cvref_t<U>::sentinel_provider_type; }) &&
                (!is_std_optional<U>) && std::is_constructible_v<T, U>
    constexpr explicit(!std::is_convertible_v<U, T>)
        compressed_optional(U&& value) noexcept(std::is_nothrow_constructible_v<T, U>)
        : value_(std::forward<U>(value)) {}

    // Both arms of the conditional are built as T on purpose. Left as
    // `other.value_ : sentinel()` the two operands are U and T, and the
    // conditional has to find a type they share -- which for a widening from
    // a derived wrapper to its base is not U, not T, and not anything: it
    // fails, though every conversion it needs exists. Saying T twice asks for
    // exactly the conversion the requires-clause above already checked.
    template <typename U, sentinel<U> USentinel>
        requires std::is_constructible_v<T, const U&>
    constexpr explicit(!std::is_convertible_v<const U&, T>)
        compressed_optional(const compressed_optional<U, USentinel>& other) noexcept
        : value_(other.has_value() ? T{other.value_} : sentinel_provider_type::sentinel()) {}

    template <typename U, sentinel<U> USentinel>
        requires std::is_constructible_v<T, U>
    constexpr explicit(!std::is_convertible_v<U, T>)
        compressed_optional(compressed_optional<U, USentinel>&& other) noexcept
        : value_(other.has_value() ? T{std::move(other.value_)}
                                   : sentinel_provider_type::sentinel()) {}

    template <typename U>
        requires std::is_constructible_v<T, const U&>
    constexpr explicit(!std::is_convertible_v<const U&, T>)
        compressed_optional(const std::optional<U>& other) noexcept
        : value_(other.has_value() ? *other : sentinel_provider_type::sentinel()) {}

    template <typename U>
        requires std::is_constructible_v<T, U>
    constexpr explicit(!std::is_convertible_v<U, T>)
        compressed_optional(std::optional<U>&& other) noexcept
        : value_(other.has_value() ? std::move(*other) : sentinel_provider_type::sentinel()) {}

    template <typename... Args>
        requires std::is_constructible_v<T, Args...>
    constexpr explicit compressed_optional(std::in_place_t, Args&&... args) noexcept(
        std::is_nothrow_constructible_v<T, Args...>)
        : value_(std::forward<Args>(args)...) {}

    template <typename U, typename... Args>
        requires std::is_constructible_v<T, std::initializer_list<U>&, Args...>
    constexpr explicit compressed_optional(
        std::in_place_t, std::initializer_list<U> ilist,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<T, std::initializer_list<U>&,
                                                                 Args...>)
        : value_(ilist, std::forward<Args>(args)...) {}

    // --- ОПЕРАТОРЫ ПРИСВАИВАНИЯ ---

    constexpr compressed_optional& operator=(const compressed_optional&) noexcept = default;
    constexpr compressed_optional& operator=(compressed_optional&&) noexcept = default;

    constexpr compressed_optional& operator=(std::nullopt_t) noexcept {
        reset();
        return *this;
    }

    template <typename S = TSentinel>
        requires requires { S::empty_is_nullptr; }
    constexpr compressed_optional& operator=(decltype(nullptr)) noexcept {
        reset();
        return *this;
    }

    template <typename U = T>
        requires(!std::is_same_v<std::remove_cvref_t<U>, compressed_optional>) &&
                (!std::is_same_v<std::remove_cvref_t<U>, std::nullopt_t>) &&
                (!requires { typename std::remove_cvref_t<U>::sentinel_provider_type; }) &&
                (!is_std_optional<U>) && std::is_constructible_v<T, U> &&
                std::is_assignable_v<T&, std::decay_t<U>>
    constexpr compressed_optional& operator=(U&& value) noexcept(
        std::is_nothrow_assignable_v<T&, U>) {
        value_ = std::forward<U>(value);
        return *this;
    }

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

    template <typename U, sentinel<U> USentinel>
        requires std::is_constructible_v<T, U> && std::is_assignable_v<T&, std::decay_t<U>>
    constexpr compressed_optional& operator=(compressed_optional<U, USentinel>&& other) noexcept {
        if (other.has_value()) [[likely]]
            value_ = std::move(other.value_);
        else
            reset();
        return *this;
    }

    template <typename U>
        requires std::is_constructible_v<T, const U&> && std::is_assignable_v<T&, std::decay_t<const U&>>
    constexpr compressed_optional& operator=(const std::optional<U>& other) noexcept {
        if (other.has_value()) [[likely]]
            value_ = *other;
        else
            reset();
        return *this;
    }

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

    constexpr void swap(compressed_optional& other) noexcept(std::is_nothrow_swappable_v<T>) {
        std::swap(value_, other.value_);
    }

    friend constexpr void swap(compressed_optional& lhs,
                               compressed_optional& rhs) noexcept(noexcept(lhs.swap(rhs))) {
        lhs.swap(rhs);
    }

    // --- МЕТОДЫ НАБЛЮДАТЕЛИ (OBSERVERS) ---

    constexpr explicit operator bool() const noexcept { return has_value(); }

    constexpr bool has_value() const noexcept {
        return !sentinel_provider_type::is_sentinel(value_);
    }

    [[nodiscard]] constexpr const_access_type value() const& {
        if (has_value()) [[likely]] {
            if constexpr (is_pointer_type) return not_null<const raw_element_type>(value_);
            else return value_;
        }
        throw std::bad_optional_access{};
    }

    [[nodiscard]] constexpr access_type value() & {
        if (has_value()) [[likely]] {
            if constexpr (is_pointer_type) return not_null<raw_element_type>(value_);
            else return value_;
        }
        throw std::bad_optional_access{};
    }

    [[nodiscard]] constexpr rvalue_access_type value() && {
        if (has_value()) [[likely]] {
            if constexpr (is_pointer_type) return not_null<raw_element_type>(value_);
            else return std::move(value_);
        }
        throw std::bad_optional_access{};
    }

    [[nodiscard]] constexpr const_rvalue_access_type value() const&& {
        if (has_value()) [[likely]] {
            if constexpr (is_pointer_type) return not_null<const raw_element_type>(value_);
            else return std::move(value_);
        }
        throw std::bad_optional_access{};
    }

    template <typename U>
        requires std::is_copy_constructible_v<T> && std::is_convertible_v<U, T>
    constexpr T value_or(U&& default_value) const& noexcept {
        if (has_value()) return value_;
        return static_cast<T>(std::forward<U>(default_value));
    }

    template <typename U>
        requires std::is_move_constructible_v<T> && std::is_convertible_v<U, T>
    constexpr T value_or(U&& default_value) && noexcept {
        if (has_value()) return std::move(value_);
        return static_cast<T>(std::forward<U>(default_value));
    }

    // --- МОНАДИЧЕСКИЙ ИНТЕРФЕЙС (AND_THEN) ---

    template <typename F>
    constexpr auto and_then(F&& f) & -> std::invoke_result_t<F, T&> {
        using ret_t = std::invoke_result_t<F, T&>;
        if (has_value()) return std::invoke(std::forward<F>(f), value_);
        return ret_t{};
    }

    template <typename F>
    constexpr auto and_then(F&& f) const& -> std::invoke_result_t<F, const T&> {
        using ret_t = std::invoke_result_t<F, const T&>;
        if (has_value()) return std::invoke(std::forward<F>(f), value_);
        return ret_t{};
    }

    template <typename F>
    constexpr auto and_then(F&& f) && -> std::invoke_result_t<F, T&&> {
        using ret_t = std::invoke_result_t<F, T&&>;
        if (has_value()) return std::invoke(std::forward<F>(f), std::move(value_));
        return ret_t{};
    }

    template <typename F>
    constexpr auto and_then(F&& f) const&& -> std::invoke_result_t<F, const T&&> {
        using ret_t = std::invoke_result_t<F, const T&&>;
        if (has_value()) return std::invoke(std::forward<F>(f), std::move(value_));
        return ret_t{};
    }

    // --- МОНАДИЧЕСКИЙ ИНТЕРФЕЙС (TRANSFORM) ---

    template <typename F>
    constexpr auto transform(F&& f) & -> nullable<std::invoke_result_t<F, T&>> {
        using ret_t = nullable<std::invoke_result_t<F, T&>>;
        if (has_value()) [[likely]]
            return ret_t(std::invoke(std::forward<F>(f), value_));  // Исправлено: std::forward<F>
        return ret_t{};
    }

    template <typename F>
    constexpr auto transform(F&& f) const& -> nullable<std::invoke_result_t<F, const T&>> {
        using ret_t = nullable<std::invoke_result_t<F, const T&>>;
        if (has_value()) [[likely]]
            return ret_t(std::invoke(std::forward<F>(f), value_));  // Исправлено: std::forward<F>
        return ret_t{};
    }

    template <typename F>
    constexpr auto transform(F&& f) && -> nullable<std::invoke_result_t<F, T&&>> {
        using ret_t = nullable<std::invoke_result_t<F, T&&>>;
        if (has_value()) [[likely]]
            return ret_t(
                std::invoke(std::forward<F>(f), std::move(value_)));  // Исправлено: std::forward<F>
        return ret_t{};
    }

    template <typename F>
    constexpr auto transform(F&& f) const&& -> nullable<std::invoke_result_t<F, const T&&>> {
        using ret_t = nullable<std::invoke_result_t<F, const T&&>>;
        if (has_value()) [[likely]]
            return ret_t(
                std::invoke(std::forward<F>(f), std::move(value_)));  // Исправлено: std::forward<F>
        return ret_t{};
    }

    // --- МОНАДИЧЕСКИЙ ИНТЕРФЕЙС (OR_ELSE) ---

    template <typename F>
        requires std::is_copy_constructible_v<T>
    constexpr auto or_else(F&& f) const& -> compressed_optional {
        if (has_value()) return *this;
        return std::forward<F>(f)();
    }

    template <typename F>
        requires std::is_move_constructible_v<T>
    constexpr auto or_else(F&& f) && -> compressed_optional {
        if (has_value()) return std::move(*this);
        return std::forward<F>(f)();
    }

    constexpr void reset() noexcept { value_ = sentinel_provider_type::sentinel(); }

    // --- ОПЕРАТОРЫ ДОСТУПА ПО УКАЗАТЕЛЮ ---

    constexpr pointer_access_type operator->() noexcept {
        if constexpr (is_pointer_type) return not_null<raw_element_type>(value_);
        else return &value_;
    }

    constexpr const_pointer_access_type operator->() const noexcept {
        if constexpr (is_pointer_type) return not_null<const raw_element_type>(value_);
        else return &value_;
    }

    constexpr access_type operator*() & noexcept {
        if constexpr (is_pointer_type) return not_null<raw_element_type>(value_);
        else return value_;
    }

    constexpr const_access_type operator*() const& noexcept {
        if constexpr (is_pointer_type) return not_null<const raw_element_type>(value_);
        else return value_;
    }

    constexpr rvalue_access_type operator*() && noexcept {
        if constexpr (is_pointer_type) return not_null<raw_element_type>(value_);
        else return std::move(value_);
    }

    constexpr const_rvalue_access_type operator*() const&& noexcept {
        if constexpr (is_pointer_type) return not_null<const raw_element_type>(value_);
        else return std::move(value_);
    }

    // --- СРАВНЕНИЯ (МЕЖДУ КОНТЕЙНЕРАМИ) ---

    friend constexpr bool operator==(const compressed_optional& lhs,
                                     const compressed_optional& rhs) noexcept {
        const bool lhs_has = lhs.has_value();
        if (lhs_has != rhs.has_value()) [[unlikely]]
            return false;
        if (!lhs_has) [[unlikely]]
            return true;
        return lhs.value_ == rhs.value_;
    }

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

    friend constexpr bool operator==(const compressed_optional& opt, std::nullopt_t) noexcept {
        return !opt.has_value();
    }

    friend constexpr auto operator<=>(const compressed_optional& opt, std::nullopt_t) noexcept
        -> std::partial_ordering {
        return opt.has_value() ? std::partial_ordering::greater : std::partial_ordering::equivalent;
    }

    //  --- СРАВНЕНИЯ С ТИПОМ T ---

    friend constexpr bool operator==(const compressed_optional& lhs, const T& rhs) noexcept {
        return lhs.has_value() && (lhs.value_ == rhs);
    }

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
    static std::exception_ptr sentinel() noexcept { return {}; }

    static bool is_sentinel(const std::exception_ptr& error) noexcept { return !error; }
};

}  // namespace impl

// The one standard type specialised here rather than beside itself, for want
// of anywhere else to put it. It earns the specialisation: an outcome that may
// be "cancelled, and that is all there is to say" or "this went wrong, and
// here is what" is exactly nullable<exception_ptr>, and it costs a pointer.
template <>
struct optional_selector<std::exception_ptr> {
    using nullable = compressed_optional<std::exception_ptr, impl::exception_ptr_sentinel>;
};

// Every number at once, on the values described above: the smallest for a
// signed integer, the largest for an unsigned one, a NaN for a float. Between
// them they cover most of what is ever optional, and none of the three costs
// a byte over the bare number.
template <std::signed_integral T>
struct optional_selector<T> {
    using nullable = compressed_optional<T, impl::integral_min_sentinel<T>>;
};

template <std::unsigned_integral T>
struct optional_selector<T> {
    using nullable = compressed_optional<T, impl::integral_max_sentinel<T>>;
};

template <std::floating_point T>
struct optional_selector<T> {
    using nullable = compressed_optional<T, impl::quiet_nan_sentinel<T>>;
};

// And the one type that cannot be compressed, spelled out here rather than
// left to fail: a bool has two values and means both of them, and a third bit
// pattern read back as bool is undefined behaviour, not a spare value. So
// nullable<bool> is std::optional<bool> -- two bytes, and no pretending
// otherwise. It beats the partial specialisation for unsigned integers above,
// which bool would otherwise match.
//
// The selector exists all the same, and writing it is the point. Code says
// nullable<T> everywhere, for every T, so that a type nobody has thought about
// still fails to compile for want of a selector. std::optional appears once,
// here, where the reason for it can be read.
template <>
struct optional_selector<bool> {
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

template <class T>
struct optional_selector<not_null<T>> {
    using nullable = compressed_optional<not_null<T>, impl::not_null_sentinel<T>>;
};

}  // export namespace wxl::core
