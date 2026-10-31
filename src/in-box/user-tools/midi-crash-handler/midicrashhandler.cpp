// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The Windows Error Reporting (WER) runtime exception module for the Windows MIDI Services tools.
//
// When a tool that registered this DLL with WerRegisterRuntimeExceptionModule crashes, WER loads it
// into WerFault.exe, outside the crashed process. That happens for every kind of crash, including
// the fail fast that WIL, C++/WinRT and WinUI end in, which an in-process unhandled exception
// filter never sees. The DLL must also be listed under
// HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting\RuntimeExceptionHelperModules, which the
// Tools installer does.
//
// All it does is start MIDI Troubleshooting and Repair on its Crash dumps page, and tell it which
// process crashed and which WerFault.exe is writing the dump. It never claims the crash, so WER
// goes on to write the LocalDumps file and its own report as it would without this DLL.
//
// This runs inside WerFault.exe while it handles a crash, so it does as little as possible, never
// waits, and never throws. It uses nothing but Win32, and links the C runtime statically, so it
// loads without the Windows App SDK or the MIDI SDK.

#include <windows.h>
#include <werapi.h>

#include <cwchar>

namespace
{
    HMODULE g_module{ nullptr };

    constexpr wchar_t TroubleshooterExecutableName[] = L"miditroubleshooter.exe";

    // The exe name from a full image path.
    wchar_t const* FileNamePart(_In_z_ wchar_t const* path) noexcept
    {
        auto const slash = std::wcsrchr(path, L'\\');

        return slash != nullptr ? slash + 1 : path;
    }

    // An exe name goes into a quoted command line argument, so anything that could end the quote
    // or start another argument keeps it off the command line altogether.
    bool IsPlainFileName(_In_z_ wchar_t const* name) noexcept
    {
        if (*name == L'\0')
        {
            return false;
        }

        for (auto character = name; *character != L'\0'; ++character)
        {
            if (*character == L'"' || *character == L'\\' || *character == L'/' || *character < L' ')
            {
                return false;
            }
        }

        return true;
    }

    // The Troubleshooter is installed in the same folder as this DLL, and is built into the same
    // folder too.
    bool GetTroubleshooterPath(_Out_writes_(cch) wchar_t* path, _In_ DWORD cch) noexcept
    {
        path[0] = L'\0';

        auto const length = ::GetModuleFileNameW(g_module, path, cch);

        if (length == 0 || length >= cch)
        {
            path[0] = L'\0';
            return false;
        }

        auto const slash = std::wcsrchr(path, L'\\');

        if (slash == nullptr)
        {
            path[0] = L'\0';
            return false;
        }

        slash[1] = L'\0';

        if (wcscat_s(path, cch, TroubleshooterExecutableName) != 0)
        {
            path[0] = L'\0';
            return false;
        }

        return ::GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
    }

    void StartTroubleshooter(_In_ DWORD crashedProcessId, _In_z_ wchar_t const* crashedExecutable) noexcept
    {
        wchar_t troubleshooterPath[MAX_PATH]{};

        if (!GetTroubleshooterPath(troubleshooterPath, ARRAYSIZE(troubleshooterPath)))
        {
            return;
        }

        wchar_t commandLine[MAX_PATH * 2]{};

        if (swprintf_s(
            commandLine,
            L"\"%s\" --crashed-pid %lu --crashed-exe \"%s\" --werfault-pid %lu",
            troubleshooterPath,
            crashedProcessId,
            crashedExecutable,
            ::GetCurrentProcessId()) < 0)
        {
            return;
        }

        STARTUPINFOW startupInfo{};
        startupInfo.cb = sizeof(startupInfo);

        PROCESS_INFORMATION processInfo{};

        // Out of WerFault's job, if it is in one, so ending WerFault can't end the Troubleshooter.
        // A job that doesn't allow that refuses the flag, so it is tried again without.
        BOOL started = ::CreateProcessW(
            troubleshooterPath, commandLine, nullptr, nullptr, FALSE,
            CREATE_BREAKAWAY_FROM_JOB | CREATE_DEFAULT_ERROR_MODE,
            nullptr, nullptr, &startupInfo, &processInfo);

        if (!started)
        {
            started = ::CreateProcessW(
                troubleshooterPath, commandLine, nullptr, nullptr, FALSE,
                CREATE_DEFAULT_ERROR_MODE,
                nullptr, nullptr, &startupInfo, &processInfo);
        }

        if (started)
        {
            ::CloseHandle(processInfo.hThread);
            ::CloseHandle(processInfo.hProcess);
        }
    }
}

BOOL WINAPI DllMain(_In_ HINSTANCE instance, _In_ DWORD reason, _In_opt_ LPVOID reserved)
{
    UNREFERENCED_PARAMETER(reserved);

    if (reason == DLL_PROCESS_ATTACH)
    {
        g_module = instance;
        ::DisableThreadLibraryCalls(instance);
    }

    return TRUE;
}

// The three exports match PFN_WER_RUNTIME_EXCEPTION_EVENT, ..._SIGNATURE and ..._DEBUGGER_LAUNCH in
// werapi.h, and are exported undecorated through midicrashhandler.def.
extern "C" HRESULT WINAPI OutOfProcessExceptionEventCallback(
    _In_ PVOID context,
    _In_ const PWER_RUNTIME_EXCEPTION_INFORMATION exceptionInformation,
    _Out_ BOOL* ownershipClaimed,
    _Out_writes_(*eventNameSize) PWSTR eventName,
    _Inout_ PDWORD eventNameSize,
    _Out_ PDWORD signatureCount)
{
    UNREFERENCED_PARAMETER(context);
    UNREFERENCED_PARAMETER(eventName);
    UNREFERENCED_PARAMETER(eventNameSize);

    // Never claimed: WER has to carry on and write the LocalDumps file and its own report.
    if (ownershipClaimed != nullptr)
    {
        *ownershipClaimed = FALSE;
    }

    if (signatureCount != nullptr)
    {
        *signatureCount = 0;
    }

    if (exceptionInformation == nullptr || exceptionInformation->hProcess == nullptr)
    {
        return S_OK;
    }

    // Only for a crash a person can see. A tool running in session 0 has no desktop to open a
    // window on.
    DWORD sessionId{ 0 };

    if (!::ProcessIdToSessionId(::GetCurrentProcessId(), &sessionId) || sessionId == 0)
    {
        return S_OK;
    }

    auto const crashedProcessId = ::GetProcessId(exceptionInformation->hProcess);

    if (crashedProcessId == 0)
    {
        return S_OK;
    }

    wchar_t imagePath[MAX_PATH]{};
    DWORD imagePathLength{ ARRAYSIZE(imagePath) };

    if (!::QueryFullProcessImageNameW(exceptionInformation->hProcess, 0, imagePath, &imagePathLength))
    {
        return S_OK;
    }

    auto const executableName = FileNamePart(imagePath);

    // A Troubleshooter that crashes as it opens would otherwise open itself again for every crash.
    if (_wcsicmp(executableName, TroubleshooterExecutableName) == 0 || !IsPlainFileName(executableName))
    {
        return S_OK;
    }

    StartTroubleshooter(crashedProcessId, executableName);

    // Not claiming the crash tells WER to carry on with the next module and its own handling.
    return S_OK;
}

extern "C" HRESULT WINAPI OutOfProcessExceptionEventSignatureCallback(
    _In_ PVOID context,
    _In_ const PWER_RUNTIME_EXCEPTION_INFORMATION exceptionInformation,
    _In_ DWORD index,
    _Out_writes_(*nameSize) PWSTR name,
    _Inout_ PDWORD nameSize,
    _Out_writes_(*valueSize) PWSTR value,
    _Inout_ PDWORD valueSize)
{
    UNREFERENCED_PARAMETER(context);
    UNREFERENCED_PARAMETER(exceptionInformation);
    UNREFERENCED_PARAMETER(index);
    UNREFERENCED_PARAMETER(name);
    UNREFERENCED_PARAMETER(nameSize);
    UNREFERENCED_PARAMETER(value);
    UNREFERENCED_PARAMETER(valueSize);

    // WER only asks a module that claimed the crash, and this one never does.
    return E_NOTIMPL;
}

extern "C" HRESULT WINAPI OutOfProcessExceptionEventDebuggerLaunchCallback(
    _In_ PVOID context,
    _In_ const PWER_RUNTIME_EXCEPTION_INFORMATION exceptionInformation,
    _Out_ PBOOL isCustomDebugger,
    _Out_writes_(*debuggerLaunchSize) PWSTR debuggerLaunch,
    _Inout_ PDWORD debuggerLaunchSize,
    _Out_ PBOOL isDebuggerAutolaunch)
{
    UNREFERENCED_PARAMETER(context);
    UNREFERENCED_PARAMETER(exceptionInformation);
    UNREFERENCED_PARAMETER(debuggerLaunch);
    UNREFERENCED_PARAMETER(debuggerLaunchSize);

    if (isCustomDebugger != nullptr)
    {
        *isCustomDebugger = FALSE;
    }

    if (isDebuggerAutolaunch != nullptr)
    {
        *isDebuggerAutolaunch = FALSE;
    }

    return S_OK;
}
