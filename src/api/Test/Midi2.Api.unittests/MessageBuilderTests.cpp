// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"

#include "catch_amalgamated.hpp"

using namespace winrt::Windows::Devices::Midi2;


TEST_CASE("Build Type 0 Utility Messages")
{
    uint8_t status = 0x0D;
    uint32_t dataOrReserved = 0x00020;
    uint32_t expectedWord0 = 0x00D00020;


    auto message = MidiMessageBuilder::BuildUtilityMessage(
        MidiClock::GetMidiTimestamp(),
        status,
        dataOrReserved
        );

    REQUIRE(message.Word0() == expectedWord0);
}

TEST_CASE("Build Type 1 System Messages")
{
    uint8_t group = 0x3;
    uint8_t status = 0xD;
    uint8_t midiByte2 = 0x3C;
    uint8_t midiByte3 = 0x3F;
    uint32_t expectedWord0 = 0x130D3C3F;


    auto message = MidiMessageBuilder::BuildSystemMessage(
        MidiClock::GetMidiTimestamp(),
        group,
        status,
        midiByte2,
        midiByte3
        );

    REQUIRE(message.Word0() == expectedWord0);
}


TEST_CASE("Build Type 2 MIDI 1.0 Channel Voice Messages")
{
    MidiGroup grp{ 0x4 };
    Midi1ChannelVoiceMessageStatus status = Midi1ChannelVoiceMessageStatus::NoteOn; // 9
    MidiChannel ch{ 0xF };
    uint8_t note{ 0x71 };
    uint8_t velocity{ 0x7F };

    // update this if you change any values from above. We're not using a 
    // function to create this because we need to check our logic in this test
    uint32_t resultingWord0 = 0x249F717F;

    std::cout << "Building MIDI 1 Channel Voice Message" << std::endl;

    auto ump = MidiMessageBuilder::BuildMidi1ChannelVoiceMessage(
        MidiClock::GetMidiTimestamp(), 
        grp.Index(), 
        status,
        ch.Index(), 
        note, 
        velocity);

    // verify values are in the UMP

    REQUIRE(ump.Word0() == resultingWord0);
}

//TEST_CASE("Build Type 3 SysEx7 Messages")
//{
//    REQUIRE(false);
//}


TEST_CASE("Build Type 4 MIDI 2.0 Channel Voice Messages")
{
    MidiGroup grp{ 0x5 };
    Midi2ChannelVoiceMessageStatus status = Midi2ChannelVoiceMessageStatus::NoteOn; // 9
    MidiChannel ch{ 0xF };
    uint16_t index{0x6655};         // not a real attribute type
    uint16_t velocity{ 0xFF7F };
    uint16_t attribute{ 0xD00B };   // not real attribute data

    uint32_t data = velocity << 16 | attribute;

    // update this if you change any values from above. We're not using a 
    // function to create this because we need to check our logic in this test
    uint32_t resultingWord0 = 0x459F6655;
    uint32_t resultingWord1 = 0xFF7FD00B;

    std::cout << "Building MIDI 2 Channel Voice Message" << std::endl;

    auto ump = MidiMessageBuilder::BuildMidi2ChannelVoiceMessage(
        MidiClock::GetMidiTimestamp(),
        grp.Index(),
        status,
        ch.Index(),
        index,
        data
        );

    // verify values are in the UMP

    REQUIRE(ump.Word0() == resultingWord0);
    REQUIRE(ump.Word1() == resultingWord1);
}


//TEST_CASE("Build Type 5 SysEx8 Messages")
//{
//    REQUIRE(false);
//}


//TEST_CASE("Build Type 5 Mixed Data Set Messages")
//{
//    REQUIRE(false);
//}


//TEST_CASE("Build Type D Flex Data Messages")
//{
//    REQUIRE(false);
//}



TEST_CASE("Offline.MessageBuilder.BuildStreamMessages 1")
{
    std::cout << "Building Stream Message 1" << std::endl;

    uint8_t form = 0x3;
    uint16_t status = 0x02;
    uint16_t word0Remaining = 0x1234;
    uint32_t word1 = 0x86753090;
    uint32_t word2 = 0x03263727;
    uint32_t word3 = 0x10203040;

    uint32_t resultingWord0 = 0xFC021234;   // shifting of form to be the two high bits of the nibble changes it from 3 to C

    auto ump = MidiMessageBuilder::BuildStreamMessage(
        MidiClock::GetMidiTimestamp(),
        form,
        status,
        word0Remaining,
        word1,
        word2,
        word3
    );

    // verify values are in the UMP

    REQUIRE(ump.Word0() == resultingWord0);
    REQUIRE(ump.Word1() == word1);
    REQUIRE(ump.Word2() == word2);
    REQUIRE(ump.Word3() == word3);
}


TEST_CASE("Offline.MessageBuilder.BuildStreamMessages 2")
{
    std::cout << "Building Stream Message 2" << std::endl;

    uint8_t form = 0x3;
    uint16_t status = 0x32;
    uint16_t word0Remaining = 0xF23F;
    uint32_t word1 = 0x86753090;
    uint32_t word2 = 0x03263727;
    uint32_t word3 = 0x10203040;

    uint32_t resultingWord0 = 0xFC32F23F;   // shifting of form to be the two high bits of the nibble changes it from 3 to C

    auto ump = MidiMessageBuilder::BuildStreamMessage(
        MidiClock::GetMidiTimestamp(),
        form,
        status,
        word0Remaining,
        word1,
        word2,
        word3
    );

    // verify values are in the UMP

    REQUIRE(ump.Word0() == resultingWord0);
    REQUIRE(ump.Word1() == word1);
    REQUIRE(ump.Word2() == word2);
    REQUIRE(ump.Word3() == word3);
}

