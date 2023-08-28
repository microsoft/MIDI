// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// ----------------------------------------------------------------------------
// This requires the version of MidiSrv with the hard-coded loopback endpoint
// ----------------------------------------------------------------------------


#include "pch.h"

#include "catch_amalgamated.hpp"

#include <iostream>
#include <algorithm>
#include <Windows.h>

//#include "..\api-core\ump_helpers.h"
#include <wil\resource.h>

//using namespace winrt;
using namespace winrt::Windows::Devices::Midi2;


TEST_CASE("Connected.Endpoint.CreateOutput Create output endpoint")
{
    auto settings = MidiSessionSettings::Default();
    auto session = MidiSession::CreateSession(L"Test Session Name", settings);

    REQUIRE((bool)(session.IsOpen()));

    REQUIRE((bool)(session.Connections().Size() == 0));

    std::cout << "Connecting to Endpoint" << std::endl;

    auto conn1 = session.ConnectOutputEndpoint(LOOPBACK_OUT_ID, nullptr);

    REQUIRE(conn1 != nullptr);
    REQUIRE(!conn1.Id().empty());

    REQUIRE(conn1.Open());
    REQUIRE(conn1.IsOpen());

    REQUIRE(session.Connections().Size() == 1);

    std::cout << "Endpoint Id: " << winrt::to_string(conn1.Id()) << std::endl;
    std::cout << "Device Id: " << winrt::to_string(conn1.DeviceId()) << std::endl;
}


TEST_CASE("Connected.Endpoint.CreateInput Create input endpoint")
{
    auto settings = MidiSessionSettings::Default();
    auto session = MidiSession::CreateSession(L"Test Session Name", settings);

    REQUIRE((bool)(session.IsOpen()));

    REQUIRE((bool)(session.Connections().Size() == 0));

    std::cout << "Connecting to Endpoint" << std::endl;

    auto conn1 = session.ConnectInputEndpoint(LOOPBACK_IN_ID, nullptr);

    REQUIRE(conn1 != nullptr);
    REQUIRE(!conn1.Id().empty());

    REQUIRE(conn1.Open());
    REQUIRE(conn1.IsOpen());

    REQUIRE(session.Connections().Size() == 1);

    std::cout << "Endpoint Id: " << winrt::to_string(conn1.Id()) << std::endl;
    std::cout << "Device Id: " << winrt::to_string(conn1.DeviceId()) << std::endl;
}



TEST_CASE("Connected.Endpoint.SingleUmp Send and receive single Ump32 message through output and input endpoints")
{
    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    auto settings = MidiSessionSettings::Default();
    auto session = MidiSession::CreateSession(L"Test Session Name", settings);

    REQUIRE((bool)(session.IsOpen()));
    REQUIRE((bool)(session.Connections().Size() == 0));

    std::cout << std::endl << "Connecting to Output Endpoint" << std::endl;
    auto connOut = session.ConnectOutputEndpoint(LOOPBACK_OUT_ID, nullptr);
    REQUIRE((bool)(connOut != nullptr));

    std::cout << std::endl << "Connecting to Input Endpoint" << std::endl;
    auto connIn = session.ConnectInputEndpoint(LOOPBACK_IN_ID, nullptr);
    REQUIRE((bool)(connIn != nullptr));


    REQUIRE((bool)(session.Connections().Size() == 2));


    bool messageReceivedFlag = false;
    MidiUmp32 sentUmp;

    auto sentMessageType = MidiUmpMessageType::Midi1ChannelVoice32;
    auto sentTimestamp = MidiClock::GetMidiTimestamp();


    auto MessageReceivedHandler = [&](winrt::Windows::Foundation::IInspectable const& sender, MidiMessageReceivedEventArgs const& args)
        {
            REQUIRE((bool)(sender != nullptr));
            REQUIRE((bool)(args != nullptr));

            // strongly typed UMP
            auto receivedUmp = args.GetUmp();

            REQUIRE(receivedUmp != nullptr);

            // verify that the message that comes back is what we sent
            REQUIRE(receivedUmp.MessageType() == sentMessageType);
            REQUIRE(receivedUmp.Timestamp() == sentTimestamp);

            // Making an assumption on type here.
            MidiUmp32 receivedUmp32 = receivedUmp.as<MidiUmp32>();

            std::cout << "Received message in test" << std::endl;
            std::cout << " - UmpPacketType:     0x" << std::hex << (int)(receivedUmp32.UmpPacketType()) << std::endl;
            std::cout << " - Timestamp:         0x" << std::hex << (receivedUmp32.Timestamp()) << std::endl;
            std::cout << " - MessageType:       0x" << std::hex << (int)(receivedUmp32.MessageType()) << std::endl;
            std::cout << " - First Word:        0x" << std::hex << (receivedUmp32.Word0()) << std::endl << std::endl;

            messageReceivedFlag = true;
            allMessagesReceived.SetEvent();
        };

    auto eventRevokeToken = connIn.MessageReceived(MessageReceivedHandler);

    REQUIRE(connOut.Open());
    REQUIRE(connIn.Open());

    // send message

    sentUmp.MessageType(sentMessageType);
    sentUmp.Timestamp(sentTimestamp);

    std::cout << "Sending message" << std::hex << (uint32_t)(sentUmp.UmpPacketType()) << std::endl;
    std::cout << " - Timestamp:   0x" << std::hex << (uint64_t)(sentUmp.Timestamp()) << std::endl;
    std::cout << " - MessageType: 0x" << std::hex << (int)(sentUmp.MessageType()) << std::endl;
    std::cout << " - First Word:  0x" << std::hex << (sentUmp.Word0()) << std::endl << std::endl;

    connOut.SendUmp(sentUmp);


    // Wait for incoming message

    // Wait for incoming message
    if (!allMessagesReceived.wait(3000))
    {
        std::cout << "Failure waiting for messages, timed out." << std::endl;
    }

    REQUIRE(messageReceivedFlag);

    // unwire event
    connIn.MessageReceived(eventRevokeToken);

    // cleanup endpoint. Technically not required as session will do it
    session.DisconnectEndpointConnection(connOut.Id());
    session.DisconnectEndpointConnection(connIn.Id());
}




//TEST_CASE("Connected.Endpoint.MultipleUmpWords Send and receive multiple words")
//{
//    wil::unique_event_nothrow allMessagesReceived;
//    allMessagesReceived.create();
//
//
//    //  uint64_t setupStartTimestamp = MidiClock::GetMidiTimestamp();
//
//
//    auto settings = MidiSessionSettings::Default();
//    auto session = MidiSession::CreateSession(L"Test Session Name", settings);
//
//    REQUIRE((bool)(session.IsOpen()));
//    REQUIRE((bool)(session.Connections().Size() == 0));
//
//    std::cout << "Connecting to Endpoint" << std::endl;
//
//    auto conn1 = session.ConnectBidirectionalEndpoint(LOOPBACK_BIDI_ID, nullptr);
//
//    REQUIRE((bool)(conn1 != nullptr));
//
//    uint32_t receivedMessageCount{};
//
//    uint32_t numMessagesToSend = 10;
//
//    auto MessageReceivedHandler = [&](winrt::Windows::Foundation::IInspectable const& sender, MidiMessageReceivedEventArgs const& args)
//        {
//            REQUIRE((bool)(sender != nullptr));
//            REQUIRE((bool)(args != nullptr));
//
//
//
//
//
//
//            receivedMessageCount++;
//
//            if (receivedMessageCount == numMessagesToSend)
//            {
//                allMessagesReceived.SetEvent();
//            }
//
//        };
//
//    auto eventRevokeToken = conn1.MessageReceived(MessageReceivedHandler);
//
//    // open connection
//    REQUIRE(conn1.Open());
//
//
//    // send messages
//
//    uint32_t numBytes = 0;
//
//
//    uint32_t words[]{ 0,0,0,0 };
//    uint32_t wordCount = 0;
//
//    for (uint32_t i = 0; i < numMessagesToSend; i++)
//    {
//        auto timestamp = MidiClock::GetMidiTimestamp();
//
//        switch (i % 4)
//        {
//        case 0:
//        {
//            words[0] = 0x20000000;
//            wordCount = (uint32_t)(MidiUmpPacketType::Ump32);
//
//        }
//        break;
//        case 1:
//        {
//            words[0] = 0x40000000;
//            wordCount = (uint32_t)(MidiUmpPacketType::Ump64);
//        }
//        break;
//        case 2:
//        {
//            words[0] = 0xB0000000;
//            wordCount = (uint32_t)(MidiUmpPacketType::Ump96);
//        }
//        break;
//        case 3:
//        {
//            words[0] = 0xF0000000;
//            wordCount = (uint32_t)(MidiUmpPacketType::Ump128);
//        }
//        break;
//        }
//
//        numBytes += sizeof(uint32_t) * wordCount + sizeof(uint64_t);
//        conn1.SendUmpWordArray(timestamp, words, wordCount);
//
//    }
//    // Wait for incoming message
//    if (!allMessagesReceived.wait(3000))
//    {
//        std::cout << "Failure waiting for messages, timed out." << std::endl;
//    }
//
//    //    REQUIRE(messageReceivedFlag);
//
//    REQUIRE(receivedMessageCount == numMessagesToSend);
//
//    // unwire event
//    //conn1.WordsReceived(eventRevokeToken);
//
//    // cleanup endpoint. Technically not required as session will do it
//    session.DisconnectEndpointConnection(conn1.Id());
//}
