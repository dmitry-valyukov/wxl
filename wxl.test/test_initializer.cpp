/**
* \file
* \brief Global fixture.
*/

// Своё раньше импорта: заголовки Windows нужны для символьного стека, а после
// import стандартный заголовок MSVC уже не принимает.
#include "platform.h"

#include <dbghelp.h>

#include <crtdbg.h>
#include <stdio.h>
#include <stdlib.h>

#include <gtest/gtest.h>

import std;
import wxl.core;

#pragma comment(lib, "dbghelp.lib")

namespace {

/// Печатает стек вызовов в stderr.
///
/// Нужно потому, что упавший assert в многопоточном тесте не говорит ничего,
/// кроме файла и строки, а под рукой не всегда есть отладчик: остановиться на
/// нём в чужом потоке негде, и место, откуда пришли, приходится угадывать.
void print_stack(const char * what) {
    void * frames[62] = {};
    const USHORT count = ::RtlCaptureStackBackTrace(2, USHORT(std::size(frames)), frames, nullptr);

    const HANDLE process = ::GetCurrentProcess();

    static const bool symbols_ready = [process] {
        ::SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
        return ::SymInitialize(process, nullptr, TRUE) != FALSE;
    }();

    std::fprintf(stderr, "\n%s\nthread %lu, stack:\n", what, ::GetCurrentThreadId());

    for (USHORT i = 0; i < count; ++i) {
        const DWORD64 address = DWORD64(frames[i]);

        char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
        SYMBOL_INFO * symbol = reinterpret_cast<SYMBOL_INFO *>(buffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;

        DWORD64 displacement = 0;
        const bool named = symbols_ready && ::SymFromAddr(process, address, &displacement, symbol);

        IMAGEHLP_LINE64 line = {};
        line.SizeOfStruct = sizeof(line);
        DWORD line_displacement = 0;
        const bool located =
            symbols_ready && ::SymGetLineFromAddr64(process, address, &line_displacement, &line);

        std::fprintf(stderr, "  %2u  %s", unsigned(i), named ? symbol->Name : "<unknown>");

        if (located) std::fprintf(stderr, "  (%s:%lu)", line.FileName, line.LineNumber);

        std::fprintf(stderr, "\n");
    }

    std::fflush(stderr);
}

int assert_report_hook(int report_type, char * message, int * /*return_value*/) {
    // Другие отчёты идут своим обычным путём: наше дело -- проваленный assert.
    if (report_type != _CRT_ASSERT) return FALSE;

    print_stack(message ? message : "assertion failed");

    // Проваленный отладочный assert CRT заканчивает процесс, как assert() UCRT:
    // gtest переводит отчёты CRT в stderr, и без этого выполнение шло бы дальше,
    // а тест с проваленной проверкой проходил. Без окна и без диалога отчёта,
    // чтобы прогон не повис.
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    std::abort();
}

/**
* \brief Sets the C library's locale to the user's (setlocale(LC_ALL, "")) before the tests.
*/
class test_initializer_environment : public ::testing::Environment
{
public:
    void SetUp() override {
        setlocale(LC_ALL, "");
    }
};

::testing::Environment* const g_test_initializer =
    ::testing::AddGlobalTestEnvironment(new test_initializer_environment);

const int g_assert_hook_installed = (_CrtSetReportHook(&assert_report_hook), 0);

}
