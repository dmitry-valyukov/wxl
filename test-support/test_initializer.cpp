/**
* \file
* \brief Global fixture.
*/

// Своё раньше импорта: заголовки Windows нужны для символьного стека, а после
// import стандартный заголовок MSVC уже не принимает.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>

#include <crtdbg.h>
#include <stdio.h>

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

int assert_report_hook(int report_type, char * message, int * return_value) {
    if (report_type == _CRT_ASSERT) print_stack(message ? message : "assertion failed");

    // Возвращаем FALSE, чтобы отчёт пошёл своим обычным путём: наше дело --
    // дописать стек, а не подменить поведение assert.
    if (return_value) *return_value = 0;

    return FALSE;
}

/**
* \brief RAII wrapper to initialize the C locale.
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
