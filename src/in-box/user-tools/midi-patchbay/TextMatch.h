// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Pure: the unit tests compile files that use this, so it includes what it needs itself.

#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <string>
#include <string_view>

namespace midipatchbay
{
    // The same, ignoring case.
    inline bool SameText(_In_ std::wstring const& left, _In_ std::wstring const& right) noexcept
    {
        return ::CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
    }

    // Case-insensitive, the way a person expects a search box to work.
    inline bool ContainsText(_In_ std::wstring_view text, _In_ std::wstring_view search) noexcept
    {
        if (search.empty())
        {
            return true;
        }

        if (text.empty())
        {
            return false;
        }

        return ::FindNLSStringEx(
            LOCALE_NAME_USER_DEFAULT,
            FIND_FROMSTART | LINGUISTIC_IGNORECASE,
            text.data(), static_cast<int>(text.size()),
            search.data(), static_cast<int>(search.size()),
            nullptr, nullptr, nullptr, 0) >= 0;
    }

    // For a key that ignores case.
    inline std::wstring LowerCopy(_In_ std::wstring value) noexcept
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

        return value;
    }

    // What a hex digit is worth, or -1 for anything else.
    inline int HexDigit(_In_ wchar_t c) noexcept
    {
        if (c >= L'0' && c <= L'9') { return c - L'0'; }
        if (c >= L'a' && c <= L'f') { return c - L'a' + 10; }
        if (c >= L'A' && c <= L'F') { return c - L'A' + 10; }

        return -1;
    }
}
