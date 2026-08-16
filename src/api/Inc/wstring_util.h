
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef WSTRING_UTIL_H
#define WSTRING_UTIL_H

#include <string>
#include <cwctype>
#include <algorithm>

#include "Feature_Servicing_MIDI2StringCharacterSets.h"

// these specifically do not include whitespace characters
#define CHARACTER_STRING_ALPHAUPPER             L"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define CHARACTER_STRING_ALPHALOWER             L"abcdefghijklmnopqrstuvwxyz"
#define CHARACTER_STRING_DIGITS                 L"0123456789"
#define CHARACTER_STRING_ALPHANUMERIC           CHARACTER_STRING_ALPHAUPPER CHARACTER_STRING_ALPHALOWER CHARACTER_STRING_DIGITS
#define CHARACTER_STRING_SWD_UNIQUE_ID_ALLOWED  L"-_" CHARACTER_STRING_ALPHANUMERIC

// Pre-fix character sets, kept only so a servicing rollback restores the previous behavior
// exactly. The uppercase set was missing X and repeated Z, and the alphanumeric set listed the
// lowercase letters twice instead of including the uppercase ones.
// REMOVE THESE, and the branches which use them, when Feature_Servicing_MIDI2StringCharacterSets
// is removed.
#define CHARACTER_STRING_ALPHAUPPER_LEGACY              L"ABCDEFGHIJKLMNOPQRSTUVWZYZ"
#define CHARACTER_STRING_ALPHANUMERIC_LEGACY            CHARACTER_STRING_ALPHALOWER CHARACTER_STRING_ALPHALOWER CHARACTER_STRING_DIGITS
#define CHARACTER_STRING_SWD_UNIQUE_ID_ALLOWED_LEGACY   L"-_" CHARACTER_STRING_ALPHAUPPER_LEGACY CHARACTER_STRING_ALPHALOWER CHARACTER_STRING_DIGITS

namespace WindowsMidiServicesInternal
{
    inline std::wstring RemoveDisallowedStringCharacters(_In_ std::wstring const& stringToClean, _In_ std::wstring const& allowedCharacters)
    {
        std::wstring result{ stringToClean };

        std::erase_if(result, [&](auto& ch)
            {
                return !std::any_of(allowedCharacters.begin(), allowedCharacters.end(), [&](auto& allowed) { return ch == allowed; });
            });

        return result;
    }

    inline std::wstring RemoveInvalidSWDUniqueIdCharacters(_In_ std::wstring const& uniqueId)
    {
        if (Feature_Servicing_MIDI2StringCharacterSets::IsEnabled())
        {
            return RemoveDisallowedStringCharacters(uniqueId, CHARACTER_STRING_SWD_UNIQUE_ID_ALLOWED);
        }
        else
        {
            return RemoveDisallowedStringCharacters(uniqueId, CHARACTER_STRING_SWD_UNIQUE_ID_ALLOWED_LEGACY);
        }
    }

    inline std::wstring RemoveNonAlphaNumericCharacters(_In_ std::wstring const& source)
    {
        if (Feature_Servicing_MIDI2StringCharacterSets::IsEnabled())
        {
            return RemoveDisallowedStringCharacters(source, CHARACTER_STRING_ALPHANUMERIC);
        }
        else
        {
            return RemoveDisallowedStringCharacters(source, CHARACTER_STRING_ALPHANUMERIC_LEGACY);
        }
    }


    // assumes the string passed in has already been trimmed of leading/trailing whitespace
    inline bool StringEndsWithSpecifiedNumber(
        _In_ std::wstring const& s,
        _In_ uint16_t const numberToFind)
    {
        // TODO: Check to see if there's a number at the end of the string (take the last numeric values) and compare *whole* value

        // Need to ensure "YAMAHA DX-MULTI 12" returns true only for 12, not for 2, 
        // for example. That's why wstring.ends_with is insufficient
        // also needs to support padded numbers "Port 02" should match the value "2"

        auto lastIndex = s.find_last_not_of(L"0123456789");

        if (lastIndex != std::wstring::npos)
        {
            if (lastIndex + 1 <= s.length())
            {
                auto trailingNumber = s.substr(lastIndex + 1);

                int i = _wtoi(trailingNumber.c_str());

                // step 3: compare to the provided number
                if (i == numberToFind)
                {
                    return true;
                }
            }
        }

        return false;
    }

    inline void SafeCopyWStringToFixedArray(wchar_t* destArray, size_t destArrayCountIncludingTerminator, std::wstring source)
    {
        size_t length = min(source.size(), destArrayCountIncludingTerminator - 1);

        memset((void*)destArray, 0, destArrayCountIncludingTerminator * sizeof(wchar_t));

        memcpy(destArray, source.c_str(), length * sizeof(wchar_t));
    }

    inline void InPlaceToUpper(_Inout_ std::wstring &s)
    {
        std::transform(s.begin(), s.end(), s.begin(), towupper);
    }

    inline void InPlaceToLower(_Inout_ std::wstring &s)
    {
        std::transform(s.begin(), s.end(), s.begin(), towlower);
    }

    inline void InPlaceTrim(_Inout_ std::wstring& ws)
    {
        std::wstring whitespace = L" \0\t\n\r";

        size_t index = ws.find_first_not_of(whitespace);
        if (index != std::wstring::npos)
        {
            ws.erase(0, index);
        }

        index = ws.find_last_not_of(whitespace);
        ws.resize(index + 1);
    }

    inline std::wstring TrimmedWStringCopy(_In_ std::wstring ws)
    {
        std::wstring newString{ ws };

        InPlaceTrim(newString);

        return newString;
    }

    inline std::wstring ToUpperWStringCopy(_In_ std::wstring s)
    {
        std::wstring ws{ s };
        InPlaceToUpper(ws);

        return ws;
    }

    inline std::wstring ToLowerWStringCopy(_In_ std::wstring s)
    {
        std::wstring ws{ s };
        InPlaceToLower(ws);

        return ws;
    }


    inline std::wstring ToUpperTrimmedWStringCopy(_In_ std::wstring s)
    {
        return ToUpperWStringCopy(TrimmedWStringCopy(s));
    }

    inline std::wstring ToLowerTrimmedWStringCopy(_In_ std::wstring s)
    {
        return ToLowerWStringCopy(TrimmedWStringCopy(s));
    }


    // The MIDI 2.0 specification states its UMP Endpoint Name and Product Instance Id limits in
    // UTF-8 BYTES, not characters. A name well under the character limit can still be over the
    // byte limit once encoded, so these must never be measured with wstring::length().

    inline std::string Utf8FromWString(_In_ std::wstring const& value) noexcept
    {
        if (value.empty())
        {
            return {};
        }

        auto required = ::WideCharToMultiByte(
            CP_UTF8, 0, value.c_str(), static_cast<int>(value.length()), nullptr, 0, nullptr, nullptr);

        if (required <= 0)
        {
            return {};
        }

        std::string utf8;
        utf8.resize(static_cast<size_t>(required));

        auto written = ::WideCharToMultiByte(
            CP_UTF8, 0, value.c_str(), static_cast<int>(value.length()), utf8.data(), required, nullptr, nullptr);

        if (written <= 0)
        {
            return {};
        }

        utf8.resize(static_cast<size_t>(written));

        return utf8;
    }

    inline std::wstring WStringFromUtf8(_In_ std::string const& value) noexcept
    {
        if (value.empty())
        {
            return {};
        }

        auto required = ::MultiByteToWideChar(
            CP_UTF8, 0, value.data(), static_cast<int>(value.length()), nullptr, 0);

        if (required <= 0)
        {
            return {};
        }

        std::wstring wide;
        wide.resize(static_cast<size_t>(required));

        auto written = ::MultiByteToWideChar(
            CP_UTF8, 0, value.data(), static_cast<int>(value.length()), wide.data(), required);

        if (written <= 0)
        {
            return {};
        }

        wide.resize(static_cast<size_t>(written));

        return wide;
    }

    // Number of bytes this string occupies when encoded as UTF-8. This is what the
    // specification's limits are expressed in.
    inline size_t Utf8ByteCount(_In_ std::wstring const& value) noexcept
    {
        return Utf8FromWString(value).length();
    }

    inline bool ExceedsUtf8ByteCount(_In_ std::wstring const& value, _In_ size_t const maxByteCount) noexcept
    {
        return Utf8ByteCount(value) > maxByteCount;
    }

    // Shortens the string so its UTF-8 encoding fits within maxByteCount, cutting only on a
    // character boundary. Returns the string unchanged when it already fits.
    inline std::wstring TruncateToUtf8ByteCount(_In_ std::wstring const& value, _In_ size_t const maxByteCount) noexcept
    {
        if (maxByteCount == 0)
        {
            return {};
        }

        auto utf8 = Utf8FromWString(value);

        if (utf8.length() <= maxByteCount)
        {
            return value;
        }

        // maxByteCount is the first byte that has to go. If it is a continuation byte then the
        // cut lands inside a character, so walk back to that character's lead byte. Checking the
        // first DROPPED byte rather than the last kept one is what keeps a character which ends
        // exactly on the limit intact.
        size_t cut = maxByteCount;

        while (cut > 0 && (static_cast<unsigned char>(utf8[cut]) & 0xC0) == 0x80)
        {
            cut--;
        }

        utf8.resize(cut);

        return WStringFromUtf8(utf8);
    }


    inline bool WStringEndsWidth(_In_ std::wstring source, _In_ std::wstring ending)
    {
        if (ending.size() > source.size())
        {
            return false;
        }
        else
        {
            return std::equal(ending.rbegin(), ending.rend(), source.rbegin());
        }
    }



    // This is just to convert all GUIDs to the same case. It does
    // not add or remove opening / closing brackets
    inline std::wstring NormalizeGuidStringCopy(_In_ std::wstring guidString)
    {
        return ToUpperTrimmedWStringCopy(guidString);
    }

    // This produces a GUID with uppercase letters and enclosing braces
    inline std::wstring GuidToString(_In_ GUID guid)
    {
        LPOLESTR str;
        if (SUCCEEDED(StringFromCLSID(guid, &str)))
        {
            std::wstring guidString{ str };

            ::CoTaskMemFree(str);

            return guidString;
        }
        else
        {
            return L"";
        }
    }

    inline std::wstring GuidToHexDigitsOnlyString(_In_ GUID guid)
    {
        LPOLESTR str;
        if (SUCCEEDED(StringFromCLSID(guid, &str)))
        {
            std::wstring guidString{ str };

            ::CoTaskMemFree(str);

            return RemoveDisallowedStringCharacters(guidString, CHARACTER_STRING_ALPHALOWER CHARACTER_STRING_ALPHAUPPER CHARACTER_STRING_DIGITS);
        }
        else
        {
            return L"";
        }
    }


    // Expects enclosing braces: {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
    inline GUID StringToGuid(_In_ std::wstring value)
    {
        GUID g;

        if (SUCCEEDED(CLSIDFromString(value.c_str(), &g)))
        {
            return g;
        }
        else
        {
            // return the empty GUID. This is a bit dumb, honestly
            return g;
        }
        
    }


    // Unlike StringToGuid above, reports failure instead of returning an uninitialized value,
    // and accepts the unbraced form as well.
    inline bool TryParseGuidString(_In_ std::wstring const& value, _Out_ GUID& result)
    {
        result = GUID{};

        if (value.empty())
        {
            return false;
        }

        std::wstring braced{ value };

        if (braced.front() != L'{')
        {
            braced = L"{" + braced + L"}";
        }

        GUID parsed{};

        if (FAILED(CLSIDFromString(braced.c_str(), &parsed)))
        {
            return false;
        }

        result = parsed;

        return true;
    }



    inline std::wstring SystemTimeToDateTimeString(SYSTEMTIME const& time)
    {
        std::wstring dateBuffer;
        dateBuffer.reserve(50);

        std::wstring timeBuffer;
        timeBuffer.reserve(50);

        GetDateFormatEx(
            LOCALE_NAME_SYSTEM_DEFAULT,
            DATE_LONGDATE,
            &time,
            NULL,
            dateBuffer.data(),
            (int)dateBuffer.capacity(),
            NULL
        );

        GetTimeFormatEx(
            LOCALE_NAME_SYSTEM_DEFAULT,
            0,
            &time,
            NULL,
            timeBuffer.data(),
            (int)timeBuffer.capacity()
        );

        std::wstring dateTime = dateBuffer + L" " + timeBuffer;

        return dateTime;
    }





}


#endif