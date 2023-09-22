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
        REQUIRE(MidiMessageUtility::GetStatusFromStreamMessageFirstWord(messages.GetAt(i).Word0()) == 0x03);

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
    REQUIRE(MidiMessageUtility::GetStatusFromStreamMessageFirstWord(messages.GetAt(0).Word0()) == 0x03);
    REQUIRE(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(0).Word0()) == 0x01);

    REQUIRE(MidiMessageUtility::GetStatusFromStreamMessageFirstWord(messages.GetAt(1).Word0()) == 0x03);
    REQUIRE(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(1).Word0()) == 0x03);

    // verify form is correct

    // reverse it back into a string and verify

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
    REQUIRE(MidiMessageUtility::GetStatusFromStreamMessageFirstWord(messages.GetAt(0).Word0()) == 0x03);

    // verify form is correct
    REQUIRE(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(0).Word0()) == 0x00);

    // reverse it back into a string and verify

}




TEST_CASE("Offline.StreamMessageBuilder.BuildEndpointProductInstanceIdNotification.Short")
{
    winrt::hstring productInstanceId = L"ABC123";
    int expectedPacketCount = 1;

    std::cout << "Testing endpoint Id: " << winrt::to_string(productInstanceId) << std::endl;

    auto messages = MidiStreamMessageBuilder::BuildEndpointProductInstanceIdNotificationMessages(
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
    REQUIRE(MidiMessageUtility::GetStatusFromStreamMessageFirstWord(messages.GetAt(0).Word0()) == 0x04);

    // verify form is correct
    REQUIRE(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(0).Word0()) == 0x00);

    // reverse it back into a string and verify

}