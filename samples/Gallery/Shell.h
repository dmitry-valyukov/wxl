#pragma once

// Оболочка Gallery: главное окно, навигация и история — MainWindow и Frame
// оригинала. Страницы зовут `navigate`, окно строит `createMainWindow`.

#include "pch.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace gallery {

// Куда ведёт переход; `id` — контрол, группа или строка запроса.
enum class Place { Home, AllControls, Section, Item, Search, Settings };

struct Destination {
    Place place = Place::Home;
    std::wstring id;
};

void navigate(Destination destination);

// Модель, поля которой привязаны к элементам страницы, живёт, пока страница
// на экране: привязка держит поле по адресу и не владеет им, а поле держит
// элемент. Страницу сменили — модель отпускается, и вместе с ней элементы.
void holdModel(std::shared_ptr<void> model);

template <class Model>
std::shared_ptr<Model> hold(std::shared_ptr<Model> model) {
    holdModel(model);
    return model;
}

// Последние открытые контролы, новые первыми, и избранные — SettingsHelper
// оригинала; здесь они живут, пока живёт окно.
std::vector<std::wstring> const& recentlyVisited();
void clearRecentlyVisited();
std::vector<std::wstring> const& favorites();
bool isFavorite(std::wstring_view id);
void setFavorite(std::wstring_view id, bool favorite);
void clearFavorites();

wxl::Window createMainWindow();

// Окно и всё, что оболочка держит, отпускаются здесь, до остановки пула.
void destroyMainWindow();

}  // namespace gallery
