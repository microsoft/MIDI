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

    TestScheduledMessagesTiming(10);
}

void MidiMessageSchedulerTests::TestScheduledMessagesTimingLarge()
{
    LOG_OUTPUT(L"Test timing large **********************************************************************");

    TestScheduledMessagesTiming(200);
}


// TODO: The large timing test tends to fail. Sometimes the small does as well. But the delay
// seems likely to be caused by the client / test. Need to investigate.


_Use_decl_annotations_
void MidiMessageSchedulerTests::TestScheduledMessagesTiming(uint16_t const messageCount)
{
    LOG_OUTPUT(L"Test timing **********************************************************************");

//    VERIFY_IS_TRUE(MidiServicesInitializer::EnsureServiceAvailable());


    wil::unique_event_nothrow allMessagesReceived;
    wil::critical_section messageLock;

    allMessagesReceived.create();

    auto session = MidiSession::Create(L"Test Session Name");

    auto connSend = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());
    auto connReceive = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId());

    uint32_t receivedMessageCount{};

    uint32_t word0 = 0x20000000;

    
    // calculate an acceptable timestamp offset
    uint32_t acceptableAverageTimestampDeltaMicroseconds = 100;  // 0.1ms
    uint32_t acceptableMaxTimestampDeltaMicroseconds = 1500;    // 1.5ms. This is only allowed for a very small % of messages

    uint64_t acceptableAverageTimestampDeltaTicks = MidiClock::OffsetTimestampByMicroseconds(0, acceptableAverageTimestampDeltaMicroseconds);
    uint64_t acceptableMaxTimestampDeltaTicks = MidiClock::OffsetTimestampByMicroseconds(0, acceptableMaxTimestampDeltaMicroseconds);

    uint32_t scheduledTimeStampOffsetMS = 2000; // time we're scheduling out from send time

    uint32_t failureCount{ 0 };
    long long totalDeltaTicks{ 0 };

    auto MessageReceivedHandler = [&](IMidiMessageReceivedEventSource const& /*sender*/, MidiMessageReceivedEventArgs const& args)
        {
            auto receivedTimestamp = MidiClock::Now();
            receivedMessageCount++;

            auto delta = std::abs((long long)(args.Timestamp() - receivedTimestamp));

            if ((uint64_t)delta > acceptableMaxTimestampDeltaTicks)
            {
                failureCount++;
            }
            else
            {
                // we only add the delta when it's not an outlier
                totalDeltaTicks += delta;
            }

            if (receivedMessageCount == messageCount)
            {
                allMessagesReceived.SetEvent();
            }
        };

    auto eventRevokeToken = connReceive.MessageReceived(MessageReceivedHandler);

    // open connection
    connSend.Open();
    connReceive.Open();

    // send messages

    std::thread sendThread([&]()
        {
            for (uint32_t i = 0; i < messageCount; i++)
            {
                uint32_t word = word0 + i;
                // we increment the message value each time so we can keep track of order as well

                /*auto sendResult = */connSend.SendSingleMessageWords(MidiClock::OffsetTimestampByMilliseconds(MidiClock::Now(), scheduledTimeStampOffsetMS), word);

                //VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(sendResult));

                Sleep(1);
            }
        });

    sendThread.detach();

//    std::cout << "Waiting for response" << std::endl;

    // Wait for incoming message
    if (!allMessagesReceived.wait(15000))
    {
        std::cout << std::endl << "Failure waiting for messages, timed out." << std::endl;
        VERIFY_FAIL();
    }

    //std::cout << "Finished waiting. Unwiring event" << std::endl;

    connReceive.MessageReceived(eventRevokeToken);

    auto averageDeltaTicks = totalDeltaTicks / (receivedMessageCount - failureCount);

    std::cout << std::endl;
    std::cout << "Count messages:                 " << std::dec << messageCount << std::endl;
    std::cout << std::endl;
    std::cout << "Count exceeding max delta:      " << std::dec << failureCount << std::endl;
    std::cout << "Acceptable max timestamp delta: +/- " << std::dec << acceptableMaxTimestampDeltaTicks << " ticks" << " (" << MidiClock::ConvertTimestampTicksToMicroseconds(acceptableMaxTimestampDeltaTicks) << " microseconds)" << " (" << MidiClock::ConvertTimestampTicksToMilliseconds(acceptableMaxTimestampDeltaTicks) << " milliseconds)" << std::endl;
    std::cout << std::endl;
    std::cout << "Average good timing offset:     " << std::dec << averageDeltaTicks << " ticks (" << MidiClock::ConvertTimestampTicksToMicroseconds(averageDeltaTicks) << " microseconds)" << " (" << MidiClock::ConvertTimestampTicksToMilliseconds(averageDeltaTicks) << " milliseconds)" << std::endl;
    std::cout << "Acceptable average delta:       +/- " << std::dec << acceptableAverageTimestampDeltaTicks << " ticks" << " (" << MidiClock::ConvertTimestampTicksToMicroseconds(acceptableAverageTimestampDeltaTicks) << " microseconds)" << " (" << MidiClock::ConvertTimestampTicksToMilliseconds(acceptableAverageTimestampDeltaTicks) << " milliseconds)" << std::endl;
    std::cout << std::endl;


    VERIFY_ARE_EQUAL(receivedMessageCount, messageCount);
    VERIFY_IS_LESS_THAN_OR_EQUAL(failureCount, (uint32_t)(receivedMessageCount *.03));  // allow up to 3% of messages to be out due to system burps
    VERIFY_IS_LESS_THAN_OR_EQUAL((uint32_t)averageDeltaTicks, acceptableAverageTimestampDeltaTicks);     // require that the average is within the range


    // cleanup endpoint. Technically not required as session will do it
    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());

    //std::cout << "Endpoints disconnected" << std::endl;

    session.Close();
}

void MidiMessageSchedulerTests::TestScheduledMessagesOrder()
{
    LOG_OUTPUT(L"TestScheduledMessages **********************************************************************");

 //   VERIFY_IS_TRUE(MidiServicesInitializer::EnsureServiceAvailable());

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
