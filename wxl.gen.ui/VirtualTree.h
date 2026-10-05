#pragma once

// Виртуальное дерево: модель отдаёт видимые строки по номеру, а дерево строит
// элементы только для строк на экране и небольшого запаса сверху и снизу — пул,
// который при листании не пересоздаётся, а получает другие строки. Высота у всех
// строк одна, поэтому строка под любым смещением находится делением, без обхода.
// Вертикальную полосу дерево ведёт само; горизонтальную — ScrollViewer, по ширине
// строк, построенных сейчас.
//
// Это управляющий класс, а не контрол: он владеет своими элементами и пулом, а
// приложение ставит его корень в разметку и подменяет ему модель.

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Color.h"
#include "ui.h"

namespace editor {

enum class Expander : uint8_t { None, Collapsed, Expanded };
// Indeterminate — у родителя, когда отмечены не все дети; Disabled — отметку
// поставить нельзя: пустой квадрат блёклым цветом, щелчок ничего не меняет;
// Locked — отмечено всегда: галочка блёклым цветом, щелчок ничего не меняет.
enum class Check : uint8_t { None, Unchecked, Checked, Indeterminate, Disabled, Locked };

// Значок строки: знак шрифта Fluent UI System Icons (Assets) и его радиальная
// заливка — светлая середина, тёмный край; один цвет на обоих — сплошная
// заливка этим цветом. Модели держат значки постоянными,
// дерево строит по кисти на значок и узнаёт его по адресу.
struct RowIcon {
    char32_t glyph;
    wxl::Color outer;
    wxl::Color inner;
};

// Радиальная заливка значка — та же у строк дерева и у кнопок панели.
wxl::Template<wxl::RadialGradientBrush> iconFill(wxl::Color outer, wxl::Color inner);

// Знак значка с его заливкой — для сведений, где значок стоит перед текстом.
wxl::TextBlock iconText(RowIcon const& icon, double size);

// Сведения о строке дерева: значок строки и разметка HtmlBlock.
struct Description {
    RowIcon const* icon = nullptr;
    std::wstring html;

    bool operator==(Description const&) const = default;
};

// Строка, как её показывает дерево. Текст — UTF-8 модели; дерево читает его
// сразу, до следующего обращения к модели.
struct TreeRow {
    std::string_view text;
    std::string_view detail;  // серым после текста: тип, сигнатура, счёт
    uint16_t depth = 0;

    RowIcon const* icon = nullptr;

    Expander expander = Expander::None;
    Check check = Check::None;

    // Выбранное помнит модель, а не дерево: номер видимой строки сдвигается,
    // когда выше неё раскрывают или сворачивают узел.
    bool selected = false;
};

// Видимые строки дерева: раскрытое и свёрнутое решает модель, номер строки —
// номер среди видимых.
class TreeModel : public wxl::core::sta_refcounted {
public:
    virtual uint32_t size() const = 0;
    virtual TreeRow row(uint32_t index) const = 0;

    virtual void toggleExpanded(uint32_t index) = 0;
    virtual void toggleChecked(uint32_t index) = 0;

    // Строку выбрали: щелчок по ней. Раскрывающуюся строку тот же щелчок
    // затем раскрывает или сворачивает.
    virtual void invoke(uint32_t index) = 0;

    // Двойной щелчок по строке или Enter; первый щелчок уже пришёл в invoke().
    virtual void activate(uint32_t) {}

    // Номер выбранной строки среди видимых; пусто, если выбранного нет или оно
    // внутри свёрнутого. От него ходят клавиши.
    virtual wxl::core::nullable<uint32_t> selectedRow() const = 0;

    // Сведения о строке, выбранной в этой модели.
    virtual Description describe() const { return {}; }
};

class VirtualTree : public wxl::core::sta_refcounted {
public:
    static wxl::core::intrusive_ptr<VirtualTree> create();

    // Корень, который ставится в разметку.
    wxl::Grid const& view() const { return root_; }

    // Показывает другую модель, с первой строки; та же модель остаётся, где была.
    void model(wxl::core::intrusive_ptr<TreeModel> value);

    // Модель изменилась, и строку index надо показать: если её не видно,
    // дерево листает так, чтобы она встала в треть высоты.
    void reveal(uint32_t index);

    // Модель изменилась сама: перечитать видимые строки.
    void refresh();

    // Показывать ли пояснение серым после текста строки.
    void details(bool show);

    // Ширина, которой дереву хватает без прокрутки вбок: самая широкая из
    // построенных строк с тех пор, как дерево раскрывали, сворачивали или меняли
    // ему модель, и полоса прокрутки. Листание её только растит — панель под
    // деревом не прыгает от строки к строке.
    wxl::core::observable<double> wantedWidth;

private:
    VirtualTree();

    struct Row {
        wxl::StackPanel panel;
        wxl::Border marker;  // черта выбранной строки, когда дерево в фокусе
        wxl::Border indent;
        wxl::TextBlock glyph;
        wxl::TextBlock check;
        wxl::TextBlock icon;
        wxl::TextBlock text;
        wxl::TextBlock detail;
        bool selected = false;  // выбрана ли строка модели, показанная в ячейке
        bool expandable = false;
    };

    Row makeRow(uint32_t slot);
    void paint(uint32_t slot);
    void resize(double height);
    void wheel(wxl::UIElement const& source, wxl::PointerRoutedEventArgs& args);
    void measureWidth();
    void key(wxl::KeyRoutedEventArgs& args);
    bool typeAhead(char16_t character);
    void expandAll(uint32_t index);
    void select(uint32_t index);
    void scrollIntoView(uint32_t index);
    void takeFocus();
    void focus(bool focused);
    void scrollBy(int64_t lines);
    void scrollTo(uint32_t top);
    void render();
    void updateBar();
    uint32_t size() const { return model_ ? model_->size() : 0; }
    wxl::Brush const& iconBrush(RowIcon const& icon);

    // Номер строки модели в ячейке пула; ячейки начинаются за запасом сверху.
    uint32_t first() const { return top_ < reserve_ ? 0 : top_ - reserve_; }

    static constexpr double rowHeight_ = 26;
    static constexpr double indentStep_ = 16;
    static constexpr uint32_t reserve_ = 3;

    wxl::Grid root_;
    wxl::StackPanel rows_;
    wxl::ScrollBar bar_;
    std::vector<Row> pool_;
    std::vector<std::pair<RowIcon const*, wxl::Brush>> fills_;
    std::u16string text_;
    std::u16string glyph_;
    std::u16string detail_;

    wxl::core::intrusive_ptr<TreeModel> model_;
    uint32_t top_ = 0;
    uint32_t visible_ = 0;  // строк, видимых целиком
    double height_ = 0;     // высота, под которую построен пул

    // Ячейка пула под указателем. Ячейка, а не строка модели: при листании
    // под неподвижным указателем остаётся та же ячейка с другой строкой.
    wxl::core::nullable<uint32_t> hovered_;

    bool focused_ = false;
    bool details_ = true;
    double widest_ = 0;  // самая широкая строка с последнего раскрытия или смены модели
    std::u16string typed_;  // набранное поиском по первым буквам
    std::chrono::steady_clock::time_point typedAt_;
};

}  // namespace editor
