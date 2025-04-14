// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

bool MidiUniqueIdTests::ClassSetup()
{
    std::cout << "MidiEndpointIdHelperTests::ClassSetup" << std::endl;

    winrt::init_apartment();

    return m_initializer.InitializeSdkRuntime();
}

bool MidiUniqueIdTests::ClassCleanup()
{
    std::cout << "MidiEndpointIdHelperTests::ClassCleanup" << std::endl;

    m_initializer.ShutdownSdkRuntime();

    winrt::uninit_apartment();

    return true;
}


#define MIDI_MUID_BROADCAST         (uint32_t)0x0FFFFFFF

#define MIDI_MUID_RESERVED_START    (uint32_t)0x0FFFFF00
#define MIDI_MUID_RESERVED_END      (uint32_t)0x0FFFFFFE

#define MIDI_MUID_MIN_VALUE         (uint32_t)0x00000000
#define MIDI_MUID_MAX_VALUE         (uint32_t)0x0FFFFFFF

void MidiUniqueIdTests::TestCreateEmptyId()
{
    MidiUniqueId id;
    
    VERIFY_ARE_EQUAL(id.Byte1(), 0);
    VERIFY_ARE_EQUAL(id.Byte2(), 0);
    VERIFY_ARE_EQUAL(id.Byte3(), 0);
    VERIFY_ARE_EQUAL(id.Byte4(), 0);
}

void MidiUniqueIdTests::TestCreateRandomId()
{
    auto id = MidiUniqueId::CreateRandom();

    auto combinedValue = id.AsCombined28BitValue();

    LOG_OUTPUT(L"Combined: %x", combinedValue);
    LOG_OUTPUT(L"Bytes:    %x %x %x %x", id.Byte4(), id.Byte3(), id.Byte2(), id.Byte1());

    // if we get a zero value, try again just to be sure
    // it's not a fail unless it happens multiple times in a row
    if (combinedValue == 0) 
        id = MidiUniqueId::CreateRandom();

    VERIFY_IS_FALSE(combinedValue == 0);

    // check we're in the general range
    VERIFY_IS_GREATER_THAN_OR_EQUAL(combinedValue, MIDI_MUID_MIN_VALUE);
    VERIFY_IS_LESS_THAN_OR_EQUAL(combinedValue, MIDI_MUID_MAX_VALUE);

    // check that we're not in the reserved range
    VERIFY_IS_FALSE(combinedValue >= MIDI_MUID_RESERVED_START && combinedValue <= MIDI_MUID_RESERVED_END);
}

void MidiUniqueIdTests::TestCreateBroadcastId()
{
    auto id = MidiUniqueId::CreateBroadcast();

    LOG_OUTPUT(L"Combined: %x", id.AsCombined28BitValue());
    LOG_OUTPUT(L"Bytes:    %x %x %x %x", id.Byte4(), id.Byte3(), id.Byte2(), id.Byte1());

    VERIFY_ARE_EQUAL(id.AsCombined28BitValue(), MIDI_MUID_BROADCAST);
}

void MidiUniqueIdTests::TestCreateFrom28BitNumber()
{
    // the number is made up of 7-bit bytes shifted over to create a 28 bit number
    // byte 1 is the LSB

    uint8_t byte1 = 0x7F;
    uint8_t byte2 = 0x7E;
    uint8_t byte3 = 0x7D;
    uint8_t byte4 = 0x7C;

    uint32_t combinedVal =
        (uint32_t)byte1 |
        (uint32_t)(byte2 << 7) |
        (uint32_t)(byte3 << 14) |
        (uint32_t)(byte4 << 21);

    MidiUniqueId id(combinedVal);

    LOG_OUTPUT(L"Combined: %x", id.AsCombined28BitValue());
    LOG_OUTPUT(L"Bytes:    %x %x %x %x", id.Byte4(), id.Byte3(), id.Byte2(), id.Byte1());

    VERIFY_ARE_EQUAL(id.Byte1(), byte1);
    VERIFY_ARE_EQUAL(id.Byte2(), byte2);
    VERIFY_ARE_EQUAL(id.Byte3(), byte3);
    VERIFY_ARE_EQUAL(id.Byte4(), byte4);

    VERIFY_ARE_EQUAL(id.AsCombined28BitValue(), combinedVal);
}

void MidiUniqueIdTests::TestCreateFromBytes()
{
    uint8_t byte1 = 0x7F;
    uint8_t byte2 = 0x7E;
    uint8_t byte3 = 0x7D;
    uint8_t byte4 = 0x7C;

    MidiUniqueId id(byte1, byte2, byte3, byte4);

    VERIFY_ARE_EQUAL(id.Byte1(), byte1);
    VERIFY_ARE_EQUAL(id.Byte2(), byte2);
    VERIFY_ARE_EQUAL(id.Byte3(), byte3);
    VERIFY_ARE_EQUAL(id.Byte4(), byte4);
}

