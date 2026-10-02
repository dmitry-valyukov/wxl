#include "CustomDataObject.h"

#include <array>

namespace gallery {

namespace {

constexpr std::array<char16_t const*, 8> dummyTexts = {
    u"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer id facilisis lectus. Cras nec convallis ante, quis "
    u"pulvinar tellus. Integer dictum accumsan pulvinar. Pellentesque eget enim sodales sapien vestibulum consequat.",
    u"Nullam eget mattis metus. Donec pharetra, tellus in mattis tincidunt, magna ipsum gravida nibh, vitae lobortis ante "
    u"odio vel quam.",
    u"Quisque accumsan pretium ligula in faucibus. Mauris sollicitudin augue vitae lorem cursus condimentum quis ac mauris. "
    u"Pellentesque quis turpis non nunc pretium sagittis. Nulla facilisi. Maecenas eu lectus ante. Proin eleifend vel lectus "
    u"non tincidunt. Fusce condimentum luctus nisi, in elementum ante tincidunt nec.",
    u"Aenean in nisl at elit venenatis blandit ut vitae lectus. Praesent in sollicitudin nunc. Pellentesque justo augue, "
    u"pretium at sem lacinia, scelerisque semper erat. Ut cursus tortor at metus lacinia dapibus.",
    u"Ut consequat magna luctus justo egestas vehicula. Integer pharetra risus libero, et posuere justo mattis et.",
    u"Proin malesuada, libero vitae aliquam venenatis, diam est faucibus felis, vitae efficitur erat nunc non mauris. "
    u"Suspendisse at sodales erat.",
    u"Aenean vulputate, turpis non tincidunt ornare, metus est sagittis erat, id lobortis orci odio eget quam. Suspendisse "
    u"ex purus, lobortis quis suscipit a, volutpat vitae turpis.",
    u"Duis facilisis, quam ut laoreet commodo, elit ex aliquet massa, non varius tellus lectus et nunc. Donec vitae risus "
    u"ut ante pretium semper. Phasellus consectetur volutpat orci, eu dapibus turpis. Fusce varius sapien eu mattis "
    u"pharetra.",
};

std::u16string number(int value) {
    std::u16string text;
    do {
        text.insert(text.begin(), static_cast<char16_t>(u'0' + value % 10));
        value /= 10;
    } while (value != 0);
    return text;
}

std::vector<CustomDataObject> make(int count) {
    std::vector<CustomDataObject> objects;
    for (int index = 0; index < count; ++index) {
        // Два числа, которые у оригинала случайны (100..999 и 10..99): вычислены из позиции.
        int const views = 100 + (index * 337 + 91) % 900;
        int const likes = 10 + (index * 53 + 17) % 90;
        objects.push_back({
            u"Item " + number(index + 1),
            u"Assets/SampleMedia/LandscapeImage" + number(index + 1) + u".jpg",
            number(views),
            number(likes),
            dummyTexts[static_cast<size_t>(index) % dummyTexts.size()],
        });
    }
    return objects;
}
}  // namespace

std::vector<CustomDataObject> const& dataObjects(bool includeAllItems) {
    static std::vector<CustomDataObject> const eight = make(8);
    static std::vector<CustomDataObject> const thirteen = make(13);
    return includeAllItems ? thirteen : eight;
}

}  // namespace gallery
