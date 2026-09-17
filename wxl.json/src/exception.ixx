// То, чем читатель отвечает на негодный документ, и больше ничего. Своя
// партиция потому, что тому, кто только ловит -- обёртке, тесту, -- нужны
// эти два типа и не нужны ни дерево, ни разбор.

export module wxl.json:exception;

import std;

export namespace wxl::json {

/// Корень всего, что бросает этот читатель: файл, который не читается,
/// байты, которые не UTF-8, документ, который не разбирается. Один тип на
/// всё, чтобы `catch (const wxl::json::exception&)` ловил любую неудачу
/// этой библиотеки и ничью больше.
///
/// Сообщение -- UTF-8: его берёт std::exception, им же отвечает всё
/// остальное здесь.
class exception : public std::runtime_error {
public:
    inline explicit exception(std::string_view message)
        : std::runtime_error(std::string(message)) {}
};

/// Документ не разобрался; несёт место, на котором разбор остановился.
class parsing_exception : public exception {
public:
    parsing_exception(std::string_view file_name, int line, int column, std::string_view reason,
                      const std::source_location& rule);

    /// Документ и место в нём, куда смотреть: счёт с единицы, колонка в
    /// байтах.
    ///
    /// Там, где правило отказывается от начатого -- строка без закрывающей
    /// кавычки, объект без закрывающей скобки, -- местом названо начало, а
    /// не байт, на котором разбор сдался: искать надо там.
    /// @{
    inline std::string_view file_name() const noexcept { return file_name_; }
    inline int line() const noexcept { return line_; }
    inline int column() const noexcept { return column_; }
    /// @}

    /// Чем документ не угодил, одной строкой и по-русски: «ожидалась
    /// запятая или »}«». Место говорит, куда смотреть, а это -- что там
    /// не так.
    inline std::string_view reason() const noexcept { return reason_; }

    /// Где в исходниках самого читателя документ был отвергнут, то есть
    /// какое правило грамматики на нём сдалось. Это для отладки читателя, а
    /// не документа, -- потому и рядом с местом, а не вместо него: вопросы
    /// разные.
    inline const std::source_location& rule() const noexcept { return rule_; }

private:
    // Копия, а не вид: исключение вполне переживает разбор, который его
    // бросил, и имя файла обязано пережить вместе с ним. Строка обычная, не
    // пуловая: заполняется один раз, а объекту, уходящему так далеко из
    // читателя, лучше не быть должным STA-пулу ничего.
    std::string file_name_;
    std::string reason_;

    std::source_location rule_;

    int line_;
    int column_;
};

}  // namespace wxl::json
