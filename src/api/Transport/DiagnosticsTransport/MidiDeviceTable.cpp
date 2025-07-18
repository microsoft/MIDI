// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
//#include "MidiDeviceTable.h"


MidiDeviceTable::MidiDeviceTable() = default;
MidiDeviceTable::~MidiDeviceTable() = default;

MidiDeviceTable& MidiDeviceTable::Current()
{
    // explanation: http://www.modernescpp.com/index.php/thread-safe-initialization-of-data/

    static MidiDeviceTable current;

    return current;
}

MidiPingBidiDevice* MidiDeviceTable::GetPingDevice()
{
    return &m_PingDevice;
}


MidiLoopbackBidiDevice* MidiDeviceTable::GetBidiDevice()
{
    return &m_BidiDevice;
}

