// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

struct MidiVirtualDeviceEndpointEntry
{
    std::wstring VirtualEndpointAssociationId{ L"" };             // how the config entries associate endpoints. Typically a GUID
    std::wstring BaseEndpointName{ L"" };
    std::wstring Description{ L"" };
    std::wstring ShortUniqueId{ L"" };
    std::wstring Manufacturer{ L"" };

    std::wstring CreatedDeviceEndpointId{ L"" };              // the device interface id
    std::wstring CreatedShortDeviceInstanceId{ L"" };

    std::wstring CreatedClientEndpointId{ L"" };
    std::wstring CreatedShortClientInstanceId{ L"" };

    bool UMPOnly{ false };

    wil::com_ptr_nothrow<CMidi2VirtualMidiBidi> MidiDeviceBidi{ nullptr };
    //wil::com_ptr_nothrow<IMidiCallback> MidiDeviceCallback{ nullptr };

    wil::com_ptr_nothrow<CMidi2VirtualMidiBidi> MidiClientBidi{ nullptr };
    //wil::com_ptr_nothrow<IMidiCallback> MidiClientCallback{ nullptr };


    //   std::vector<wil::com_ptr_nothrow<CMidi2VirtualMidiBidi>> MidiClientConnections{ };

    ~MidiVirtualDeviceEndpointEntry()
    {
        //MidiClientConnections.clear();

        if (MidiDeviceBidi)
        {
            MidiDeviceBidi.reset();
        }

        //if (MidiDeviceCallback)
        //{
        //    MidiDeviceCallback.reset();
        //}

        if (MidiClientBidi)
        {
            MidiClientBidi.reset();
        }

        //if (MidiClientCallback)
        //{
        //    MidiClientCallback.reset();
        //}

    }
};
