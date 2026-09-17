// Дерево, которое получается из разбора: значения, их имена и то немногое,
// что у них можно спросить. Всё здесь -- вид в документ, которым владеет
// читатель, и потому живёт ровно столько же, сколько он.
//
// Вид проверенный. Документ проверяется целиком до того, как по нему пойдёт
// грамматика, так что всякая строка, вырезанная из него, -- заведомо
// правильный UTF-8, и wxl::core::u8_view говорит это вызывающему, которому
// иначе пришлось бы писать assume_valid() снова и снова. Сравнение с
// обычным литералом от этого не меняется.
//
// Один тип на все шесть видов значения. Разделять их классами было бы не за
// что: у значения одно поле нагрузки и тег, наследование добавило бы vptr
// ради экономии восьми байт, а вызывающему -- приведение типа на каждом
// шаге.
//
// Поле объекта -- такое же значение, только с именем. Отдельного типа
// «пара имя-значение» нет нарочно: массив и объект тогда хранились бы
// по-разному, разбору понадобились бы два стека вместо одного, а обходу --
// две записи вместо одной. Цена -- пустое имя у элемента массива, то есть
// два слова на элемент; выгода -- вдвое меньше кода в разборе.

export module wxl.json:value;

import std;
import wxl.core;

export namespace wxl::json {

/// Чем является значение. Шесть видов JSON, и седьмого не бывает.
enum class value_type : std::uint8_t {
    null,     ///< `null`
    boolean,  ///< `true`, `false`
    number,   ///< `-1`, `0.5`, `1e9`
    string,   ///< `"текст"`
    array,    ///< `[…]`
    object,   ///< `{…}`
};

/// Значение документа: что оно такое, как его зовут (если оно поле
/// объекта) и что в нём лежит.
class value {
public:
    /// Значение по умолчанию -- `null` без имени. Такое же, каким отвечает
    /// поиск поля, которого нет.
    constexpr value() noexcept = default;

    /// @name Из чего значение делается
    /// Публично, потому что дерево -- обычные данные: собрать такое же
    /// руками должен уметь и тест, и тот, кто отдаёт готовый ответ вместо
    /// запроса к сети.
    /// @{
    static constexpr value of_null(core::u8_view name = {}) noexcept {
        return value(value_type::null, name, payload_t());
    }
    static constexpr value of_boolean(bool flag, core::u8_view name = {}) noexcept {
        return value(value_type::boolean, name, payload_t(flag));
    }
    static constexpr value of_number(double number, core::u8_view name = {}) noexcept {
        return value(value_type::number, name, payload_t(number));
    }
    static constexpr value of_string(core::u8_view text, core::u8_view name = {}) noexcept {
        return value(value_type::string, name, payload_t(text));
    }

    /// Массив или объект над готовым куском памяти: значения лежат подряд,
    /// и владеет ими тот, кто их туда положил, -- при разборе это арена
    /// документа.
    static constexpr value of_items(value_type kind, std::span<const value> items,
                                    core::u8_view name = {}) noexcept {
        return value(kind, name,
                     payload_t(run{items.data(), static_cast<std::uint32_t>(items.size())}));
    }
    /// @}

    constexpr value_type type() const noexcept { return type_; }

    /// Имя, под которым значение лежит в объекте. Пусто у элемента массива
    /// и у корня документа.
    constexpr core::u8_view name() const noexcept { return name_; }

    /// @name Что это
    /// @{
    constexpr bool is_null() const noexcept { return type_ == value_type::null; }
    constexpr bool is_boolean() const noexcept { return type_ == value_type::boolean; }
    constexpr bool is_number() const noexcept { return type_ == value_type::number; }
    constexpr bool is_string() const noexcept { return type_ == value_type::string; }
    constexpr bool is_array() const noexcept { return type_ == value_type::array; }
    constexpr bool is_object() const noexcept { return type_ == value_type::object; }
    /// @}

    /// @name Что внутри
    ///
    /// Каждая отвечает подставленным значением, когда значение другого
    /// вида, и это не снисходительность к документу, а признание того, как
    /// устроен обмен: поле, которого сервер не прислал, и поле, которое он
    /// прислал пустым, -- обычное дело, и писать проверку типа над каждым
    /// из двух десятков полей DTO значит писать её двадцать раз зря. Кому
    /// разница важна, тот спрашивает type() или find().
    /// @{
    constexpr bool as_bool(bool fallback = false) const noexcept {
        return type_ == value_type::boolean ? payload_.boolean : fallback;
    }
    constexpr double as_number(double fallback = 0.0) const noexcept {
        return type_ == value_type::number ? payload_.number : fallback;
    }

    /// Число, приведённое к целому. Ровно до 2^53 -- дальше double считает
    /// с шагом больше единицы; в обмене, где целые это идентификаторы,
    /// счётчики и смещения, столько не бывает.
    constexpr std::int64_t as_int(std::int64_t fallback = 0) const noexcept {
        return type_ == value_type::number ? static_cast<std::int64_t>(payload_.number) : fallback;
    }

    constexpr core::u8_view as_string(core::u8_view fallback = {}) const noexcept {
        return type_ == value_type::string ? payload_.text : fallback;
    }
    /// @}

    /// Элементы массива, по порядку. Пусто у всего остального.
    constexpr std::span<const value> elements() const noexcept {
        return type_ == value_type::array ? items() : std::span<const value>();
    }

    /// Поля объекта, в порядке документа. Пусто у всего остального.
    constexpr std::span<const value> members() const noexcept {
        return type_ == value_type::object ? items() : std::span<const value>();
    }

    /// Сколько внутри -- элементов у массива, полей у объекта, ноль у
    /// остальных.
    constexpr std::size_t size() const noexcept {
        return type_ == value_type::array || type_ == value_type::object ? payload_.items.count : 0;
    }

    /// Поле объекта по имени, или nullptr, когда такого поля нет. Это
    /// дверь для того, кому важно отличить «поля не было» от «поле было
    /// null»: operator[] эту разницу нарочно стирает.
    ///
    /// Перебором, а не поиском: у объекта обмена полей десяток-другой, и
    /// на таком счёте таблица проигрывает подряд лежащим именам.
    /// Одноимённых полей JSON не запрещает; отвечает первое, как читают
    /// все.
    inline const value* find(std::string_view field) const noexcept {
        for (const value& member : members())
            if (member.name_ == field) return &member;

        return nullptr;
    }

    /// Поле объекта по имени. Поля нет -- ответом `null`, и потому цепочка
    /// вида `root["author"]["displayName"].as_string()` дописывается до
    /// конца, не спрашивая по дороге, был ли автор.
    const value& operator[](std::string_view field) const noexcept;

    /// Элемент массива или поле объекта по номеру. За краем -- `null`.
    const value& operator[](std::size_t index) const noexcept;

private:
    /// Кусок памяти, где лежат элементы. Не std::span: span требует
    /// полного типа элемента, а он тут ещё не полон -- определяется прямо
    /// сейчас.
    struct run {
        const value* first = nullptr;
        std::uint32_t count = 0;
    };

    /// Нагрузка: у каждого вида своя, и больше одной у значения не бывает.
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

/// Значение, которым отвечает поиск, когда искомого нет. Это настоящий
/// `null`, а не особая метка: документ, где поле выписано как `null`, и
/// документ, где его нет вовсе, для читающего DTO -- одно и то же, а кому
/// не одно и то же, у того есть find().
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
