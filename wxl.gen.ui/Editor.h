#pragma once

// Редактор одного профиля: сам профиль, метаданные, которые он называет, и
// модели двух деревьев над ними — типы по пространствам имён и члены
// выбранного типа. Отметки правят профиль в памяти.

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include "VirtualTree.h"

namespace editor {

class Editor : public wxl::core::sta_refcounted {
public:
    // Читает профиль и открывает метаданные его пакетов.
    static wxl::core::intrusive_ptr<Editor> open(std::filesystem::path const& profile);

    ~Editor() override;

    // Левое дерево: пространства имён и их типы.
    wxl::core::intrusive_ptr<TreeModel> types() const { return types_; }

    // Правое дерево: члены выбранного типа; пусто, пока тип не выбран.
    wxl::core::observable<wxl::core::intrusive_ptr<TreeModel>> members;

    wxl::core::observable<std::u16string> profileName;
    wxl::core::observable<std::u16string> typeName;

    // Отметка изменила профиль: деревья перечитывают видимые строки.
    wxl::core::observable<uint32_t> revision;

private:
    Editor();

    struct Data;
    friend class TypesModel;
    friend class MembersModel;

    std::unique_ptr<Data> data_;
    wxl::core::intrusive_ptr<TreeModel> types_;
};

}  // namespace editor
