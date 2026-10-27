// The tree a parse produces: values, their names and the few queries they
// answer. Everything here is a view into the document the reader owns, and so
// lives exactly as long as the document does.
//
// The views are checked text: the document is validated whole before parsing,
// so every string taken from it is well-formed UTF-8, and wxl::core::u8_view
// states that without the caller writing assume_valid(). Comparison with an
// ordinary literal is unaffected.
//
// One type for all six kinds of value. Separate classes would gain nothing: a
// value has one payload field and a tag; inheritance would add a vptr to every
// value, and a cast at every step for the caller.
//
// An object member is a value like any other, with a name. There is no
// separate name-value pair type: with one, arrays and objects would be stored
// differently, the parser would need two stacks instead of one and a traversal
// two code paths. The cost is an empty name on each array element, two words
// per element.

export module wxl.json:value;

import std;
import wxl.core;

export namespace wxl::json {

/// Specifies the type of a value: one of the six types JSON defines.
enum class value_type : std::uint8_t {
    null,     ///< `null`
    boolean,  ///< `true`, `false`
    number,   ///< `-1`, `0.5`, `1e9`
    string,   ///< `"text"`
    array,    ///< `[…]`
    object,   ///< `{…}`
};

/// Represents a value of a document: its type, its name (when it is an object
/// member) and its content.
class value {
public:
    /// Creates a `null` value without a name, the same value that a lookup of a
    /// missing member returns.
    constexpr value() noexcept = default;

    /// @name Construction
    /// Public because the tree is plain data: a test, or code that supplies a prepared
    /// response instead of a network request, has to be able to build one by hand.
    /// @{

    /// Creates a `null` value with the name `name`.
    static constexpr value of_null(core::u8_view name = {}) noexcept {
        return value(value_type::null, name, payload_t());
    }
    /// Creates a Boolean value `flag` with the name `name`.
    static constexpr value of_boolean(bool flag, core::u8_view name = {}) noexcept {
        return value(value_type::boolean, name, payload_t(flag));
    }
    /// Creates a number value `number` with the name `name`.
    static constexpr value of_number(double number, core::u8_view name = {}) noexcept {
        return value(value_type::number, name, payload_t(number));
    }
    /// Creates a string value `text` with the name `name`. The text is not copied: the
    /// value holds a view into it.
    static constexpr value of_string(core::u8_view text, core::u8_view name = {}) noexcept {
        return value(value_type::string, name, payload_t(text));
    }

    /// Creates an array or an object over existing contiguous storage. The values are
    /// owned by whoever placed them there; for a parsed document, that is the
    /// document's arena.
    static constexpr value of_items(value_type kind, std::span<const value> items,
                                    core::u8_view name = {}) noexcept {
        return value(kind, name,
                     payload_t(run{items.data(), static_cast<std::uint32_t>(items.size())}));
    }
    /// @}

    /// Returns the type of the value.
    constexpr value_type type() const noexcept { return type_; }

    /// Returns the name of the value as an object member; empty for an array element
    /// and for the document root.
    constexpr core::u8_view name() const noexcept { return name_; }

    /// @name Type tests
    /// @{

    /// Determines whether the value is `null`.
    constexpr bool is_null() const noexcept { return type_ == value_type::null; }
    /// Determines whether the value is a Boolean: `true` or `false`.
    constexpr bool is_boolean() const noexcept { return type_ == value_type::boolean; }
    /// Determines whether the value is a number.
    constexpr bool is_number() const noexcept { return type_ == value_type::number; }
    /// Determines whether the value is a string.
    constexpr bool is_string() const noexcept { return type_ == value_type::string; }
    /// Determines whether the value is an array.
    constexpr bool is_array() const noexcept { return type_ == value_type::array; }
    /// Determines whether the value is an object.
    constexpr bool is_object() const noexcept { return type_ == value_type::object; }
    /// @}

    /// @name Content
    ///
    /// Each accessor returns the fallback when the value is of another type. A missing
    /// or empty member is common in exchanged data, and a type check on each field of a
    /// DTO would be redundant. Code that needs the distinction calls `type()` or `find()`.
    /// @{

    /// Returns the Boolean value, or `fallback` if the value is not a Boolean.
    constexpr bool as_bool(bool fallback = false) const noexcept {
        return type_ == value_type::boolean ? payload_.boolean : fallback;
    }
    /// Returns the number, or `fallback` if the value is not a number.
    constexpr double as_number(double fallback = 0.0) const noexcept {
        return type_ == value_type::number ? payload_.number : fallback;
    }

    /// Returns the number converted to an integer. The result is exact up to 2^53,
    /// beyond which adjacent doubles are more than one apart.
    constexpr std::int64_t as_int(std::int64_t fallback = 0) const noexcept {
        return type_ == value_type::number ? static_cast<std::int64_t>(payload_.number) : fallback;
    }

    /// Returns the text of the string, or `fallback` if the value is not a string.
    constexpr core::u8_view as_string(core::u8_view fallback = {}) const noexcept {
        return type_ == value_type::string ? payload_.text : fallback;
    }
    /// @}

    /// Returns the elements of an array, in order; empty for any other type.
    constexpr std::span<const value> elements() const noexcept {
        return type_ == value_type::array ? items() : std::span<const value>();
    }

    /// Returns the members of an object, in document order; empty for any other type.
    constexpr std::span<const value> members() const noexcept {
        return type_ == value_type::object ? items() : std::span<const value>();
    }

    /// Returns the number of elements of an array or members of an object; zero for
    /// other types.
    constexpr std::size_t size() const noexcept {
        return type_ == value_type::array || type_ == value_type::object ? payload_.items.count : 0;
    }

    /// Returns the object member named `field`, or `nullptr` if there is none. It
    /// distinguishes a missing member from a member that is `null`, a difference that
    /// `operator[]` deliberately hides.
    ///
    /// The search is linear over the contiguous members; no lookup table is built. JSON
    /// does not forbid duplicate names; the first one is returned.
    inline const value* find(std::string_view field) const noexcept {
        for (const value& member : members())
            if (member.name_ == field) return &member;

        return nullptr;
    }

    /// Returns the object member named `field`, or a `null` value if there is none, so a
    /// chain such as `root["author"]["displayName"].as_string()` can be written in full
    /// without checking each step.
    const value& operator[](std::string_view field) const noexcept;

    /// Returns the array element or object member at `index`; `null` past the end.
    const value& operator[](std::size_t index) const noexcept;

private:
    /// The contiguous storage of the elements. Not `std::span`: it requires a complete
    /// element type, and `value` is still being defined here.
    struct run {
        const value* first = nullptr;
        std::uint32_t count = 0;
    };

    /// The payload: each type has its own, and a value holds only one.
    union payload_t {
        constexpr payload_t() noexcept : number(0.0) {}
        constexpr explicit payload_t(bool flag) noexcept : boolean(flag) {}
        constexpr explicit payload_t(double taken) noexcept : number(taken) {}
        constexpr explicit payload_t(core::u8_view taken) noexcept : text(taken) {}
        constexpr explicit payload_t(run taken) noexcept : items(taken) {}

        bool boolean;
        double number;
        core::u8_view text;
        run items;
    };

    constexpr value(value_type type, core::u8_view name, payload_t payload) noexcept
        : name_(name), payload_(payload), type_(type) {}

    constexpr std::span<const value> items() const noexcept {
        return {payload_.items.first, payload_.items.count};
    }

    core::u8_view name_;
    payload_t payload_;
    value_type type_ = value_type::null;
};

/// Represents the value a lookup returns when nothing is found. It is an ordinary
/// `null`, not a special marker: to code that reads a DTO, a member written as `null`
/// and a missing member are the same, and `find()` tells them apart.
inline constexpr value absent;

inline const value& value::operator[](const std::string_view field) const noexcept {
    const value* const found = find(field);

    return found ? *found : absent;
}

inline const value& value::operator[](const std::size_t index) const noexcept {
    const std::span<const value> inside = items();

    return (type_ == value_type::array || type_ == value_type::object) && index < inside.size()
               ? inside[index]
               : absent;
}

}  // namespace wxl::json
