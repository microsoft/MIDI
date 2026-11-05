// PROTOTYPE. WER runtime exception module for the Windows MIDI Services crash helper experiment.
// See ..\README.md.
//
// WER loads this DLL into WerFault.exe when a process that called WerRegisterRuntimeExceptionModule
// with this DLL's path crashes, provided the path is also listed under
// HKLM (or HKCU) \SOFTWARE\Microsoft\Windows\Windows Error Reporting\RuntimeExceptionHelperModules.
//
// The callback never claims the crash, so WER carries on with its normal report and with the
// LocalDumps dump. All it does is log what it saw and start WerCrashHelper.exe, which sits next to
// this DLL, without waiting for it.

#include <windows.h>
#include <werapi.h>

#include "protolog.h"

namespace
{
    HMODULE g_module{ nullptr };
    wchar_t g_logFolder[MAX_PATH]{};

    void LogPathFor(_In_ DWORD crashedPid, _Out_writes_(cch) wchar_t* path, _In_ size_t cch) noexcept
    {
        swprintf_s(path, cch, L"%smodule-%lu.log", g_logFolder, crashedPid);
    }

    void DescribeJob(_Out_writes_(cch) wchar_t* text, _In_ size_t cch) noexcept
    {
        BOOL inJob = FALSE;
        if (!IsProcessInJob(GetCurrentProcess(), nullptr, &inJob))
        {
            swprintf_s(text, cch, L"in_job=unknown(%lu)", GetLastError());
            return;
        }

        if (!inJob)
        {
            swprintf_s(text, cch, L"in_job=0");
            return;
        }

        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
        if (QueryInformationJobObject(nullptr, JobObjectExtendedLimitInformation, &info, sizeof(info), nullptr))
        {
            DWORD flags = info.BasicLimitInformation.LimitFlags;
            swprintf_s(text, cch, L"in_job=1 job_flags=0x%08lX kill_on_close=%d breakaway_ok=%d silent_breakaway=%d",
                flags,
                (flags & JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE) ? 1 : 0,
                (flags & JOB_OBJECT_LIMIT_BREAKAWAY_OK) ? 1 : 0,
                (flags & JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK) ? 1 : 0);
        }
        else
        {
            swprintf_s(text, cch, L"in_job=1 job_flags=unknown(%lu)", GetLastError());
        }
    }

    void DescribeContext(_In_ HANDLE process, _In_opt_ PVOID context, _Out_writes_(cch) wchar_t* text, _In_ size_t cch) noexcept
    {
        if (context == nullptr)
        {
            swprintf_s(text, cch, L"context=null");
            return;
        }

        ProtoWerContext remote{};
        SIZE_T read = 0;
        if (!ReadProcessMemory(process, context, &remote, sizeof(remote), &read) || read != sizeof(remote))
        {
            swprintf_s(text, cch, L"context_read=failed(%lu)", GetLastError());
            return;
        }

        remote.AppId[_countof(remote.AppId) - 1] = L'\0';
        swprintf_s(text, cch, L"context_read=ok magic_ok=%d app_id=\"%s\"", remote.Magic == ProtoWerContextMagic ? 1 : 0, remote.AppId);
    }

    bool DumpExists(_In_z_ const wchar_t* dumpPath) noexcept
    {
        return GetFileAttributesW(dumpPath) != INVALID_FILE_ATTRIBUTES;
    }

    // The folder LocalDumps will write to for this exe, read the same way the helper reads it.
    void ExpectedDumpPath(_In_z_ const wchar_t* exeName, _In_ DWORD pid, _Out_writes_(cch) wchar_t* path, _In_ size_t cch) noexcept
    {
        path[0] = L'\0';

        wchar_t key[MAX_PATH]{};
        swprintf_s(key, L"SOFTWARE\\Microsoft\\Windows\\Windows Error Reporting\\LocalDumps\\%s", exeName);

        wchar_t raw[MAX_PATH]{};
        DWORD size = sizeof(raw);
        if (RegGetValueW(HKEY_LOCAL_MACHINE, key, L"DumpFolder", RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND, nullptr, raw, &size) != ERROR_SUCCESS)
        {
            return;
        }

        wchar_t folder[MAX_PATH]{};
        if (ExpandEnvironmentStringsW(raw, folder, _countof(folder)) == 0)
        {
            return;
        }

        swprintf_s(path, cch, L"%s\\%s.%lu.dmp", folder, exeName, pid);
    }

    HRESULT StartHelper(_In_ DWORD crashedPid, _In_z_ const wchar_t* exeName, _In_ DWORD exceptionCode, _In_ ULONGLONG callbackStart, _In_z_ const wchar_t* logPath) noexcept
    {
        wchar_t helperPath[MAX_PATH]{};
        DWORD len = GetModuleFileNameW(g_module, helperPath, _countof(helperPath));
        if (len == 0 || len >= _countof(helperPath))
        {
            return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
        }

        wchar_t* slash = wcsrchr(helperPath, L'\\');
        if (slash == nullptr)
        {
            return E_UNEXPECTED;
        }

        slash[1] = L'\0';
        if (wcscat_s(helperPath, L"WerCrashHelper.exe") != 0)
        {
            return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
        }

        wchar_t commandLine[1024]{};
        swprintf_s(commandLine, L"\"%s\" --pid %lu --exe \"%s\" --werfault-pid %lu --code 0x%08lX --callback-start %llu",
            helperPath, crashedPid, exeName, GetCurrentProcessId(), exceptionCode, callbackStart);

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        // Try to leave WerFault's job, if it has one, so the helper cannot be killed with it.
        DWORD flags = CREATE_BREAKAWAY_FROM_JOB | CREATE_DEFAULT_ERROR_MODE;
        BOOL started = CreateProcessW(helperPath, commandLine, nullptr, nullptr, FALSE, flags, nullptr, nullptr, &si, &pi);
        DWORD firstError = started ? ERROR_SUCCESS : GetLastError();

        if (!started)
        {
            flags = CREATE_DEFAULT_ERROR_MODE;
            started = CreateProcessW(helperPath, commandLine, nullptr, nullptr, FALSE, flags, nullptr, nullptr, &si, &pi);
        }

        if (!started)
        {
            DWORD error = GetLastError();
            ProtoAppendLine(logPath, L"event=helper_start_failed breakaway_error=%lu error=%lu path=\"%s\"", firstError, error, helperPath);
            return HRESULT_FROM_WIN32(error);
        }

        ProtoAppendLine(logPath, L"event=helper_started helper_pid=%lu breakaway=%d breakaway_error=%lu",
            pi.dwProcessId, (flags & CREATE_BREAKAWAY_FROM_JOB) ? 1 : 0, firstError);

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return S_OK;
    }
}

BOOL WINAPI DllMain(_In_ HINSTANCE instance, _In_ DWORD reason, _In_opt_ LPVOID /*reserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_module = instance;
        DisableThreadLibraryCalls(instance);

        if (ProtoLogFolder(instance, g_logFolder, _countof(g_logFolder)))
        {
            wchar_t loadLog[MAX_PATH]{};
            swprintf_s(loadLog, L"%smodule-loads.log", g_logFolder);
            ProtoAppendLine(loadLog, L"event=dll_loaded host_cmdline=\"%s\"", GetCommandLineW());
        }
    }

    return TRUE;
}

// The three exports below match PFN_WER_RUNTIME_EXCEPTION_EVENT, ..._SIGNATURE and
// ..._DEBUGGER_LAUNCH in werapi.h, and are exported undecorated through WerHelperModule.def.
extern "C" HRESULT WINAPI OutOfProcessExceptionEventCallback(
    _In_ PVOID pContext,
    _In_ const PWER_RUNTIME_EXCEPTION_INFORMATION pExceptionInformation,
    _Out_ BOOL* pbOwnershipClaimed,
    _Out_writes_(*pchSize) PWSTR pwszEventName,
    _Inout_ PDWORD pchSize,
    _Out_ PDWORD pdwSignatureCount)
{
    ULONGLONG start = ProtoNow();

    // Never claim the crash: WER must go on to write the LocalDumps file and its own report.
    if (pbOwnershipClaimed != nullptr)
    {
        *pbOwnershipClaimed = FALSE;
    }

    if (pdwSignatureCount != nullptr)
    {
        *pdwSignatureCount = 0;
    }

    UNREFERENCED_PARAMETER(pwszEventName);
    UNREFERENCED_PARAMETER(pchSize);

    if (pExceptionInformation == nullptr || pExceptionInformation->hProcess == nullptr)
    {
        return S_OK;
    }

    DWORD pid = GetProcessId(pExceptionInformation->hProcess);

    wchar_t logPath[MAX_PATH]{};
    LogPathFor(pid, logPath, _countof(logPath));

    wchar_t imagePath[MAX_PATH]{};
    DWORD imageLen = _countof(imagePath);
    if (!QueryFullProcessImageNameW(pExceptionInformation->hProcess, 0, imagePath, &imageLen))
    {
        wcscpy_s(imagePath, L"unknown.exe");
    }

    const wchar_t* exeName = wcsrchr(imagePath, L'\\');
    exeName = (exeName != nullptr) ? exeName + 1 : imagePath;

    const EXCEPTION_RECORD& record = pExceptionInformation->exceptionRecord;
    ULONG_PTR info0 = record.NumberParameters > 0 ? record.ExceptionInformation[0] : 0;

    wchar_t jobText[160]{};
    DescribeJob(jobText, _countof(jobText));

    wchar_t contextText[160]{};
    DescribeContext(pExceptionInformation->hProcess, pContext, contextText, _countof(contextText));

    wchar_t dumpPath[MAX_PATH]{};
    ExpectedDumpPath(exeName, pid, dumpPath, _countof(dumpPath));

    DWORD session = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &session);

    ProtoAppendLine(logPath, L"event=callback_enter crashed_pid=%lu exe=\"%s\" code=0x%08lX info0=0x%llX fatal=%d session=%lu %s %s dump_exists_at_enter=%d werfault_cmdline=\"%s\"",
        pid, exeName, record.ExceptionCode, static_cast<ULONGLONG>(info0), pExceptionInformation->bIsFatal ? 1 : 0,
        session, jobText, contextText, dumpPath[0] != L'\0' && DumpExists(dumpPath) ? 1 : 0, GetCommandLineW());

    HRESULT hr = StartHelper(pid, exeName, record.ExceptionCode, start, logPath);

    ULONGLONG end = ProtoNow();
    ProtoAppendLine(logPath, L"event=callback_exit hr=0x%08lX callback_us=%llu", static_cast<unsigned long>(hr), (end - start) / 10);

    // Returning S_OK with no claim tells WER to carry on to the next module and its own handling.
    return S_OK;
}

extern "C" HRESULT WINAPI OutOfProcessExceptionEventSignatureCallback(
    _In_ PVOID /*pContext*/,
    _In_ const PWER_RUNTIME_EXCEPTION_INFORMATION pExceptionInformation,
    _In_ DWORD dwIndex,
    _Out_writes_(*pchName) PWSTR /*pwszName*/,
    _Inout_ PDWORD /*pchName*/,
    _Out_writes_(*pchValue) PWSTR /*pwszValue*/,
    _Inout_ PDWORD /*pchValue*/)
{
    // WER calls this only for a module that claimed the crash. Log it in case that is not so.
    DWORD pid = (pExceptionInformation != nullptr && pExceptionInformation->hProcess != nullptr) ? GetProcessId(pExceptionInformation->hProcess) : 0;
    wchar_t logPath[MAX_PATH]{};
    LogPathFor(pid, logPath, _countof(logPath));
    ProtoAppendLine(logPath, L"event=signature_callback index=%lu", dwIndex);
    return E_NOTIMPL;
}

extern "C" HRESULT WINAPI OutOfProcessExceptionEventDebuggerLaunchCallback(
    _In_ PVOID /*pContext*/,
    _In_ const PWER_RUNTIME_EXCEPTION_INFORMATION pExceptionInformation,
    _Out_ PBOOL pbIsCustomDebugger,
    _Out_writes_(*pchDebuggerLaunch) PWSTR /*pwszDebuggerLaunch*/,
    _Inout_ PDWORD /*pchDebuggerLaunch*/,
    _Out_ PBOOL pbIsDebuggerAutolaunch)
{
    if (pbIsCustomDebugger != nullptr)
    {
        *pbIsCustomDebugger = FALSE;
    }

    if (pbIsDebuggerAutolaunch != nullptr)
    {
        *pbIsDebuggerAutolaunch = FALSE;
    }

    DWORD pid = (pExceptionInformation != nullptr && pExceptionInformation->hProcess != nullptr) ? GetProcessId(pExceptionInformation->hProcess) : 0;
    wchar_t logPath[MAX_PATH]{};
    LogPathFor(pid, logPath, _countof(logPath));
    ProtoAppendLine(logPath, L"event=debugger_launch_callback");
    return S_OK;
}
