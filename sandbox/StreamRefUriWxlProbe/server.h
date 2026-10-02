#pragma once

// Что в пробе делается на cppwinrt, а не обёртками wxl: картинка от системного кодировщика, сервер на петле и
// разбор исключения WinRT. Проверки, ради которых проба заведена, идут через обёртки, в main.cpp; этот файл --
// то, что вокруг них, и обёртками быть не должно.

#include <cstdint>
#include <exception>
#include <string>
#include <vector>

namespace probe {

/// JPEG 320x240, закодированный системным кодировщиком.
std::vector<std::uint8_t> makeImage();

/// Сервер на 127.0.0.1, отвечающий этой картинкой на любой запрос. Возвращает порт.
std::uint16_t serve(std::vector<std::uint8_t> image);

/// HRESULT и сообщение исключения, которое достала корутина; для всего остального -- what().
std::string describe(std::exception_ptr error);

}  // namespace probe
