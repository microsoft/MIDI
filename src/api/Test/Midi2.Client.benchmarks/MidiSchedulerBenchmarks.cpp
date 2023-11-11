// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================



#include "stdafx.h"


_Use_decl_annotations_
void MidiSchedulerBenchmarks::BenchmarkSendReceiveScheduledMessages(_In_ uint32_t messageCount)
{
    LOG_OUTPUT(L"API Scheduling benchmark **********************************************************************");

    uint32_t numMessagesToSend = messageCount;
    int64_t offsetMilliseconds = 5000;

    uint32_t receivedMessageCount{};

    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    auto session = MidiSession::CreateSession(L"Scheduling Benchmark Session");

    auto connSend = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackAEndpointId());
    auto connReceive = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackBEndpointId());

    VERIFY_IS_NOT_NULL(connSend);
    VERIFY_IS_NOT_NULL(connReceive);


    auto ump32mt = MidiMessageType::Midi1ChannelVoice32;
    auto ump64mt = MidiMessageType::Midi2ChannelVoice64;
    auto ump96mt = MidiMessageType::FutureReservedB96;
    auto ump128mt = MidiMessageType::Stream128;


    std::vector<int64_t> timestampDeltas;
    timestampDeltas.reserve(numMessagesToSend);


    auto ReceivedHandler = [&allMessagesReceived, &receivedMessageCount, &numMessagesToSend, &timestampDeltas](winrt::Windows::Foundation::IInspectable const& /*sender*/, MidiMessageReceivedEventArgs const& args)
        {
            receivedMessageCount++;

            // this works because the loopback transport does not change the timestamps
            // when they are sent. The timestamp in the original sent message is what
            // is returned in this event.

            uint64_t currentStamp = MidiClock::Now();
            uint64_t umpStamp = args.Timestamp();
            //uint64_t umpStamp = args.Timestamp();
            timestampDeltas.push_back((int64_t)(currentStamp - umpStamp));

            if (receivedMessageCount == numMessagesToSend)
            {
                allMessagesReceived.SetEvent();
            }

        };

    auto eventRevokeToken = connReceive.MessageReceived(ReceivedHandler);

    connSend.Open();
    connReceive.Open();

    uint32_t numBytes = 0;
    uint32_t ump32Count = 0;
    uint32_t ump64Count = 0;
    uint32_t ump96Count = 0;
    uint32_t ump128Count = 0;



    // the distribution of messages here tries to mimic what we expect to be
    // a reasonably typical mix. In reality, almost all messages going back
    // and forth with an endpoint during a performance are going to be
    // UMP32 (MIDI 1.0 CV) or UMP64 (MIDI 2.0 CV), with the others in only at 
    // certain times. The exception is any device which relies on SysEx for
    // parameter changes on the fly.

    uint32_t words[]{ 0x40000000,0,0,0 };
    uint32_t wordCount{};

    for (uint32_t i = 0; i < numMessagesToSend; i++)
    {
        switch (i % 12)
        {
        case 0:
        case 1:
        case 2:
        case 3:
        {
            words[0] = (uint32_t)ump32mt << 28;
            wordCount = 1;
            ump32Count++;
        }
        break;

        case 4:
        case 5:
        case 6:
        case 7:
        {
            words[0] = (uint32_t)ump64mt << 28;
            wordCount = 2;
            ump64Count++;
        }
        break;

        case 8:
        {
            words[0] = (uint32_t)ump96mt << 28;
            wordCount = 3;
            ump96Count++;
        }
        break;

        case 9:
        case 10:
        case 11:
        {
            words[0] = (uint32_t)ump128mt << 28;
            wordCount = 4;
            ump128Count++;
        }
        break;
        }

        numBytes += sizeof(uint32_t) * wordCount + sizeof(uint64_t);

        auto timeStamp = MidiClock::OffsetTimestampByMilliseconds(MidiClock::Now(), offsetMilliseconds);

        connSend.SendMessageWordArray(timeStamp, words, 0, (uint8_t)wordCount);
    }

    // Wait for incoming message
    if (!allMessagesReceived.wait(60000))
    {
        std::cout << "Failure waiting for messages, timed out." << std::endl;
    }

    uint64_t freq = MidiClock::TimestampFrequency();


    VERIFY_ARE_EQUAL(receivedMessageCount, numMessagesToSend);

    std::cout << "Schedule Offset:             " << std::dec << offsetMilliseconds << "ms" << std::endl;
    std::cout << "Num Messages:                " << std::dec << numMessagesToSend << std::endl;
    std::cout << "- Count UMP32                " << std::dec << ump32Count << std::endl;
    std::cout << "- Count UMP64                " << std::dec << ump64Count << std::endl;
    std::cout << "- Count UMP96                " << std::dec << ump96Count << std::endl;
    std::cout << "- Count UMP128               " << std::dec << ump128Count << std::endl;
    std::cout << "Num Bytes (inc timestamp):   " << std::dec << numBytes << std::endl;
    std::cout << "-----------------------------" << std::endl;

    // uint64_t totalTimestampDelta{ 0 };
    // std::cout << "Timestamp Deltas   " << std::endl;
    //for (auto delta : timestampDeltas)
    //{
    //    std::cout << "Ticks:   " << std::dec << delta << std::endl;

    //    totalTimestampDelta += delta;
    //}

    if (timestampDeltas.size() > 0)
    {
        auto [minDeltaTicks, maxDeltaTicks] = std::minmax_element(begin(timestampDeltas), end(timestampDeltas));

        double minDeltaMilliseconds = *minDeltaTicks / (double)freq;
        double minDeltaMicroseconds = minDeltaMilliseconds * 1000;

        double maxDeltaMilliseconds = *maxDeltaTicks / (double)freq;
        double maxDeltaMicroseconds = maxDeltaMilliseconds * 1000;

        double minToMaxDeltaMilliseconds = maxDeltaMilliseconds - minDeltaMilliseconds;
        double minToMaxDeltaMicroseconds = maxDeltaMicroseconds - minDeltaMicroseconds;

        double totalDeltaTicks = std::accumulate(begin(timestampDeltas), end(timestampDeltas), 0.0);
        double avgDeltaTicks = totalDeltaTicks / (double)numMessagesToSend;    // this should be close to the other calculated average
        double avgDeltaTicksMilliseconds = avgDeltaTicks / (double)freq;
        double avgDeltaTicksMicroseconds = avgDeltaTicksMilliseconds * 1000;

        std::cout << "Scheduled vs Received Deltas" << std::endl;
        std::cout << "- Max:                         " << std::dec << std::fixed << maxDeltaMilliseconds << "ms (" << maxDeltaMicroseconds << " microseconds)." << std::endl;
        std::cout << "- Min:                         " << std::dec << std::fixed << minDeltaMilliseconds << "ms (" << minDeltaMicroseconds << " microseconds)." << std::endl;
        std::cout << "- Max - min:                   " << std::dec << std::fixed << minToMaxDeltaMilliseconds << "ms (" << minToMaxDeltaMicroseconds << " microseconds)." << std::endl;
        std::cout << "- Average:                     " << std::dec << std::fixed << avgDeltaTicksMilliseconds << "ms (" << avgDeltaTicksMicroseconds << " microseconds)." << std::endl;
    }

    //int index{ 0 };
    //for (auto delta : timestampDeltas)
    //{
    //    index++;
    //    std::cout << std::setw(5) << std::dec << index << ": "
    //              << std::setw(12) << std::dec << delta 
    //              << std::endl;
    //}


    // unwire event
    connReceive.MessageReceived(eventRevokeToken);

    // cleanup endpoint. Technically not required as session will do it
    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());

    session.Close();

}



void MidiSchedulerBenchmarks::BenchmarkSendReceiveScheduledMessagesLowCount()
{
    LOG_OUTPUT(L"API Scheduling benchmark LOW message count ***********************************************************************");

    BenchmarkSendReceiveScheduledMessages(50);
    BenchmarkSendReceiveScheduledMessages(200);
}

void MidiSchedulerBenchmarks::BenchmarkSendReceiveScheduledMessagesMediumCount()
{
    LOG_OUTPUT(L"API Scheduling benchmark MEDIUM message count ***********************************************************************");

    BenchmarkSendReceiveScheduledMessages(1000);
    //BenchmarkSendReceiveScheduledMessages(2000);
    BenchmarkSendReceiveScheduledMessages(3000);
    //BenchmarkSendReceiveScheduledMessages(5000);
    //BenchmarkSendReceiveScheduledMessages(10000);
}


void MidiSchedulerBenchmarks::BenchmarkSendReceiveScheduledMessagesHighCount()
{
    LOG_OUTPUT(L"API Scheduling benchmark HIGH (Max) message count **********************************************************************");

    BenchmarkSendReceiveScheduledMessages(MIDI_OUTGOING_MESSAGE_QUEUE_MAX_MESSAGE_COUNT);
}

