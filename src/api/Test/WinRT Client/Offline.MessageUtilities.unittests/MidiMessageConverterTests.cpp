// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"


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

// Status
// 0x0 Complete SysEx in one UMP
// 0x1 Start SysEx
// 0x2 Continue SysEx
// 0x3 End SysEx

// This verifies that Sysex state is dumped when the end of the input data is reached
void MidiMessageConverterTests::TestIncompleteSysEx7Conversion()
{
    std::vector<uint8_t> bytes = { 0xF0, 0x7E, 0x7F, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };

    // assumes group index 5 in the output
    MidiGroup group(5);
    std::vector<uint32_t> expectedResults = { 0x35167E7F, 0x00010203, 0x35230405, 0x06000000 };

    auto actualResults = MidiMessageConverter::ConvertMidi1CompleteMessageBytesToUmpWords(group, bytes, false);

    VERIFY_IS_NOT_NULL(actualResults);
    VERIFY_ARE_EQUAL(expectedResults.size(), actualResults.Size());

    for (uint32_t i = 0; i < expectedResults.size(); i++)
    {
        VERIFY_ARE_EQUAL(expectedResults[i], actualResults.GetAt(i));
    }
}

// This verifies that Sysex state works properly across calls
void MidiMessageConverterTests::TestStatefulSysEx7Conversion()
{
    // this is what is used to retain state across calls. You'll want to have
    // one of these per endpoint/group that you're converting data for. When
    // it goes out of scope, the state is lost. You can also allocate this using
    // winrt::make and then explicitly destroy the object, like you would with
    // any other WinRT type
    MidiBytestreamToUmpMessageConverterState state;

    std::vector<uint8_t> bytes1 = { 0xF0, 0x7E, 0x7F, 0x00 };
    std::vector<uint8_t> bytes2 = { 0x01, 0x02 };
    std::vector<uint8_t> bytes3 = { 0x03, 0x04, 0x05, 0x06, 0xF7 };

    // assumes group index 5 in the output
    MidiGroup group(5);
    std::vector<uint32_t> expectedResults = { 0x35167E7F, 0x00010203, 0x35330405, 0x06000000 };

    auto actualResults1 = MidiMessageConverter::ConvertMidi1CompleteMessageBytesToUmpWords(group, bytes1, false, state);
    auto actualResults2 = MidiMessageConverter::ConvertMidi1CompleteMessageBytesToUmpWords(group, bytes2, false, state);
    auto actualResults3 = MidiMessageConverter::ConvertMidi1CompleteMessageBytesToUmpWords(group, bytes3, false, state);

    std::vector<uint32_t> actualResults{};

    actualResults.insert(actualResults.end(), actualResults1.begin(), actualResults1.end());
    actualResults.insert(actualResults.end(), actualResults2.begin(), actualResults2.end());
    actualResults.insert(actualResults.end(), actualResults3.begin(), actualResults3.end());

    VERIFY_ARE_EQUAL(expectedResults.size(), actualResults.size());

    for (uint32_t i = 0; i < expectedResults.size(); i++)
    {
        VERIFY_ARE_EQUAL(expectedResults[i], actualResults[i]);
    }
}



void MidiMessageConverterTests::TestConvertSystemCommonMidiTimeCode()
{
    uint8_t statusByte = 0xF1;
    uint8_t dataByte1 = 0x26;

    LOG_OUTPUT(L"Building MIDI Time Code Message");

    for (uint8_t groupIndex = 0; groupIndex < 16; groupIndex++)
    {
        auto message = MidiMessageConverter::ConvertMidi1Message(
            0,
            MidiGroup(groupIndex),
            statusByte,
            dataByte1,
            0
        );

        auto message2 = MidiMessageConverter::ConvertMidi1Message(
            0,
            MidiGroup(groupIndex),
            statusByte,
            dataByte1
        );

        uint32_t expectedWord0 = 0x10000000 | (groupIndex << 24) | (statusByte << 16) | (dataByte1 << 8);

        std::cout << "Expecting: 0x" << std::hex << expectedWord0 << std::endl;

        VERIFY_ARE_EQUAL(message.Word0(), expectedWord0);
        VERIFY_ARE_EQUAL(message2.Word0(), expectedWord0);
    }
}


void MidiMessageConverterTests::TestConvertSystemCommonMidiSongPositionPointer()
{
    uint8_t statusByte = 0xF2;
    uint8_t dataByte1 = 0x26;
    uint8_t dataByte2 = 0x17;

    LOG_OUTPUT(L"Building MIDI Song Position Pointer Message");

    for (uint8_t groupIndex = 0; groupIndex < 16; groupIndex++)
    {
        auto message = MidiMessageConverter::ConvertMidi1Message(
            0,
            MidiGroup(groupIndex),
            statusByte,
            dataByte1,
            dataByte2
        );

        uint32_t expectedWord0 = 0x10000000 | (groupIndex << 24) | (statusByte << 16) | (dataByte1 << 8) | (dataByte2);

        std::cout << "Expecting: 0x" << std::hex << expectedWord0 << std::endl;

        VERIFY_ARE_EQUAL(message.Word0(), expectedWord0);
    }
}

void MidiMessageConverterTests::TestConvertSystemCommonMidiSongSelect()
{
    uint8_t statusByte = 0xF3;
    uint8_t dataByte1 = 0x26;

    LOG_OUTPUT(L"Building MIDI Time Code Message");

    for (uint8_t groupIndex = 0; groupIndex < 16; groupIndex++)
    {
        auto message = MidiMessageConverter::ConvertMidi1Message(
            0,
            MidiGroup(groupIndex),
            statusByte,
            dataByte1,
            0
        );

        auto message2 = MidiMessageConverter::ConvertMidi1Message(
            0,
            MidiGroup(groupIndex),
            statusByte,
            dataByte1
        );

        uint32_t expectedWord0 = 0x10000000 | (groupIndex << 24) | (statusByte << 16) | (dataByte1 << 8);

        std::cout << "Expecting: 0x" << std::hex << expectedWord0 << std::endl;

        VERIFY_ARE_EQUAL(message.Word0(), expectedWord0);
        VERIFY_ARE_EQUAL(message2.Word0(), expectedWord0);
    }
}


void MidiMessageConverterTests::TestConvertSystemCommonMidiTuneRequest()
{
    uint8_t statusByte = 0xF6;

    for (uint8_t groupIndex = 0; groupIndex < 16; groupIndex++)
    {
        auto message = MidiMessageConverter::ConvertMidi1Message(
            0,
            MidiGroup(groupIndex),
            statusByte,
            0,
            0
        );

        uint32_t expectedWord0 = 0x10000000 | (groupIndex << 24) | (statusByte << 16);

        std::cout << "Expecting: 0x" << std::hex << expectedWord0 << std::endl;

        VERIFY_ARE_EQUAL(message.Word0(), expectedWord0);
    }
}

void MidiMessageConverterTests::TestConvertSystemCommonMidiStart()
{
    uint8_t statusByte = 0xFA;

    for (uint8_t groupIndex = 0; groupIndex < 16; groupIndex++)
    {
        auto message = MidiMessageConverter::ConvertMidi1Message(
            0,
            MidiGroup(groupIndex),
            statusByte,
            0,
            0
        );

        uint32_t expectedWord0 = 0x10000000 | (groupIndex << 24) | (statusByte << 16);

        std::cout << "Expecting: 0x" << std::hex << expectedWord0 << std::endl;

        VERIFY_ARE_EQUAL(message.Word0(), expectedWord0);
    }
}

void MidiMessageConverterTests::TestConvertSystemCommonMidiContinue()
{
    uint8_t statusByte = 0xFB;

    for (uint8_t groupIndex = 0; groupIndex < 16; groupIndex++)
    {
        auto message = MidiMessageConverter::ConvertMidi1Message(
            0,
            MidiGroup(groupIndex),
            statusByte,
            0,
            0
        );

        uint32_t expectedWord0 = 0x10000000 | (groupIndex << 24) | (statusByte << 16);

        std::cout << "Expecting: 0x" << std::hex << expectedWord0 << std::endl;

        VERIFY_ARE_EQUAL(message.Word0(), expectedWord0);
    }
}

void MidiMessageConverterTests::TestConvertSystemCommonMidiStop()
{
    uint8_t statusByte = 0xFC;

    for (uint8_t groupIndex = 0; groupIndex < 16; groupIndex++)
    {
        auto message = MidiMessageConverter::ConvertMidi1Message(
            0,
            MidiGroup(groupIndex),
            statusByte,
            0,
            0
        );

        uint32_t expectedWord0 = 0x10000000 | (groupIndex << 24) | (statusByte << 16);

        std::cout << "Expecting: 0x" << std::hex << expectedWord0 << std::endl;

        VERIFY_ARE_EQUAL(message.Word0(), expectedWord0);
    }
}

void MidiMessageConverterTests::TestConvertSystemCommonMidiActiveSensing()
{
    uint8_t statusByte = 0xFE;

    for (uint8_t groupIndex = 0; groupIndex < 16; groupIndex++)
    {
        auto message = MidiMessageConverter::ConvertMidi1Message(
            0,
            MidiGroup(groupIndex),
            statusByte,
            0,
            0
        );

        uint32_t expectedWord0 = 0x10000000 | (groupIndex << 24) | (statusByte << 16);

        std::cout << "Expecting: 0x" << std::hex << expectedWord0 << std::endl;

        VERIFY_ARE_EQUAL(message.Word0(), expectedWord0);
    }
}


void MidiMessageConverterTests::TestConvertSystemCommonMidiReset()
{
    uint8_t statusByte = 0xFF;

    for (uint8_t groupIndex = 0; groupIndex < 16; groupIndex++)
    {
        auto message = MidiMessageConverter::ConvertMidi1Message(
            0,
            MidiGroup(groupIndex),
            statusByte,
            0,
            0
        );

        uint32_t expectedWord0 = 0x10000000 | (groupIndex << 24) | (statusByte << 16);

        std::cout << "Expecting: 0x" << std::hex << expectedWord0 << std::endl;

        VERIFY_ARE_EQUAL(message.Word0(), expectedWord0);
    }
}

