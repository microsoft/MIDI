// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

bool MidiMessageBuilderTests::ClassSetup()
{
    std::cout << "MidiEndpointIdHelperTests::ClassSetup" << std::endl;

    winrt::init_apartment();

    return m_initializer.InitializeSdkRuntime();
}

bool MidiMessageBuilderTests::ClassCleanup()
{
    std::cout << "MidiEndpointIdHelperTests::ClassCleanup" << std::endl;

    m_initializer.ShutdownSdkRuntime();

    winrt::uninit_apartment();

    return true;
}

void MidiMessageBuilderTests::TestBuildType0UtilityMessages()
{
    uint8_t status = 0x0D;
    uint32_t dataOrReserved = 0x00020;
    uint32_t expectedWord0 = 0x00D00020;

    LOG_OUTPUT(L"Building Utility Message");

    auto message = MidiMessageBuilder::BuildUtilityMessage(
        MidiClock::Now(),
        status,
        dataOrReserved
    );

    VERIFY_ARE_EQUAL(message.Word0(), expectedWord0);
}

void MidiMessageBuilderTests::TestBuildType1SystemMessages()
{
    uint8_t group = 0x3;
    uint8_t status = 0xD;
    uint8_t midiByte2 = 0x3C;
    uint8_t midiByte3 = 0x3F;
    uint32_t expectedWord0 = 0x130D3C3F;

    LOG_OUTPUT(L"Building System Message");

    auto message = MidiMessageBuilder::BuildSystemMessage(
        MidiClock::Now(),
        MidiGroup(group),
        status,
        midiByte2,
        midiByte3
    );

    VERIFY_ARE_EQUAL(message.Word0(), expectedWord0);
}

void MidiMessageBuilderTests::TestBuildType2Midi1ChannelVoiceMessages()
{
    MidiGroup grp{ 0x4 };
    Midi1ChannelVoiceMessageStatus status = Midi1ChannelVoiceMessageStatus::NoteOn; // 9
    MidiChannel ch{ 0xF };
    uint8_t note{ 0x71 };
    uint8_t velocity{ 0x7F };

    // update this if you change any values from above. We're not using a 
    // function to create this because we need to check our logic in this test
    uint32_t resultingWord0 = 0x249F717F;

    LOG_OUTPUT(L"Building MIDI 1 Channel Voice Message");

    auto ump = MidiMessageBuilder::BuildMidi1ChannelVoiceMessage(
        MidiClock::Now(),
        grp,
        status,
        ch,
        note,
        velocity);

    // verify values are in the UMP

    VERIFY_ARE_EQUAL(ump.Word0(), resultingWord0);
}

void MidiMessageBuilderTests::TestBuildType4Midi2ChannelVoiceMessages()
{
    MidiGroup grp{ 0x5 };
    Midi2ChannelVoiceMessageStatus status = Midi2ChannelVoiceMessageStatus::NoteOn; // 9
    MidiChannel ch{ 0xF };
    uint16_t index{ 0x6655 };         // not a real attribute type
    uint16_t velocity{ 0xFF7F };
    uint16_t attribute{ 0xD00B };   // not real attribute data

    uint32_t data = velocity << 16 | attribute;

    // update this if you change any values from above. We're not using a 
    // function to create this because we need to check our logic in this test
    uint32_t resultingWord0 = 0x459F6655;
    uint32_t resultingWord1 = 0xFF7FD00B;

    LOG_OUTPUT(L"Building MIDI 2 Channel Voice Message");

    auto ump = MidiMessageBuilder::BuildMidi2ChannelVoiceMessage(
        MidiClock::Now(),
        grp,
        status,
        ch,
        index,
        data
    );

    // verify values are in the UMP

    VERIFY_ARE_EQUAL(ump.Word0(), resultingWord0);
    VERIFY_ARE_EQUAL(ump.Word1(), resultingWord1);
}

void MidiMessageBuilderTests::TestBuildTypeFStreamMessages1()
{
    LOG_OUTPUT(L"Building Stream Message 1");

    uint8_t form = 0x3;
    uint16_t status = 0x02;
    uint16_t word0Remaining = 0x1234;
    uint32_t word1 = 0x86753090;
    uint32_t word2 = 0x03263727;
    uint32_t word3 = 0x10203040;

    uint32_t resultingWord0 = 0xFC021234;   // shifting of form to be the two high bits of the nibble changes it from 3 to C

    auto ump = MidiMessageBuilder::BuildStreamMessage(
        MidiClock::Now(),
        form,
        status,
        word0Remaining,
        word1,
        word2,
        word3
    );

    // verify values are in the UMP

    VERIFY_ARE_EQUAL(ump.Word0(), resultingWord0);
    VERIFY_ARE_EQUAL(ump.Word1(), word1);
    VERIFY_ARE_EQUAL(ump.Word2(), word2);
    VERIFY_ARE_EQUAL(ump.Word3(), word3);
}

void MidiMessageBuilderTests::TestBuildTypeFStreamMessages2()
{
    LOG_OUTPUT(L"Building Stream Message 2");

    uint8_t form = 0x3;
    uint16_t status = 0x32;
    uint16_t word0Remaining = 0xF23F;
    uint32_t word1 = 0x86753090;
    uint32_t word2 = 0x03263727;
    uint32_t word3 = 0x10203040;

    uint32_t resultingWord0 = 0xFC32F23F;   // shifting of form to be the two high bits of the nibble changes it from 3 to C

    auto ump = MidiMessageBuilder::BuildStreamMessage(
        MidiClock::Now(),
        form,
        status,
        word0Remaining,
        word1,
        word2,
        word3
    );

    // verify values are in the UMP

    VERIFY_ARE_EQUAL(ump.Word0(), resultingWord0);
    VERIFY_ARE_EQUAL(ump.Word1(), word1);
    VERIFY_ARE_EQUAL(ump.Word2(), word2);
    VERIFY_ARE_EQUAL(ump.Word3(), word3);
}

void MidiMessageBuilderTests::TestBuildMixedDatasetHeaderMessage()
{
    LOG_OUTPUT(L"Building Mixed Dataset Header");

    uint8_t group{ 0xA };
    uint8_t mdsId{ 0xD };
    uint16_t numberOfValidBytesInChunk{ 0x0099 };
    uint16_t numberOfChunksInDatSet{ 8 };
    uint16_t numberOfThisChunk{ 7 };
    uint16_t manufacturerId{ 0x5150 };
    uint16_t deviceId{ 0x1984 };
    uint16_t subId1{ 0x0326 };
    uint16_t subId2{ 0x3827 };

    uint32_t resultingWord0 = 0x5A8D0099;
    uint32_t resultingWord1 = 0x00080007;
    uint32_t resultingWord2 = 0x51501984;
    uint32_t resultingWord3 = 0x03263827;

    auto ump = MidiMessageBuilder::BuildMixedDataSetChunkHeaderMessage(
        MidiClock::Now(),
        MidiGroup(group),
        mdsId,
        numberOfValidBytesInChunk,
        numberOfChunksInDatSet,
        numberOfThisChunk,
        manufacturerId,
        deviceId,
        subId1,
        subId2
    );

    // verify values are in the UMP

    VERIFY_ARE_EQUAL(ump.Word0(), resultingWord0);
    VERIFY_ARE_EQUAL(ump.Word1(), resultingWord1);
    VERIFY_ARE_EQUAL(ump.Word2(), resultingWord2);
    VERIFY_ARE_EQUAL(ump.Word3(), resultingWord3);
}

void MidiMessageBuilderTests::TestBuildMixedDatasetPayloadMessage()
{
    LOG_OUTPUT(L"Building Mixed Dataset Payload");

    uint8_t group{ 0xA };
    uint8_t mdsId{ 0xD };

    uint32_t resultingWord0 = 0x5A9D0102;
    uint32_t resultingWord1 = 0x03040506;
    uint32_t resultingWord2 = 0x0708090A;
    uint32_t resultingWord3 = 0x0B0C0D0E;

    auto ump = MidiMessageBuilder::BuildMixedDataSetChunkDataMessage(
        MidiClock::Now(),
        MidiGroup(group),
        mdsId,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E
    );

    // verify values are in the UMP

    VERIFY_ARE_EQUAL(ump.Word0(), resultingWord0);
    VERIFY_ARE_EQUAL(ump.Word1(), resultingWord1);
    VERIFY_ARE_EQUAL(ump.Word2(), resultingWord2);
    VERIFY_ARE_EQUAL(ump.Word3(), resultingWord3);
}


