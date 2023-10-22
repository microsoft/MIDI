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


struct MidiVirtualDeviceEntry
{
    GUID key;                                   // this is the key that is used in config before there's any SWD to refer to

    std::string InstanceId;                     // the part of the instance ID we (mostly) control

    std::shared_ptr<IMidiBiDi> MidiDeviceBiDi;

};


class MidiDeviceTable
{
public:
    //std::shared_ptr<LoopbackDevice> GetDevice(std::wstring deviceId);
    //MidiLoopbackDevice* GetBidiDevice();
    //MidiLoopbackDevice* GetInOutDevice();

    static MidiDeviceTable& Current();

    // no copying
    MidiDeviceTable(_In_ const MidiDeviceTable&) = delete;
    MidiDeviceTable& operator=(_In_ const MidiDeviceTable&) = delete;

private:
    MidiDeviceTable();
    ~MidiDeviceTable();


    //MidiLoopbackDevice m_bidiDevice;
    //MidiLoopbackDevice m_inOutDevice;
};