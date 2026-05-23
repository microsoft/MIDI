// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

bool MidiClockTests::ClassSetup()
{
    std::cout << "MidiEndpointIdHelperTests::ClassSetup" << std::endl;

    winrt::init_apartment();

    return m_initializer.InitializeSdkRuntime();
}

bool MidiClockTests::ClassCleanup()
{
    std::cout << "MidiEndpointIdHelperTests::ClassCleanup" << std::endl;

    m_initializer.ShutdownSdkRuntime();

    winrt::uninit_apartment();

    return true;
}



void MidiClockTests::TestMidiClockBasics()
{
    for (int i = 0; i < 10; i++)
    {
        std::cout << "Timestamp: " << i << " : " << MidiClock::Now() << std::endl;
    }

    VERIFY_IS_GREATER_THAN(MidiClock::TimestampFrequency(), (uint32_t)0);
}

// TODO: Should test the convenience methods as well