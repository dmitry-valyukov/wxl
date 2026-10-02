#pragma once

// Контакт примеров ListView — Contact оригинала (ListViewPage.xaml.cs): имя, фамилия, компания; Name — «имя фамилия».
// У оригинала класс со свойствами для привязки; здесь структура, а шаблон — функция от позиции (wxl::ItemBuilder).

#include <string>
#include <vector>

namespace gallery {

struct Contact {
    std::u16string firstName;
    std::u16string lastName;
    std::u16string company;

    std::u16string name() const { return firstName + u" " + lastName; }
};

// Все контакты Contacts.txt, в порядке файла.
std::vector<Contact> const& contacts();

}  // namespace gallery
