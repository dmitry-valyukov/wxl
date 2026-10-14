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
    // его; номер его строки среди видимых, если такой тип там есть. Типа, которого
    // в дереве нет (интерфейс, делегат), — его карточка в сведениях.
    wxl::core::nullable<uint32_t> reveal(std::wstring_view type);

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

    // Справа внизу: сведения о выбранной строке — значок и разметка. Следует
    // за выбором и за профилем.
    wxl::core::observable<Description> info;

    // Строка состояния: полный путь открытого профиля.
    wxl::core::observable<std::u16string> path;

    // Отметка изменила профиль: деревья перечитывают видимые строки.
    wxl::core::observable<uint32_t> revision;

    // Открыта документация, разобранная не вся: сведениям хватает найти член,
    // а остальное окно дочитывает в фоне, шагами prepareDocumentation().
    wxl::core::observable<bool> documentationPending;

    // Один шаг фонового разбора, в несколько миллисекунд; остался ли ещё.
    bool prepareDocumentation();

    // Настройки: скрывать ли в членах идентификаторы свойств зависимостей
    // (статические свойства только для чтения с именем на Property) и брать ли
    // справочник Windows SDK на английском.
    void hideDependencyProperties(bool hide);
    void englishDocumentation(bool english);

    // Скрывать ли в дереве типов классы аргументов событий (имя на EventArgs у
    // самого класса или у его базы) и типы, предопределённые вне проекции, — те,
    // что wxl пишет сам (predefined в types.json); показанные не отмечаются.
    void hideEventArgs(bool hide);
    void hidePredefined(bool hide);

    // Панель инструментов: какие строки деревьев типов и членов показывать —
    // выбранные профилем, включённые замыканием как зависимости выбранного и
    // прочие. Выбранная строка видна всегда.
    wxl::core::observable<bool> showChosen {true};
    wxl::core::observable<bool> showDependencies {true};
    wxl::core::observable<bool> showNotChosen {true};

private:
    Editor();

    // Отметка изменила профиль: замыкание устарело, а скрытые фильтром
    // строки отбираются заново.
    void marksChanged();
    void refilterTrees();

    struct Data;
    friend class TypesModel;
    friend class MembersModel;
    friend class ResourcesModel;
    friend class TypeCard;

    std::unique_ptr<Data> data_;
    wxl::core::intrusive_ptr<TypesModel> types_;
    wxl::core::intrusive_ptr<ResourcesModel> resources_;
};

}  // namespace editor
