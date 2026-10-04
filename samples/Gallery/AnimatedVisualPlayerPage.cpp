// Страница AnimatedVisualPlayer -- AnimatedVisualPlayerPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include "LottieLogo.h"
#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Microsoft.UI.Xaml.Documents.h>


using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t lottieHeader[] = {
#include "Snippets/AnimatedVisualPlayer/AnimatedVisualPlayerPlaybackLottieAnimation.html.embed"
};
constexpr char8_t lottieCode[] = {
#include "Snippets/AnimatedVisualPlayer/AnimatedVisualPlayerPlaybackLottieAnimation.h.embed"
};

FrameworkElement lottie() {
#include "Snippets/AnimatedVisualPlayer/AnimatedVisualPlayerPlaybackLottieAnimation.h"

    return gallery::controlExample({
        .header = gallery::snippet(lottieHeader),
        .example = example,
        .code = gallery::snippet(lottieCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::animatedVisualPlayerPage() {
    return StackPanel {lottie()};
}
