// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "stdafx.h"

#include "MidiMessageSchedulerTests.h"

void MidiMessageSchedulerTests::TestScheduledMessagesTimingSmall()
{
    LOG_OUTPUT(L"Test timing small **********************************************************************");

    TestScheduledMessagesTiming(25);
}

void MidiMessageSchedulerTests::TestScheduledMessagesTimingLarge()
{
    LOG_OUTPUT(L"Test timing large **********************************************************************");

    TestScheduledMessagesTiming(100);
}



_Use_decl_annotations_
void MidiMessageSchedulerTests::TestScheduledMessagesTiming(uint16_t const messageCount)
{
    LOG_OUTPUT(L"Test timing **********************************************************************");

    wil::unique_event_nothrow allMessagesReceived;
    wil::critical_section messageLock;

    allMessagesReceived.create();

    auto session = MidiSession::CreateSession(L"Test Session Name");

    auto connSend = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackAEndpointId());
    auto connReceive = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackBEndpointId());

    uint32_t receivedMessageCount{};

    uint32_t word0 = 0x20000000;

    
    // calculate an acceptable timestamp offset
    uint32_t acceptableTimestampDeltaMicroseconds = 200;    // 0.2ms

    uint64_t acceptableTimestampDeltaTicks = (acceptableTimestampDeltaMicroseconds * MidiClock::TimestampFrequency()) / 1000000;


    std::cout << "Acceptable timestamp delta is +/- " << std::dec << acceptableTimestampDeltaTicks << " ticks" << std::endl;

    uint32_t scheduledTimeStampOffsetMS = 2000; // time we're scheduling out from send time


    auto MessageReceivedHandler = [&](IMidiMessageReceivedEventSource const& sender, MidiMessageReceivedEventArgs const& args)
        {
            // make sure it's one of our messages

            try
            {
                auto receivedTimestamp = MidiClock::Now();

                UNREFERENCED_PARAMETER(sender);

                //auto lock = messageLock.lock();

                receivedMessageCount++;

                // verify the timestamp is within an acceptable performance window
                VERIFY_IS_GREATER_THAN_OR_EQUAL(receivedTimestamp, args.Timestamp() - acceptableTimestampDeltaTicks);
                VERIFY_IS_LESS_THAN_OR_EQUAL(receivedTimestamp, args.Timestamp() + acceptableTimestampDeltaTicks);

                //lock.reset();

                if (receivedMessageCount == messageCount)
                {
                    allMessagesReceived.SetEvent();
                }
            }
            catch (...)
            {
                VERIFY_FAIL();
            }
        };

    auto eventRevokeToken = connReceive.MessageReceived(MessageReceivedHandler);

    // open connection
    connSend.Open();
    connReceive.Open();


    std::cout << "Sending messages" << std::endl;

    // send messages

    for (uint32_t i = 0; i < messageCount; i++)
    {
        uint32_t word = word0 + i;

        std::cout << "Sending: 0x" << std::hex << word << std::endl;

        // we increment the message value each time so we can keep track of order as well

        auto sendResult = connSend.SendMessageWords(MidiClock::OffsetTimestampByMilliseconds(MidiClock::Now(), scheduledTimeStampOffsetMS), word);

        VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(sendResult));
    }

    std::cout << "Waiting for response" << std::endl;

    // Wait for incoming message
    if (!allMessagesReceived.wait(15000))
    {
        std::cout << "Failure waiting for messages, timed out." << std::endl;
    }

    std::cout << "Finished waiting. Unwiring event" << std::endl;

    connReceive.MessageReceived(eventRevokeToken);

    VERIFY_ARE_EQUAL(receivedMessageCount, messageCount);

    std::cout << "Disconnecting endpoints" << std::endl;

    // cleanup endpoint. Technically not required as session will do it
    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());

    std::cout << "Endpoints disconnected" << std::endl;

    session.Close();
}

void MidiMessageSchedulerTests::TestScheduledMessagesOrder()
{
    LOG_OUTPUT(L"TestScheduledMessages **********************************************************************");

    wil::unique_event_nothrow allMessagesReceived;
    wil::critical_section messageLock;

    allMessagesReceived.create();

    auto session = MidiSession::CreateSession(L"Test Session Name");

    auto connSend = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackAEndpointId());
    auto connReceive = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackBEndpointId());

    uint32_t receivedMessageCount{};

    uint32_t lastReceivedMessageValue{ 0 };

    uint32_t word0 = 0x20000000;
    uint16_t sentMessageCount = 500; // upped this from 20 because one earlier bug only manifested when sending > 20 messages


    auto MessageReceivedHandler = [&](IMidiMessageReceivedEventSource const& sender, MidiMessageReceivedEventArgs const& args)
        {
            try
            {
                UNREFERENCED_PARAMETER(sender);

                //auto lock = messageLock.lock();

                receivedMessageCount++;

                //       std::cout << "Received: 0x" << std::hex << args.PeekFirstWord() << std::endl;

                if (lastReceivedMessageValue != 0)
                {
                    // check to ensure messages are arriving in order they were sent
                    VERIFY_ARE_EQUAL(args.PeekFirstWord(), lastReceivedMessageValue + 1);
                }


                // update the last received message with this value
                lastReceivedMessageValue = args.PeekFirstWord();

                //lock.reset();



                if (receivedMessageCount == sentMessageCount)
                {
                    allMessagesReceived.SetEvent();
                }
            }
            catch (...)
            {
                VERIFY_FAIL();
            }
        };

    auto eventRevokeToken = connReceive.MessageReceived(MessageReceivedHandler);

    // open connection
    connSend.Open();
    connReceive.Open();


    // schedule all messages to arrive at the same time: 2 seconds into the future
    // this will also test to ensure they arrive in the order sent
    auto scheduledTimeStamp = MidiClock::OffsetTimestampByMilliseconds(MidiClock::Now(), 2000);

    std::cout << "Sending messages" << std::endl;

    // send messages

    for (uint32_t i = 0; i < sentMessageCount; i++)
    {
        uint32_t word = word0 + i;

        std::cout << "Sending: 0x" << std::hex << word << std::endl;

        // we increment the message value each time so we can keep track of order
        VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendMessageWords(scheduledTimeStamp, word)));
    }

    std::cout << "Waiting for response" << std::endl;

    // Wait for incoming message
    if (!allMessagesReceived.wait(15000))
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
