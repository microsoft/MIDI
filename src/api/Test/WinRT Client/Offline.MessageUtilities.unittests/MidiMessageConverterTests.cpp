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


void MidiMessageConverterTests::TestConvertHexStringToBytes()
{
    winrt::hstring string1 = L"F0 7E 7F 06 01 F7";
    std::vector<uint8_t> expectedResults1 = { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };

    auto actualResults1 = MidiMessageConverter::ConvertHexByteStringToByteArray(string1);

    VERIFY_IS_NOT_NULL(actualResults1);
    VERIFY_ARE_EQUAL(expectedResults1.size(), actualResults1.Size());

    for (uint32_t i = 0; i < expectedResults1.size(); i++)
    {
        VERIFY_ARE_EQUAL(expectedResults1[i], actualResults1.GetAt(i));
    }
}

void MidiMessageConverterTests::TestConvertHexStringNoSpacesToBytes()
{
    winrt::hstring string1 = L"F07E7F0601F7";
    std::vector<uint8_t> expectedResults1 = { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };

    auto actualResults1 = MidiMessageConverter::ConvertHexByteStringToByteArray(string1);

    VERIFY_IS_NOT_NULL(actualResults1);
    VERIFY_ARE_EQUAL(expectedResults1.size(), actualResults1.Size());

    for (uint32_t i = 0; i < expectedResults1.size(); i++)
    {
        VERIFY_ARE_EQUAL(expectedResults1[i], actualResults1.GetAt(i));
    }
}

void MidiMessageConverterTests::TestConvertHexStringMessyToBytes()
{
    winrt::hstring string1 = L"F0    7E/7Fz06 01F7  ";
    std::vector<uint8_t> expectedResults1 = { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };

    auto actualResults1 = MidiMessageConverter::ConvertHexByteStringToByteArray(string1);

    VERIFY_IS_NOT_NULL(actualResults1);
    VERIFY_ARE_EQUAL(expectedResults1.size(), actualResults1.Size());

    for (uint32_t i = 0; i < expectedResults1.size(); i++)
    {
        VERIFY_ARE_EQUAL(expectedResults1[i], actualResults1.GetAt(i));
    }
}



void MidiMessageConverterTests::TestConvertHexBytesToUMP()
{
    std::vector<uint8_t> bytes = { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };

    // assumes group index 6 in the output
    MidiGroup group(6);
    std::vector<uint32_t> expectedResults = { 0x36047E7F, 0x06010000 };

    auto actualResults = MidiMessageConverter::ConvertMidi1CompleteMessageBytesToUmpWords(group, bytes, false);

    VERIFY_IS_NOT_NULL(actualResults);
    VERIFY_ARE_EQUAL(expectedResults.size(), actualResults.Size());

    for (uint32_t i = 0; i < expectedResults.size(); i++)
    {
        VERIFY_ARE_EQUAL(expectedResults[i], actualResults.GetAt(i));
    }

}

void MidiMessageConverterTests::TestConvertUMPToHexBytes()
{
    std::vector<uint32_t> words = { 0x36047E7F, 0x06010000 };
    std::vector<uint8_t> expectedResults = { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };

    auto actualResults = MidiMessageConverter::ConvertSingleGroupCompleteMessageUmpWordsToMidi1Bytes(words);

    VERIFY_IS_NOT_NULL(actualResults);
    VERIFY_ARE_EQUAL(expectedResults.size(), actualResults.Size());

    for (uint32_t i = 0; i < expectedResults.size(); i++)
    {
        VERIFY_ARE_EQUAL(expectedResults[i], actualResults.GetAt(i));
    }

}
