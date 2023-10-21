// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "stdafx.h"


#include "MidiEndpointConnectionTests.h"


#include <wil\resource.h>

using namespace winrt::Windows::Devices::Midi2;


void MidiEndpointConnectionTests::TestCreateBiDiLoopbackA()
{
    LOG_OUTPUT(L"TestCreateBiDiLoopbackA **********************************************************************");

    auto session = MidiSession::CreateSession(L"Test Session Name");

    VERIFY_IS_TRUE(session.IsOpen());

    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)0);

    LOG_OUTPUT(L"Connecting to Bidirectional Loopback Endpoint A");

    auto conn1 = session.CreateEndpointConnection(MIDI_DIAGNOSTICS_LOOPBACK_BIDI_ID_A);

    VERIFY_IS_NOT_NULL(conn1);

    VERIFY_IS_TRUE(conn1.Open());
    VERIFY_IS_TRUE(conn1.IsOpen());

    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)1);

    session.DisconnectEndpointConnection(conn1.ConnectionId());
    session.Close();
}

void MidiEndpointConnectionTests::TestCreateBiDiLoopbackB()
{
    LOG_OUTPUT(L"TestCreateBiDiLoopbackB **********************************************************************");

    auto session = MidiSession::CreateSession(L"Test Session Name");

    VERIFY_IS_TRUE(session.IsOpen());
    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)0);

    LOG_OUTPUT(L"Connecting to Bidirectional Loopback Endpoint B");

    auto conn1 = session.CreateEndpointConnection(MIDI_DIAGNOSTICS_LOOPBACK_BIDI_ID_B);

    VERIFY_IS_NOT_NULL(conn1);

    VERIFY_IS_TRUE(conn1.Open());
    VERIFY_IS_TRUE(conn1.IsOpen());

    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)1);

    session.DisconnectEndpointConnection(conn1.ConnectionId());
    session.Close();
}

void MidiEndpointConnectionTests::TestSendAndReceiveUmpStruct()
{
    LOG_OUTPUT(L"TestSendAndReceiveUmpStruct **********************************************************************");

    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    auto session = MidiSession::CreateSession(L"Test Session Name");

    VERIFY_IS_TRUE(session.IsOpen());
    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)0);

    LOG_OUTPUT(L"Connecting to both Loopback A and Loopback B");

    MidiEndpointConnectionOptions options;

    auto connSend = session.CreateEndpointConnection(MIDI_DIAGNOSTICS_LOOPBACK_BIDI_ID_A, options);
    auto connReceive = session.CreateEndpointConnection(MIDI_DIAGNOSTICS_LOOPBACK_BIDI_ID_B, options);

    VERIFY_IS_NOT_NULL(connSend);
    VERIFY_IS_NOT_NULL(connReceive);

    bool messageReceivedFlag = false;
    MidiMessageStruct sentUmp;

 //   auto sentMessageType = MidiMessageType::Midi2ChannelVoice64;
    auto sentTimestamp = MidiClock::GetMidiTimestamp();

    auto MessageReceivedHandler = [&](winrt::Windows::Foundation::IInspectable const& sender, MidiMessageReceivedEventArgs const& args)
        {
            MidiMessageStruct receivedUmp;

            VERIFY_IS_NOT_NULL(sender);
            VERIFY_IS_NOT_NULL(args);

            // strongly typed UMP
            args.FillMessageStruct(receivedUmp);

            std::cout << "Received message in test" << std::endl;
            std::cout << " - Timestamp:         0x" << std::hex << (args.Timestamp()) << std::endl;
            std::cout << " - MessageType:       0x" << std::hex << (int)(args.MessageType()) << std::endl;
            std::cout << " - First Word:        0x" << std::hex << (receivedUmp.Word0) << std::endl << std::endl;

            messageReceivedFlag = true;
            allMessagesReceived.SetEvent();
        };

    auto eventRevokeToken = connReceive.MessageReceived(MessageReceivedHandler);

    VERIFY_IS_TRUE(connSend.Open());
    VERIFY_IS_TRUE(connReceive.Open());

    // send message

    uint32_t firstWord = 0x41234567;
    sentUmp.Word0 = firstWord;
    sentUmp.Word1 = 0x11112222;

    std::cout << "Sending message struct" << std::endl;
    std::cout << " - Timestamp:   0x" << std::hex << (uint64_t)(sentTimestamp) << std::endl;
    std::cout << " - First Word:  0x" << std::hex << (sentUmp.Word0) << std::endl << std::endl;

    VERIFY_ARE_EQUAL(connSend.SendMessageStruct(sentTimestamp, sentUmp, 2), MidiSendMessageResult::Success);


    // Wait for incoming message
    if (!allMessagesReceived.wait(3000))
    {
        std::cout << "Failure waiting for messages, timed out." << std::endl;
    }

    VERIFY_IS_TRUE(messageReceivedFlag);

    // unwire event
    connReceive.MessageReceived(eventRevokeToken);

    // cleanup endpoint. Technically not required as session will do it
    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());

    session.Close();
}

void MidiEndpointConnectionTests::TestSendAndReceiveUmp32()
{
    LOG_OUTPUT(L"TestSendAndReceiveUmp32 **********************************************************************");

    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    auto session = MidiSession::CreateSession(L"Test Session Name");

    VERIFY_IS_TRUE(session.IsOpen());
    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)0);


    std::cout << std::endl << "Connecting to both Loopback A and Loopback B" << std::endl;


    MidiEndpointConnectionOptions options;

    auto connSend = session.CreateEndpointConnection(MIDI_DIAGNOSTICS_LOOPBACK_BIDI_ID_A, options);
    auto connReceive = session.CreateEndpointConnection(MIDI_DIAGNOSTICS_LOOPBACK_BIDI_ID_B, options);

    VERIFY_IS_NOT_NULL(connSend);
    VERIFY_IS_NOT_NULL(connReceive);


    bool messageReceivedFlag = false;
    MidiMessage32 sentUmp;

    auto sentMessageType = MidiMessageType::Midi1ChannelVoice32;
    auto sentTimestamp = MidiClock::GetMidiTimestamp();


    auto MessageReceivedHandler = [&](winrt::Windows::Foundation::IInspectable const& sender, MidiMessageReceivedEventArgs const& args)
        {
            VERIFY_IS_NOT_NULL(sender);
            VERIFY_IS_NOT_NULL(args);

            // strongly typed UMP
            auto receivedUmp = args.GetMessagePacket();

            VERIFY_IS_NOT_NULL(receivedUmp);

            // verify that the message that comes back is what we sent
            VERIFY_ARE_EQUAL(receivedUmp.MessageType(), sentMessageType);
            VERIFY_ARE_EQUAL(receivedUmp.Timestamp(), sentTimestamp);

            // Making an assumption on type here.
            MidiMessage32 receivedUmp32 = receivedUmp.as<MidiMessage32>();

            std::cout << "Received message in test" << std::endl;
            std::cout << " - PacketType:        0x" << std::hex << (uint8_t)(receivedUmp32.PacketType()) << std::endl;
            std::cout << " - Timestamp:         0x" << std::hex << (receivedUmp32.Timestamp()) << std::endl;
            std::cout << " - MessageType:       0x" << std::hex << (uint8_t)(receivedUmp32.MessageType()) << std::endl;
            std::cout << " - First Word:        0x" << std::hex << (receivedUmp32.Word0()) << std::endl << std::endl;

            messageReceivedFlag = true;
            allMessagesReceived.SetEvent();
        };

    auto eventRevokeToken = connReceive.MessageReceived(MessageReceivedHandler);

    VERIFY_IS_TRUE(connSend.Open());
    VERIFY_IS_TRUE(connReceive.Open());


    // send message

    sentUmp.MessageType(sentMessageType);
    sentUmp.Timestamp(sentTimestamp);

    std::cout << "Sending message" << std::hex << (uint32_t)(sentUmp.PacketType()) << std::endl;
    std::cout << " - Timestamp:   0x" << std::hex << (uint64_t)(sentUmp.Timestamp()) << std::endl;
    std::cout << " - MessageType: 0x" << std::hex << (uint8_t)(sentUmp.MessageType()) << std::endl;
    std::cout << " - First Word:  0x" << std::hex << (sentUmp.Word0()) << std::endl << std::endl;

    connSend.SendMessagePacket(sentUmp);


    // Wait for incoming message

    // Wait for incoming message
    if (!allMessagesReceived.wait(3000))
    {
        std::cout << "Failure waiting for messages, timed out." << std::endl;
    }

    VERIFY_IS_TRUE(messageReceivedFlag);

    // unwire event
    connReceive.MessageReceived(eventRevokeToken);

    // cleanup endpoint. Technically not required as session will do it
    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());

    session.Close();
}

void MidiEndpointConnectionTests::TestSendAndReceiveWords()
{
    LOG_OUTPUT(L"TestSendAndReceiveWords **********************************************************************");

    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();


  //  uint64_t setupStartTimestamp = MidiClock::GetMidiTimestamp();


    auto session = MidiSession::CreateSession(L"Test Session Name");

    VERIFY_IS_TRUE(session.IsOpen());
    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)0);


    LOG_OUTPUT(L"Connecting to BiDi loopback Endpoints A and B");


    MidiEndpointConnectionOptions options;

    auto connSend = session.CreateEndpointConnection(MIDI_DIAGNOSTICS_LOOPBACK_BIDI_ID_A, options);
    auto connReceive = session.CreateEndpointConnection(MIDI_DIAGNOSTICS_LOOPBACK_BIDI_ID_B, options);

    VERIFY_IS_NOT_NULL(connSend);
    VERIFY_IS_NOT_NULL(connReceive);


    uint32_t receivedMessageCount{};

    uint32_t numMessagesToSend = 10;

    auto MessageReceivedHandler = [&](winrt::Windows::Foundation::IInspectable const& sender, MidiMessageReceivedEventArgs const& args)
        {
            VERIFY_IS_NOT_NULL(sender);
            VERIFY_IS_NOT_NULL(args);

            receivedMessageCount++;

            if (receivedMessageCount == numMessagesToSend)
            {
                allMessagesReceived.SetEvent();
            }

        };

    auto eventRevokeToken = connReceive.MessageReceived(MessageReceivedHandler);

    // open connection
    VERIFY_IS_TRUE(connSend.Open());
    VERIFY_IS_TRUE(connReceive.Open());



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

        VERIFY_ARE_EQUAL(connSend.SendMessageWordArray(timestamp, words, 0, wordCount), MidiSendMessageResult::Success);

    }

    std::cout << "Waiting for response" << std::endl;

    // Wait for incoming message
    if (!allMessagesReceived.wait(3000))
    {
        std::cout << "Failure waiting for messages, timed out." << std::endl;
    }

    std::cout << "Finished waiting. Unwiring event" << std::endl;

    connReceive.MessageReceived(eventRevokeToken);

    VERIFY_ARE_EQUAL(receivedMessageCount, numMessagesToSend);

    std::cout << "Disconnecting endpoints" << std::endl;

    // cleanup endpoint. Technically not required as session will do it
    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());

    std::cout << "Endpoints disconnected" << std::endl;

    session.Close();
}
