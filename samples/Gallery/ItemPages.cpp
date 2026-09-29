// Какие контролы перенесены: идентификатор из каталога и функция, строящая
// страницу примеров. Строка сюда — единственное, что нужно добавить, когда
// перенесена очередная страница; в навигации и на плитках контрол
// включается сам.

#include "Pages.h"

#include <iterator>

namespace {

struct Entry {
    std::wstring_view id;
    gallery::ControlPage page;
};

constexpr Entry ported[] = {
    {L"Button", &gallery::buttonPage},
};

}  // namespace

gallery::ControlPage gallery::pageFor(std::wstring_view uniqueId) {
    for (auto const& entry : ported) {
        if (entry.id == uniqueId) {
            return entry.page;
        }
    }
    return nullptr;
}
