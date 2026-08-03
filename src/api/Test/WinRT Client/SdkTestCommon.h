// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include <string>
#include <mmsystem.h>



inline void StartWinRTMTA()
{
    std::cout << "Initializing apartment (MTA)" << std::endl;
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
}

inline void StartWinRTSTA()
{
    std::cout << "Initializing apartment (STA)" << std::endl;
    winrt::init_apartment(winrt::apartment_type::single_threaded);
}


inline void ShutdownWinRT()
{
 
    std::cout << "Uninitializing apartment" << std::endl;
    winrt::uninit_apartment();
}



inline bool HStringsAreCaseInsensitiveEqual(const winrt::hstring& lhs, const winrt::hstring& rhs)
{
    return CompareStringOrdinal(lhs.c_str(), -1, rhs.c_str(), -1, TRUE) == CSTR_EQUAL;
}


// The set of characters which are valid in an SWD unique id. This mirrors
// CHARACTER_STRING_SWD_UNIQUE_ID_ALLOWED used internally by the SDK.
#define TEST_SWD_UNIQUE_ID_ALLOWED_CHARACTERS \
    L"-_ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"


// Returns the unique id as the loopback managers will clean it up before submitting
// it to the service: any character which isn't valid in an SWD unique id is removed,
// and the result is truncated to the maximum allowed length.
inline std::wstring ExpectedCleanedUniqueId(_In_ std::wstring const& uniqueId)
{
    const std::wstring allowedCharacters{ TEST_SWD_UNIQUE_ID_ALLOWED_CHARACTERS };

    std::wstring result{};

    for (auto const& ch : uniqueId)
    {
        if (allowedCharacters.find(ch) != std::wstring::npos)
        {
            result += ch;
        }
    }

    if (result.length() > MAXPNAMELEN - 1)
    {
        result = result.substr(0, MAXPNAMELEN - 1);
    }

    return result;
}


// Returns true if the supplied unique id contains only characters which are
// valid in an SWD unique id.
inline bool UniqueIdContainsOnlyValidCharacters(_In_ std::wstring const& uniqueId)
{
    const std::wstring allowedCharacters{ TEST_SWD_UNIQUE_ID_ALLOWED_CHARACTERS };

    return uniqueId.find_first_not_of(allowedCharacters) == std::wstring::npos;
}


// A unique id which deliberately contains spaces, symbols, and punctuation, none of
// which are valid in an SWD unique id. The supplied prefix should contain only valid
// characters so that the cleaned result is still unique and non-empty.
inline std::wstring MakeGarbageUniqueId(_In_ std::wstring const& validPrefix)
{
    return validPrefix + L" !@#$%^&*(){}[]<>?/\\|+=.,;:'\"~`";
}

