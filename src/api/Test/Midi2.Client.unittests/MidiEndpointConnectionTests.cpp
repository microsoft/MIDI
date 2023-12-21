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


void MidiEndpointConnectionTests::TestSendMessageInvalidConnectionFailureReturnCode()
{
    LOG_OUTPUT(L"TestSendMessageInvalidConnectionFailureReturnCode **********************************************************************");

    auto session = MidiSession::CreateSession(L"Test Session Name");

    VERIFY_IS_TRUE(session.IsOpen());

    auto connSend = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackAEndpointId());

    VERIFY_IS_NOT_NULL(connSend);
    //VERIFY_IS_TRUE(connSend.Open());      // we don't open the connection here


    // wrong message type for word count
    auto connectionClosedResult = connSend.SendMessageWords(0, 0x21111111);

    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageFailed(connectionClosedResult));
    VERIFY_IS_TRUE((connectionClosedResult & MidiSendMessageResult::EndpointConnectionClosedOrInvalid) == MidiSendMessageResult::EndpointConnectionClosedOrInvalid);


    // cleanup endpoint. Technically not required as session will do it
    session.DisconnectEndpointConnection(connSend.ConnectionId());

    session.Close();
}


void MidiEndpointConnectionTests::TestSendMessageValidationFailureReturnCode()
{
    LOG_OUTPUT(L"TestSendMessageValidationFailureReturnCode **********************************************************************");

    auto session = MidiSession::CreateSession(L"Test Session Name");

    VERIFY_IS_TRUE(session.IsOpen());

    auto connSend = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackAEndpointId());

    VERIFY_IS_NOT_NULL(connSend);
    VERIFY_IS_TRUE(connSend.Open());


    // wrong message type for word count
    auto badMessageTypeResult = connSend.SendMessageWords(0, 0x41111111);

    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageFailed(badMessageTypeResult));
    VERIFY_IS_TRUE((badMessageTypeResult & MidiSendMessageResult::InvalidMessageTypeForWordCount) == MidiSendMessageResult::InvalidMessageTypeForWordCount);


    // cleanup endpoint. Technically not required as session will do it
    session.DisconnectEndpointConnection(connSend.ConnectionId());

    session.Close();
}

//void MidiEndpointConnectionTests::TestSendMessageSuccessScheduledReturnCode()
//{
//    LOG_OUTPUT(L"TestSendMessageSuccessScheduledReturnCode **********************************************************************");
//
//    auto session = MidiSession::CreateSession(L"Test Session Name");
//
//    VERIFY_IS_TRUE(session.IsOpen());
//
//    auto connSend = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackAEndpointId());
//
//    VERIFY_IS_NOT_NULL(connSend);
//    VERIFY_IS_TRUE(connSend.Open());
//
//    // scheduled
//    auto scheduledResult = connSend.SendMessageWords(MidiClock::OffsetTimestampByMilliseconds(MidiClock::Now(), 2000), 0x27654321);
//
//    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(scheduledResult));
//    VERIFY_IS_TRUE((scheduledResult & MidiSendMessageResult::Scheduled) == MidiSendMessageResult::Scheduled);
//
//
//    // cleanup endpoint. Technically not required as session will do it
//    session.DisconnectEndpointConnection(connSend.ConnectionId());
//
//    session.Close();
//}
//
//void MidiEndpointConnectionTests::TestSendMessageSuccessImmediateReturnCode()
//{
//    LOG_OUTPUT(L"TestSendMessageSuccessImmediateReturnCode **********************************************************************");
//
//    auto session = MidiSession::CreateSession(L"Test Session Name");
//
//    VERIFY_IS_TRUE(session.IsOpen());
//
//    auto connSend = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackAEndpointId());
//
//    VERIFY_IS_NOT_NULL(connSend);
//    VERIFY_IS_TRUE(connSend.Open());
//
//
//    // immediate
//    auto immediateResult = connSend.SendMessageWords(0, 0x21234567);
//
//    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(immediateResult));
//    VERIFY_IS_TRUE((immediateResult & MidiSendMessageResult::SentImmediately) == MidiSendMessageResult::SentImmediately);
//
//    // cleanup endpoint. Technically not required as session will do it
//    session.DisconnectEndpointConnection(connSend.ConnectionId());
//
//    session.Close();
//}


void MidiEndpointConnectionTests::TestCreateBiDiLoopbackA()
{
    LOG_OUTPUT(L"TestCreateBiDiLoopbackA **********************************************************************");

    auto session = MidiSession::CreateSession(L"Test Session Name");

    VERIFY_IS_TRUE(session.IsOpen());

    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)0);

    LOG_OUTPUT(L"Connecting to Bidirectional Loopback Endpoint A");

    auto conn1 = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackAEndpointId());


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

    auto conn1 = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackBEndpointId());

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

    auto connSend = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackAEndpointId(), options);
    auto connReceive = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackBEndpointId(), options);

    VERIFY_IS_NOT_NULL(connSend);
    VERIFY_IS_NOT_NULL(connReceive);

    bool messageReceivedFlag = false;
    MidiMessageStruct sentUmp;

 //   auto sentMessageType = MidiMessageType::Midi2ChannelVoice64;
    auto sentTimestamp = MidiClock::Now();

    auto MessageReceivedHandler = [&](IMidiMessageReceivedEventSource const& sender, MidiMessageReceivedEventArgs const& args)
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

    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendMessageStruct(sentTimestamp, sentUmp, 2)));


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

    auto connSend = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackAEndpointId(), options);
    auto connReceive = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackBEndpointId(), options);

    VERIFY_IS_NOT_NULL(connSend);
    VERIFY_IS_NOT_NULL(connReceive);


    bool messageReceivedFlag = false;
    MidiMessage32 sentUmp;

    auto sentMessageType = MidiMessageType::Midi1ChannelVoice32;
    auto sentTimestamp = MidiClock::Now();


    auto MessageReceivedHandler = [&](IMidiMessageReceivedEventSource const& sender, MidiMessageReceivedEventArgs const& args)
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

    auto connSend = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackAEndpointId(), options);
    auto connReceive = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackBEndpointId(), options);

    VERIFY_IS_NOT_NULL(connSend);
    VERIFY_IS_NOT_NULL(connReceive);


    uint32_t receivedMessageCount{};

    uint32_t numMessagesToSend = 10;

    auto MessageReceivedHandler = [&](IMidiMessageReceivedEventSource const& sender, MidiMessageReceivedEventArgs const& args)
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

    uint32_t words[4]{};
    uint8_t wordCount = 0;

    std::cout << "Creating message" << std::endl;

    for (uint32_t i = 0; i < numMessagesToSend; i++)
    {
        auto timestamp = MidiClock::Now();

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

        auto result = connSend.SendMessageWordArray(timestamp, words, 0, wordCount);
        std::cout << "Send result: 0x" << std::hex << (uint32_t)result << std::endl;

        VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(result));

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



void MidiEndpointConnectionTests::TestSendWordArrayBoundsError()
{
    LOG_OUTPUT(L"TestSendWordArrayBoundsError **********************************************************************");

    auto session = MidiSession::CreateSession(L"Test Session Name");

    VERIFY_IS_TRUE(session.IsOpen());
    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)0);

    auto connSend = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackAEndpointId());

    VERIFY_IS_NOT_NULL(connSend);

    uint32_t sendBuffer[4]{};

    sendBuffer[0] = 0x41234567;
    sendBuffer[1] = 0xDEADBEEF;
    sendBuffer[2] = 0x41234567;
    sendBuffer[3] = 0x41234567;


    VERIFY_IS_TRUE(connSend.Open());

    // out of bounds
    auto result = connSend.SendMessageWordArray(0, sendBuffer, 3, 2);
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageFailed(result));
    VERIFY_IS_TRUE((result & MidiSendMessageResult::DataIndexOutOfRange) == MidiSendMessageResult::DataIndexOutOfRange);

    session.DisconnectEndpointConnection(connSend.ConnectionId());

    session.Close();
}


void MidiEndpointConnectionTests::TestSendAndReceiveWordArray()
{
    LOG_OUTPUT(L"TestSendAndReceiveWordArray **********************************************************************");

    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    auto session = MidiSession::CreateSession(L"Test Session Name");

    VERIFY_IS_TRUE(session.IsOpen());
    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)0);

    auto connSend = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackAEndpointId());
    auto connReceive = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackBEndpointId());

    VERIFY_IS_NOT_NULL(connSend);
    VERIFY_IS_NOT_NULL(connReceive);

    bool messageReceivedFlag = false;

    uint32_t receiveBuffer[50]{};
    uint8_t sentWordCount = 2;
    uint32_t sentIndex = 10;
    uint32_t receiveIndex = 20;

    uint32_t sendBuffer[50]{};

    sendBuffer[sentIndex + 0] = 0x41234567;
    sendBuffer[sentIndex + 1] = 0xDEADBEEF;

    auto MessageReceivedHandler = [&](IMidiMessageReceivedEventSource const& sender, MidiMessageReceivedEventArgs const& args)
        {
            VERIFY_IS_NOT_NULL(sender);
            VERIFY_IS_NOT_NULL(args);

            // testing that we fill at the correct offset
            auto wordCount = args.FillWordArray(receiveBuffer, receiveIndex);

            VERIFY_ARE_EQUAL(sentWordCount, wordCount);

            // check to see that we received what we sent
            for (int i = 0; i < sentWordCount; i++)
            {
                VERIFY_ARE_EQUAL(sendBuffer[sentIndex+i], receiveBuffer[receiveIndex+i]);
            }


            messageReceivedFlag = true;
            allMessagesReceived.SetEvent();
        };

    auto eventRevokeToken = connReceive.MessageReceived(MessageReceivedHandler);

    VERIFY_IS_TRUE(connSend.Open());
    VERIFY_IS_TRUE(connReceive.Open());

    auto result = connSend.SendMessageWordArray(0, sendBuffer, sentIndex, sentWordCount);

    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(result));


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


