// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

bool MidiMessageConverterTests::ClassSetup()
{
    std::cout << "MidiMessageConverterTests::ClassSetup" << std::endl;

    winrt::init_apartment();

    return m_initializer.InitializeSdkRuntime();
}


bool MidiMessageConverterTests::ClassCleanup()
{
    std::cout << "MidiMessageConverterTests::ClassCleanup" << std::endl;

    m_initializer.ShutdownSdkRuntime();

    winrt::uninit_apartment();

    return true;
}




void MidiMessageConverterTests::TestConvertControlChangeMessages()
{
    uint8_t statusAndChannelByte = 0xB0;
    uint8_t data1Byte = 0x3C;
    uint8_t data2Byte = 0x01;

    LOG_OUTPUT(L"Building MIDI 1.0 CV Message");

    for (uint8_t groupIndex = 0; groupIndex < 16; groupIndex++)
    {
        auto message = MidiMessageConverter::ConvertMidi1Message(
            0,
            MidiGroup(groupIndex),
            statusAndChannelByte,
            data1Byte,
            data2Byte
        );

        uint32_t expectedWord0 = 0x20000000 | (groupIndex << 24) | (statusAndChannelByte << 16) | (data1Byte << 8) | data2Byte;

        std::cout << "Expecting: 0x" << std::hex << expectedWord0 << std::endl;
        VERIFY_ARE_EQUAL(message.Word0(), expectedWord0);
    }
}

void MidiMessageConverterTests::TestConvertNoteOnMessages()
{
    uint8_t statusAndChannelByte = 0x95;
    uint8_t data1Byte = 0x12;
    uint8_t data2Byte = 0x27;

    LOG_OUTPUT(L"Building MIDI 1.0 CV Message");

    for (uint8_t groupIndex = 0; groupIndex < 16; groupIndex++)
    {
        auto message = MidiMessageConverter::ConvertMidi1Message(
            0,
            MidiGroup(groupIndex),
            statusAndChannelByte,
            data1Byte,
            data2Byte
        );

        uint32_t expectedWord0 = 0x20000000 | (groupIndex << 24) | (statusAndChannelByte << 16) | (data1Byte << 8) | data2Byte;

        std::cout << "Expecting: 0x" << std::hex << expectedWord0 << std::endl;
        VERIFY_ARE_EQUAL(message.Word0(), expectedWord0);
    }
}

void MidiMessageConverterTests::TestConvertNoteOffMessages()
{
    uint8_t statusAndChannelByte = 0x8F;
    uint8_t data1Byte = 0x42;
    uint8_t data2Byte = 0x00;

    LOG_OUTPUT(L"Building MIDI 1.0 Note Off Message");

    for (uint8_t groupIndex = 0; groupIndex < 16; groupIndex++)
    {
        auto message = MidiMessageConverter::ConvertMidi1Message(
            0,
            MidiGroup(groupIndex),
            statusAndChannelByte,
            data1Byte,
            data2Byte
        );


        uint32_t expectedWord0 = 0x20000000 | (groupIndex << 24) | (statusAndChannelByte << 16) | (data1Byte << 8) | data2Byte;

        std::cout << "Expecting: 0x" << std::hex << expectedWord0 << std::endl;
        VERIFY_ARE_EQUAL(message.Word0(), expectedWord0);
    }
}

void MidiMessageConverterTests::TestConvertClockMessages()
{
    uint8_t statusByte = 0xF8;

    LOG_OUTPUT(L"Building MIDI 1.0 Clock Message");

    for (uint8_t groupIndex = 0; groupIndex < 16; groupIndex++)
    {
        auto message = MidiMessageConverter::ConvertMidi1Message(
            0,
            MidiGroup(groupIndex),
            statusByte,
            0,
            0
        );

        auto message2 = MidiMessageConverter::ConvertMidi1Message(
            0,
            MidiGroup(groupIndex),
            statusByte,
            0
        );

        auto message3 = MidiMessageConverter::ConvertMidi1Message(
            0,
            MidiGroup(groupIndex),
            statusByte
        );

        uint32_t expectedWord0 = 0x10000000 | (groupIndex << 24) | (statusByte << 16);

        std::cout << "Expecting: 0x" << std::hex << expectedWord0 << std::endl;

        VERIFY_ARE_EQUAL(message.Word0(), expectedWord0);
        VERIFY_ARE_EQUAL(message2.Word0(), expectedWord0);
        VERIFY_ARE_EQUAL(message3.Word0(), expectedWord0);
    }
}

