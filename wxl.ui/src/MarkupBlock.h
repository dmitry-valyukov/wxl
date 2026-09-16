#pragma once

// wxl::MarkupBlock -- общий этаж контролов разметки между FormattedBlock и
// парсерами: HtmlBlock, BbBlock и RsdnBlock стоят на нём. Здесь живёт то,
// что у них одинаково и не зависит от записи тегов: тема вида (HtmlTheme),
// базовый каталог картинок, реестр именованных стилей и — внутри, в
// markup_build.cpp, — сборщик, который обходит дерево семейства wxl.html и
// говорит его содержимое примитивами FormattedBlock.
//
// Своих дверей разбора у MarkupBlock нет: какую разметку контрол читает --
// дело наследника.

#include <optional>
#include <string>
#include <string_view>

#include "FormattedBlock.h"

namespace wxl {

// Именованный стиль: строчные грани плюс блочные. Блочный тег носит обе,
// строчный — только текстовую. Регистрирует их HtmlBlock (style="имя" —
// дверь HTML-разметки), а хранятся они здесь: читает реестр общий сборщик.
struct HtmlStyle {
    TextStyle text;
    BlockStyle block;
};

// Вид тегов — то, чего разметка сказать не может, а тема читателя должна:
// отступы, цвет цитаты, моноширинная гарнитура, масштабы заголовков.
// Умолчания — вид первой очереди как есть; заданная theme() тема действует
// на всё, что строится после. Масштабы — множители кегля самого блока,
// поэтому смену кегля тема переживает нетронутой. Имя — по первому и
// главному потребителю, как у HtmlError: BB и RSDN носят ту же тему.
struct HtmlTheme {
    Thickness paragraphMargin{0, 0, 0, 8};   // <p>
    Thickness headingMargin{0, 12, 0, 6};    // <h1>..<h3>
    Thickness quoteMargin{24, 4, 0, 4};      // <blockquote>
    Thickness preMargin{0, 8, 0, 8};         // <pre>
    double headingScale[3]{2.0, 1.5, 1.17};  // <h1>, <h2>, <h3>

    // Цвет текста цитаты по уровню вложенности: [0] — верхняя цитата, [1] —
    // цитата внутри цитаты, [2] и глубже — последний заданный. Незаданный
    // уровень берёт ближайший заданный над собой, поэтому одноцветной теме
    // хватает одного `.quoteColor = {серый}`.
    //
    // Уровни, а не один цвет, потому что так устроены форумы: в переписке на
    // десять ответов цитата цитаты — обычное дело, и одним цветом видно
    // только то, что это цитата, а не чья. RSDN красит три уровня зелёным от
    // тёмного к светлому; так же поступает его клиент jana, и так же может
    // поступить всякий, кому это нужно.
    //
    // Всё пусто — ресурс темы TextFillColorSecondaryBrush, разрешаемый при
    // постройке: тёмная и светлая тема получают каждая свой серый.
    std::optional<Color> quoteColor[3];

    // <code> и <pre>.
    sta_wstring monospace{L"Consolas"};

    // Подложка под <pre>: карточка, как wxl::Card, но в масштабе абзаца —
    // скругление мельче, подъём меньше, фон на ступень серее. Код на
    // странице должен читаться подложенным, а не приподнятым листом, и без
    // подложки он на ней просто плавает.
    //
    // Пустые цвета — ресурсы темы окна (CardBackgroundFillColorSecondary и
    // CardStrokeColorDefault), свои у светлой и у тёмной: на светлой это и
    // даёт тот самый лёгкий серый.
    Thickness prePadding{10, 6, 10, 6};
    double preRadius = 4;
    double preElevation = 16;
    std::optional<Color> preBackground;
    std::optional<Color> preBorderColor;

    double listIndent = 24;  // на уровень вложенности <ul>/<ol>

    // <sub>/<sup>: размер против окружающего текста и сдвиг базовой линии в
    // долях этого размера (отрицательный поднимает).
    double scriptScale = 0.62;
    double subscriptDrop = 0.25;
    double superscriptDrop = -0.6;

    // <img> без width/height: место держит квадрат этой стороны.
    double imageSide = 24;

    // Подсветка кода: цвета кусков, которые токенизатор размечает как
    // <span style="kw">, "str" и "com" — ключевое слово, строка,
    // комментарий, имена RsdnFormatter. Пустое поле — палитра по теме
    // окна, светлой или тёмной, выбираемая при постройке; зарегистрированный
    // стиль с тем же именем сильнее темы.
    std::optional<Color> keywordColor;
    std::optional<Color> stringColor;
    std::optional<Color> commentColor;

    // Вторая очередь: линейка <hr>, таблица и раскрывающийся <details>.
    // Пустой цвет линий — разделитель темы окна (DividerStrokeColorDefault),
    // свой у светлой и у тёмной.
    Thickness ruleMargin{0, 8, 0, 8};
    double ruleThickness = 1;
    std::optional<Color> ruleColor;
    Thickness tableMargin{0, 8, 0, 8};
    Thickness cellPadding{6, 2, 6, 2};
    std::optional<Color> tableBorderColor;
    Thickness detailsMargin{0, 8, 0, 8};
};

class MarkupBlock : public FormattedBlock {
    using base_t = FormattedBlock;

public:
    class Impl;

    // Вид тегов для всего, что строится с этого момента; уже построенное
    // хранит вид, которым строилось, — как и именованные стили.
    void theme(HtmlTheme const& value) const;

    // Каталог, от которого разрешается голый относительный <img src>: для
    // разметки из файла — каталог этого файла. Пустой (умолчание) —
    // рабочий каталог процесса, что верно для ресурсов приложения.
    void baseDirectory(std::wstring_view directory) const;

protected:
    explicit MarkupBlock(Impl* impl) noexcept;

    friend class Object::Impl;
};

}  // namespace wxl
