// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

bool MidiChannelTests::ClassSetup()
{
    std::cout << "MidiEndpointIdHelperTests::ClassSetup" << std::endl;

    winrt::init_apartment();

    return m_initializer.InitializeSdkRuntime();
}

bool MidiChannelTests::ClassCleanup()
{
    std::cout << "MidiEndpointIdHelperTests::ClassCleanup" << std::endl;

    m_initializer.ShutdownSdkRuntime();

    winrt::uninit_apartment();

    return true;
}


void MidiChannelTests::TestBasics()
{
    uint8_t index{ 0x08 };

    MidiChannel ch(index);

    VERIFY_ARE_EQUAL(ch.Index(), index);
    VERIFY_ARE_EQUAL(ch.DisplayValue(), index + 1);

}

void MidiChannelTests::TestLabels()
{
    VERIFY_IS_FALSE(MidiChannel::LongLabel().empty());
    VERIFY_IS_FALSE(MidiChannel::ShortLabel().empty());

}

void MidiChannelTests::TestInvalidData()
{
    // The class is designed to ignore the most significant nibble because
    // that allows for passing in a full status + channel byte without pre-cleaning

    MidiChannel ch(0xAC);

    VERIFY_ARE_EQUAL(ch.Index(), 0x0C);
}

