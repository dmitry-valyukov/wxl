# Сниппет в байты: то, что вернул бы #embed, — числа через запятую, по 32 на
# строку. Режим скрипта: cmake -DIN=<файл> -DOUT=<файл> -P embed.cmake.
#
# Нужен, пока MSVC не умеет #embed (14.51 отвечает C1021). Когда научится,
# `#include "Snippets/…/X.h.embed"` в страницах меняется на
# `#embed "Snippets/…/X.h"`, и этот файл вместе с правилом в CMakeLists уходит.
file(READ "${IN}" hex HEX)
string(LENGTH "${hex}" size)
set(text "")
set(at 0)
while(at LESS size)
    string(SUBSTRING "${hex}" ${at} 64 chunk)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," chunk "${chunk}")
    string(APPEND text "${chunk}\n")
    math(EXPR at "${at} + 64")
endwhile()
file(WRITE "${OUT}.tmp" "${text}")
file(COPY_FILE "${OUT}.tmp" "${OUT}" ONLY_IF_DIFFERENT)
file(REMOVE "${OUT}.tmp")
