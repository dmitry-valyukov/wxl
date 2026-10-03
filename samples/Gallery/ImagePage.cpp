// Страница Image -- ImagePage оригинала.

#include "Pages.h"
#include "Shell.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Media.Imaging.h"


using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t basicHeader[] = {
#include "Snippets/Image/BasicImageLocalFile.html.embed"
};
constexpr char8_t basicCode[] = {
#include "Snippets/Image/BasicImageLocalFile.h.embed"
};

FrameworkElement basic() {
#include "Snippets/Image/BasicImageLocalFile.h"

    return gallery::controlExample({
        .header = gallery::snippet(basicHeader),
        .example = example,
        .code = gallery::snippet(basicCode),
    });
}

constexpr char8_t decodedHeader[] = {
#include "Snippets/Image/ImageDecodedRenderingSize.html.embed"
};
constexpr char8_t decodedCode[] = {
#include "Snippets/Image/ImageDecodedRenderingSize.h.embed"
};

FrameworkElement decoded() {
#include "Snippets/Image/ImageDecodedRenderingSize.h"

    return gallery::controlExample({
        .header = gallery::snippet(decodedHeader),
        .example = example,
        .code = gallery::snippet(decodedCode),
    });
}

constexpr char8_t stretchingHeader[] = {
#include "Snippets/Image/ImageStretching.html.embed"
};
constexpr char8_t stretchingCode[] = {
#include "Snippets/Image/ImageStretching.h.embed"
};

FrameworkElement stretching() {
#include "Snippets/Image/ImageStretching.h"

    return gallery::controlExample({
        .header = gallery::snippet(stretchingHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(stretchingCode),
    });
}

constexpr char8_t nineGridImagesHeader[] = {
#include "Snippets/Image/NineGridImages.html.embed"
};
constexpr char8_t nineGridImagesCode[] = {
#include "Snippets/Image/NineGridImages.h.embed"
};

FrameworkElement nineGridImages() {
#include "Snippets/Image/NineGridImages.h"

    return gallery::controlExample({
        .header = gallery::snippet(nineGridImagesHeader),
        .example = example,
        .code = gallery::snippet(nineGridImagesCode),
    });
}

constexpr char8_t svgHeader[] = {
#include "Snippets/Image/SvgImage.html.embed"
};
constexpr char8_t svgCode[] = {
#include "Snippets/Image/SvgImage.h.embed"
};

FrameworkElement svg() {
#include "Snippets/Image/SvgImage.h"

    return gallery::controlExample({
        .header = gallery::snippet(svgHeader),
        .example = example,
        .code = gallery::snippet(svgCode),
    });
}

constexpr char8_t animatedGifHeader[] = {
#include "Snippets/Image/AnimatedGif.html.embed"
};
constexpr char8_t animatedGifCode[] = {
#include "Snippets/Image/AnimatedGif.h.embed"
};

FrameworkElement animatedGif() {
#include "Snippets/Image/AnimatedGif.h"

    return gallery::controlExample({
        .header = gallery::snippet(animatedGifHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(animatedGifCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::imagePage() {
    return StackPanel {basic(), decoded(), stretching(), nineGridImages(), svg(), animatedGif()};
}
