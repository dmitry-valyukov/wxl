// Тест зовёт саму Windows (SetLastError, коды ошибок), поэтому её заголовок
// идёт первым -- до import, как того требует порядок в проекте.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <gtest/gtest.h>

import std;
import wxl.core;
import wxl.async;

using namespace wxl::async;

// Сообщение собирается из трёх частей, и все три обязаны быть на месте: наше
// объяснение, объяснение системы и сам код -- по коду ошибку ищут в документации.
//
// --run_test=System/system_exception/messageCarriesOurTextTheSystemsAndTheCode
TEST(SystemExceptionTest, MessageCarriesOurTextTheSystemsAndTheCode)
{
    const system_exception ex("Opening the door", ERROR_FILE_NOT_FOUND);

    const std::string text = ex.what();

    EXPECT_EQ(static_cast<int>(ERROR_FILE_NOT_FOUND), ex.err_code());
    EXPECT_NE(text.find("Opening the door"), std::string::npos) << text;
    EXPECT_NE(text.find(std::format("System error code = {}", ERROR_FILE_NOT_FOUND)), std::string::npos) << text;

    // Объяснение системы -- это то, чего в нашем тексте не было; какое именно,
    // зависит от языка Windows, поэтому проверяется лишь его наличие.
    const std::string prefix = "Opening the door. ";
    EXPECT_TRUE(text.starts_with(prefix)) << text;
    EXPECT_GT(text.size(), prefix.size() + std::string("System error code = 2").size()) << text;
}

// Код, которого система не знает, объяснения не получает -- и это не повод
// потерять ни наш текст, ни сам код.
//
// --run_test=System/system_exception/anUnknownCodeStillLeavesAReadableMessage
TEST(SystemExceptionTest, AnUnknownCodeStillLeavesAReadableMessage)
{
    const int nonsense_code = 0x1FFFFFFF;
    const system_exception ex("Doing the impossible", nonsense_code);

    const std::string text = ex.what();

    EXPECT_EQ(nonsense_code, ex.err_code());
    EXPECT_TRUE(text.starts_with("Doing the impossible")) << text;
    EXPECT_NE(text.find(std::format("System error code = {}", nonsense_code)), std::string::npos) << text;
}

// Код по умолчанию -- последняя ошибка потока, снятая в точке броска.
//
// --run_test=System/system_exception/takesTheLastErrorWhenNoneIsGiven
TEST(SystemExceptionTest, TakesTheLastErrorWhenNoneIsGiven)
{
    ::SetLastError(ERROR_ACCESS_DENIED);

    const system_exception ex("Entering the room");

    EXPECT_EQ(static_cast<int>(ERROR_ACCESS_DENIED), ex.err_code());
}
