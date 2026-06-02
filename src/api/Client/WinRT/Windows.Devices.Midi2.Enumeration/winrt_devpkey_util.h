// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef WINRT_DEVPKEY_UTIL_H
#define WINRT_DEVPKEY_UTIL_H

#ifndef DEVPKEY_H_INCLUDED
#include <devpkey.h>
#endif

#include <format>

namespace WindowsMidiServicesInternal
{
    inline winrt::hstring DevPropKeyToWinRTPropertyHString(_In_ DEVPROPKEY const& key)
    {
        winrt::hstring formatted = std::format(
            L"{{{:08x}-{:04x}-{:04x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}}} {}", 
            key.fmtid.Data1, 
            key.fmtid.Data2, 
            key.fmtid.Data3, 
            key.fmtid.Data4[0], 
            key.fmtid.Data4[1], 
            key.fmtid.Data4[2], 
            key.fmtid.Data4[3], 
            key.fmtid.Data4[4], 
            key.fmtid.Data4[5], 
            key.fmtid.Data4[6], 
            key.fmtid.Data4[7], 
            key.pid
        ).c_str();

       // OutputDebugString(formatted.c_str());

        return formatted;
    }


}




#endif // WINRT_DEVPKEY_UTIL_H
