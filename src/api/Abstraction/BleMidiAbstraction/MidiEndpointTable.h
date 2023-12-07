// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

//#include "MidiVirtualDevice.h"

// thread-safe meyers singleton for storing the devices we'll use


struct MidiVirtualEndpointEntry
{
    GUID JsonKey;                               // this is the key that is used in config before there's any SWD to refer to

    std::wstring InstanceId;                    // the part of the instance ID we (mostly) control

    std::wstring EndpointDeviceId;              // the device interface id

    wil::com_ptr_nothrow<IMidiBiDi> MidiDeviceBiDi;

    ~MidiVirtualEndpointEntry()
    {
        if (MidiDeviceBiDi)
        {
            MidiDeviceBiDi.reset();
        }
    }
};


class MidiEndpointTable
{
public:
    //std::shared_ptr<LoopbackDevice> GetDevice(std::wstring deviceId);
    //MidiLoopbackDevice* GetBidiDevice();
    //MidiLoopbackDevice* GetInOutDevice();

    static MidiEndpointTable& Current();

    // no copying
    MidiEndpointTable(_In_ const MidiEndpointTable&) = delete;
    MidiEndpointTable& operator=(_In_ const MidiEndpointTable&) = delete;


    wil::com_ptr_nothrow<IMidiBiDi> GetEndpointInterfaceForId(_In_ std::wstring EndpointDeviceId) const noexcept;
    void RemoveEndpointEntry(_In_ std::wstring EndpointDeviceId) noexcept;

private:
    MidiEndpointTable();
    ~MidiEndpointTable();

    // key is EndpointDeviceId (the device interface id)
    std::map<std::wstring, MidiVirtualEndpointEntry> m_Endpoints;


    //MidiLoopbackDevice m_BidiDevice;
    //MidiLoopbackDevice m_InOutDevice;
};