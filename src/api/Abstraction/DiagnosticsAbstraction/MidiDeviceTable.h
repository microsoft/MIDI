// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

//#include "MidiLoopbackDevice.h"

// thread-safe meyers singleton for storing the devices we'll use

// for simple loopback, no need for a map or anything like we'll use 
// for other transports. We have only three devices: Two for BiDi, 
// the thirds is for In/Out, so just keep them as member variables.


class MidiDeviceTable
{
public:
    MidiLoopbackBidiDevice* GetBidiDevice();
    MidiLoopbackDevice* GetInOutDevice();

    MidiPingBidiDevice* GetPingDevice();

    static MidiDeviceTable& Current();

    // no copying
    MidiDeviceTable(_In_ const MidiDeviceTable&) = delete;
    MidiDeviceTable& operator=(_In_ const MidiDeviceTable&) = delete;

private:
    MidiDeviceTable();
    ~MidiDeviceTable();


    MidiLoopbackBidiDevice m_bidiDevice;
    MidiLoopbackDevice m_inOutDevice;

    MidiPingBidiDevice m_pingDevice;
};
