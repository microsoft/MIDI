// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"

#include "catch_amalgamated.hpp"

#include <algorithm>
#include <Windows.h>

//#include "..\api-core\ump_helpers.h"
#include <wil\resource.h>

//using namespace winrt;
using namespace winrt::Windows::Devices::Midi2;


TEST_CASE("Connected.Endpoint.CreateBidi Create bidirectional A endpoint")
{
    auto session = MidiSession::CreateSession(L"Test Session Name");

    REQUIRE((bool)(session.IsOpen()));

    REQUIRE((bool)(session.Connections().Size() == 0));

    std::cout << "Connecting to Bidirectional Loopback Endpoint A" << std::endl;

    auto conn1 = session.ConnectBidirectionalEndpoint(LOOPBACK_BIDI_ID_A);

    REQUIRE(conn1 != nullptr);

    REQUIRE(conn1.Open());
    REQUIRE(conn1.IsOpen());

    REQUIRE(session.Connections().Size() == 1);
}

TEST_CASE("Connected.Endpoint.CreateBidi Create bidirectional B endpoint")
{
    auto session = MidiSession::CreateSession(L"Test Session Name");

    REQUIRE((bool)(session.IsOpen()));

    REQUIRE((bool)(session.Connections().Size() == 0));

    std::cout << "Connecting to Bidirectional Loopback Endpoint B" << std::endl;

    auto conn1 = session.ConnectBidirectionalEndpoint(LOOPBACK_BIDI_ID_B);

    REQUIRE(conn1 != nullptr);

    REQUIRE(conn1.Open());
    REQUIRE(conn1.IsOpen());

    REQUIRE(session.Connections().Size() == 1);
}

TEST_CASE("Connected.Endpoint.SingleUmp Send and receive single Ump32 message")
{
    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    auto session = MidiSession::CreateSession(L"Test Session Name");

    REQUIRE((bool)(session.IsOpen()));
    REQUIRE((bool)(session.Connections().Size() == 0));

    std::cout << std::endl << "Connecting to both Loopback A and Loopback B" << std::endl;

    auto connSend = session.ConnectBidirectionalEndpoint(LOOPBACK_BIDI_ID_A);
    auto connReceive = session.ConnectBidirectionalEndpoint(LOOPBACK_BIDI_ID_B);

    REQUIRE((bool)(connSend != nullptr));
    REQUIRE((bool)(connReceive != nullptr));

    bool messageReceivedFlag = false;
    MidiMessage32 sentUmp;

    auto sentMessageType = MidiMessageType::Midi1ChannelVoice32;
    auto sentTimestamp = MidiClock::GetMidiTimestamp();


    auto MessageReceivedHandler = [&](winrt::Windows::Foundation::IInspectable const& sender, MidiMessageReceivedEventArgs const& args)
        {
            REQUIRE((bool)(sender != nullptr));
            REQUIRE((bool)(args != nullptr));

            // strongly typed UMP
            auto receivedUmp = args.GetMessagePacket();

            REQUIRE(receivedUmp != nullptr);

            // verify that the message that comes back is what we sent
            REQUIRE(receivedUmp.MessageType() == sentMessageType);
            REQUIRE(receivedUmp.Timestamp() == sentTimestamp);

            // Making an assumption on type here.
            MidiMessage32 receivedUmp32 = receivedUmp.as<MidiMessage32>();

            std::cout << "Received message in test" << std::endl;
            std::cout << " - PacketType:        0x" << std::hex << (int)(receivedUmp32.PacketType()) << std::endl;
            std::cout << " - Timestamp:         0x" << std::hex << (receivedUmp32.Timestamp()) << std::endl;
            std::cout << " - MessageType:       0x" << std::hex << (int)(receivedUmp32.MessageType()) << std::endl;
            std::cout << " - First Word:        0x" << std::hex << (receivedUmp32.Word0()) << std::endl << std::endl;

            messageReceivedFlag = true;
            allMessagesReceived.SetEvent();
        };

    auto eventRevokeToken = connReceive.MessageReceived(MessageReceivedHandler);

    REQUIRE(connSend.Open());
    REQUIRE(connReceive.Open());

    // send message

    sentUmp.MessageType(sentMessageType);
    sentUmp.Timestamp(sentTimestamp);

    std::cout << "Sending message" << std::hex << (uint32_t)(sentUmp.PacketType()) << std::endl;
    std::cout << " - Timestamp:   0x" << std::hex << (uint64_t)(sentUmp.Timestamp()) << std::endl;
    std::cout << " - MessageType: 0x" << std::hex << (int)(sentUmp.MessageType()) << std::endl;
    std::cout << " - First Word:  0x" << std::hex << (sentUmp.Word0()) << std::endl << std::endl;

    connSend.SendMessagePacket(sentUmp);


    // Wait for incoming message

    // Wait for incoming message
    if (!allMessagesReceived.wait(3000))
    {
        std::cout << "Failure waiting for messages, timed out." << std::endl;
    }

    REQUIRE(messageReceivedFlag);

    // unwire event
    connReceive.MessageReceived(eventRevokeToken);

    // cleanup endpoint. Technically not required as session will do it
    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());
}



TEST_CASE("Connected.Endpoint.MultipleUmpWords Send and receive multiple words")
{
    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();


  //  uint64_t setupStartTimestamp = MidiClock::GetMidiTimestamp();


    auto session = MidiSession::CreateSession(L"Test Session Name");

    REQUIRE((bool)(session.IsOpen()));
    REQUIRE((bool)(session.Connections().Size() == 0));

    std::cout << "Connecting to BiDi loopback Endpoints A and B" << std::endl;

    auto connSend = session.ConnectBidirectionalEndpoint(LOOPBACK_BIDI_ID_A);
    auto connReceive = session.ConnectBidirectionalEndpoint(LOOPBACK_BIDI_ID_B);

    REQUIRE((bool)(connSend != nullptr));
    REQUIRE((bool)(connReceive != nullptr));

    uint32_t receivedMessageCount{};

    uint32_t numMessagesToSend = 10;

    auto MessageReceivedHandler = [&](winrt::Windows::Foundation::IInspectable const& sender, MidiMessageReceivedEventArgs const& args)
        {
            REQUIRE((bool)(sender != nullptr));
            REQUIRE((bool)(args != nullptr));

            receivedMessageCount++;

            if (receivedMessageCount == numMessagesToSend)
            {
                allMessagesReceived.SetEvent();
            }

        };

    auto eventRevokeToken = connReceive.MessageReceived(MessageReceivedHandler);

    // open connection
    REQUIRE(connSend.Open());
    REQUIRE(connReceive.Open());


    // send messages

    uint32_t words[]{ 0,0,0,0 };
    uint8_t wordCount = 0;

    std::cout << "Creating message" << std::endl;

    for (uint32_t i = 0; i < numMessagesToSend; i++)
    {
        auto timestamp = MidiClock::GetMidiTimestamp();

        switch (i % 4)
        {
        case 0:
        {
            words[0] = 0x20000000;
            wordCount = (uint32_t)(MidiPacketType::UniversalMidiPacket32);

        }
        break;
        case 1:
        {
            words[0] = 0x40000000;
            wordCount = (uint32_t)(MidiPacketType::UniversalMidiPacket64);
        }
        break;
        case 2:
        {
            words[0] = 0xB0000000;
            wordCount = (uint32_t)(MidiPacketType::UniversalMidiPacket96);
        }
        break;
        case 3:
        {
            words[0] = 0xF0000000;
            wordCount = (uint32_t)(MidiPacketType::UniversalMidiPacket128);
        }
        break;
        }

        std::cout << "Sending UMP Word Array" << std::endl;

        connSend.SendMessageWordArray(timestamp, words, 0, wordCount);

    }

    std::cout << "Waiting for response" << std::endl;

    // Wait for incoming message
    if (!allMessagesReceived.wait(3000))
    {
        std::cout << "Failure waiting for messages, timed out." << std::endl;
    }

    std::cout << "Finished waiting. Unwiring event" << std::endl;

    connReceive.MessageReceived(eventRevokeToken);

//    REQUIRE(messageReceivedFlag);

    REQUIRE(receivedMessageCount == numMessagesToSend);

    // unwire event
    //conn1.WordsReceived(eventRevokeToken);

    std::cout << "Disconnecting endpoints" << std::endl;

    // cleanup endpoint. Technically not required as session will do it
    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());

    std::cout << "Endpoints disconnected" << std::endl;

}
