// Тест зовёт саму Windows (SetLastError, коды ошибок), поэтому её заголовок
// идёт первым -- до import, как того требует порядок в проекте.
#include "platform.h"

#include <gtest/gtest.h>

import std;
import wxl.core;
import wxl.async;

using namespace wxl::async;

// Сообщение собирается из трёх частей, и все три обязаны быть на месте: наше
// объяснение, код -- по коду ошибку ищут в документации -- и объяснение системы,
// которое дописывает std::system_error.
TEST(LastErrorTest, MessageCarriesOurTextTheCodeAndTheSystems)
{
    const last_error ex("Opening the door", ERROR_FILE_NOT_FOUND);

    const std::string text = ex.what();

    EXPECT_EQ(static_cast<int>(ERROR_FILE_NOT_FOUND), ex.code().value());
    EXPECT_EQ(&std::system_category(), &ex.code().category());

    // Объяснение системы -- это то, чего в нашем тексте не было; какое именно,
    // зависит от языка Windows, поэтому проверяется лишь его наличие.
    const std::string ours =
        std::format("Opening the door (system error code = {}): ", static_cast<int>(ERROR_FILE_NOT_FOUND));
    EXPECT_TRUE(text.starts_with(ours)) << text;
    EXPECT_GT(text.size(), ours.size()) << text;
}

// Код, которого система не знает, объяснения не получает -- и это не повод
// потерять ни наш текст, ни сам код.
TEST(LastErrorTest, AnUnknownCodeStillLeavesAReadableMessage)
{
    const int nonsense_code = 0x1FFFFFFF;
    const last_error ex("Doing the impossible", nonsense_code);

    const std::string text = ex.what();

    EXPECT_EQ(nonsense_code, ex.code().value());
    EXPECT_TRUE(text.starts_with(std::format("Doing the impossible (system error code = {})", nonsense_code)))
        << text;
}

// Код по умолчанию -- последняя ошибка потока, снятая в точке броска.
TEST(LastErrorTest, TakesTheLastErrorWhenNoneIsGiven)
{
    ::SetLastError(ERROR_ACCESS_DENIED);

    const last_error ex("Entering the room");

    EXPECT_EQ(static_cast<int>(ERROR_ACCESS_DENIED), ex.code().value());
}

// Это std::system_error: ловится как он, и код сравним с std::errc -- категория
// переводит коды Windows в условия POSIX.
TEST(LastErrorTest, IsASystemErrorToWhoeverCatchesOne)
{
    try {
        throw last_error("CreateFileW", ERROR_FILE_NOT_FOUND);
    } catch (const std::system_error& failure) {
        EXPECT_EQ(failure.code(), std::errc::no_such_file_or_directory);
        return;
    }
    FAIL() << "not caught as std::system_error";
}
