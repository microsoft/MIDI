// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

#include <mmsystem.h>
#include <atomic>
#include <vector>

#pragma comment(lib, "winmm.lib")


#define BENCHMARK_MESSAGE_COUNT         100000
#define BENCHMARK_COM_BATCH_SIZE        10
#define BENCHMARK_RECEIVE_TIMEOUT_MS    120000


// Prints a consistent summary for every benchmark in this file. All timings come
// from MidiClock ticks, converted using the MidiClock conversion helpers.
static void ReportBenchmarkResults(
    _In_ std::string const& benchmarkName,
    _In_ uint64_t const sendStartTimestamp,
    _In_ uint64_t const sendEndTimestamp,
    _In_ uint64_t const receiveEndTimestamp,
    _In_ uint32_t const messagesSent,
    _In_ uint32_t const messagesReceived)
{
    uint64_t sendOnlyTicks = sendEndTimestamp - sendStartTimestamp;
    uint64_t sendReceiveTicks = receiveEndTimestamp - sendStartTimestamp;

    double sendOnlySeconds = MidiClock::ConvertTimestampTicksToSeconds(sendOnlyTicks);
    double sendOnlyMilliseconds = MidiClock::ConvertTimestampTicksToMilliseconds(sendOnlyTicks);

    double sendReceiveSeconds = MidiClock::ConvertTimestampTicksToSeconds(sendReceiveTicks);
    double sendReceiveMilliseconds = MidiClock::ConvertTimestampTicksToMilliseconds(sendReceiveTicks);

    // Averages divide the converted totals rather than the raw ticks. At these
    // message counts the per-message tick count rounds to 0 or 1, so dividing
    // ticks first would throw away most of the precision.
    double averageSendMilliseconds{ 0.0 };
    double averageSendMicroseconds{ 0.0 };

    if (messagesSent > 0)
    {
        averageSendMilliseconds = sendOnlyMilliseconds / (double)messagesSent;
        averageSendMicroseconds = MidiClock::ConvertTimestampTicksToMicroseconds(sendOnlyTicks) / (double)messagesSent;
    }

    double averageRoundTripMilliseconds{ 0.0 };
    double averageRoundTripMicroseconds{ 0.0 };

    if (messagesReceived > 0)
    {
        averageRoundTripMilliseconds = sendReceiveMilliseconds / (double)messagesReceived;
        averageRoundTripMicroseconds = MidiClock::ConvertTimestampTicksToMicroseconds(sendReceiveTicks) / (double)messagesReceived;
    }

    double messagesPerSecond = sendReceiveSeconds > 0 ? messagesReceived / sendReceiveSeconds : 0.0;

    std::cout << std::endl;
    std::cout << "==========================================================================" << std::endl;
    std::cout << " " << benchmarkName << std::endl;
    std::cout << "==========================================================================" << std::endl;
    std::cout << "Timestamp frequency:          " << std::dec << MidiClock::TimestampFrequency() << " hz (ticks/second)" << std::endl;
    std::cout << "Messages sent:                " << std::dec << messagesSent << std::endl;
    std::cout << "Messages received:            " << std::dec << messagesReceived << std::endl;
    std::cout << "--------------------------------------------------------------------------" << std::endl;
    std::cout << std::fixed;
    std::cout << "Send loop only" << std::endl;
    std::cout << " - Total:                     " << sendOnlyMilliseconds << " ms (" << sendOnlySeconds << " seconds)" << std::endl;
    std::cout << " - Average per message:       " << averageSendMilliseconds << " ms (" << averageSendMicroseconds << " microseconds)" << std::endl;
    std::cout << std::endl;
    std::cout << "Send and receive all messages" << std::endl;
    std::cout << " - Total:                     " << sendReceiveMilliseconds << " ms (" << sendReceiveSeconds << " seconds)" << std::endl;
    std::cout << " - Average per message:       " << averageRoundTripMilliseconds << " ms (" << averageRoundTripMicroseconds << " microseconds)" << std::endl;
    std::cout << " - Throughput:                " << messagesPerSecond << " messages/second" << std::endl;
    std::cout << "==========================================================================" << std::endl;
    std::cout << std::endl;
}


// Creates a transient A/B loopback for a benchmark to use.
static MidiLoopbackCreationResponse CreateBenchmarkLoopback(_In_ winrt::hstring const& name)
{
    auto uniqueId = L"BM" + winrt::to_hstring(MidiClock::Now());

    MidiLoopbackEndpointDefinition definitionA(
        name + L" A",
        uniqueId + L"-A",
        L"A-side loopback created by the Windows MIDI Services benchmarks."
    );

    MidiLoopbackEndpointDefinition definitionB(
        name + L" B",
        uniqueId + L"-B",
        L"B-side loopback created by the Windows MIDI Services benchmarks."
    );

    MidiLoopbackCreationConfig creationConfig(
        foundation::GuidHelper::CreateNewGuid(), definitionA, definitionB);

    auto response = MidiLoopbackManager::CreateTransientLoopback(creationConfig);

    VERIFY_IS_NOT_NULL(response);

    if (!response.Success())
    {
        std::wcout << L"Error Message: " << response.ErrorMessage().c_str() << std::endl;
    }

    VERIFY_IS_TRUE(response.Success());
    VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry());
    VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry().EndpointA());
    VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry().EndpointB());
    VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId().empty());
    VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId().empty());

    return response;
}

static void RemoveBenchmarkLoopback(winrt::guid const& associationId)
{
    MidiLoopbackRemovalConfig removalConfig(associationId);
    auto removalResponse = MidiLoopbackManager::RemoveTransientLoopback(removalConfig);

    VERIFY_IS_NOT_NULL(removalResponse);
    VERIFY_IS_TRUE(removalResponse.Success());
}


// ============================================================================
// 100,000 MIDI 1.0 UMP messages, sent 10 at a time through the COM extensions.
// Sent into the A side, received on the B side.
// ============================================================================
void MidiLoopbackBenchmarks::BenchmarkComExtensionsSendReceive()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    auto response = CreateBenchmarkLoopback(L"Benchmark Loopback COM");

    auto associationId = response.CreatedLoopbackEntry().AssociationId();
    auto endpointAId = response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId();
    auto endpointBId = response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId();

    auto cleanupLoopback = wil::scope_exit([&] { RemoveBenchmarkLoopback(associationId); });

    auto session = MidiSession::Create(L"Loopback COM Benchmark");
    VERIFY_IS_NOT_NULL(session);

    auto connSend = session.CreateEndpointConnection(endpointAId);
    VERIFY_IS_NOT_NULL(connSend);

    auto connReceive = session.CreateEndpointConnection(endpointBId);
    VERIFY_IS_NOT_NULL(connReceive);

    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    std::atomic<uint32_t> receivedWordCount{ 0 };

    // Each MIDI 1.0 channel voice UMP is a single 32-bit word, so the word count
    // and the message count are the same here.
    m_midiInCallback = [&](GUID, GUID, UINT64, UINT32 wordCount, UINT32*)
        {
            auto total = receivedWordCount.fetch_add(wordCount) + wordCount;

            if (total >= BENCHMARK_MESSAGE_COUNT)
            {
                allMessagesReceived.SetEvent();
            }
        };

    auto receiveExtension = connReceive.as<IMidiEndpointConnectionRaw>();
    VERIFY_IS_NOT_NULL(receiveExtension);

    receiveExtension->SetMessagesReceivedCallback(this);

    auto sendExtension = connSend.as<IMidiEndpointConnectionRaw>();
    VERIFY_IS_NOT_NULL(sendExtension);

    // make sure our batch size is within what the transport will accept
    auto maxWords = sendExtension->GetSupportedMaxMidiWordsPerTransmission();
    VERIFY_IS_LESS_THAN_OR_EQUAL((uint32_t)BENCHMARK_COM_BATCH_SIZE, maxWords);

    VERIFY_IS_TRUE(connSend.Open());
    VERIFY_IS_TRUE(connReceive.Open());

    // Build one batch of 10 MIDI 1.0 note on messages, reused for every send.
    uint32_t sendBuffer[BENCHMARK_COM_BATCH_SIZE]{};

    for (uint32_t i = 0; i < BENCHMARK_COM_BATCH_SIZE; i++)
    {
        sendBuffer[i] = 0x20901500 + i;
    }

    const uint32_t batchCount = BENCHMARK_MESSAGE_COUNT / BENCHMARK_COM_BATCH_SIZE;

    LOG_OUTPUT(L"Sending messages through the COM extensions");

    auto sendStartTimestamp = MidiClock::Now();

    for (uint32_t batch = 0; batch < batchCount; batch++)
    {
        auto sendResult = sendExtension->SendMidiMessagesRaw(
            MidiClock::TimestampConstantSendImmediately(),
            BENCHMARK_COM_BATCH_SIZE,
            sendBuffer);

        if (FAILED(sendResult))
        {
            std::cout << "Send failed on batch " << std::dec << batch << std::endl;
            VERIFY_SUCCEEDED(sendResult);
            break;
        }
    }

    auto sendEndTimestamp = MidiClock::Now();

    if (!allMessagesReceived.wait(BENCHMARK_RECEIVE_TIMEOUT_MS))
    {
        std::cout << "Timed out waiting for messages." << std::endl;
    }

    auto receiveEndTimestamp = MidiClock::Now();

    ReportBenchmarkResults(
        "A/B Loopback - 100,000 MIDI 1.0 UMPs, 10 at a time, COM extensions",
        sendStartTimestamp,
        sendEndTimestamp,
        receiveEndTimestamp,
        BENCHMARK_MESSAGE_COUNT,
        receivedWordCount.load());

    // release the COM references before tearing anything else down
    receiveExtension->RemoveMessagesReceivedCallback();
    m_midiInCallback = nullptr;
    receiveExtension = nullptr;
    sendExtension = nullptr;

    VERIFY_ARE_EQUAL(receivedWordCount.load(), (uint32_t)BENCHMARK_MESSAGE_COUNT);

    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());
    session.Close();
}


// ============================================================================
// 100,000 MIDI 1.0 UMP messages using MidiMessage32 and the MessageReceived event.
// Sent into the A side, received on the B side.
// ============================================================================
void MidiLoopbackBenchmarks::BenchmarkMidiMessage32SendReceive()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    auto response = CreateBenchmarkLoopback(L"Benchmark Loopback Msg32");

    auto associationId = response.CreatedLoopbackEntry().AssociationId();
    auto endpointAId = response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId();
    auto endpointBId = response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId();

    auto cleanupLoopback = wil::scope_exit([&] { RemoveBenchmarkLoopback(associationId); });

    auto session = MidiSession::Create(L"Loopback Message32 Benchmark");
    VERIFY_IS_NOT_NULL(session);

    auto connSend = session.CreateEndpointConnection(endpointAId);
    VERIFY_IS_NOT_NULL(connSend);

    auto connReceive = session.CreateEndpointConnection(endpointBId);
    VERIFY_IS_NOT_NULL(connReceive);

    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    std::atomic<uint32_t> receivedMessageCount{ 0 };

    auto eventToken = connReceive.MessageReceived([&](IMidiMessageReceivedEventSource const&, MidiMessageReceivedEventArgs const&)
        {
            auto total = receivedMessageCount.fetch_add(1) + 1;

            if (total >= BENCHMARK_MESSAGE_COUNT)
            {
                allMessagesReceived.SetEvent();
            }
        });

    VERIFY_IS_TRUE(connSend.Open());
    VERIFY_IS_TRUE(connReceive.Open());

    LOG_OUTPUT(L"Sending MidiMessage32 messages");

    auto sendStartTimestamp = MidiClock::Now();

    for (uint32_t i = 0; i < BENCHMARK_MESSAGE_COUNT; i++)
    {
        MidiMessage32 message(
            MidiClock::TimestampConstantSendImmediately(),
            0x20901500 + (i & 0x7F));

        auto sendResult = connSend.SendSingleMessagePacket(message);

        if (!MidiEndpointConnection::SendMessageSucceeded(sendResult))
        {
            std::cout << "Send failed on message " << std::dec << i
                << " with result 0x" << std::hex << (uint32_t)sendResult << std::dec << std::endl;

            VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(sendResult));
            break;
        }
    }

    auto sendEndTimestamp = MidiClock::Now();

    if (!allMessagesReceived.wait(BENCHMARK_RECEIVE_TIMEOUT_MS))
    {
        std::cout << "Timed out waiting for messages." << std::endl;
    }

    auto receiveEndTimestamp = MidiClock::Now();

    ReportBenchmarkResults(
        "A/B Loopback - 100,000 MIDI 1.0 UMPs, MidiMessage32 + event handler",
        sendStartTimestamp,
        sendEndTimestamp,
        receiveEndTimestamp,
        BENCHMARK_MESSAGE_COUNT,
        receivedMessageCount.load());

    connReceive.MessageReceived(eventToken);

    VERIFY_ARE_EQUAL(receivedMessageCount.load(), (uint32_t)BENCHMARK_MESSAGE_COUNT);

    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());
    session.Close();
}


// WinMM input callback. dwInstance points at the received-message counter.
static void CALLBACK BenchmarkMidiInProc(
    HMIDIIN /*hMidiIn*/,
    UINT wMsg,
    DWORD_PTR dwInstance,
    DWORD_PTR /*dwParam1*/,
    DWORD_PTR /*dwParam2*/)
{
    if (wMsg != MIM_DATA) return;

    auto counter = reinterpret_cast<std::atomic<uint32_t>*>(dwInstance);

    if (counter != nullptr)
    {
        counter->fetch_add(1);
    }
}


// ============================================================================
// 100,000 MIDI 1.0 byte messages using WinMM midiOutShortMsg, received through
// a WinMM midiInOpen callback. Sent to the A side destination port, received on
// the B side source port. MidiClock remains the time source.
// ============================================================================
void MidiLoopbackBenchmarks::BenchmarkWinMMShortMessageSendReceive()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    auto response = CreateBenchmarkLoopback(L"Benchmark Loopback WinMM");

    auto associationId = response.CreatedLoopbackEntry().AssociationId();
    auto endpointAId = response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId();
    auto endpointBId = response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId();

    auto cleanupLoopback = wil::scope_exit([&] { RemoveBenchmarkLoopback(associationId); });

    // The MIDI 1.0 ports are created a little after the endpoints, and their WinMM
    // port numbers are assigned asynchronously, so resolve them with retries. An A/B
    // loopback has four ports; this benchmark uses the A destination (to send into)
    // and the B source (to receive from).
    LOG_OUTPUT(L"Waiting for the legacy WinMM ports to enumerate");

    uint32_t destinationPortNumberA{ 0 };
    uint32_t sourcePortNumberB{ 0 };

    VERIFY_IS_TRUE(TryResolveWinMMPortNumber(endpointAId, Midi1PortFlow::MidiMessageDestination, destinationPortNumberA));
    VERIFY_IS_TRUE(TryResolveWinMMPortNumber(endpointBId, Midi1PortFlow::MidiMessageSource, sourcePortNumberB));

    LOG_OUTPUT(WEX::Common::String().Format(
        L"WinMM A destination port: %u, B source port: %u", destinationPortNumberA, sourcePortNumberB));

    std::atomic<uint32_t> receivedMessageCount{ 0 };

    HMIDIIN hMidiIn{ nullptr };
    auto inResult = midiInOpen(
        &hMidiIn,
        sourcePortNumberB,
        reinterpret_cast<DWORD_PTR>(BenchmarkMidiInProc),
        reinterpret_cast<DWORD_PTR>(&receivedMessageCount),
        CALLBACK_FUNCTION);

    VERIFY_ARE_EQUAL(inResult, static_cast<MMRESULT>(MMSYSERR_NOERROR));

    HMIDIOUT hMidiOut{ nullptr };
    auto outResult = midiOutOpen(&hMidiOut, destinationPortNumberA, 0, 0, CALLBACK_NULL);

    VERIFY_ARE_EQUAL(outResult, static_cast<MMRESULT>(MMSYSERR_NOERROR));

    auto cleanupPorts = wil::scope_exit([&]
        {
            if (hMidiIn != nullptr)
            {
                midiInStop(hMidiIn);
                midiInReset(hMidiIn);
                midiInClose(hMidiIn);
            }

            if (hMidiOut != nullptr)
            {
                midiOutClose(hMidiOut);
            }
        });

    VERIFY_ARE_EQUAL(midiInStart(hMidiIn), static_cast<MMRESULT>(MMSYSERR_NOERROR));

    LOG_OUTPUT(L"Sending WinMM short messages");

    auto sendStartTimestamp = MidiClock::Now();

    for (uint32_t i = 0; i < BENCHMARK_MESSAGE_COUNT; i++)
    {
        // Note On, channel 0. WinMM short messages are packed low byte first:
        // status | note << 8 | velocity << 16
        DWORD message = 0x90 | ((i & 0x7F) << 8) | (0x40 << 16);

        auto sendResult = midiOutShortMsg(hMidiOut, message);

        if (sendResult != MMSYSERR_NOERROR)
        {
            std::cout << "midiOutShortMsg failed on message " << std::dec << i
                << " with result " << sendResult << std::endl;

            VERIFY_ARE_EQUAL(sendResult, static_cast<MMRESULT>(MMSYSERR_NOERROR));
            break;
        }
    }

    auto sendEndTimestamp = MidiClock::Now();

    // Wait for the receive side to drain. WinMM has no completion event, so poll
    // until the count stops moving or we have everything.
    uint32_t lastCount{ 0 };
    uint32_t idlePolls{ 0 };

    for (uint32_t poll = 0; poll < BENCHMARK_RECEIVE_TIMEOUT_MS / 50; poll++)
    {
        auto current = receivedMessageCount.load();

        if (current >= BENCHMARK_MESSAGE_COUNT) break;

        if (current == lastCount)
        {
            // nothing arrived for 2 seconds, assume the stream has drained
            if (++idlePolls > 40) break;
        }
        else
        {
            idlePolls = 0;
            lastCount = current;
        }

        Sleep(50);
    }

    auto receiveEndTimestamp = MidiClock::Now();

    ReportBenchmarkResults(
        "A/B Loopback - 100,000 MIDI 1.0 3-byte messages, WinMM midiOutShortMsg",
        sendStartTimestamp,
        sendEndTimestamp,
        receiveEndTimestamp,
        BENCHMARK_MESSAGE_COUNT,
        receivedMessageCount.load());

    VERIFY_ARE_EQUAL(receivedMessageCount.load(), (uint32_t)BENCHMARK_MESSAGE_COUNT);
}


// ============================================================================
// 100,000 MIDI 1.0 messages using the older Windows.Devices.Midi WinRT API.
//
// That API sends a single message object at a time and raises MessageReceived
// once per incoming message, so this mirrors the MidiMessage32 + event handler
// benchmark. Sent into the A side destination port, received on the B side
// source port. MidiClock remains the time source.
// ============================================================================
void MidiLoopbackBenchmarks::BenchmarkWinRTMidi1SendReceive()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    auto response = CreateBenchmarkLoopback(L"Benchmark Loopback WinRT MIDI 1");

    auto associationId = response.CreatedLoopbackEntry().AssociationId();
    auto endpointAId = response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId();
    auto endpointBId = response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId();

    auto cleanupLoopback = wil::scope_exit([&] { RemoveBenchmarkLoopback(associationId); });

    // The MIDI 1.0 ports show up a little after the endpoints do
    LOG_OUTPUT(L"Waiting for the legacy MIDI 1.0 ports to enumerate");

    winrt::hstring destinationPortIdA{};
    winrt::hstring sourcePortIdB{};

    VERIFY_IS_TRUE(TryResolveMidi1PortDeviceId(endpointAId, Midi1PortFlow::MidiMessageDestination, destinationPortIdA));
    VERIFY_IS_TRUE(TryResolveMidi1PortDeviceId(endpointBId, Midi1PortFlow::MidiMessageSource, sourcePortIdB));

    std::wcout << L"MIDI 1.0 A destination port: " << destinationPortIdA.c_str() << std::endl;
    std::wcout << L"MIDI 1.0 B source port:      " << sourcePortIdB.c_str() << std::endl;

    auto outPort = midi1::MidiOutPort::FromIdAsync(destinationPortIdA).get();
    VERIFY_IS_NOT_NULL(outPort);

    auto inPort = midi1::MidiInPort::FromIdAsync(sourcePortIdB).get();
    VERIFY_IS_NOT_NULL(inPort);

    auto cleanupPorts = wil::scope_exit([&]
        {
            if (inPort != nullptr) inPort.Close();
            if (outPort != nullptr) outPort.Close();
        });

    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    std::atomic<uint32_t> receivedMessageCount{ 0 };

    auto eventToken = inPort.MessageReceived([&](midi1::MidiInPort const&, midi1::MidiMessageReceivedEventArgs const&)
        {
            auto total = receivedMessageCount.fetch_add(1) + 1;

            if (total >= BENCHMARK_MESSAGE_COUNT)
            {
                allMessagesReceived.SetEvent();
            }
        });

    LOG_OUTPUT(L"Sending Windows.Devices.Midi messages");

    auto sendStartTimestamp = MidiClock::Now();

    for (uint32_t i = 0; i < BENCHMARK_MESSAGE_COUNT; i++)
    {
        // one message object per send, which is how this API is normally used
        midi1::MidiNoteOnMessage message(
            0,                                  // channel
            static_cast<uint8_t>(i & 0x7F),     // note
            0x40);                              // velocity

        outPort.SendMessage(message);
    }

    auto sendEndTimestamp = MidiClock::Now();

    if (!allMessagesReceived.wait(BENCHMARK_RECEIVE_TIMEOUT_MS))
    {
        std::cout << "Timed out waiting for messages." << std::endl;
    }

    auto receiveEndTimestamp = MidiClock::Now();

    ReportBenchmarkResults(
        "A/B Loopback - 100,000 MIDI 1.0 messages, Windows.Devices.Midi WinRT API",
        sendStartTimestamp,
        sendEndTimestamp,
        receiveEndTimestamp,
        BENCHMARK_MESSAGE_COUNT,
        receivedMessageCount.load());

    inPort.MessageReceived(eventToken);

    VERIFY_ARE_EQUAL(receivedMessageCount.load(), (uint32_t)BENCHMARK_MESSAGE_COUNT);
}
