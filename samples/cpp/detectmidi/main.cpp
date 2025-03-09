// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

// This sample shows how to detect Windows MIDI Services without using the SDK
// It's better to use the SDK functions, but this will work if you must check for
// system installation without the SDK being involved (for example, if you want
// to see if the service is installed before you prompt for an SDK download)

#pragma once

#include "pch.h"

struct __declspec(uuid("2BA15E4E-5417-4A66-85B8-2B2260EFBC84")) MidiSrvTransportPlaceholder : ::IUnknown
{ };


int __cdecl main(int argc, char* argv[])
{
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    wil::com_ptr_nothrow<IUnknown> servicePointer;

    auto hr = CoCreateInstance(
        __uuidof(MidiSrvTransportPlaceholder),
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&servicePointer)
    );

    if (SUCCEEDED(hr))
    {
        std::cout << "Windows MIDI Services appears to be installed" << std::endl;
        return 0;
    }
    else if (hr == REGDB_E_CLASSNOTREG)
    {
        std::cout << "Class not registered. Windows MIDI Services does NOT appear to be installed" << std::endl;
        return 1;
    }
    else
    {
        std::cout << "Other error. Unexpected." << std::endl;
        return 1;
    }

}

