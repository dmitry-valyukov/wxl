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

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ui.h"

namespace editor {

enum class Expander : uint8_t { None, Collapsed, Expanded };
enum class Check : uint8_t { None, Unchecked, Checked };

// Строка, как её показывает дерево. Текст — UTF-8 модели; дерево читает его
// сразу, до следующего обращения к модели.
struct TreeRow {
    std::string_view text;
    uint16_t depth = 0;
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

    // Строку выбрали: щелчок по ней.
    virtual void invoke(uint32_t index) = 0;
};

class VirtualTree : public wxl::core::sta_refcounted {
public:
    static wxl::core::intrusive_ptr<VirtualTree> create();

    // Корень, который ставится в разметку.
    wxl::Grid const& view() const { return root_; }

    // Показывает другую модель, с первой строки.
    void model(wxl::core::intrusive_ptr<TreeModel> value);

    // Модель изменилась сама: перечитать видимые строки.
    void refresh();

private:
    VirtualTree();

    struct Row {
        wxl::StackPanel panel;
        wxl::Border indent;
        wxl::TextBlock glyph;
        wxl::CheckBox check;
        wxl::TextBlock text;
    };

    Row makeRow(uint32_t slot);
    void resize(double height);
    void scrollBy(int64_t lines);
    void scrollTo(uint32_t top);
    void render();
    void updateBar();
    uint32_t size() const { return model_ ? model_->size() : 0; }

    // Номер строки модели в ячейке пула; ячейки начинаются за запасом сверху.
    uint32_t first() const { return top_ < reserve_ ? 0 : top_ - reserve_; }

    static constexpr double rowHeight_ = 28;
    static constexpr double indentStep_ = 16;
    static constexpr uint32_t reserve_ = 3;

    wxl::Grid root_;
    wxl::StackPanel rows_;
    wxl::ScrollBar bar_;
    std::vector<Row> pool_;
    std::wstring text_;

    wxl::core::intrusive_ptr<TreeModel> model_;
    uint32_t top_ = 0;
    uint32_t visible_ = 0;  // строк, видимых целиком
};

}  // namespace editor
