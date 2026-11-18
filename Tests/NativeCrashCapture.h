#pragma once
#include <Windows.h>
#include <DbgHelp.h>

// Opt-in diagnostics for disposable native fixtures. An exception still follows
// its original handler/search path; no assertion or failure is converted to success.
namespace native_crash_capture
{
    inline HANDLE dump = INVALID_HANDLE_VALUE;
    inline decltype(&MiniDumpWriteDump) writeDump = nullptr;
    inline LONG captured = 0;
    inline LONG CALLBACK Capture(EXCEPTION_POINTERS* exception)
    {
        if (exception && exception->ExceptionRecord && writeDump &&
            exception->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
            InterlockedCompareExchange(&captured, 1, 0) == 0)
        {
            MINIDUMP_EXCEPTION_INFORMATION info{GetCurrentThreadId(), exception, FALSE};
            if (writeDump(GetCurrentProcess(), GetCurrentProcessId(), dump,
                    MiniDumpNormal, &info, nullptr, nullptr))
                FlushFileBuffers(dump);
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }
    inline void Enable()
    {
        wchar_t path[32768]{};
        const DWORD size = GetEnvironmentVariableW(L"RENEGADE_NATIVE_PROOF_CRASH_DUMP",
            path, DWORD(sizeof(path) / sizeof(path[0])));
        if (!size || size >= sizeof(path) / sizeof(path[0])) return;
        const auto library = LoadLibraryW(L"dbghelp.dll");
        if (!library) return;
        writeDump = reinterpret_cast<decltype(writeDump)>(GetProcAddress(library, "MiniDumpWriteDump"));
        if (!writeDump) return;
        dump = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (dump == INVALID_HANDLE_VALUE) { writeDump = nullptr; return; }
        // Keep this test-only capture available through late native teardown.
        AddVectoredExceptionHandler(0, Capture);
    }
}
