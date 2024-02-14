// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "stdafx.h"


#include "MidiFunctionBlockMessageBuilderTests.h"

using namespace winrt::Windows::Devices::Midi2;



void MidiFunctionBlockMessageBuilderTests::TestBuildFunctionBlockNameNotificationLong()
{
    LOG_OUTPUT(L"Test long function block name");

    winrt::hstring name = L"This is an function block name that is longer than the supported 91 characters for a function block name in MIDI 2";
    uint32_t expectedPacketCount = 7;

    uint8_t functionBlockNumber{ 5 };

    std::cout << "Testing function block Name: " << winrt::to_string(name) << std::endl;

    auto messages = MidiStreamMessageBuilder::BuildFunctionBlockNameNotificationMessages(
        MidiClock::Now(),
        functionBlockNumber,
        name
    );

    std::cout << "Expected ump count " << expectedPacketCount << ", received " << messages.Size() << std::endl;

    // count the number of messages we get back
    VERIFY_ARE_EQUAL(messages.Size(), expectedPacketCount);

    for (auto ump : messages)
    {
        auto message = ump.as<MidiMessage128>();

        std::cout << "Stream Message words"
            << " 0x" << std::hex << message.Word0()
            << " 0x" << std::hex << message.Word1()
            << " 0x" << std::hex << message.Word2()
            << " 0x" << std::hex << message.Word3()
            << std::endl;
    }


    for (uint32_t i = 0; i < messages.Size(); i++)
    {
        std::cout << "Stream word0 0x" << std::hex << messages.GetAt(i).PeekFirstWord() << std::endl;

        // verify status    
        VERIFY_ARE_EQUAL(MidiMessageUtility::GetStatusFromStreamMessageFirstWord(messages.GetAt(i).PeekFirstWord()), MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION);

        // verify form is correct
        if (i == 0)
        {
            // first message
            VERIFY_ARE_EQUAL(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(i).PeekFirstWord()), (uint8_t)0x01);
        }
        else if (i == messages.Size() - 1)
        {
            // last message
            VERIFY_ARE_EQUAL(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(i).PeekFirstWord()), (uint8_t)0x03);
        }
        else
        {
            // interim messages
            VERIFY_ARE_EQUAL(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(i).PeekFirstWord()), (uint8_t)0x02);
        }


        // TODO: Verify function block number is correct


    }


    // reverse it back into a string and verify

    auto s = MidiStreamMessageBuilder::ParseFunctionBlockNameNotificationMessages(messages);
    std::cout << "Parsed: '" << winrt::to_string(s) << "'" << std::endl;
    VERIFY_ARE_EQUAL(s, L"This is an function block name that is longer than the supported 91 characters for a functi");
}

void MidiFunctionBlockMessageBuilderTests::TestBuildFunctionBlockNameNotificationMedium()
{
    LOG_OUTPUT(L"Test medium function block name");

    winrt::hstring name = L"A medium-sized name";
    uint32_t expectedPacketCount = 2;

    uint8_t functionBlockNumber{ 5 };

    std::cout << "Testing function block Name: " << winrt::to_string(name) << std::endl;

    auto messages = MidiStreamMessageBuilder::BuildFunctionBlockNameNotificationMessages(
        MidiClock::Now(),
        functionBlockNumber,
        name
    );

    std::cout << "Expected ump count " << expectedPacketCount << ", received " << messages.Size() << std::endl;

    // count the number of messages we get back
    VERIFY_ARE_EQUAL(messages.Size(), expectedPacketCount);

    for (auto ump : messages)
    {
        auto message = ump.as<MidiMessage128>();

        std::cout << "Stream Message words"
            << " 0x" << std::hex << message.Word0()
            << " 0x" << std::hex << message.Word1()
            << " 0x" << std::hex << message.Word2()
            << " 0x" << std::hex << message.Word3()
            << std::endl;
    }


    // verify status is correct
    VERIFY_ARE_EQUAL(MidiMessageUtility::GetStatusFromStreamMessageFirstWord(messages.GetAt(0).PeekFirstWord()), MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION);
    VERIFY_ARE_EQUAL(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(0).PeekFirstWord()), 0x01);

    VERIFY_ARE_EQUAL(MidiMessageUtility::GetStatusFromStreamMessageFirstWord(messages.GetAt(1).PeekFirstWord()), MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION);
    VERIFY_ARE_EQUAL(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(1).PeekFirstWord()), 0x03);


    // verify form is correct

    // TODO: Verify function block number is correct

    // reverse it back into a string and verify


    auto s = MidiStreamMessageBuilder::ParseFunctionBlockNameNotificationMessages(messages);
    std::cout << "Parsed: '" << winrt::to_string(s) << "'" << std::endl;
    VERIFY_ARE_EQUAL(s, name);
}

void MidiFunctionBlockMessageBuilderTests::TestBuildFunctionBlockInfoNotification()
{
    LOG_OUTPUT(L"Building Function Block Info Notification");

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
        MidiClock::Now(),
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

    auto msg = ump.as<MidiMessage128>();

    VERIFY_ARE_EQUAL(msg.Word0(), resultingWord0);
    VERIFY_ARE_EQUAL(msg.Word1(), resultingWord1);
    VERIFY_ARE_EQUAL(msg.Word2(), (uint32_t)0);
    VERIFY_ARE_EQUAL(msg.Word3(), (uint32_t)0);
}

