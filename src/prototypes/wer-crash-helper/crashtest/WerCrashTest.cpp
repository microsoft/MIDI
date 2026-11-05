// PROTOTYPE. Crashes on purpose, in one of several ways, so the WER crash helper experiment can see
// which crashes reach which hook. See ..\README.md.
//
//   WerCrashTest.exe <kind> [--no-register] [--no-filter] [--no-ui]
//
//   none       exits normally (checks the setup without crashing)
//   av         access violation (0xC0000005)
//   cpp        unhandled C++ exception (0xE06D7363)
//   terminate  std::terminate, as a noexcept violation would do: abort, then a fail fast
//   fastfail   __fastfail(FAST_FAIL_FATAL_APP_EXIT) (0xC0000409)
//   wil        WIL THROW_HR, caught, then FAIL_FAST_HR on the same HRESULT. With
//              wil/result_originate.h included, WIL ends this in RoFailFastWithErrorContext.
//   wilplain   WIL FAIL_FAST_HR with no originated error, so WIL fails fast on its own
//   stowed     what a WinUI 3 / C++/WinRT app does with an unhandled exception: a
//              winrt::hresult_error (which originates the error) ends in
//              RoFailFastWithErrorContext (0xC000027B, stowed exception)
//
// --no-register  don't call WerRegisterRuntimeExceptionModule
// --no-filter    don't call SetUnhandledExceptionFilter
// --no-ui        WerSetFlags(WER_FAULT_REPORTING_NO_UI), so WER shows no dialog

#include <windows.h>
#include <werapi.h>
#include <roerrorapi.h>

#include <stdexcept>
#include <exception>
#include <stdio.h>

#include <winrt/base.h>
#include <wil/result.h>
#include <wil/result_originate.h>

#include "protolog.h"

namespace
{
    ProtoWerContext g_context{ ProtoWerContextMagic, 1, L"WerCrashTest" };
    wchar_t g_logPath[MAX_PATH]{};

    LONG WINAPI Filter(_In_ EXCEPTION_POINTERS* pointers)
    {
        DWORD code = (pointers != nullptr && pointers->ExceptionRecord != nullptr) ? pointers->ExceptionRecord->ExceptionCode : 0;
        ProtoAppendLine(g_logPath, L"event=filter_called code=0x%08lX", code);
        return EXCEPTION_CONTINUE_SEARCH;
    }

    __declspec(noinline) void CrashAv()
    {
        volatile int* nothing = nullptr;
        *nothing = 1;
    }

    __declspec(noinline) void CrashCpp()
    {
        throw std::runtime_error("WerCrashTest unhandled C++ exception");
    }

    __declspec(noinline) void CrashWil()
    {
        try
        {
            THROW_HR_MSG(E_BOUNDS, "WerCrashTest WIL error");
        }
        catch (...)
        {
        }

        FAIL_FAST_HR(E_BOUNDS);
    }

    __declspec(noinline) void CrashWilPlain()
    {
        FAIL_FAST_HR(E_ILLEGAL_METHOD_CALL);
    }

    __declspec(noinline) void CrashStowed()
    {
        try
        {
            throw winrt::hresult_error(E_NOTIMPL, L"WerCrashTest C++/WinRT error");
        }
        catch (...)
        {
            RoFailFastWithErrorContext(winrt::to_hresult());
        }
    }
}

int wmain(int argc, wchar_t** argv)
{
    wchar_t logFolder[MAX_PATH]{};
    ProtoLogFolder(nullptr, logFolder, _countof(logFolder));
    swprintf_s(g_logPath, L"%sapp-%lu.log", logFolder, GetCurrentProcessId());

    if (argc < 2)
    {
        fwprintf(stderr, L"usage: WerCrashTest <none|av|cpp|terminate|fastfail|wil|wilplain|stowed> [--no-register] [--no-filter] [--no-ui]\n");
        return 2;
    }

    const wchar_t* kind = argv[1];
    bool doRegister = true;
    bool doFilter = true;
    bool noUi = false;

    for (int i = 2; i < argc; ++i)
    {
        if (_wcsicmp(argv[i], L"--no-register") == 0) { doRegister = false; }
        else if (_wcsicmp(argv[i], L"--no-filter") == 0) { doFilter = false; }
        else if (_wcsicmp(argv[i], L"--no-ui") == 0) { noUi = true; }
    }

    wprintf(L"pid=%lu\n", GetCurrentProcessId());
    fflush(stdout);

    HRESULT registerHr = S_FALSE;
    if (doRegister)
    {
        wchar_t modulePath[MAX_PATH]{};
        GetModuleFileNameW(nullptr, modulePath, _countof(modulePath));
        wchar_t* slash = wcsrchr(modulePath, L'\\');
        if (slash != nullptr)
        {
            slash[1] = L'\0';
        }

        wcscat_s(modulePath, L"WerHelperModule.dll");
        registerHr = WerRegisterRuntimeExceptionModule(modulePath, &g_context);
    }

    HRESULT flagsHr = S_FALSE;
    if (noUi)
    {
        flagsHr = WerSetFlags(WER_FAULT_REPORTING_NO_UI);
    }

    if (doFilter)
    {
        SetUnhandledExceptionFilter(Filter);
    }

    ProtoAppendLine(g_logPath, L"event=app_start kind=%s register_hr=0x%08lX filter=%d no_ui=%d flags_hr=0x%08lX",
        kind, static_cast<unsigned long>(registerHr), doFilter ? 1 : 0, noUi ? 1 : 0, static_cast<unsigned long>(flagsHr));

    ProtoAppendLine(g_logPath, L"event=crashing kind=%s", kind);

    if (_wcsicmp(kind, L"none") == 0) { ProtoAppendLine(g_logPath, L"event=clean_exit"); return 0; }
    else if (_wcsicmp(kind, L"av") == 0) { CrashAv(); }
    else if (_wcsicmp(kind, L"cpp") == 0) { CrashCpp(); }
    else if (_wcsicmp(kind, L"terminate") == 0) { std::terminate(); }
    else if (_wcsicmp(kind, L"fastfail") == 0) { __fastfail(FAST_FAIL_FATAL_APP_EXIT); }
    else if (_wcsicmp(kind, L"wil") == 0) { CrashWil(); }
    else if (_wcsicmp(kind, L"wilplain") == 0) { CrashWilPlain(); }
    else if (_wcsicmp(kind, L"stowed") == 0) { CrashStowed(); }
    else
    {
        fwprintf(stderr, L"unknown kind %s\n", kind);
        return 2;
    }

    ProtoAppendLine(g_logPath, L"event=survived_crash");
    return 1;
}
