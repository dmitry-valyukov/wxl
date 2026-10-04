// Страница эффекта GaussianBlur — перенесена из приложения Effects: описание, примеры и их исходники в духе Gallery.
//
// Код каждого примера — файл в Snippets/GaussianBlur/: он включается по #include в функцию, которая строит пример, и он же,
// вшитый байтами, показывается под примером. Показанное и работающее — один и тот же текст.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t KeycapHeader[] = {
#include "Snippets/GaussianBlur/Keycap.html.embed"
};
constexpr char8_t KeycapCode[] = {
#include "Snippets/GaussianBlur/Keycap.h.embed"
};

FrameworkElement keycapExample() {
#include "Snippets/GaussianBlur/Keycap.h"

    return gallery::controlExample({
        .header = gallery::snippet(KeycapHeader),
        .example = example,
        .code = gallery::snippet(KeycapCode),
    });
}

constexpr char8_t CompareHeader[] = {
#include "Snippets/GaussianBlur/Compare.html.embed"
};
constexpr char8_t CompareCode[] = {
#include "Snippets/GaussianBlur/Compare.h.embed"
};

FrameworkElement compareExample() {
#include "Snippets/GaussianBlur/Compare.h"

    return gallery::controlExample({
        .header = gallery::snippet(CompareHeader),
        .example = example,
        .code = gallery::snippet(CompareCode),
    });
}

constexpr char8_t GammaHeader[] = {
#include "Snippets/GaussianBlur/Gamma.html.embed"
};
constexpr char8_t GammaCode[] = {
#include "Snippets/GaussianBlur/Gamma.h.embed"
};

FrameworkElement gammaExample() {
#include "Snippets/GaussianBlur/Gamma.h"

    return gallery::controlExample({
        .header = gallery::snippet(GammaHeader),
        .example = example,
        .code = gallery::snippet(GammaCode),
    });
}

constexpr char8_t OnTopHeader[] = {
#include "Snippets/GaussianBlur/OnTop.html.embed"
};
constexpr char8_t OnTopCode[] = {
#include "Snippets/GaussianBlur/OnTop.h.embed"
};

FrameworkElement onTopExample() {
#include "Snippets/GaussianBlur/OnTop.h"

    return gallery::controlExample({
        .header = gallery::snippet(OnTopHeader),
        .example = example,
        .code = gallery::snippet(OnTopCode),
    });
}

constexpr char8_t NeonSignHeader[] = {
#include "Snippets/GaussianBlur/NeonSign.html.embed"
};
constexpr char8_t NeonSignCode[] = {
#include "Snippets/GaussianBlur/NeonSign.h.embed"
};

FrameworkElement neonSignExample() {
#include "Snippets/GaussianBlur/NeonSign.h"

    return gallery::controlExample({
        .header = gallery::snippet(NeonSignHeader),
        .example = example,
        .code = gallery::snippet(NeonSignCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::gaussianBlurPage() {
    return StackPanel {keycapExample(), compareExample(), gammaExample(), onTopExample(), neonSignExample()};
}
