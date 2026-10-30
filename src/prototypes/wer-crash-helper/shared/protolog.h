// PROTOTYPE. Logging shared by the WER crash helper prototype. See ..\README.md.
//
// Every line starts with "t=<FILETIME>" from GetSystemTimePreciseAsFileTime, so lines written by
// the crashing app, WerFault.exe and the helper can be put on one timeline. The value is in 100 ns
// units, and the run script turns differences into milliseconds.

#pragma once

#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

inline ULONGLONG ProtoNow() noexcept
{
    FILETIME ft{};
    GetSystemTimePreciseAsFileTime(&ft);
    return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

// Writes the logs folder next to the given module into folder (with a trailing backslash) and
// creates it. Returns false if the path does not fit.
inline bool ProtoLogFolder(_In_opt_ HMODULE module, _Out_writes_(cch) wchar_t* folder, _In_ size_t cch) noexcept
{
    if (cch == 0)
    {
        return false;
    }

    folder[0] = L'\0';

    DWORD len = GetModuleFileNameW(module, folder, static_cast<DWORD>(cch));
    if (len == 0 || len >= cch)
    {
        folder[0] = L'\0';
        return false;
    }

    wchar_t* slash = wcsrchr(folder, L'\\');
    if (slash == nullptr)
    {
        folder[0] = L'\0';
        return false;
    }

    slash[1] = L'\0';

    if (wcscat_s(folder, cch, L"logs\\") != 0)
    {
        folder[0] = L'\0';
        return false;
    }

    CreateDirectoryW(folder, nullptr);
    return true;
}

// Appends one line, as UTF-8, to the file. Opens and closes the file every time so that several
// processes can append to the same file and nothing is lost if this process is killed.
inline void ProtoAppendLine(_In_z_ const wchar_t* path, _In_z_ _Printf_format_string_ const wchar_t* format, ...) noexcept
{
    wchar_t line[2048]{};
    int prefix = swprintf_s(line, L"t=%llu pid=%lu ", ProtoNow(), GetCurrentProcessId());
    if (prefix < 0)
    {
        return;
    }

    va_list args;
    va_start(args, format);
    int body = _vsnwprintf_s(line + prefix, _countof(line) - prefix, _TRUNCATE, format, args);
    va_end(args);

    size_t used = (body < 0) ? wcslen(line) : static_cast<size_t>(prefix + body);
    if (used + 2 < _countof(line))
    {
        line[used++] = L'\r';
        line[used++] = L'\n';
        line[used] = L'\0';
    }

    char utf8[4096]{};
    int bytes = WideCharToMultiByte(CP_UTF8, 0, line, static_cast<int>(used), utf8, sizeof(utf8), nullptr, nullptr);
    if (bytes <= 0)
    {
        return;
    }

    HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }

    DWORD written = 0;
    WriteFile(file, utf8, static_cast<DWORD>(bytes), &written, nullptr);
    CloseHandle(file);
}

// The context block the crashing app passes to WerRegisterRuntimeExceptionModule. WER hands the
// address back to the module, which reads it out of the crashed process with ReadProcessMemory.
// This proves a real app could pass something like its app id or a session id this way.
struct ProtoWerContext
{
    DWORD Magic;        // 'MIDI'
    DWORD Version;
    wchar_t AppId[64];
};

constexpr DWORD ProtoWerContextMagic = 0x4944494D; // "MIDI" as little-endian bytes
