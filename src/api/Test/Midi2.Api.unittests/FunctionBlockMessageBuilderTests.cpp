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



#define MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_INFO_NOTIFICATION 0x11
#define MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION 0x12

TEST_CASE("Offline.StreamMessageBuilder.BuildFunctionBlockNameNotification.LongName")
{
    winrt::hstring name = L"This is an function block name that is longer than the supported 91 characters for a function block name in MIDI 2";
    uint32_t expectedPacketCount = 7;

    uint8_t functionBlockNumber{ 5 };

    std::cout << "Testing function block Name: " << winrt::to_string(name) << std::endl;

    auto messages = MidiStreamMessageBuilder::BuildFunctionBlockNameNotificationMessages(
        MidiClock::GetMidiTimestamp(),
        functionBlockNumber,
        name
    );

    std::cout << "Expected ump count " << expectedPacketCount << ", received " << messages.Size() << std::endl;

    // count the number of messages we get back
    REQUIRE(messages.Size() == expectedPacketCount);

    for (auto message : messages)
    {
        std::cout << "Stream Message words"
            << " 0x" << std::hex << message.Word0()
            << " 0x" << std::hex << message.Word1()
            << " 0x" << std::hex << message.Word2()
            << " 0x" << std::hex << message.Word3()
            << std::endl;
    }


    for (uint32_t i = 0; i < messages.Size(); i++)
    {
        std::cout << "Stream word0 0x" << std::hex << messages.GetAt(i).Word0() << std::endl;

        // verify status    
        REQUIRE(MidiMessageUtility::GetStatusFromStreamMessageFirstWord(messages.GetAt(i).Word0()) == MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION);

        // verify form is correct
        if (i == 0)
        {
            // first message
            REQUIRE(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(i).Word0()) == 0x01);
        }
        else if (i == messages.Size() - 1)
        {
            // last message
            REQUIRE(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(i).Word0()) == 0x03);
        }
        else
        {
            // interim messages
            REQUIRE(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(i).Word0()) == 0x02);
        }


        // TODO: Verify function block number is correct


    }


    // reverse it back into a string and verify

    auto s = MidiStreamMessageBuilder::ParseFunctionBlockNameNotificationMessages(messages);
    std::cout << "Parsed: '" << winrt::to_string(s) << "'" << std::endl;
    REQUIRE(s == L"This is an function block name that is longer than the supported 91 characters for a functi");


}


TEST_CASE("Offline.StreamMessageBuilder.BuildFunctionBlockNameNotification.MediumName")
{
    winrt::hstring name = L"A medium-sized name";
    uint32_t expectedPacketCount = 2;

    uint8_t functionBlockNumber{ 5 };

    std::cout << "Testing function block Name: " << winrt::to_string(name) << std::endl;

    auto messages = MidiStreamMessageBuilder::BuildFunctionBlockNameNotificationMessages(
        MidiClock::GetMidiTimestamp(),
        functionBlockNumber,
        name
    );

    std::cout << "Expected ump count " << expectedPacketCount << ", received " << messages.Size() << std::endl;

    // count the number of messages we get back
    REQUIRE(messages.Size() == expectedPacketCount);

    for (auto message : messages)
    {
        std::cout << "Stream Message words"
            << " 0x" << std::hex << message.Word0()
            << " 0x" << std::hex << message.Word1()
            << " 0x" << std::hex << message.Word2()
            << " 0x" << std::hex << message.Word3()
            << std::endl;

    }


    // verify status is correct
    REQUIRE(MidiMessageUtility::GetStatusFromStreamMessageFirstWord(messages.GetAt(0).Word0()) == MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION);
    REQUIRE(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(0).Word0()) == 0x01);

    REQUIRE(MidiMessageUtility::GetStatusFromStreamMessageFirstWord(messages.GetAt(1).Word0()) == MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION);
    REQUIRE(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(1).Word0()) == 0x03);


    // verify form is correct

    // TODO: Verify function block number is correct

    // reverse it back into a string and verify


    auto s = MidiStreamMessageBuilder::ParseFunctionBlockNameNotificationMessages(messages);
    std::cout << "Parsed: '" << winrt::to_string(s) << "'" << std::endl;
    REQUIRE(s == name);
}



TEST_CASE("Offline.StreamMessageBuilder.BuildFunctionInfoNotification")
{
    std::cout << "Building Function Block Info Notification" << std::endl;

    bool active{ true };
    uint8_t functionBlockNumber{ 5 };
    MidiFunctionBlockUIHint uiHint{ MidiFunctionBlockUIHint::Receiver };                // 1
    MidiFunctionBlockMidi10 midi10{ MidiFunctionBlockMidi10::YesBandwidthRestricted };  // 2
    MidiFunctionBlockDirection direction{ MidiFunctionBlockDirection::Bidirectional };  // 3
    
    uint8_t firstGroup{ 6 };
    uint8_t numberOfGroups{ 4 };
    uint8_t midiCIVersionFormat{ 1 };
    uint8_t maxNumberOfSysEx8Streams{ 34 }; // 0x22

    uint32_t resultingWord0{ 0xF011851B };      // 0x11 is function block info notification
    uint32_t resultingWord1{ 0x06040122 };

    auto ump = MidiStreamMessageBuilder::BuildFunctionBlockInfoNotificationMessage(
        MidiClock::GetMidiTimestamp(),
        active,
        functionBlockNumber,
        uiHint,
        midi10,
        direction,
        firstGroup,
        numberOfGroups,
        midiCIVersionFormat,
        maxNumberOfSysEx8Streams
       );

    // verify values are in the UMP

    REQUIRE(ump.Word0() == resultingWord0);
    REQUIRE(ump.Word1() == resultingWord1);
    REQUIRE(ump.Word2() == 0);
    REQUIRE(ump.Word3() == 0);
}
