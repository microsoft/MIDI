// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include <iostream>

#include <ftxui/screen/string.hpp>

#include "console_output.h"

namespace midi2console
{
    namespace
    {
        bool g_stylingEnabled{ false };
        bool g_writingToConsole{ false };
    }

    bool StylingEnabled()
    {
        return g_stylingEnabled;
    }

    bool InitializeConsole()
    {
        auto const stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);

        DWORD consoleMode{ 0 };
        g_writingToConsole = GetConsoleMode(stdoutHandle, &consoleMode) != 0;

        g_stylingEnabled = g_writingToConsole;

        if (g_writingToConsole)
        {
            // Without this the escape sequences fmt and FTXUI emit are printed literally.
            SetConsoleMode(stdoutHandle, consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }

        // UTF-8 on both directions. FTXUI reads and writes narrow UTF-8, and the emoji in the
        // endpoint picker need it regardless.
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);

        return true;
    }

    std::string ToUtf8(_In_ std::wstring_view value)
    {
        if (value.empty())
        {
            return {};
        }

        auto const byteCount = WideCharToMultiByte(
            CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);

        if (byteCount <= 0)
        {
            return {};
        }

        std::string result(static_cast<size_t>(byteCount), '\0');

        WideCharToMultiByte(
            CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), byteCount, nullptr, nullptr);

        return result;
    }

    std::string ToUtf8(_In_ winrt::hstring const& value)
    {
        return ToUtf8(std::wstring_view{ value });
    }

    std::wstring FromUtf8(_In_ std::string_view value)
    {
        if (value.empty())
        {
            return {};
        }

        auto const charCount = MultiByteToWideChar(
            CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);

        if (charCount <= 0)
        {
            return {};
        }

        std::wstring result(static_cast<size_t>(charCount), L'\0');

        MultiByteToWideChar(
            CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), charCount);

        return result;
    }

    size_t DisplayWidth(_In_ std::string_view value)
    {
        // FTXUI already carries the Unicode width tables, so we do not need our own.
        auto const width = ftxui::string_width(std::string{ value });

        return width < 0 ? 0 : static_cast<size_t>(width);
    }

    std::string PadRightToWidth(_In_ std::string_view value, _In_ size_t width)
    {
        std::string result{ value };

        auto const current = DisplayWidth(value);

        if (current < width)
        {
            result.append(width - current, ' ');
        }

        return result;
    }

    void WriteLine(_In_ std::string_view text)
    {
        // On a console fmt writes through WriteConsoleW, bypassing the CRT, so the carriage
        // return has to be explicit. When redirected it goes through the CRT in text mode,
        // which turns a lone \n into CRLF already - emitting CRLF there would give CR CR LF.
        if (g_writingToConsole)
        {
            fmt::print("{}\r\n", text);
        }
        else
        {
            fmt::print("{}\n", text);
        }
    }

    void WriteBlankLine()
    {
        if (g_writingToConsole)
        {
            fmt::print("\r\n");
        }
        else
        {
            fmt::print("\n");
        }
    }

    void WriteInfoLine(_In_ std::string_view text)
    {
        WriteLine(fmt::format("{}", Styled(text, infoTextStyle)));
    }

    void WriteNormalLine(_In_ std::string_view text)
    {
        WriteLine(fmt::format("{}", Styled(text, normalTextStyle)));
    }

    void WriteErrorLine(_In_ std::string_view text)
    {
        WriteLine(fmt::format("{}", Styled(text, errorTextStyle)));
    }

    void WriteWarningLine(_In_ std::string_view text)
    {
        WriteLine(fmt::format("{}", Styled(text, warningTextStyle)));
    }

    void WriteSuccessLine(_In_ std::string_view text)
    {
        WriteLine(fmt::format("{}", Styled(text, successTextStyle)));
    }

    void SetConsoleTitleText(_In_ std::string_view title)
    {
        SetConsoleTitleW(FromUtf8(title).c_str());
    }

    void ErasePopupFrame(_In_ int frameHeight)
    {
        if (!g_writingToConsole || frameHeight <= 0)
        {
            return;
        }

        std::fflush(stdout);
        std::cout.flush();

        // Cursor-relative, deliberately. Absolute buffer coordinates cannot reach these rows:
        // under a conpty the buffer is only as tall as the window, so once output reaches the
        // bottom the cursor stays pinned there and the frame has already scrolled out of it.
        // The rows are still on the visible screen, which is what CUU and ED address.
        fmt::print("\x1b[{}A\r\x1b[0J", frameHeight);

        std::fflush(stdout);
    }
}
