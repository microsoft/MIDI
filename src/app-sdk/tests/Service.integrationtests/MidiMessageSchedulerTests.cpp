// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

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

    VERIFY_IS_TRUE(MidiServicesInitializer::EnsureServiceAvailable());


    wil::unique_event_nothrow allMessagesReceived;
    wil::critical_section messageLock;

    allMessagesReceived.create();

    auto session = MidiSession::Create(L"Test Session Name");

    auto connSend = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());
    auto connReceive = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId());

    uint32_t receivedMessageCount{};

    uint32_t word0 = 0x20000000;

    
    // calculate an acceptable timestamp offset
    // We'll likely want to do better than this in the future, but 100 microseconds for scheduling isn't bad.
    uint32_t acceptableTimestampDeltaMicroseconds = 100;    // 0.1ms

    uint64_t acceptableTimestampDeltaTicks = (uint64_t)((acceptableTimestampDeltaMicroseconds * MidiClock::TimestampFrequency()) / 1000000.0);


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

    // send messages

    for (uint32_t i = 0; i < messageCount; i++)
    {
        uint32_t word = word0 + i;

 //       std::cout << "Sending: 0x" << std::hex << word << std::endl;

        // we increment the message value each time so we can keep track of order as well

        auto sendResult = connSend.SendSingleMessageWords(MidiClock::OffsetTimestampByMilliseconds(MidiClock::Now(), scheduledTimeStampOffsetMS), word);

        VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(sendResult));

        // if we don't sleep here, the send and receive, and the TAEF output, get in the 
        // way of each other and mess up timing. That's entirely a client-side problem
        // and isn't how "normal" apps would work anyway.
        Sleep(1);
    }

//    std::cout << "Waiting for response" << std::endl;

    // Wait for incoming message
    if (!allMessagesReceived.wait(15000))
    {
        std::cout << std::endl << "Failure waiting for messages, timed out." << std::endl;
    }

    //std::cout << "Finished waiting. Unwiring event" << std::endl;

    connReceive.MessageReceived(eventRevokeToken);

    VERIFY_ARE_EQUAL(receivedMessageCount, messageCount);

    //std::cout << "Disconnecting endpoints" << std::endl;

    // cleanup endpoint. Technically not required as session will do it
    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());

    //std::cout << "Endpoints disconnected" << std::endl;

    session.Close();
}

void MidiMessageSchedulerTests::TestScheduledMessagesOrder()
{
    LOG_OUTPUT(L"TestScheduledMessages **********************************************************************");

    VERIFY_IS_TRUE(MidiServicesInitializer::EnsureServiceAvailable());

    wil::unique_event_nothrow allMessagesReceived;
    wil::critical_section messageLock;

    allMessagesReceived.create();

    auto session = MidiSession::Create(L"Test Session Name");

    auto connSend = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());
    auto connReceive = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId());

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

    //std::cout << "Sending messages" << std::endl;

    // send messages

    for (uint32_t i = 0; i < sentMessageCount; i++)
    {
        uint32_t word = word0 + i;

        //std::cout << "Sending: 0x" << std::hex << word << std::endl;

        // we increment the message value each time so we can keep track of order
        VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(scheduledTimeStamp, word)));
    }

    std::cout << "Waiting for response" << std::endl;

    // Wait for incoming message
    if (!allMessagesReceived.wait(15000))
    {
        std::cout << "Failure waiting for messages, timed out." << std::endl;
    }

    //std::cout << "Finished waiting. Unwiring event" << std::endl;

    connReceive.MessageReceived(eventRevokeToken);

    VERIFY_ARE_EQUAL(receivedMessageCount, sentMessageCount);

    //std::cout << "Disconnecting endpoints" << std::endl;

    // cleanup endpoint. Technically not required as session will do it
    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());

    //std::cout << "Endpoints disconnected" << std::endl;

    session.Close();
}
