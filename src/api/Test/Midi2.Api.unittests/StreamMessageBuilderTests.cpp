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


#define MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_DISCOVERY 0x00
#define MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_INFO_NOTIFICATION 0x01
#define MIDI_STREAM_MESSAGE_STATUS_DEVICE_IDENTITY_NOTIFICATION 0x02
#define MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION 0x03
#define MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_PRODUCT_INSTANCE_ID_NOTIFICATION 0x04
#define MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_REQUEST 0x05
#define MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_NOTIFICATION 0x06



TEST_CASE("Offline.StreamMessageBuilder.BuildEndpointNameNotification.LongName")
{
    winrt::hstring name = L"This is an endpoint name that is longer than the supported 98 characters for an endpoint name in MIDI 2";
    int expectedPacketCount = 7;


    std::cout << "Testing endpoint Name: " << winrt::to_string(name) << std::endl;

    auto messages = MidiStreamMessageBuilder::BuildEndpointNameNotificationMessages(
        MidiClock::GetMidiTimestamp(),
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


    for (int i = 0; i < messages.Size(); i++)
    {
        std::cout << "Stream word0 0x" << std::hex << messages.GetAt(i).Word0() << std::endl;

        // verify status    
        REQUIRE(MidiMessageUtility::GetStatusFromStreamMessageFirstWord(messages.GetAt(i).Word0()) == MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION);

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
    }

    // reverse it back into a string and verify

    auto s = MidiStreamMessageBuilder::ParseEndpointNameNotificationMessages(messages);
    std::cout << "Parsed: '" << winrt::to_string(s) << "'" << std::endl;
    REQUIRE(s == L"This is an endpoint name that is longer than the supported 98 characters for an endpoint name in M");


}


TEST_CASE("Offline.StreamMessageBuilder.BuildEndpointNameNotification.MediumName")
{
    winrt::hstring name = L"This is medium-sized";
    int expectedPacketCount = 2;

    std::cout << "Testing endpoint Name: " << winrt::to_string(name) << std::endl;

    auto messages = MidiStreamMessageBuilder::BuildEndpointNameNotificationMessages(
        MidiClock::GetMidiTimestamp(),
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
    REQUIRE(MidiMessageUtility::GetStatusFromStreamMessageFirstWord(messages.GetAt(0).Word0()) == MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION);
    REQUIRE(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(0).Word0()) == 0x01);

    REQUIRE(MidiMessageUtility::GetStatusFromStreamMessageFirstWord(messages.GetAt(1).Word0()) == MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION);
    REQUIRE(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(1).Word0()) == 0x03);

    // verify form is correct

    // reverse it back into a string and verify


    auto s = MidiStreamMessageBuilder::ParseEndpointNameNotificationMessages(messages);
    std::cout << "Parsed: '" << winrt::to_string(s) << "'" << std::endl;
    REQUIRE(s == name);
}

TEST_CASE("Offline.StreamMessageBuilder.BuildEndpointNameNotification.ShortName")
{
    winrt::hstring name = L"Short";
    int expectedPacketCount = 1;

    std::cout << "Testing endpoint Name: " << winrt::to_string(name) << std::endl;

    auto messages = MidiStreamMessageBuilder::BuildEndpointNameNotificationMessages(
        MidiClock::GetMidiTimestamp(),
        name
    );

    std::cout << "Expected ump count " << expectedPacketCount << ", received " << messages.Size() << std::endl;

    std::cout << "Stream Message 1 word0 0x" << std::hex << messages.GetAt(0).Word0() << std::endl;

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
    REQUIRE(MidiMessageUtility::GetStatusFromStreamMessageFirstWord(messages.GetAt(0).Word0()) == MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION);

    // verify form is correct
    REQUIRE(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(0).Word0()) == 0x00);

    // reverse it back into a string and verify
    auto s = MidiStreamMessageBuilder::ParseEndpointNameNotificationMessages(messages);
    std::cout << "Parsed: '" << winrt::to_string(s) << "'" << std::endl;
    REQUIRE(s == name);

}




TEST_CASE("Offline.StreamMessageBuilder.BuildProductInstanceIdNotification.Short")
{
    winrt::hstring productInstanceId = L"ABC123";
    int expectedPacketCount = 1;

    std::cout << "Testing endpoint Id: " << winrt::to_string(productInstanceId) << std::endl;

    auto messages = MidiStreamMessageBuilder::BuildProductInstanceIdNotificationMessages(
        MidiClock::GetMidiTimestamp(),
        productInstanceId
    );

    std::cout << "Expected ump count " << expectedPacketCount << ", received " << messages.Size() << std::endl;

    std::cout << "Stream Message 1 word0 0x" << std::hex << messages.GetAt(0).Word0() << std::endl;

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
    REQUIRE(MidiMessageUtility::GetStatusFromStreamMessageFirstWord(messages.GetAt(0).Word0()) == MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_PRODUCT_INSTANCE_ID_NOTIFICATION);

    // verify form is correct
    REQUIRE(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(0).Word0()) == 0x00);

    // reverse it back into a string and verify



}