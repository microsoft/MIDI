// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef CONSOLE_TOOLS_SHARED_H
#define CONSOLE_TOOLS_SHARED_H

#include <conio.h>
#include <stdio.h>
#include <string>

// requires wil
#include <wil\result_macros.h>
#include <wil\tracelogging.h>
#include <wil\registry.h>
#include <wil\registry_helpers.h>

#include <io.h>
#include <fcntl.h>

#define RETURN_SUCCESS                      0
#define RETURN_INSUFFICIENT_PERMISSIONS     1
#define RETURN_USER_ABORTED                 2
#define RETURN_INVALID_MODE                 3
#define RETURN_ERROR_SETTING_CONSOLE_MODE   4

#define RETURN_REGISTRY_READ_FAILED         10
#define RETURN_REGISTRY_WRITE_FAILED        11

#define RETURN_INVALID_PORT_NUMBER          20
#define RETURN_UNABLE_TO_OPEN_PORT          21

#define RETURN_GENERAL_FAILURE              99

// requires the fmt library vcpkg
#include <fmt/base.h>
#include <fmt/xchar.h>
#include <fmt/format.h>
#include <fmt/color.h>

#define LINE_LENGTH 79


const auto infoTextStyle = fmt::fg(fmt::color::steel_blue);
const auto errorTextStyle = fmt::fg(fmt::color::pink);
const auto normalTextStyle = fmt::fg(fmt::color::light_gray);
const auto separatorTextStyle = fmt::fg(fmt::color::gray);
const auto darkLabelTextStyle = fmt::fg(fmt::color::gray);

const auto highlightTextStyle = fmt::fg(fmt::color::aqua);
const auto highlight2TextStyle = fmt::fg(fmt::color::light_yellow);
const auto promptTextStyle = fmt::fg(fmt::color::light_green);
const auto warningTextStyle = fmt::fg(fmt::color::light_yellow);

const auto fieldLabelTextStyle = fmt::fg(fmt::color::dark_golden_rod);
const auto fieldValueTextStyle = fmt::fg(fmt::color::light_gray);

const auto portNumberFieldValueTextStyle = fmt::fg(fmt::color::golden_rod);
const auto filterIdFieldValueTextStyle = fmt::fg(fmt::color::golden_rod);
const auto entityIdentifierFieldValueTextStyle = fmt::fg(fmt::color::steel_blue);
const auto entityNameFieldValueTextStyle = fmt::fg(fmt::color::aquamarine);


// Styling is emitted as ANSI escape sequences. Those are fine on a console but land in a
// redirected file as literal escape codes, so they are suppressed when output is not a console.
// Set once by TrySetConsoleTextMode.
inline bool& ConsoleStylingEnabled()
{
    static bool enabled{ true };

    return enabled;
}

// Use this in place of fmt::styled everywhere in the console tools. An empty text_style makes
// fmt emit no escape sequences at all, so suppression costs nothing at the call site and none
// of the column widths or multi-styled lines have to change.
template <typename TValue>
inline auto Styled(TValue const& value, fmt::text_style const& style)
{
    return fmt::styled(value, ConsoleStylingEnabled() ? style : fmt::text_style{});
}

#define KEY_ESCAPE 0x1B
#define KEY_SPACE  0x20

const std::wstring drivers32HklmKey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Drivers32";
const std::wstring drivers32WOWHklmKey = L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows NT\\CurrentVersion\\Drivers32";
const std::wstring legacyMidiRegistryValueName = L"UseLegacyMidi";

enum RegistryApiMode
{
    UseMidiServices = 0,
    UseLegacyMidi = 1,
    UseHybridMidi = 2
};

inline RegistryApiMode GetRegistryApiMode()
{
    wil::unique_hkey key;

    auto openResult = RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        drivers32HklmKey.c_str(),
        0,
        KEY_QUERY_VALUE,
        key.put());

    if (openResult != ERROR_SUCCESS)
    {
        // drivers32 should always be there, but ..
        // default when not specified is to use Midi Services
        return RegistryApiMode::UseMidiServices;
    }

    DWORD value{ 0 };
    DWORD valueSize{ sizeof(value) };
    DWORD valueType{ 0 };

    auto queryResult = RegQueryValueExW(
        key.get(),
        legacyMidiRegistryValueName.c_str(),
        nullptr,
        &valueType,
        reinterpret_cast<LPBYTE>(&value),
        &valueSize);

    if (queryResult == ERROR_FILE_NOT_FOUND)
    {
        // not set, so the default mode is in effect
        return RegistryApiMode::UseMidiServices;
    }

    if (queryResult != ERROR_SUCCESS || valueType != REG_DWORD)
    {
        return RegistryApiMode::UseMidiServices;
    }

    if (value < 0 || value > 2)
    {
        return RegistryApiMode::UseMidiServices;
    }
    else
    {
        return static_cast<RegistryApiMode>(value);
    }
}



inline bool TrySetConsoleTextMode()
{
    // _O_U16TEXT reaches a real console through WriteConsoleW, with no code page involved, but
    // redirected output then lands in the file as BOM-less UTF-16LE which many tools misread.
    DWORD consoleMode{ 0 };
    bool const writingToConsole = GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &consoleMode) != 0;

    ConsoleStylingEnabled() = writingToConsole;

    auto setModeResult = _setmode(_fileno(stdout), writingToConsole ? _O_U16TEXT : _O_U8TEXT);

    if (setModeResult == -1)
    {
        perror("Unable to set stdout to Unicode mode. ");
        return false;
    }

    return true;
}


inline void WriteInfoLine(_In_ std::wstring const& info)
{
    fmt::println(L"{}", Styled(info, infoTextStyle));
}

inline void WriteNormalLine(_In_ std::wstring const& text)
{
    fmt::println(L"{}", Styled(text, normalTextStyle));
}

inline void WriteHighlightLine(_In_ std::wstring const& text)
{
    fmt::println(L"{}", Styled(text, highlightTextStyle));
}

inline void WriteHighlightLine2(_In_ std::wstring const& text)
{
    fmt::println(L"{}", Styled(text, highlightTextStyle));
}


inline void WriteWarningLine(_In_ std::wstring const& text)
{
    fmt::println(L"{}", Styled(text, warningTextStyle));
}

inline void WritePromptLine(_In_ std::wstring const& text)
{
    fmt::println(L"{}", Styled(text, promptTextStyle));
}

inline void WriteErrorLine(_In_ std::wstring const& error)
{
    fmt::println(L"{}", Styled(error, errorTextStyle));
}

inline void WriteDoubleSeparatorLine()
{
    fmt::println(L"{}", Styled(std::wstring(LINE_LENGTH, L'='), separatorTextStyle));
}

inline void WriteSingleSeparatorLine()
{
    fmt::println(L"{}", Styled(std::wstring(LINE_LENGTH, L'-'), separatorTextStyle));
}

inline void WriteBlankLine()
{
    fmt::println(L"");
}


inline bool CheckForAdminPermissions()
{
    bool elevated{ false };

    HANDLE tokenHandle{ nullptr };
    TOKEN_ELEVATION elevation;
    DWORD tokenInfoSize{ 0 };

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tokenHandle))
    {
        if (GetTokenInformation(tokenHandle, TokenElevation, &elevation, sizeof(elevation), &tokenInfoSize))
        {
            elevated = elevation.TokenIsElevated;
        }
    }

    if (tokenHandle)
    {
        CloseHandle(tokenHandle);
    }


    return elevated;
}


// Discards anything already typed (or auto-repeated from a held-down key) so that a
// keystroke intended for an earlier prompt can never be consumed by a later one.

inline void DrainKeyboardInput()
{
    // the CRT keeps its own pushback buffer for _getch, so drain that first
    while (_kbhit())
    {
        (void)(_getch());
    }

    auto consoleInput = GetStdHandle(STD_INPUT_HANDLE);

    if (consoleInput != INVALID_HANDLE_VALUE && consoleInput != nullptr)
    {
        FlushConsoleInputBuffer(consoleInput);
    }
}



#define KEY_UPPERCASE_Y     0x59
#define KEY_LOWERCASE_Y     0x79

inline bool PromptForYes(_In_ std::wstring const& promptText)
{
    DrainKeyboardInput();

    WritePromptLine(promptText);

    auto ch = _getch();

    if (ch == 0 || ch == 0xE0)
    {
        // function or arrow key. Consume the second half of the sequence so it
        // isn't left behind to be read as an answer to something else
        (void)_getch();

        return false;
    }

    return (ch == KEY_UPPERCASE_Y || ch == KEY_LOWERCASE_Y);
}


#endif