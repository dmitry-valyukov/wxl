#pragma once

// Редактор одного профиля: сам профиль, метаданные и словари XAML, которые он
// называет, и модели деревьев над ними — типы по файлам и пространствам имён,
// именованные ресурсы словарей и члены выбранного типа. Отметки правят профиль
// в памяти.

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include "VirtualTree.h"

namespace editor {

class TypesModel;
class ResourcesModel;

class Editor : public wxl::core::sta_refcounted {
public:
    // Читает профиль, открывает метаданные его пакетов и читает их словари.
    static wxl::core::intrusive_ptr<Editor> open(std::filesystem::path const& profile);

    ~Editor() override;

    // Левое дерево, вкладка Types: файлы winmd, их пространства имён и типы.
    wxl::core::intrusive_ptr<TreeModel> types() const;

    // Левое дерево, вкладка Resources: стили по своим типам и кисти.
    wxl::core::intrusive_ptr<TreeModel> resources() const;

    // Правое дерево: члены выбранного типа; пусто, пока тип не выбран.
    wxl::core::observable<wxl::core::intrusive_ptr<TreeModel>> members;

    // Заголовок окна: имя файла профиля и редактора, как у редакторов документов.
    wxl::core::observable<std::u16string> title;
    wxl::core::observable<std::u16string> typeName;

    // Строка состояния: полный путь открытого профиля.
    wxl::core::observable<std::u16string> path;

    // Отметка изменила профиль: деревья перечитывают видимые строки.
    wxl::core::observable<uint32_t> revision;

private:
    Editor();

    struct Data;
    friend class TypesModel;
    friend class MembersModel;
    friend class ResourcesModel;

    std::unique_ptr<Data> data_;
    wxl::core::intrusive_ptr<TypesModel> types_;
    wxl::core::intrusive_ptr<ResourcesModel> resources_;
};

}  // namespace editor
