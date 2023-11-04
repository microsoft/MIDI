// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "stdafx.h"


#include "MidiStreamMessageBuilderTests.h"

using namespace winrt::Windows::Devices::Midi2;

void MidiStreamMessageBuilderTests::TestBuildEndpointNameNotificationLong()
{
    winrt::hstring name = L"This is an endpoint name that is longer than the supported 98 characters for an endpoint name in MIDI 2";
    uint32_t expectedPacketCount = 7;


    std::cout << "Testing endpoint Name: " << winrt::to_string(name) << std::endl;

    auto messages = MidiStreamMessageBuilder::BuildEndpointNameNotificationMessages(
        MidiClock::Now(),
        name
    );

    std::cout << "Expected ump count " << expectedPacketCount << ", received " << messages.Size() << std::endl;

    // count the number of messages we get back
    VERIFY_ARE_EQUAL(messages.Size(), expectedPacketCount);

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
        VERIFY_ARE_EQUAL(MidiMessageUtility::GetStatusFromStreamMessageFirstWord(messages.GetAt(i).Word0()), MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION);

        // verify form is correct
        if (i == 0)
        {
            // first message
            VERIFY_ARE_EQUAL(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(i).Word0()), 0x01);
        }
        else if (i == messages.Size() - 1)
        {
            // last message
            VERIFY_ARE_EQUAL(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(i).Word0()), 0x03);
        }
        else
        {
            // interim messages
            VERIFY_ARE_EQUAL(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(i).Word0()), 0x02);
        }
    }

    // reverse it back into a string and verify

    auto s = MidiStreamMessageBuilder::ParseEndpointNameNotificationMessages(messages);
    std::cout << "Parsed: '" << winrt::to_string(s) << "'" << std::endl;
    VERIFY_ARE_EQUAL(s, L"This is an endpoint name that is longer than the supported 98 characters for an endpoint name in M");
}


void MidiStreamMessageBuilderTests::TestBuildEndpointNameNotificationMedium()
{
    winrt::hstring name = L"This is medium-sized";
    uint32_t expectedPacketCount = 2;

    std::cout << "Testing endpoint Name: " << winrt::to_string(name) << std::endl;

    auto messages = MidiStreamMessageBuilder::BuildEndpointNameNotificationMessages(
        MidiClock::Now(),
        name
    );

    std::cout << "Expected ump count " << expectedPacketCount << ", received " << messages.Size() << std::endl;

    // count the number of messages we get back
    VERIFY_ARE_EQUAL(messages.Size(), expectedPacketCount);

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
    VERIFY_ARE_EQUAL(MidiMessageUtility::GetStatusFromStreamMessageFirstWord(messages.GetAt(0).Word0()), MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION);
    VERIFY_ARE_EQUAL(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(0).Word0()), 0x01);

    VERIFY_ARE_EQUAL(MidiMessageUtility::GetStatusFromStreamMessageFirstWord(messages.GetAt(1).Word0()), MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION);
    VERIFY_ARE_EQUAL(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(1).Word0()), 0x03);

    // verify form is correct

    // reverse it back into a string and verify


    auto s = MidiStreamMessageBuilder::ParseEndpointNameNotificationMessages(messages);
    std::cout << "Parsed: '" << winrt::to_string(s) << "'" << std::endl;
    VERIFY_ARE_EQUAL(s, name);
    
}

void MidiStreamMessageBuilderTests::TestBuildEndpointNameNotificationShort()
{
    winrt::hstring name = L"Short";
    uint32_t expectedPacketCount = 1;

    std::cout << "Testing endpoint Name: " << winrt::to_string(name) << std::endl;

    auto messages = MidiStreamMessageBuilder::BuildEndpointNameNotificationMessages(
        MidiClock::Now(),
        name
    );

    std::cout << "Expected ump count " << expectedPacketCount << ", received " << messages.Size() << std::endl;

    std::cout << "Stream Message 1 word0 0x" << std::hex << messages.GetAt(0).Word0() << std::endl;

    // count the number of messages we get back
    VERIFY_ARE_EQUAL(messages.Size(), expectedPacketCount);

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
    VERIFY_ARE_EQUAL(MidiMessageUtility::GetStatusFromStreamMessageFirstWord(messages.GetAt(0).Word0()), MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION);

    // verify form is correct
    VERIFY_ARE_EQUAL(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(0).Word0()), 0x00);

    // reverse it back into a string and verify
    auto s = MidiStreamMessageBuilder::ParseEndpointNameNotificationMessages(messages);
    std::cout << "Parsed: '" << winrt::to_string(s) << "'" << std::endl;
    VERIFY_ARE_EQUAL(s, name);
}


void MidiStreamMessageBuilderTests::TestBuildProductInstanceIdNotificationShort()
{
    winrt::hstring productInstanceId = L"ABC123";
    uint32_t expectedPacketCount = 1;

    std::cout << "Testing endpoint Id: " << winrt::to_string(productInstanceId) << std::endl;

    auto messages = MidiStreamMessageBuilder::BuildProductInstanceIdNotificationMessages(
        MidiClock::Now(),
        productInstanceId
    );

    std::cout << "Expected ump count " << expectedPacketCount << ", received " << messages.Size() << std::endl;

    std::cout << "Stream Message 1 word0 0x" << std::hex << messages.GetAt(0).Word0() << std::endl;

    // count the number of messages we get back
    VERIFY_ARE_EQUAL(messages.Size(), expectedPacketCount);

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
    VERIFY_ARE_EQUAL(MidiMessageUtility::GetStatusFromStreamMessageFirstWord(messages.GetAt(0).Word0()), MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_PRODUCT_INSTANCE_ID_NOTIFICATION);

    // verify form is correct
    VERIFY_ARE_EQUAL(MidiMessageUtility::GetFormFromStreamMessageFirstWord(messages.GetAt(0).Word0()), 0x00);

    // reverse it back into a string and verify

}