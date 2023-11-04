// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "stdafx.h"

#include "MidiClockTests.h"


using namespace winrt::Windows::Devices::Midi2;


void MidiClockTests::TestMidiClockBasics()
{
    for (int i = 0; i < 20; i++)
    {
        std::cout << "Timestamp: " << i << " : " << MidiClock::Now() << std::endl;
    }

    VERIFY_IS_GREATER_THAN(MidiClock::GetMidiTimestampFrequency(), (uint32_t)0);
}

// TODO: Should test the convenience methods as well