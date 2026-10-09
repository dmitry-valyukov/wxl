#pragma once

// Шов между двумя половинами пробы. main.cpp видит wxl и не видит проекции cppwinrt, probes.cpp -- наоборот (как
// samples/Gallery/LottieLogo.cpp), поэтому через шов идут только встроенные типы и голый указатель ABI. Стандартных
// заголовков здесь нет: в main.cpp этот файл стоит после заголовков wxl, то есть после импорта модуля.

struct IInspectable;

namespace probe {

/// Принимает список, пока жива обёртка wxl, которая его сделала: указатель одолжен на время вызова.
using ListTaker = void (*)(::IInspectable* list, void* context);

/// Строит ListView синтаксисом wxl -- `itemTemplate` (impl/item_template.cpp) и `itemsSource = indexList(2)`; каждый
/// вызов строителя прибавляет единицу к *builds. Список отдаётся take и умирает вместе с обёрткой, если take его не
/// удержал.
using WxlListMaker = void (*)(int* builds, ListTaker take, void* context);

/// Из wxl_launched: окно и сценарий вопросов (а)-(г) и (д).
void start(WxlListMaker makeList);

/// Из обработчика Teardown, когда XAML уже остановлен: вопрос (е) дочерним процессом, итог; возвращает число
/// неожиданностей -- код выхода пробы.
int finish();

}  // namespace probe
