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

    // Классы аргументов событий в дереве типов: имя на EventArgs у самого
    // класса или у его базы.
    wxl::core::observable<bool> hideEventArgs {true};

    // Типы, предопределённые вне проекции, — их wxl пишет сам; показанные
    // отметить нельзя.
    wxl::core::observable<bool> hidePredefined {true};

    // Объявление члена и база класса серым после имени в деревьях.
    wxl::core::observable<bool> showSignatures {true};

    // Документация Windows SDK на английском, а не на языке интерфейса.
    wxl::core::observable<bool> englishDocumentation {false};

    // Кегль сведений справа внизу.
    wxl::core::observable<int> detailsFontSize {16};

    // Как виден фокус с клавиатуры — номер в wxl::FocusLook: рамка системы,
    // подъём, свечение.
    wxl::core::observable<int> focusLook {1};
};

// Заполняет настройки сохранёнными; при первом запуске остаются умолчания.
void loadSettings(Settings& settings);

void saveSettings(Settings const& settings);

}  // namespace editor
