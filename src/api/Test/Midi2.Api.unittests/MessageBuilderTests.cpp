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


//TEST_CASE("Build Type 0 Utility Messages")
//{
//    MidiMessageBuilder::BuildUtilityMessage(MidiClock::GetMidiTimestamp(), );
//
//    REQUIRE(false);
//}

//TEST_CASE("Build Type 1 System Messages")
//{
//    REQUIRE(false);
//}


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
    uint8_t note{ 0x71 };
    uint16_t index{0x6655};         // not a real attribute type
    uint16_t velocity{ 0xFF7F };
    uint16_t attribute{ 0xD00B };   // not real attribute data

    uint32_t data = velocity << 16 | attribute;

    // update this if you change any values from above. We're not using a 
    // function to create this because we need to check our logic in this test
    uint32_t resultingWord0 = 0x459F6655;
    uint32_t resultingWord1 = 0xFF7FD00B;

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
//
//TEST_CASE("Build Type 5 Mixed Data Set Messages")
//{
//    REQUIRE(false);
//}
//
//TEST_CASE("Build Type D Flex Data Messages")
//{
//    REQUIRE(false);
//}
//
//TEST_CASE("Build Type F Stream Messages")
//{
//    REQUIRE(false);
//}
