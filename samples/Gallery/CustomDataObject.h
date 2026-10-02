#pragma once

// Данные примеров коллекций — CustomDataObject оригинала (ConnectedAnimationPage.xaml.cs): подпись, картинка,
// просмотры, отметки, описание. У оригинала это класс со свойствами, к которым привязывается DataTemplate;
// здесь это структура, а шаблон — функция от позиции в векторе (см. wxl::ItemBuilder).

#include <string>
#include <vector>

namespace gallery {

struct CustomDataObject {
    std::u16string title;
    std::u16string imageLocation;  // путь от папки рядом с exe, как у остальных картинок
    std::u16string views;
    std::u16string likes;
    std::u16string description;
};

// Восемь объектов, или тринадцать при includeAllItems — как GetDataObjects оригинала. Просмотры и отметки
// у оригинала случайны; здесь числа те же при каждом запуске, чтобы снимки сравнивались.
std::vector<CustomDataObject> const& dataObjects(bool includeAllItems = false);

}  // namespace gallery
