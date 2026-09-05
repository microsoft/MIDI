// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include <charconv>

#include "console_output.h"
#include "midi_formatting.h"
#include "word_parsing.h"

namespace midi2console
{
    namespace
    {
        bool TryParseWithBase(_In_ std::string_view text, _In_ int base, _Out_ uint32_t& value)
        {
            value = 0;

            if (text.empty())
            {
                return false;
            }

            uint64_t parsed{ 0 };

            auto const result = std::from_chars(text.data(), text.data() + text.size(), parsed, base);

            if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
            {
                return false;
            }

            if (parsed > UINT32_MAX)
            {
                return false;
            }

            value = static_cast<uint32_t>(parsed);

            return true;
        }
    }

    bool TryParseMidiWord(
        _In_ std::string_view text,
        _In_ std::string_view defaultFormat,
        _Out_ uint32_t& value)
    {
        value = 0;

        auto working = TrimCopy(text);

        if (working.empty())
        {
            return false;
        }

        auto const lowered = ToLowerCopy(working);

        if (lowered.size() > 2 && lowered.compare(0, 2, "0x") == 0)
        {
            return TryParseWithBase(std::string_view{ working }.substr(2), 16, value);
        }

        if (lowered.size() > 2 && lowered.compare(0, 2, "0b") == 0)
        {
            return TryParseWithBase(std::string_view{ working }.substr(2), 2, value);
        }

        // Trailing radix markers, as used in the sample capture files.
        if (lowered.size() > 1)
        {
            auto const suffix = lowered.back();
            auto const body = std::string_view{ working }.substr(0, working.size() - 1);

            if (suffix == 'h') return TryParseWithBase(body, 16, value);
            if (suffix == 'd') return TryParseWithBase(body, 10, value);

            // A trailing 'b' is ambiguous: it is a valid hex digit as well as a binary marker.
            // Only treat it as a marker when the remaining text cannot be anything but binary.
            if (suffix == 'b' && body.find_first_not_of("01") == std::string_view::npos)
            {
                return TryParseWithBase(body, 2, value);
            }
        }

        if (EqualsIgnoreCase(defaultFormat, "binary"))  return TryParseWithBase(working, 2, value);
        if (EqualsIgnoreCase(defaultFormat, "decimal")) return TryParseWithBase(working, 10, value);

        return TryParseWithBase(working, 16, value);
    }

    std::vector<std::string> SplitDelimitedLine(_In_ std::string_view line, _In_ std::string_view delimiter)
    {
        char separator{ '\0' };

        if (EqualsIgnoreCase(delimiter, "comma"))       separator = ',';
        else if (EqualsIgnoreCase(delimiter, "pipe"))   separator = '|';
        else if (EqualsIgnoreCase(delimiter, "tab"))    separator = '\t';
        else if (EqualsIgnoreCase(delimiter, "space"))  separator = ' ';
        else
        {
            // Auto: the first explicit separator present on the line wins, else whitespace.
            if (line.find(',') != std::string_view::npos)       separator = ',';
            else if (line.find('|') != std::string_view::npos)  separator = '|';
            else if (line.find('\t') != std::string_view::npos) separator = '\t';
            else                                                separator = ' ';
        }

        std::vector<std::string> fields;

        size_t start{ 0 };

        while (start <= line.size())
        {
            auto const end = line.find(separator, start);
            auto const piece = line.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);

            auto const trimmed = TrimCopy(piece);

            if (!trimmed.empty())
            {
                fields.push_back(trimmed);
            }

            if (end == std::string_view::npos)
            {
                break;
            }

            start = end + 1;
        }

        return fields;
    }

    std::string ExpandEnvironmentPath(_In_ std::string_view path)
    {
        auto const wide = FromUtf8(path);

        auto const required = ExpandEnvironmentStringsW(wide.c_str(), nullptr, 0);

        if (required == 0)
        {
            return std::string{ path };
        }

        std::wstring expanded(required, L'\0');

        if (ExpandEnvironmentStringsW(wide.c_str(), expanded.data(), required) == 0)
        {
            return std::string{ path };
        }

        while (!expanded.empty() && expanded.back() == L'\0')
        {
            expanded.pop_back();
        }

        return ToUtf8(expanded);
    }
}
