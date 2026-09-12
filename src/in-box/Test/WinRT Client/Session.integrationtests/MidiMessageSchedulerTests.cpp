// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

#include <numeric>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <cmath>

void MidiMessageSchedulerTests::TestScheduledMessagesTimingSmall()
{
    TestScheduledMessagesTiming(10, 2000);
}

void MidiMessageSchedulerTests::TestScheduledMessagesTimingLarge()
{
    TestScheduledMessagesTiming(200, 2000);
}

// 2000 ms sits exactly on the boundary of the sleep floor the original scheduler used, so the two
// cases above never exercised a lead time a sequencer would actually use.
void MidiMessageSchedulerTests::TestScheduledMessagesTimingShortLead()
{
    TestScheduledMessagesTiming(200, 250);
}


// These used to fail intermittently, which was put down to the client or the test. After the
// outbound scheduler stopped busy waiting and stopped raising the priority class of the whole
// service process, 5 consecutive runs of all three cases passed with 0 messages outside the
// tolerance. Worth watching rather than declaring fixed.


void MidiMessageSchedulerTests::DiscoverLoopbackGroupMapping()
{
    wchar_t endpointId[512]{};

    if (GetEnvironmentVariableW(L"MIDI_RTT_TEST_ENDPOINT_ID", endpointId, ARRAYSIZE(endpointId)) == 0)
    {
        WEX::Logging::Log::Comment(L"MIDI_RTT_TEST_ENDPOINT_ID is not set. Skipping the loopback mapping scan.");
        return;
    }

    uint32_t const maximumGroupCount{ 16 };
    uint32_t const probesPerGroup{ 8 };

    // Devices with automatic input/output detection, such as the ESI M8U eX and MIDIMATE eX, only
    // reverse a port's direction after SUSTAINED TRAFFIC, not after mere elapsed time: measured at
    // roughly 60 to 93 messages. Probes are therefore spread across this window rather than sent as
    // a burst. Raise it to about 3000 to map a device with adaptive ports in both directions.
    uint32_t settleMilliseconds{ 400 };

    wchar_t settleText[16]{};

    if (GetEnvironmentVariableW(L"MIDI_RTT_TEST_SCAN_SETTLE_MS", settleText, ARRAYSIZE(settleText)) > 0)
    {
        auto const parsed = _wtoi(settleText);

        if (parsed >= 100 && parsed <= 30000)
        {
            settleMilliseconds = static_cast<uint32_t>(parsed);
        }
    }

    uint32_t highestGroup{ maximumGroupCount };

    wchar_t groupText[16]{};

    if (GetEnvironmentVariableW(L"MIDI_RTT_TEST_MAX_GROUP", groupText, ARRAYSIZE(groupText)) > 0)
    {
        auto const parsed = _wtoi(groupText);

        if (parsed >= 1 && parsed <= static_cast<int>(maximumGroupCount))
        {
            highestGroup = static_cast<uint32_t>(parsed);
        }
    }

    uint32_t matrix[maximumGroupCount][maximumGroupCount]{};
    std::mutex matrixLock;
    std::atomic<uint32_t> currentGroupReceived{ 0 };

    auto session = MidiSession::Create(L"Loopback group mapping scan");
    auto connection = session.CreateEndpointConnection(winrt::hstring{ endpointId });

    VERIFY_IS_NOT_NULL(connection);

    auto handler = [&](IMidiMessageReceivedEventSource const&, MidiMessageReceivedEventArgs const& args)
        {
            auto const word = args.PeekFirstWord();

            // Everything except the group nibble and the value byte has to match the probe.
            if ((word & 0xF0FFFF00u) != 0x20B01400u) return;

            auto const sentGroup = static_cast<uint32_t>(word & 0xFF);
            auto const arrivedGroup = static_cast<uint32_t>((word >> 24) & 0x0F) + 1;

            if (sentGroup < 1 || sentGroup > maximumGroupCount) return;

            {
                std::lock_guard<std::mutex> guard(matrixLock);
                matrix[sentGroup - 1][arrivedGroup - 1]++;
            }

            currentGroupReceived++;
        };

    auto token = connection.MessageReceived(handler);

    VERIFY_IS_TRUE(connection.Open());

    for (uint32_t group = 1; group <= highestGroup; group++)
    {
        currentGroupReceived.store(0);

        // The probe carries its own output group in the value byte, so a message that arrives
        // identifies both halves of the pair: the group nibble is where it came back in, the
        // value byte is where it went out.
        uint32_t const word = 0x20B01400u | ((group - 1) << 24) | group;

        // Spread, not burst. A burst also tells us nothing useful about a device that drops
        // everything after the first message of a block, which is a separate test.
        auto const spacingMilliseconds = settleMilliseconds / probesPerGroup;

        for (uint32_t i = 0; i < probesPerGroup; i++)
        {
            connection.SendSingleMessageWords(0, word);

            Sleep(spacingMilliseconds);
        }

        auto const deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);

        while (currentGroupReceived.load() < probesPerGroup && std::chrono::steady_clock::now() < deadline)
        {
            Sleep(10);
        }
    }

    connection.MessageReceived(token);
    session.DisconnectEndpointConnection(connection.ConnectionId());

    std::cout << std::endl << "Loopback group mapping" << std::endl;
    std::cout << "  endpoint : " << winrt::to_string(winrt::hstring{ endpointId }) << std::endl;

    bool foundAny{ false };

    std::lock_guard<std::mutex> guard(matrixLock);

    for (uint32_t outGroup = 1; outGroup <= highestGroup; outGroup++)
    {
        for (uint32_t inGroup = 1; inGroup <= maximumGroupCount; inGroup++)
        {
            auto const count = matrix[outGroup - 1][inGroup - 1];

            if (count > 0)
            {
                foundAny = true;

                std::cout << "  out group " << outGroup
                          << "  ->  in group " << inGroup
                          << "   (" << count << " of " << probesPerGroup << " probes)" << std::endl;
            }
        }
    }

    if (!foundAny)
    {
        std::cout << "  Nothing came back on any group. Check the cable, and on a routing device such as" << std::endl;
        std::cout << "  a MIDIhub check the active preset actually routes USB to the DIN outputs and the" << std::endl;
        std::cout << "  DIN inputs back to USB." << std::endl;
    }
}


void MidiMessageSchedulerTests::MeasureDeviceRoundTripLatency()
{
    wchar_t endpointId[512]{};

    if (GetEnvironmentVariableW(L"MIDI_RTT_TEST_ENDPOINT_ID", endpointId, ARRAYSIZE(endpointId)) == 0)
    {
        WEX::Logging::Log::Comment(L"MIDI_RTT_TEST_ENDPOINT_ID is not set. Skipping the device round trip measurement.");
        return;
    }

    uint32_t messageCount{ 100 };
    uint32_t const leadMilliseconds{ 250 };
    uint32_t const gapMilliseconds{ 20 };

    // The normal run spaces messages 20 ms apart, so each one gets its own USB transfer and a
    // per-transfer cost is indistinguishable from a per-message cost. Sending them as one block
    // separates the two: a per-transfer cost stays flat as the block grows, a per-message cost
    // scales with it.
    uint32_t burstCount{ 0 };

    wchar_t burstText[16]{};

    if (GetEnvironmentVariableW(L"MIDI_RTT_TEST_BURST", burstText, ARRAYSIZE(burstText)) > 0)
    {
        auto const parsed = _wtoi(burstText);

        if (parsed >= 2 && parsed <= 100)
        {
            burstCount = static_cast<uint32_t>(parsed);
            messageCount = burstCount;
        }
    }

    // Multi-port interfaces put each physical port on its own group, so the loopback cable decides
    // which one to use. Given as the group number the console displays, which is 1 based.
    uint32_t groupNumber{ 1 };

    wchar_t groupText[16]{};

    if (GetEnvironmentVariableW(L"MIDI_RTT_TEST_GROUP", groupText, ARRAYSIZE(groupText)) > 0)
    {
        auto const parsed = _wtoi(groupText);

        if (parsed >= 1 && parsed <= 16)
        {
            groupNumber = static_cast<uint32_t>(parsed);
        }
    }

    uint32_t const groupBits = (groupNumber - 1) << 24;

    // Some interfaces do not return on the group they were sent on: the ESI MIDIMATE eX has its
    // physical out on group 1 and its physical in on group 2. DiscoverLoopbackGroupMapping reports
    // the pair. Defaults to the send group, which is the usual case.
    uint32_t inGroupNumber{ groupNumber };

    wchar_t inGroupText[16]{};

    if (GetEnvironmentVariableW(L"MIDI_RTT_TEST_IN_GROUP", inGroupText, ARRAYSIZE(inGroupText)) > 0)
    {
        auto const parsed = _wtoi(inGroupText);

        if (parsed >= 1 && parsed <= 16)
        {
            inGroupNumber = static_cast<uint32_t>(parsed);
        }
    }

    uint32_t const inGroupBits = (inGroupNumber - 1) << 24;

    // Message length is the one part of the round trip we know exactly: a DIN cable carries a byte
    // in 320 us, so a 3 byte controller change costs 960 us on the wire and a 2 byte program change
    // 640 us. Measuring both and subtracting gives the per byte wire cost, and what is left is the
    // fixed overhead of USB, the driver, the service and the client.
    // 0 = 3 byte controller change, 1 = 2 byte program change, 2 = alternate the two.
    uint32_t messageKind{ 0 };

    wchar_t kindText[16]{};

    if (GetEnvironmentVariableW(L"MIDI_RTT_TEST_MESSAGE", kindText, ARRAYSIZE(kindText)) > 0)
    {
        if (_wcsicmp(kindText, L"pc") == 0) messageKind = 1;
        else if (_wcsicmp(kindText, L"mixed") == 0) messageKind = 2;
    }

    auto const isTwoByte = [messageKind, burstCount](uint32_t const index)
        {
            // A burst keeps one message length so the wire time per message is uniform.
            if (burstCount > 0) return false;

            return (messageKind == 1) || (messageKind == 2 && ((index & 1) != 0));
        };

    // The index rides in the last data byte of whichever message is used, so it survives the trip.
    auto const makeWord = [groupBits, &isTwoByte](uint32_t const index)
        {
            return isTwoByte(index)
                ? (0x20C00000u | groupBits | (index << 8))
                : (0x20B01400u | groupBits | index);
        };

    auto session = MidiSession::Create(L"Device round trip measurement");
    auto connection = session.CreateEndpointConnection(winrt::hstring{ endpointId });

    VERIFY_IS_NOT_NULL(connection);

    // Two clocks on purpose. The service timestamp is authoritative for native UMP devices, but on
    // the legacy path it is m_StartTime + a whole-millisecond driver delta, so it cannot resolve
    // anything finer than 1 ms. Reading the clock in the handler is coarser but is a real arrival.
    std::vector<uint64_t> sentTimestamps(messageCount, 0);
    std::vector<uint64_t> serviceTimestamps(messageCount, 0);
    std::vector<uint64_t> clientTimestamps(messageCount, 0);
    std::atomic<uint32_t> receivedCount{ 0 };

    auto handler = [&](IMidiMessageReceivedEventSource const&, MidiMessageReceivedEventArgs const& args)
        {
            auto const arrival = MidiClock::Now();

            auto const word = args.PeekFirstWord();

            if ((word & 0xFF000000u) != (0x20000000u | inGroupBits)) return;

            auto const statusAndChannel = (word >> 16) & 0xFF;

            uint32_t index{};

            if (statusAndChannel == 0xB0 && ((word >> 8) & 0xFF) == 0x14)
            {
                index = word & 0xFF;
            }
            else if (statusAndChannel == 0xC0)
            {
                index = (word >> 8) & 0xFF;
            }
            else
            {
                return;
            }

            if (index < messageCount && clientTimestamps[index] == 0)
            {
                serviceTimestamps[index] = args.Timestamp();
                clientTimestamps[index] = arrival;
                receivedCount++;
            }
        };

    auto token = connection.MessageReceived(handler);

    VERIFY_IS_TRUE(connection.Open());

    // Scheduled rather than sent immediately, so the scheduler's release time is the reference and
    // the client side of the send is not part of what gets measured.
    auto const start = MidiClock::Now();

    if (burstCount > 0)
    {
        for (uint32_t i = 0; i < messageCount; i++)
        {
            sentTimestamps[i] = start;

            connection.SendSingleMessageWords(0, makeWord(i));
        }
    }
    else
    {
        for (uint32_t i = 0; i < messageCount; i++)
        {
            auto const timestamp = MidiClock::OffsetTimestampByMilliseconds(
                start, static_cast<int64_t>(leadMilliseconds + (i * gapMilliseconds)));

            sentTimestamps[i] = timestamp;

            connection.SendSingleMessageWords(timestamp, makeWord(i));
        }
    }

    auto const deadline = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(leadMilliseconds + (messageCount * gapMilliseconds) + 5000);

    while (receivedCount.load() < messageCount && std::chrono::steady_clock::now() < deadline)
    {
        Sleep(20);
    }

    connection.MessageReceived(token);
    session.DisconnectEndpointConnection(connection.ConnectionId());

    std::vector<int64_t> serviceRoundTrip;
    std::vector<int64_t> clientRoundTrip;
    std::vector<int64_t> clientTwoByte;
    std::vector<int64_t> clientThreeByte;

    struct Sample
    {
        uint32_t Index;
        int64_t Service;
        int64_t Client;
    };

    std::vector<Sample> samples;

    for (uint32_t i = 0; i < messageCount; i++)
    {
        if (clientTimestamps[i] != 0)
        {
            auto const elapsed = static_cast<int64_t>(clientTimestamps[i]) - static_cast<int64_t>(sentTimestamps[i]);
            auto const serviceElapsed = static_cast<int64_t>(serviceTimestamps[i]) - static_cast<int64_t>(sentTimestamps[i]);

            serviceRoundTrip.push_back(serviceElapsed);
            clientRoundTrip.push_back(elapsed);
            samples.push_back({ i, serviceElapsed, elapsed });

            if (isTwoByte(i))
            {
                clientTwoByte.push_back(elapsed);
            }
            else
            {
                clientThreeByte.push_back(elapsed);
            }
        }
    }

    wchar_t const* const kindName =
        messageKind == 1 ? L"program change, 2 bytes on the wire" :
        messageKind == 2 ? L"alternating controller change and program change, 3 and 2 bytes" :
                           L"controller change, 3 bytes on the wire";

    std::cout << std::endl << "Device round trip, OUT to IN" << std::endl;
    std::cout << "  endpoint        : " << winrt::to_string(winrt::hstring{ endpointId }) << std::endl;
    std::cout << "  group           : out " << groupNumber << ", in " << inGroupNumber << std::endl;
    std::wcout << L"  message         : " << kindName << std::endl;
    std::cout << "  sent / returned : " << messageCount << " / " << clientRoundTrip.size() << std::endl;

    if (clientRoundTrip.empty())
    {
        std::cout << "  Nothing came back. Check that a MIDI cable connects this device's OUT to its IN." << std::endl;
        VERIFY_FAIL();
        return;
    }

    // Signed. A negative value would mean the reply was timestamped before the message was due,
    // which says the incoming timestamp is not on the same basis as the outgoing one.
    auto const toMicroseconds = [](int64_t const ticks)
        {
            auto const magnitude = MidiClock::ConvertTimestampTicksToMicroseconds(
                static_cast<uint64_t>(ticks < 0 ? -ticks : ticks));

            return ticks < 0 ? -magnitude : magnitude;
        };

    auto const report = [&toMicroseconds](wchar_t const* label, std::vector<int64_t> values)
        {
            std::sort(values.begin(), values.end());

            auto const total = std::accumulate(values.begin(), values.end(), static_cast<int64_t>(0));
            auto const mean = total / static_cast<int64_t>(values.size());

            auto const at = [&values](double const fraction)
                {
                    auto index = static_cast<size_t>(fraction * static_cast<double>(values.size() - 1));
                    if (index >= values.size()) index = values.size() - 1;
                    return values[index];
                };

            double sumOfSquares{ 0.0 };

            for (auto const value : values)
            {
                auto const difference = static_cast<double>(value - mean);
                sumOfSquares += difference * difference;
            }

            auto const standardDeviation = static_cast<int64_t>(
                std::sqrt(sumOfSquares / static_cast<double>(values.size())));

            // Max minus min is what one stray message did. The interquartile range is what the
            // stream actually does, which is the number that matters for jitter.
            std::wcout << L"  " << label
                       << L" p50 " << toMicroseconds(at(0.50))
                       << L"  mean " << toMicroseconds(mean)
                       << L"  sd " << toMicroseconds(standardDeviation)
                       << L"  iqr " << toMicroseconds(at(0.75) - at(0.25))
                       << L"  p95 " << toMicroseconds(at(0.95))
                       << L"  min " << toMicroseconds(values.front())
                       << L"  max " << toMicroseconds(values.back()) << std::endl;
        };

    std::wcout << L"  round trip, microseconds" << std::endl;
    report(L"service timestamp :", serviceRoundTrip);
    report(L"client arrival    :", clientRoundTrip);

    if (!clientTwoByte.empty() && !clientThreeByte.empty())
    {
        report(L"  3 byte          :", clientThreeByte);
        report(L"  2 byte          :", clientTwoByte);

        auto const median = [](std::vector<int64_t> values)
            {
                std::sort(values.begin(), values.end());
                return values[values.size() / 2];
            };

        auto const perByte = toMicroseconds(median(clientThreeByte) - median(clientTwoByte));

        std::wcout << L"  one byte of wire time measures " << perByte
                   << L" us against 320 us expected" << std::endl;
        std::wcout << L"  fixed overhead, everything that is not wire time, is about "
                   << (toMicroseconds(median(clientThreeByte)) - (3 * perByte)) << L" us" << std::endl;
    }

    std::cout << "  (a DIN cable carries one byte in 320 us: 960 for a 3 byte message, 640 for 2)" << std::endl;
    std::cout << "  (a spread of exactly 0 means the timestamp is derived, not measured)" << std::endl;

    if (burstCount > 0 && clientRoundTrip.size() > 1)
    {
        auto sorted = clientRoundTrip;
        std::sort(sorted.begin(), sorted.end());

        auto const first = toMicroseconds(sorted.front());
        auto const last = toMicroseconds(sorted.back());

        std::wcout << L"  burst of " << messageCount << L" messages handed over as one block"
                   << std::endl;
        std::wcout << L"    first back " << first
                   << L" us, block complete " << last
                   << L" us, mean gap " << ((last - first) / static_cast<int64_t>(sorted.size() - 1))
                   << L" us against 960 of wire time per message" << std::endl;
    }

    // An outlier in client arrival alone could just be this test process losing its quantum, which
    // would say nothing about the device. The service timestamp is taken before the hop to the
    // client, and even at whole millisecond resolution it resolves a spike of this size, so the two
    // together say whether the delay happened at or below the driver, or on the way to the client.
    {
        auto sortedClient = clientRoundTrip;
        std::sort(sortedClient.begin(), sortedClient.end());

        auto const medianMicroseconds = toMicroseconds(sortedClient[sortedClient.size() / 2]);
        auto const outlierThreshold = medianMicroseconds + 2000;

        bool reportedHeader{ false };

        for (auto const& sample : samples)
        {
            auto const clientMicroseconds = toMicroseconds(sample.Client);

            if (clientMicroseconds > outlierThreshold)
            {
                if (!reportedHeader)
                {
                    std::wcout << L"  outliers over median plus 2000 us, of " << samples.size() << L" messages" << std::endl;
                    std::wcout << L"    position  service    client   client minus service" << std::endl;
                    reportedHeader = true;
                }

                auto const serviceMicroseconds = toMicroseconds(sample.Service);

                std::wcout << L"    " << sample.Index
                           << L"\t      " << serviceMicroseconds
                           << L"\t " << clientMicroseconds
                           << L"\t " << (clientMicroseconds - serviceMicroseconds) << std::endl;
            }
        }

        if (!reportedHeader)
        {
            std::wcout << L"  no message exceeded the median by 2000 us" << std::endl;
        }
    }
}


_Use_decl_annotations_
void MidiMessageSchedulerTests::TestScheduledMessagesTiming(
    uint16_t const messageCount,
    uint32_t const scheduledTimeStampOffsetMS)
{

    {

        wil::unique_event_nothrow allMessagesReceived;
        wil::critical_section messageLock;

        allMessagesReceived.create();

        auto session = MidiSession::Create(L"TestScheduledMessagesTiming");

        auto connSend = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());
        auto connReceive = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId());

        uint32_t receivedMessageCount{};

        uint32_t word0 = 0x20000000;


        // calculate an acceptable timestamp offset
        uint32_t acceptableAverageTimestampDeltaMicroseconds = 100;  // 0.1ms
        uint32_t acceptableMaxTimestampDeltaMicroseconds = 1500;    // 1.5ms. This is only allowed for a very small % of messages

        uint64_t acceptableAverageTimestampDeltaTicks = MidiClock::OffsetTimestampByMicroseconds(0, acceptableAverageTimestampDeltaMicroseconds);
        uint64_t acceptableMaxTimestampDeltaTicks = MidiClock::OffsetTimestampByMicroseconds(0, acceptableMaxTimestampDeltaMicroseconds);

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
        VERIFY_IS_LESS_THAN_OR_EQUAL(failureCount, (uint32_t)(receivedMessageCount * .03));  // allow up to 3% of messages to be out due to system burps
        VERIFY_IS_LESS_THAN_OR_EQUAL((uint32_t)averageDeltaTicks, acceptableAverageTimestampDeltaTicks);     // require that the average is within the range


        // cleanup endpoint. Technically not required as session will do it
        session.DisconnectEndpointConnection(connSend.ConnectionId());
        session.DisconnectEndpointConnection(connReceive.ConnectionId());

        //std::cout << "Endpoints disconnected" << std::endl;

        session.Close();
    }

}

void MidiMessageSchedulerTests::TestScheduledMessagesOrder()
{
    {

        wil::unique_event_nothrow allMessagesReceived;
        wil::critical_section messageLock;

        allMessagesReceived.create();

        auto session = MidiSession::Create(L"TestScheduledMessagesOrder");

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
}
