#pragma once

// Настройки редактора — что показывать и как, — из попапа шестерёнки. Помнятся
// между запусками в реестре (HKCU\Software\wxl\wxl.gen.ui): окно пишет их на
// каждое изменение.

#include "ui.h"

namespace editor {

struct Settings {
    // Статические свойства-идентификаторы XxxProperty типа DependencyProperty:
    // их у контролов по одному на свойство, и список членов вдвое длиннее.
    wxl::core::observable<bool> hideDependencyProperties {true};

    // Объявление члена и база класса серым после имени в деревьях.
    wxl::core::observable<bool> showSignatures {true};

    // Документация Windows SDK на английском, а не на языке интерфейса.
    wxl::core::observable<bool> englishDocumentation {false};

    // Кегль сведений справа внизу.
    wxl::core::observable<int> detailsFontSize {16};
};

// Заполняет настройки сохранёнными; при первом запуске остаются умолчания.
void loadSettings(Settings& settings);

void saveSettings(Settings const& settings);

}  // namespace editor
