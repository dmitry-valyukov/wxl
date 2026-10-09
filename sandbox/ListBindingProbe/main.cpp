// ListBindingProbe, половина на wxl: точка входа и списки, которые строит сам wxl.
//
// Здесь только то, что требует настоящей машинки wxl, -- вопрос (г): состояние шаблона ListView в карте по адресу ABI
// (wxl.ui/src/impl/item_template.cpp). Список описан так, как его описывает приложение, и отдаётся второй половине
// (probes.cpp) голым указателем; всё остальное -- там, на проекции cppwinrt. Что проба спрашивает и как её читать --
// README.md рядом.

#include "ui.h"
#include "Box.h"
#include "launch.h"
#include "probe.h"

import std;

using namespace wxl;
using namespace wxl::dsl;

namespace {

void makeList(int* builds, probe::ListTaker take, void* context) {
    auto const list = ListView {
        width = 140,
        height = 64,
        itemTemplate = [builds](Object const&) {
            ++*builds;
            return TextBlock {u"item"};
        },
        itemsSource = indexList(2),
    };
    take(list.get_abi(), context);
}

}  // namespace

wxl::Teardown wxl_launched() {
    probe::start(&makeList);
    return [](wxl::TeardownReason) { return probe::finish(); };
}
