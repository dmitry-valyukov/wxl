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
    static wxl::core::intrusive_ptr<Editor> open(std::filesystem::path const& prof);

    ~Editor() override;

    // Левое дерево, вкладка Types: файлы winmd, их пространства имён и типы.
    wxl::core::intrusive_ptr<TreeModel> types() const;

    // Левое дерево, вкладка Resources: стили по своим типам и кисти.
    wxl::core::intrusive_ptr<TreeModel> resources() const;

    // Переход по ссылке из сведений: раскрывает тип в дереве Types и выбирает
    // его; номер его строки среди видимых, если такой тип там есть.
    wxl::core::nullable<uint32_t> reveal(std::wstring_view type) const;

    // Правое дерево: члены выбранного типа; пусто, пока тип не выбран.
    wxl::core::observable<wxl::core::intrusive_ptr<TreeModel>> members;

    // Заголовок окна: имя файла профиля и редактора, как у редакторов документов.
    wxl::core::observable<std::u16string> title;
    wxl::core::observable<std::u16string> typeName;

    // Строка, выбранная последней в любом из деревьев: её модель и сама строка.
    struct Selection {
        wxl::core::intrusive_ptr<TreeModel> model;
        void const* row = nullptr;

        bool operator==(Selection const&) const = default;
    };
    wxl::core::observable<Selection> selection;

    // Справа внизу: сведения о выбранной строке — разметка HtmlBlock. Следует
    // за выбором и за профилем.
    wxl::core::observable<std::wstring> info;

    // Строка состояния: полный путь открытого профиля.
    wxl::core::observable<std::u16string> path;

    // Отметка изменила профиль: деревья перечитывают видимые строки.
    wxl::core::observable<uint32_t> revision;

    // Открыта документация, разобранная не вся: сведениям хватает найти член,
    // а остальное окно дочитывает в фоне, шагами prepareDocumentation().
    wxl::core::observable<bool> documentationPending;

    // Один шаг фонового разбора, в несколько миллисекунд; остался ли ещё.
    bool prepareDocumentation();

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
