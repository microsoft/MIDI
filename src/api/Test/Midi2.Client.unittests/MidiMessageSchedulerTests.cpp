// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "stdafx.h"

#include "MidiMessageSchedulerTests.h"


void MidiMessageSchedulerTests::TestScheduledMessages()
{
    LOG_OUTPUT(L"TestScheduledMessages **********************************************************************");

    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    auto session = MidiSession::CreateSession(L"Test Session Name");

    auto connSend = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackAEndpointId());
    auto connReceive = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackBEndpointId());

    uint32_t receivedMessageCount{};

    uint32_t lastReceivedMessageValue{ 0 };

    uint32_t word0 = 0x20000000;
    uint8_t sentMessageCount = 20;

    auto MessageReceivedHandler = [&](IMidiMessageReceivedEventSource const& sender, MidiMessageReceivedEventArgs const& args)
        {
            UNREFERENCED_PARAMETER(sender);

            receivedMessageCount++;

            std::cout << "Received: 0x" << std::hex << args.PeekFirstWord() << std::endl;

            if (lastReceivedMessageValue != 0)
            {
                // check to ensure messages are arriving in order they were sent
                VERIFY_ARE_EQUAL(args.PeekFirstWord(), lastReceivedMessageValue + 1);
            }

            // update the last received message with this value
            lastReceivedMessageValue = args.PeekFirstWord();


            if (receivedMessageCount == sentMessageCount)
            {
                allMessagesReceived.SetEvent();
            }

        };

    auto eventRevokeToken = connReceive.MessageReceived(MessageReceivedHandler);

    // open connection
    connSend.Open();
    connReceive.Open();


    // schedule all messages to arrive at the same time: 2 seconds into the future
    auto scheduledTimeStamp = MidiClock::OffsetTimestampByMilliseconds(MidiClock::Now(), 2000);

    std::cout << "Sending messages" << std::endl;

    // send messages

    for (uint32_t i = 0; i < sentMessageCount; i++)
    {
        uint32_t word = word0 + i;

        std::cout << "Sending: 0x" << std::hex << word << std::endl;

        // we increment the message value each time so we can keep track of order
        VERIFY_ARE_EQUAL(connSend.SendMessageWords(scheduledTimeStamp, word), MidiSendMessageResult::Success);
    }

    std::cout << "Waiting for response" << std::endl;

    // Wait for incoming message
    if (!allMessagesReceived.wait(10000))
    {
        std::cout << "Failure waiting for messages, timed out." << std::endl;
    }

    std::cout << "Finished waiting. Unwiring event" << std::endl;

    connReceive.MessageReceived(eventRevokeToken);

    VERIFY_ARE_EQUAL(receivedMessageCount, sentMessageCount);

    std::cout << "Disconnecting endpoints" << std::endl;

    // cleanup endpoint. Technically not required as session will do it
    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());

    std::cout << "Endpoints disconnected" << std::endl;

    session.Close();
}