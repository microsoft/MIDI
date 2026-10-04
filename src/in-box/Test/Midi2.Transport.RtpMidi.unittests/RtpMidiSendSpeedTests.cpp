// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// The send speed limit, end to end: what the service sends to an endpoint reaches a remote no
// faster than the limit, a message on a quiet connection is never held back, and the limit can
// change while connected.
// ============================================================================

#include "pch.h"
#include "RtpMidiTransportTests.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using RtpMidiTest::IsSuccess;
using RtpMidiTest::NewGuidText;
using RtpMidiTest::Peer;
using RtpMidiTest::WaitFor;

namespace json = winrt::Windows::Data::Json;

namespace
{
    // removes the test's host however the test ends
    template <typename Action>
    class AtExit
    {
    public:
        explicit AtExit(_In_ Action action) : m_action(std::move(action)) {}
        ~AtExit() { try { m_action(); } catch (...) {} }

        AtExit(AtExit const&) = delete;
        AtExit& operator=(AtExit const&) = delete;

    private:
        Action m_action;
    };

    // the SysEx7 packets the service hands a transport for these data bytes
    std::vector<uint32_t> SysEx7Words(_In_ std::vector<uint8_t> const& data)
    {
        std::vector<uint32_t> words;

        size_t const packetCount = (std::max)((data.size() + 5) / 6, static_cast<size_t>(1));

        for (size_t packet = 0; packet < packetCount; packet++)
        {
            size_t const offset = packet * 6;
            size_t const count = (std::min)(static_cast<size_t>(6), data.size() - offset);

            uint32_t const form =
                packetCount == 1 ? 0u :
                packet == 0 ? 1u :
                packet == packetCount - 1 ? 3u : 2u;

            uint8_t bytes[6]{};
            for (size_t i = 0; i < count; i++) bytes[i] = data[offset + i];

            words.push_back(0x30000000u | (form << 20) | (static_cast<uint32_t>(count) << 16) | (static_cast<uint32_t>(bytes[0]) << 8) | bytes[1]);
            words.push_back((static_cast<uint32_t>(bytes[2]) << 24) | (static_cast<uint32_t>(bytes[3]) << 16) | (static_cast<uint32_t>(bytes[4]) << 8) | bytes[5]);
        }

        return words;
    }

    bool ContainsSequence(_In_ std::vector<uint8_t> const& haystack, _In_ std::vector<uint8_t> const& needle)
    {
        return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end()) != haystack.end();
    }

    double MillisecondsSince(_In_ std::chrono::steady_clock::time_point const start)
    {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    }

    // 3,000 data bytes: 3,002 bytes on a MIDI 1.0 cable, which takes about 0.96 s at wire speed
    std::vector<uint8_t> LargeSysExData()
    {
        std::vector<uint8_t> data(3000);
        for (size_t i = 0; i < data.size(); i++) data[i] = static_cast<uint8_t>(i % 0x80);
        return data;
    }

    std::vector<uint8_t> AsMidi1(_In_ std::vector<uint8_t> const& data)
    {
        std::vector<uint8_t> bytes{ 0xF0 };
        bytes.insert(bytes.end(), data.begin(), data.end());
        bytes.push_back(0xF7);
        return bytes;
    }
}


void RtpMidiTransportTests::TestSendSpeedLimitPacesALargeSysEx()
{
    uint16_t port{ 0 };
    auto const hostId = CreateHost(L"\"name\":\"Wire Speed Host\",\"sendSpeedLimit\":1", port);
    AtExit removeHost([&]() { RemoveHost(hostId); });

    auto const host = FindHost(hostId);
    VERIFY_ARE_EQUAL(host.GetNamedNumber(L"sendSpeedLimit", -1), 1.0, L"the host reports its limit");
    VERIFY_ARE_EQUAL(host.GetNamedNumber(L"currentSendSpeedLimit", -1), 1.0, L"and is running with it");

    auto const baseline = m_deviceManager->Endpoints().size();

    Peer remote("Wire Speed Peer", false);
    VERIFY_IS_TRUE(remote.Start(), L"the remote binds a loopback port pair");
    remote.Invite(port);

    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints().size() == baseline + 1; }, 5000), L"the remote gets an endpoint");

    RtpMidiTest::OpenedEndpoint opened(m_transport, m_deviceManager->Endpoints()[baseline].InterfaceId, 61);
    VERIFY_IS_TRUE(opened.IsOpen(), L"the service opens the endpoint");

    auto const data = LargeSysExData();
    auto const words = SysEx7Words(data);
    auto const expected = AsMidi1(data);

    auto const start = std::chrono::steady_clock::now();

    VERIFY_SUCCEEDED(opened.Send(words.data(), static_cast<UINT>(words.size() * sizeof(uint32_t))), L"the service sends the whole SysEx in one call");

    auto const sendMilliseconds = MillisecondsSince(start);

    Sleep(300);
    auto const receivedEarly = remote.Received().size();

    VERIFY_IS_TRUE(WaitFor([&]() { return ContainsSequence(remote.Received(), expected); }, 5000), L"the whole SysEx arrives intact");

    auto const totalMilliseconds = MillisecondsSince(start);

    Log::Comment(String().Format(L"The send call took %.1f ms. %zu bytes had arrived after 300 ms. All %zu arrived after %.0f ms.",
        sendMilliseconds, receivedEarly, expected.size(), totalMilliseconds));

    // an empty queue takes the whole call, so the app is not held up by the limit itself
    VERIFY_IS_LESS_THAN(sendMilliseconds, 250.0, L"the send call returns once the SysEx is queued");

    // about 1,000 bytes would have crossed a MIDI 1.0 cable in 300 ms
    VERIFY_IS_LESS_THAN(receivedEarly, static_cast<size_t>(1500), L"after 300 ms, much less than the whole SysEx has gone");

    VERIFY_IS_GREATER_THAN_OR_EQUAL(totalMilliseconds, 800.0, L"it takes about as long as on a MIDI 1.0 cable");
    VERIFY_IS_LESS_THAN(totalMilliseconds, 3000.0, L"and not much longer");

    // the rest of the connection carried on as normal
    remote.Send({ 0x90, 0x3C, 0x64 });
    VERIFY_IS_TRUE(WaitFor([&]()
    {
        auto const received = opened.Received().Words();
        return std::find(received.begin(), received.end(), 0x20903C64u) != received.end();
    }, 3000), L"messages still arrive from the remote");
}

void RtpMidiTransportTests::TestSendSpeedLimitNeverDelaysAQuietConnection()
{
    uint16_t port{ 0 };
    auto const hostId = CreateHost(L"\"name\":\"Quiet Wire Speed Host\",\"sendSpeedLimit\":1", port);
    AtExit removeHost([&]() { RemoveHost(hostId); });

    auto const baseline = m_deviceManager->Endpoints().size();

    Peer remote("Quiet Peer", false);
    VERIFY_IS_TRUE(remote.Start(), L"the remote binds a loopback port pair");
    remote.Invite(port);

    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints().size() == baseline + 1; }, 5000), L"the remote gets an endpoint");

    RtpMidiTest::OpenedEndpoint opened(m_transport, m_deviceManager->Endpoints()[baseline].InterfaceId, 62);
    VERIFY_IS_TRUE(opened.IsOpen(), L"the service opens the endpoint");

    std::vector<double> latencies;

    // a key, now and then: each one is sent on the caller's thread the moment it arrives
    for (uint8_t velocity = 0x41; velocity < 0x48; velocity++)
    {
        uint32_t const noteOn{ 0x20903C00u | velocity };
        std::vector<uint8_t> const expected{ 0x90, 0x3C, velocity };

        auto const start = std::chrono::steady_clock::now();

        VERIFY_SUCCEEDED(opened.Send(&noteOn, sizeof(noteOn)));

        // spun rather than slept, because a sleep is longer than what is being measured
        bool arrived{ false };

        while (!arrived && MillisecondsSince(start) < 1000.0)
        {
            arrived = ContainsSequence(remote.Received(), expected);
            if (!arrived) std::this_thread::yield();
        }

        VERIFY_IS_TRUE(arrived, L"the note arrives");
        latencies.push_back(MillisecondsSince(start));

        Sleep(200);
    }

    std::sort(latencies.begin(), latencies.end());
    auto const median = latencies[latencies.size() / 2];

    Log::Comment(String().Format(L"Fastest %.2f ms, median %.2f ms, slowest %.2f ms", latencies.front(), median, latencies.back()));

    VERIFY_IS_LESS_THAN(median, 10.0, L"a note on a quiet connection is not held back by the limit");
}

void RtpMidiTransportTests::TestSendSpeedLimitChangesWithoutReconnecting()
{
    auto const fields = [](uint32_t const speed)
    {
        return L"\"name\":\"Changing Speed Host\",\"port\":\"auto\",\"advertise\":false,\"sendSpeedLimit\":" + std::to_wstring(speed);
    };

    uint16_t port{ 0 };
    auto const hostId = CreateHost(L"\"name\":\"Changing Speed Host\",\"sendSpeedLimit\":1", port);
    AtExit removeHost([&]() { RemoveHost(hostId); });

    auto const baseline = m_deviceManager->Endpoints().size();

    Peer remote("Changing Speed Peer", false);
    VERIFY_IS_TRUE(remote.Start(), L"the remote binds a loopback port pair");
    remote.Invite(port);

    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints().size() == baseline + 1; }, 5000), L"the remote gets an endpoint");

    RtpMidiTest::OpenedEndpoint opened(m_transport, m_deviceManager->Endpoints()[baseline].InterfaceId, 63);
    VERIFY_IS_TRUE(opened.IsOpen(), L"the service opens the endpoint");

    // the same host again, with no limit
    VERIFY_IS_TRUE(IsSuccess(Send(L"{\"create\":{\"hosts\":{\"" + hostId + L"\":{" + fields(0) + L"}}}}")), L"the change is accepted");

    VERIFY_IS_TRUE(WaitFor([&]()
    {
        auto const host = FindHost(hostId);
        return host != nullptr && host.GetNamedNumber(L"currentSendSpeedLimit", -1) == 0.0;
    }, 5000), L"the running host takes the new limit");

    auto const data = LargeSysExData();
    auto const words = SysEx7Words(data);
    auto const expected = AsMidi1(data);

    auto const start = std::chrono::steady_clock::now();
    VERIFY_SUCCEEDED(opened.Send(words.data(), static_cast<UINT>(words.size() * sizeof(uint32_t))));

    VERIFY_IS_TRUE(WaitFor([&]() { return ContainsSequence(remote.Received(), expected); }, 5000), L"the SysEx arrives intact");

    auto const milliseconds = MillisecondsSince(start);
    Log::Comment(String().Format(L"With no limit, 3,002 bytes arrived after %.0f ms", milliseconds));

    VERIFY_IS_LESS_THAN(milliseconds, 500.0, L"with no limit, it is not paced");

    VERIFY_IS_TRUE(remote.Ended().empty(), L"the connection was never dropped");
    VERIFY_IS_FALSE(m_deviceManager->Endpoints()[baseline].Removed, L"and the endpoint stayed");
    VERIFY_ARE_EQUAL(m_deviceManager->Endpoints().size(), baseline + 1, L"no new endpoint was made");
}

void RtpMidiTransportTests::TestSendSpeedLimitSlowsAFastSenderWithoutLosingAnything()
{
    uint16_t port{ 0 };
    auto const hostId = CreateHost(L"\"name\":\"Busy Wire Speed Host\",\"sendSpeedLimit\":4", port);
    AtExit removeHost([&]() { RemoveHost(hostId); });

    auto const baseline = m_deviceManager->Endpoints().size();

    Peer remote("Busy Peer", false);
    VERIFY_IS_TRUE(remote.Start(), L"the remote binds a loopback port pair");
    remote.Invite(port);

    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints().size() == baseline + 1; }, 5000), L"the remote gets an endpoint");

    RtpMidiTest::OpenedEndpoint opened(m_transport, m_deviceManager->Endpoints()[baseline].InterfaceId, 64);
    VERIFY_IS_TRUE(opened.IsOpen(), L"the service opens the endpoint");

    // Ten SysEx of 2,000 bytes, one call each, as fast as the calls return: the way WinMM hands
    // over a large dump. About 20 KB, which takes 1.6 s at four times wire speed.
    constexpr size_t MessageCount{ 10 };

    std::vector<uint8_t> expected;
    std::vector<double> callMilliseconds;

    auto const start = std::chrono::steady_clock::now();

    for (size_t message = 0; message < MessageCount; message++)
    {
        std::vector<uint8_t> data(2000);
        for (size_t i = 0; i < data.size(); i++) data[i] = static_cast<uint8_t>((i + message * 7) % 0x80);

        auto const words = SysEx7Words(data);
        auto const midi1 = AsMidi1(data);
        expected.insert(expected.end(), midi1.begin(), midi1.end());

        auto const callStart = std::chrono::steady_clock::now();
        VERIFY_SUCCEEDED(opened.Send(words.data(), static_cast<UINT>(words.size() * sizeof(uint32_t))));
        callMilliseconds.push_back(MillisecondsSince(callStart));
    }

    auto const sendingMilliseconds = MillisecondsSince(start);

    VERIFY_IS_TRUE(WaitFor([&]() { return ContainsSequence(remote.Received(), expected); }, 10000), L"all ten arrive, intact and in order");

    auto const totalMilliseconds = MillisecondsSince(start);
    auto const longestCall = *std::max_element(callMilliseconds.begin(), callMilliseconds.end());

    Log::Comment(String().Format(L"Sending took %.0f ms, the longest call %.0f ms. Everything arrived after %.0f ms.",
        sendingMilliseconds, longestCall, totalMilliseconds));

    VERIFY_IS_GREATER_THAN(longestCall, 50.0, L"a sender faster than the limit is held back");
    VERIFY_IS_LESS_THAN(longestCall, 950.0, L"but never for longer than the service pipe waits");
    VERIFY_IS_GREATER_THAN(sendingMilliseconds, 1000.0, L"so the sending itself is slowed to about the limit");
    VERIFY_IS_GREATER_THAN_OR_EQUAL(totalMilliseconds, 1300.0, L"and the remote gets it at about four times wire speed");
    VERIFY_IS_LESS_THAN(totalMilliseconds, 5000.0, L"and not much slower");
}

void RtpMidiTransportTests::TestSendSpeedLimitReadsAnythingAboveTheMaximumAsUnlimited()
{
    struct Case { std::wstring Value; double Expected; wchar_t const* What; };

    std::vector<Case> const cases =
    {
        { L"1", 1, L"wire speed" },
        { L"16", 16, L"16 times" },
        { L"32", 32, L"32 times, the fastest limit" },
        { L"33", 0, L"just past the fastest limit" },
        { L"1000000", 0, L"far past it" },
        { L"0", 0, L"no limit" },
        { L"-4", 0, L"a negative number" },
        { L"\"fast\"", 0, L"text" },
        { L"true", 0, L"a boolean" },
    };

    for (auto const& testCase : cases)
    {
        auto const hostId = NewGuidText();

        VERIFY_IS_TRUE(IsSuccess(Send(L"{\"create\":{\"hosts\":{\"" + hostId +
            L"\":{\"name\":\"Speed Value Host\",\"port\":\"auto\",\"advertise\":false,\"enabled\":false,\"sendSpeedLimit\":" + testCase.Value + L"}}}}")),
            String().Format(L"%s: the host is accepted", testCase.What));

        auto const host = FindHost(hostId);
        auto const reported = host != nullptr ? host.GetNamedNumber(L"sendSpeedLimit", -1) : -1;

        RemoveHost(hostId);

        VERIFY_ARE_EQUAL(reported, testCase.Expected, String().Format(L"%s: %s reads as %.0f", testCase.What, testCase.Value.c_str(), testCase.Expected));
    }
}
