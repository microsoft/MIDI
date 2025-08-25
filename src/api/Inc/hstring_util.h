
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef HSTRING_UTIL_H
#define HSTRING_UTIL_H

#include <string>
#include <cwctype>
//#include <hstring.h>
#include <winrt\windows.foundation.h>

namespace WindowsMidiServicesInternal
{
    inline void SafeCopyHStringToFixedArray(wchar_t* destArray, size_t destArrayCountIncludingTerminator, winrt::hstring source)
    {
        size_t length = min(source.size(), destArrayCountIncludingTerminator - 1);

        memset((void*)destArray, 0, destArrayCountIncludingTerminator * sizeof(wchar_t));

        memcpy(destArray, source.c_str(), length * sizeof(wchar_t));
    }


    inline winrt::hstring TruncateHStringCopy(_In_ winrt::hstring const& s, _In_ winrt::hstring::size_type newSize)
    {
        return winrt::hstring(s.c_str(), min(newSize, s.size()));
    }


    inline winrt::hstring TrimmedHStringCopy(_In_ winrt::hstring const& s)
    {
        std::wstring whitespace = L" \0\t\n\r";

        std::wstring ws{ s.c_str() };

        size_t index = ws.find_first_not_of(whitespace);
        if (index != std::wstring::npos)
        {
            ws.erase(0, index);
        }

        index = ws.find_last_not_of(whitespace);
        ws.resize(index + 1);

        return winrt::hstring{ ws.c_str() };
    }

    inline winrt::hstring ToUpperHStringCopy(_In_ winrt::hstring const& s)
    {
        std::wstring ws{ s };
        std::transform(ws.begin(), ws.end(), ws.begin(), std::towupper); 

        return winrt::hstring{ ws };
    }

    inline winrt::hstring ToLowerHStringCopy(_In_ winrt::hstring const& s)
    {
        std::wstring ws{ s };
        std::transform(ws.begin(), ws.end(), ws.begin(), std::towlower);

        return winrt::hstring{ ws };
    }

    inline winrt::hstring ToUpperTrimmedHStringCopy(_In_ winrt::hstring const& s)
    {
        return ToUpperHStringCopy(TrimmedHStringCopy(s));
    }

    inline winrt::hstring ToLowerTrimmedHStringCopy(_In_ winrt::hstring const& s)
    {
        return ToLowerHStringCopy(TrimmedHStringCopy(s));
    }


    // This is for the device instance id. Not to be confused with the interface id
    inline winrt::hstring NormalizeDeviceInstanceIdHStringCopy(_In_ winrt::hstring const& deviceInstanceId)
    {
        return ToUpperTrimmedHStringCopy(deviceInstanceId);
    }

    // This is for the endpoint device interface id (the long SWD id with the GUID)
    inline winrt::hstring NormalizeEndpointInterfaceIdHStringCopy(_In_ winrt::hstring const& endpointInterfaceId)
    {
        return ToLowerTrimmedHStringCopy(endpointInterfaceId);
    }


}


#endif