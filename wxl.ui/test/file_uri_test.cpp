// Путь из адреса file:// -- impl::try_path_of_file_uri.
//
// Обычная программа, а не gtest, по той же причине, что teardown_test: file_uri.h доходит
// до wxl.core через core.h (include, затем import), а стандартные заголовки gtest с этим
// import в одной единице трансляции не уживаются. Итог -- в коде выхода.
//
// Что проверяется: все написания адреса одного и того же файла, которые принимает
// Windows.Foundation.Uri (три слеша, один, два с диском, localhost, обратные слеши, пробел
// как есть и как %20), дают один путь; escape-последовательности читаются как UTF-8 --
// Windows.Foundation.Uri сам читает их по байту и портит кириллицу; запрос и фрагмент
// путь обрывают; сетевой путь получает свой сервер; не-файловый адрес и escape-ы не из
// UTF-8 -- отказ.

#include <cstdio>
#include <string>

#include "impl/file_uri.h"

namespace {

int failures = 0;

void check(bool ok, char const* what) {
    if (!ok) {
        std::fprintf(stderr, "file_uri_test: FAILED -- %s\n", what);
        ++failures;
    }
}

// Путь, в который адрес читается, или текст отказа.
std::u16string read(std::u16string_view address) {
    std::u16string path;
    return wxl::impl::try_path_of_file_uri(address, path) ? path : u"<refused>";
}

void all_read_as(std::u16string const& expected, std::initializer_list<std::u16string_view> addresses, char const* what) {
    std::size_t index = 0;
    for (auto const address : addresses) {
        if (read(address) != expected) {
            std::fprintf(stderr, "file_uri_test: FAILED -- %s: address #%zu\n", what, index);
            ++failures;
        }
        ++index;
    }
}

}  // namespace

int main() {
    all_read_as(u"M:\\Temp\\a.jpg",
                {u"file:///M:/Temp/a.jpg", u"file:/M:/Temp/a.jpg", u"file://M:/Temp/a.jpg", u"file://localhost/M:/Temp/a.jpg",
                 u"file://LocalHost/M:/Temp/a.jpg", u"file:///M:\\Temp\\a.jpg", u"FILE:///M:/Temp/a.jpg", u"file:///M:/Temp/a.jpg?x=1#part"},
                "one file, every spelling of the address");

    all_read_as(u"M:\\Temp\\with space.jpg",
                {u"file:///M:/Temp/with space.jpg", u"file:///M:/Temp/with%20space.jpg", u"file:///M:\\Temp\\with%20space.jpg"},
                "a space as it is and as %20");

    all_read_as(u"M:\\Temp\\папка с пробелом\\Кириллица.jpg",
                {u"file:///M:/Temp/папка с пробелом/Кириллица.jpg", u"file:///M:/Temp/папка%20с%20пробелом/Кириллица.jpg",
                 u"file:///M:/Temp/%D0%BF%D0%B0%D0%BF%D0%BA%D0%B0%20%D1%81%20%D0%BF%D1%80%D0%BE%D0%B1%D0%B5%D0%BB%D0%BE%D0%BC/"
                 u"%D0%9A%D0%B8%D1%80%D0%B8%D0%BB%D0%BB%D0%B8%D1%86%D0%B0.jpg",
                 u"file:///M:/Temp/%d0%bf%d0%b0%d0%bf%d0%ba%d0%b0%20%d1%81%20%d0%bf%d1%80%d0%be%d0%b1%d0%b5%d0%bb%d0%be%d0%bc/Кириллица.jpg"},
                "Cyrillic written as itself and as UTF-8 escapes, any case of the hex digits");

    // Символ вне плоскости 0: пара суррогатов из четырёх байт UTF-8.
    check(read(u"file:///M:/%F0%9F%93%B7.jpg") == u"M:\\\U0001F4F7.jpg", "an escape of four bytes is one code point outside the BMP");

    check(read(u"file:///M:/a%23b%3Fc.jpg") == u"M:\\a#b?c.jpg", "escaped # and ? are part of the name; unescaped ones end the path");
    check(read(u"file:///M:/a%b.jpg") == u"M:\\a%b.jpg", "a % that opens no escape is a character");
    check(read(u"file:///M:/a%4.jpg") == u"M:\\a%4.jpg", "a % with one digit is a character");
    check(read(u"file:///M:/a%") == u"M:\\a%", "a % at the end is a character");

    check(read(u"file://server/share/dir/a b.jpg") == u"\\\\server\\share\\dir\\a b.jpg", "a host that is not local names a network path");
    check(read(u"file://server/share/%D0%A4.jpg") == u"\\\\server\\share\\Ф.jpg", "escapes in a network path are UTF-8 too");
    check(read(u"file:///dir/a.jpg") == u"\\dir\\a.jpg", "a path with no drive keeps its leading separator");

    check(read(u"https://example.com/a.jpg") == u"<refused>", "another scheme is not a file address");
    check(read(u"ms-appx:///a.jpg") == u"<refused>", "ms-appx is not a file address");
    check(read(u"M:/Temp/a.jpg") == u"<refused>", "a path is not an address");
    check(read(u"") == u"<refused>", "nothing is not an address");
    check(read(u"file:///M:/%FF.jpg") == u"<refused>", "an escape that is not UTF-8 is refused");
    check(read(u"file:///M:/%D0.jpg") == u"<refused>", "half of a UTF-8 character is refused");
    check(read(u"file://%FF/share/a.jpg") == u"<refused>", "the same of a host");

    return failures == 0 ? 0 : 1;
}
