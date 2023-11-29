// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once


namespace Windows::Devices::Midi2::Internal
{
    inline winrt::hstring TrimmedHStringCopy(_In_ winrt::hstring s)
    {
        std::wstring ws{ s };

        size_t index = ws.find_first_not_of(' ');
        if (index != std::wstring::npos)
        {
            ws.erase(0, index);
        }

        index = ws.find_last_not_of(' ');
        ws.resize(index + 1);

        return winrt::hstring{ ws };
    }

    inline winrt::hstring ToUpperHStringCopy(_In_ winrt::hstring s)
    {
        std::wstring ws{ s };
        std::transform(ws.begin(), ws.end(), ws.begin(), std::towupper); 

        return winrt::hstring{ ws };
    }

    inline winrt::hstring ToUpperTrimmedHStringCopy(_In_ winrt::hstring s)
    {
        return ToUpperHStringCopy(TrimmedHStringCopy(s));
    }

    inline winrt::hstring ToLowerHStringCopy(_In_ winrt::hstring s)
    {
        std::wstring ws{ s };
        std::transform(ws.begin(), ws.end(), ws.begin(), std::towlower);

        return winrt::hstring{ ws };
    }


}
