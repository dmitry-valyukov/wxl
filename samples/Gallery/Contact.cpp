#include "Contact.h"

namespace gallery {

namespace {

// Contacts.txt оригинала (Assets/SampleMedia/Contacts.txt): имя, фамилия, компания — по строке на поле.
// Здесь это таблица в коде, а не чтение файла: данные нужны при построении страницы, а диск в потоке
// интерфейса не читается.
std::vector<Contact> make() {
    std::vector<Contact> contacts = {
    {u"Kendall", u"Collins", u"Adatum Corporation"},
    {u"Henry", u"Ross", u"Adventure Works Cycles"},
    {u"Vance", u"DeLeon", u"Alpine Ski House"},
    {u"Victoria", u"Burke", u"Bellows College"},
    {u"Amber", u"Rodriguez", u"Best For You Organics Company"},
    {u"Amari", u"Rivera", u"Contoso, Ltd."},
    {u"Jessie", u"Irwin", u"Contoso Pharmaceuticals"},
    {u"Quinn", u"Campbell", u"Contoso Suites"},
    {u"Olivia", u"Wilson", u"Consolidated Messenger"},
    {u"Ana", u"Bowman", u"Fabrikam, Inc."},
    {u"Shawn", u"Hughes", u"Fabrikam Residences"},
    {u"Oscar", u"Ward", u"First Up Consultants"},
    {u"Madison", u"Butler", u"Fourth Coffee"},
    {u"Graham", u"Barnes", u"Graphic Design Institute"},
    {u"Anthony", u"Ivanov", u"Humongous Insurance"},
    {u"Michael", u"Peltier", u"Lamna Healthcare Company"},
    {u"Morgan", u"Connors", u"Liberty's Delightful Sinful Bakery & Cafe"},
    {u"Andre", u"Lawson", u"Lucerne Publishing"},
    {u"Preston", u"Morales", u"Margie's Travel"},
    {u"Briana", u"Hernandez", u"Nod Publishers"},
    {u"Nicole", u"Wagner", u"Northwind Traders"},
    {u"Mario", u"Rogers", u"Proseware, Inc."},
    {u"Eugenia", u"Lopez", u"Relecloud"},
    {u"Nathan", u"Rigby", u"School of Fine Art"},
    {u"Ellis", u"Turner", u"Southridge Video"},
    {u"Miguel", u"Reyes", u"Tailspin Toys"},
    {u"Hayden", u"Cook", u"Tailwind Traders"},
    };
    return contacts;
}

}  // namespace

std::vector<Contact> const& contacts() {
    static std::vector<Contact> const all = make();
    return all;
}

}  // namespace gallery