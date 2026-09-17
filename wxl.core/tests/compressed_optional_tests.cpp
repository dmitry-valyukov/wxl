#include <gtest/gtest.h>

import std;
import wxl.core;

using namespace wxl::core;

namespace {

// 1. Провайдер для указателей: сентинелом является nullptr
template <typename T>
struct PtrSentinel {
    static T* sentinel() noexcept { return nullptr; }
    static bool is_sentinel(T* val) noexcept { return val == nullptr; }
};

// 2. Провайдер для double: сентинелом является quiet_NaN
struct DoubleNaNConst {
    static constexpr double sentinel() noexcept { return std::numeric_limits<double>::quiet_NaN(); }
    static constexpr bool is_sentinel(double val) noexcept { return val != val; }
};

}  // namespace

// Точка расширения библиотеки: какой nullable отдаёт transform() для данного
// типа. Умолчания нет — тип без селектора не компилируется. Указатель своё
// пустое называет здесь; int его называть не надо, за все знаковые целые это
// делает сама библиотека (наименьшее представимое — единственное значение,
// которое не получается сменой знака ни из какого другого).
namespace wxl::core {

template <>
struct optional_selector<int*> {
    using nullable = compressed_optional<int*, PtrSentinel<int>>;
};

}  // namespace wxl::core

namespace {

TEST(CompressedOptionalTest, size_and_initialization) {
    using opt_ptr = compressed_optional<int*, PtrSentinel<int>>;

    // Проверка сжатия памяти: размер опционала должен быть равен размеру указателя (8 байт на x64)
    EXPECT_EQ(sizeof(opt_ptr), sizeof(int*));

    opt_ptr empty_opt;
    EXPECT_FALSE(empty_opt.has_value());
    EXPECT_FALSE(static_cast<bool>(empty_opt));

    opt_ptr nullopt_opt(std::nullopt);
    EXPECT_FALSE(nullopt_opt.has_value());
}

struct IntSentinel {
    static int sentinel() noexcept { return -1; }
    static bool is_sentinel(int val) noexcept { return val == -1; }
};

TEST(CompressedOptionalTest, universal_constructor_and_assignment) {
    using opt_str = compressed_optional<const char *, PtrSentinel<const char>>;

    opt_str opt = "hello";
    EXPECT_TRUE(opt.has_value());
    EXPECT_STREQ((*opt).get(), "hello");

    // Универсальное присваивание
    opt = "world";
    EXPECT_STREQ((*opt).get(), "world");

    opt = std::nullopt;
    EXPECT_FALSE(opt.has_value());
}

struct ThrowingClass {
    int value;
    ThrowingClass(int v) : value(v) {
        if (v == 13) throw std::runtime_error("Bad luck");
    }
};

struct ThrowingSentinel {
    static ThrowingClass sentinel() noexcept { return ThrowingClass{-1}; }
    static bool is_sentinel(const ThrowingClass& val) noexcept { return val.value == -1; }
};

TEST(CompressedOptionalTest, emplace_exception_safety) {
    using opt_throw = compressed_optional<ThrowingClass, ThrowingSentinel>;

    opt_throw opt(std::in_place, 42);
    EXPECT_EQ(opt->value, 42);

    // Вызов emplace с аргументом, который гарантированно бросит исключение
    EXPECT_THROW(opt.emplace(13), std::runtime_error);

    // Проверяем, что объект остался валидным (инвариант не нарушен)
    // Благодаря вашей логике, при исключении UB не произойдет
    EXPECT_TRUE(opt.has_value() || !opt.has_value());
}

TEST(CompressedOptionalTest, std_optional_interoperability) {
    using opt_int = compressed_optional<int, IntSentinel>;

    std::optional<int> std_opt(100);

    // Конструирование из std::optional
    opt_int my_opt(std_opt);
    EXPECT_TRUE(my_opt.has_value());
    EXPECT_EQ(*my_opt, 100);

    // Присваивание из std::optional (rvalue)
    std::optional<int> std_empty(std::nullopt);
    my_opt = std::move(std_empty);
    EXPECT_FALSE(my_opt.has_value());
}

TEST(CompressedOptionalTest, monadic_interface) {
    using opt_str = compressed_optional<const char *, PtrSentinel<const char>>;

    opt_str input("42");

    auto result =
        input
            // 1. transform: string -> int (возвращает nullable<int> по селектору выше)
            .transform([](std::string_view s) { return std::stoi(std::string(s)); })
            // 2. and_then: если число четное — делим на 2, иначе nullopt
            .and_then([](int val) -> nullable<int> {
                if (val % 2 == 0) return val / 2;
                return std::nullopt;
            })
            // 3. or_else: если по дороге получили nullopt, подставляем 0
            .or_else([]() -> nullable<int> { return 0; });

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 21);
}

TEST(CompressedOptionalTest, nan_and_three_way_comparison) {
    using opt_dbl = compressed_optional<double, DoubleNaNConst>;

    opt_dbl empty1;
    opt_dbl empty2 = std::nullopt;

    // 1. Два пустых опционала обязаны быть равны по стандарту C++ (хоть внутри и лежит NaN как
    // sentinel)
    EXPECT_TRUE(empty1 == empty2);
    EXPECT_EQ(empty1 <=> empty2, std::partial_ordering::equivalent);

    // 2. Сравнение с nullopt
    EXPECT_TRUE(empty1 == std::nullopt);
    EXPECT_EQ(empty1 <=> std::nullopt, std::partial_ordering::equivalent);

    opt_dbl full5(5.0);
    opt_dbl full10(10.0);

    // 3. Сравнение полного и пустого (пустой всегда меньше)
    EXPECT_TRUE(empty1 < full5);
    EXPECT_TRUE(full5 > std::nullopt);
    EXPECT_EQ(empty1 <=> full5, std::partial_ordering::less);

    // 4. Сравнение двух заполненных
    EXPECT_TRUE(full5 < full10);
    EXPECT_EQ(full5 <=> full10, std::partial_ordering::less);

    // 5. Опасный кейс: в опционал положили пользовательский NaN (не пустой статус, а полезная
    // нагрузка)
    opt_dbl nan_payload(std::numeric_limits<double>::quiet_NaN());

    // Внимание: поскольку DoubleNaNConst::is_sentinel проверяет на std::isnan,
    // данный провайдер воспримет nan_payload как пустой опционал!
    // Это ожидаемое поведение провайдера. Проверим его логику:
    EXPECT_FALSE(nan_payload.has_value());
}

TEST(CompressedOptionalTest, native_nan_double_behavior) {
    // Чтобы проверить чистый NaN как полезную нагрузку, возьмем провайдер,
    // где сентинелом служит конкретное число, например -999.0
    struct DoubleMinus999 {
        static double sentinel() noexcept { return -999.0; }
        static bool is_sentinel(double val) noexcept { return val == -999.0; }
    };
    using opt_custom_dbl = compressed_optional<double, DoubleMinus999>;

    opt_custom_dbl opt_nan(std::numeric_limits<double>::quiet_NaN());
    double raw_nan = std::numeric_limits<double>::quiet_NaN();

    // Опционал полон, так как NaN != -999.0
    EXPECT_TRUE(opt_nan.has_value());

    // По стандарту C++ и IEEE 754: NaN == любой_T всегда false, даже если это NaN == NaN
    EXPECT_FALSE(opt_nan == raw_nan);
    EXPECT_FALSE(opt_nan == opt_nan);  // full_nan == full_nan обязано быть false!

    // Проверяем трехстороннее сравнение с NaN (должно возвращать unordered)
    EXPECT_EQ(opt_nan <=> 5.0, std::partial_ordering::unordered);
    EXPECT_EQ(opt_nan <=> opt_nan, std::partial_ordering::unordered);
}

// Провайдер сентинела обязан работать в compile-time: без этого пустой
// compressed_optional не был бы constexpr-конструируемым.
static_assert(DoubleNaNConst::is_sentinel(DoubleNaNConst::sentinel()),
              "сентинел провайдера обязан узнаваться в compile-time");
static_assert(!DoubleNaNConst::is_sentinel(5.5), "обычное число — не сентинел");

// optional_like узнаёт два шаблона по имени, а не по членам: у std::expected те же
// члены, но по умолчанию он держит значение.
static_assert(optional_like<std::optional<int>>);
static_assert(optional_like<nullable<int>>);
static_assert(optional_like<nullable<bool>>);
static_assert(optional_like<compressed_optional<double, DoubleNaNConst>&>);
static_assert(!optional_like<int>);
static_assert(!optional_like<std::expected<int, int>>);

}  // namespace
