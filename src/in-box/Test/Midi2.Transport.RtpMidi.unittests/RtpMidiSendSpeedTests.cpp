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

using RtpMidiTest::FindConnection;
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


namespace
{
    // One host's speeds for its remotes, as the create section a configuration change sends
    std::wstring RemoteClientSettingsSection(_In_ std::wstring const& hostId, _In_ std::wstring const& remoteClients)
    {
        return L"{\"create\":{\"remoteClientSettings\":{\"" + hostId + L"\":{\"remoteClients\":[" + remoteClients + L"]}}}}";
    }

    std::wstring RemoteClientSpeed(_In_ std::wstring const& remoteName, _In_ std::wstring const& sendSpeedLimit)
    {
        return L"{\"remoteName\":\"" + remoteName + L"\",\"sendSpeedLimit\":" + sendSpeedLimit + L"}";
    }

    // A different 3,000 byte SysEx for each seed, so one arriving cannot be mistaken for another
    std::vector<uint8_t> LargeSysExData(_In_ uint8_t const seed)
    {
        std::vector<uint8_t> data(3000);
        for (size_t i = 0; i < data.size(); i++) data[i] = static_cast<uint8_t>((i + seed) % 0x80);
        return data;
    }

    // How long a large SysEx takes to reach the remote, or a negative number if it never does
    double MillisecondsToDeliver(_In_ RtpMidiTest::OpenedEndpoint& opened, _In_ Peer& remote, _In_ uint8_t const seed)
    {
        auto const data = LargeSysExData(seed);
        auto const words = SysEx7Words(data);
        auto const expected = AsMidi1(data);

        auto const start = std::chrono::steady_clock::now();

        if (FAILED(opened.Send(words.data(), static_cast<UINT>(words.size() * sizeof(uint32_t))))) return -1;
        if (!WaitFor([&]() { return ContainsSequence(remote.Received(), expected); }, 5000)) return -1;

        return MillisecondsSince(start);
    }
}

void RtpMidiTransportTests::TestRemoteClientSpeedIsUsedInsteadOfTheHostSpeed()
{
    // the host itself has no limit
    uint16_t port{ 0 };
    auto const hostId = CreateHost(L"\"name\":\"Device Speed Host\"", port);
    AtExit removeHost([&]() { RemoveHost(hostId); });

    VERIFY_IS_TRUE(IsSuccess(Send(RemoteClientSettingsSection(hostId, RemoteClientSpeed(L"Slow Peer", L"1")))),
        L"one remote is given a speed of its own");

    auto const host = FindHost(hostId);
    VERIFY_IS_TRUE(host != nullptr && host.HasKey(L"remoteClientSettings"), L"the host lists the speeds of its remotes");

    auto const listed = host.GetNamedArray(L"remoteClientSettings");
    VERIFY_ARE_EQUAL(listed.Size(), 1u, L"one of them");
    VERIFY_IS_TRUE(std::wstring{ listed.GetObjectAt(0).GetNamedString(L"remoteName", L"") } == L"Slow Peer", L"for the remote it names");
    VERIFY_ARE_EQUAL(listed.GetObjectAt(0).GetNamedNumber(L"sendSpeedLimit", -1), 1.0, L"at wire speed");

    auto const baseline = m_deviceManager->Endpoints().size();

    // one at a time, so each endpoint is known to belong to the remote just connected
    Peer slow("Slow Peer", false);
    VERIFY_IS_TRUE(slow.Start(), L"the first remote binds a loopback port pair");
    slow.Invite(port);
    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints().size() == baseline + 1; }, 5000), L"the remote with its own speed gets an endpoint");
    VERIFY_IS_TRUE(m_deviceManager->Endpoints()[baseline].EndpointName == L"Slow Peer");

    Peer fast("Fast Peer", false);
    VERIFY_IS_TRUE(fast.Start(), L"the second remote binds a loopback port pair");
    fast.Invite(port);
    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints().size() == baseline + 2; }, 5000), L"the other remote gets an endpoint");
    VERIFY_IS_TRUE(m_deviceManager->Endpoints()[baseline + 1].EndpointName == L"Fast Peer");

    auto const connected = FindHost(hostId);
    auto const slowConnection = FindConnection(connected, L"Slow Peer");
    auto const fastConnection = FindConnection(connected, L"Fast Peer");

    VERIFY_IS_TRUE(slowConnection != nullptr && fastConnection != nullptr, L"the host lists both connections");

    VERIFY_IS_TRUE(slowConnection.GetNamedBoolean(L"usesRemoteClientSettings", false), L"the first remote uses its own speed");
    VERIFY_ARE_EQUAL(slowConnection.GetNamedNumber(L"sendSpeedLimit", -1), 1.0, L"which is wire speed");
    VERIFY_ARE_EQUAL(slowConnection.GetNamedNumber(L"currentSendSpeedLimit", -1), 1.0, L"and it is sent to at that speed");

    VERIFY_IS_FALSE(fastConnection.GetNamedBoolean(L"usesRemoteClientSettings", true), L"the other uses the host's speed");
    VERIFY_ARE_EQUAL(fastConnection.GetNamedNumber(L"sendSpeedLimit", -1), 0.0, L"which is no limit");
    VERIFY_ARE_EQUAL(fastConnection.GetNamedNumber(L"currentSendSpeedLimit", -1), 0.0, L"and it is sent to without one");

    RtpMidiTest::OpenedEndpoint slowEndpoint(m_transport, m_deviceManager->Endpoints()[baseline].InterfaceId, 70);
    RtpMidiTest::OpenedEndpoint fastEndpoint(m_transport, m_deviceManager->Endpoints()[baseline + 1].InterfaceId, 71);
    VERIFY_IS_TRUE(slowEndpoint.IsOpen() && fastEndpoint.IsOpen(), L"the service opens both endpoints");

    auto const slowMilliseconds = MillisecondsToDeliver(slowEndpoint, slow, 1);
    auto const fastMilliseconds = MillisecondsToDeliver(fastEndpoint, fast, 2);

    Log::Comment(String().Format(L"3,002 bytes took %.0f ms to the remote with its own speed, and %.0f ms to the other.",
        slowMilliseconds, fastMilliseconds));

    VERIFY_IS_GREATER_THAN_OR_EQUAL(slowMilliseconds, 800.0, L"the remote with its own speed is sent to at about wire speed");
    VERIFY_IS_LESS_THAN(slowMilliseconds, 3000.0, L"and not much slower");
    VERIFY_IS_GREATER_THAN_OR_EQUAL(fastMilliseconds, 0.0, L"the other remote gets its SysEx");
    VERIFY_IS_LESS_THAN(fastMilliseconds, 500.0, L"without being slowed down");
}

void RtpMidiTransportTests::TestRemoteClientSpeedChangesWithoutReconnecting()
{
    uint16_t port{ 0 };
    auto const hostId = CreateHost(L"\"name\":\"Changing Device Speed Host\"", port);
    AtExit removeHost([&]() { RemoveHost(hostId); });

    auto const baseline = m_deviceManager->Endpoints().size();

    Peer remote("Changing Device Peer", false);
    VERIFY_IS_TRUE(remote.Start(), L"the remote binds a loopback port pair");
    remote.Invite(port);

    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints().size() == baseline + 1; }, 5000), L"the remote gets an endpoint");

    RtpMidiTest::OpenedEndpoint opened(m_transport, m_deviceManager->Endpoints()[baseline].InterfaceId, 72);
    VERIFY_IS_TRUE(opened.IsOpen(), L"the service opens the endpoint");

    auto const connectionReports = [&](bool const usesOwnSpeed, double const currentSendSpeedLimit)
    {
        auto const connection = FindConnection(FindHost(hostId), L"Changing Device Peer");

        return connection != nullptr &&
            connection.GetNamedBoolean(L"usesRemoteClientSettings", !usesOwnSpeed) == usesOwnSpeed &&
            connection.GetNamedNumber(L"currentSendSpeedLimit", -1) == currentSendSpeedLimit;
    };

    VERIFY_IS_TRUE(connectionReports(false, 0), L"the remote starts on the host's speed");

    // named without matching case, the way a remembered decision is matched
    VERIFY_IS_TRUE(IsSuccess(Send(RemoteClientSettingsSection(hostId, RemoteClientSpeed(L"changing device peer", L"1")))),
        L"the connected remote is given a speed of its own");

    VERIFY_IS_TRUE(WaitFor([&]() { return connectionReports(true, 1); }, 3000), L"the connection takes it straight away");

    auto const pacedMilliseconds = MillisecondsToDeliver(opened, remote, 3);

    VERIFY_IS_TRUE(IsSuccess(Send(RemoteClientSettingsSection(hostId, L""))), L"the remote's own speed is removed");

    VERIFY_IS_TRUE(WaitFor([&]() { return connectionReports(false, 0); }, 3000), L"the connection goes back to the host's speed");

    auto const unpacedMilliseconds = MillisecondsToDeliver(opened, remote, 4);

    Log::Comment(String().Format(L"With its own speed, 3,002 bytes took %.0f ms. Back on the host's speed, %.0f ms.",
        pacedMilliseconds, unpacedMilliseconds));

    VERIFY_IS_GREATER_THAN_OR_EQUAL(pacedMilliseconds, 800.0, L"its own speed paced the SysEx");
    VERIFY_IS_GREATER_THAN_OR_EQUAL(unpacedMilliseconds, 0.0, L"the second SysEx arrives");
    VERIFY_IS_LESS_THAN(unpacedMilliseconds, 500.0, L"and the host's speed did not pace it");

    VERIFY_IS_TRUE(remote.Ended().empty(), L"the connection was never dropped");
    VERIFY_IS_FALSE(m_deviceManager->Endpoints()[baseline].Removed, L"and the endpoint stayed");
    VERIFY_ARE_EQUAL(m_deviceManager->Endpoints().size(), baseline + 1, L"no new endpoint was made");
}

void RtpMidiTransportTests::TestRemoteClientSpeedsAreCheckedAndKeptWithTheHost()
{
    auto const hostId = NewGuidText();

    auto const createHost = [&](std::wstring const& extraFields)
    {
        return Send(L"{\"create\":{\"hosts\":{\"" + hostId +
            L"\":{\"name\":\"Kept Speeds Host\",\"port\":\"auto\",\"advertise\":false,\"enabled\":false" + extraFields + L"}}}}");
    };

    auto const reported = [&]()
    {
        std::vector<std::pair<std::wstring, double>> speeds;

        auto const host = FindHost(hostId);
        if (host == nullptr || !host.HasKey(L"remoteClientSettings")) return speeds;

        auto const listed = host.GetNamedArray(L"remoteClientSettings");

        for (uint32_t i = 0; i < listed.Size(); i++)
        {
            auto const entry = listed.GetObjectAt(i);
            speeds.emplace_back(std::wstring{ entry.GetNamedString(L"remoteName", L"") }, entry.GetNamedNumber(L"sendSpeedLimit", -1));
        }

        return speeds;
    };

    VERIFY_IS_TRUE(IsSuccess(createHost(L"")), L"the host is accepted");
    AtExit removeHost([&]() { RemoveHost(hostId); });

    std::wstring const tooLong(256, L'n');

    auto const list =
        RemoteClientSpeed(L"Kept Peer", L"2") + L"," +
        RemoteClientSpeed(L"KEPT PEER", L"8") + L"," +         // the same remote again
        RemoteClientSpeed(L"", L"4") + L"," +                  // no name
        L"42," +                                                // not an entry at all
        L"{\"sendSpeedLimit\":4}," +                            // no name either
        RemoteClientSpeed(tooLong, L"4") + L"," +               // longer than any name a remote sends
        RemoteClientSpeed(L"Too Fast Peer", L"1000");           // past the fastest limit

    VERIFY_IS_TRUE(IsSuccess(Send(RemoteClientSettingsSection(hostId, list))), L"the list is accepted");

    auto speeds = reported();
    VERIFY_ARE_EQUAL(speeds.size(), static_cast<size_t>(2), L"only the usable entries are kept");

    if (speeds.size() == 2)
    {
        VERIFY_IS_TRUE(speeds[0].first == L"Kept Peer" && speeds[0].second == 2.0, L"the first entry for a remote is the one kept");
        VERIFY_IS_TRUE(speeds[1].first == L"Too Fast Peer" && speeds[1].second == 0.0, L"a speed past the fastest limit reads as no limit");
    }

    // a host is changed by creating it again with the same id
    VERIFY_IS_TRUE(IsSuccess(createHost(L",\"sendSpeedLimit\":4")), L"the host is changed");
    VERIFY_ARE_EQUAL(reported().size(), static_cast<size_t>(2), L"changing the host keeps the speeds of its remotes");

    std::wstring many;

    for (size_t i = 0; i < 300; i++)
    {
        if (!many.empty()) many += L",";
        many += RemoteClientSpeed(L"Peer " + std::to_wstring(i), L"1");
    }

    VERIFY_IS_TRUE(IsSuccess(Send(RemoteClientSettingsSection(hostId, many))), L"a very long list is accepted");
    VERIFY_ARE_EQUAL(reported().size(), static_cast<size_t>(256), L"but only the first 256 are kept");

    RemoveHost(hostId);

    VERIFY_IS_TRUE(IsSuccess(createHost(L"")), L"a host with the same id is created again");
    VERIFY_ARE_EQUAL(reported().size(), static_cast<size_t>(0), L"removing the host removed the speeds of its remotes");

    // speeds for a host which does not exist yet are not kept for one created later
    auto const laterHostId = NewGuidText();

    VERIFY_IS_TRUE(IsSuccess(Send(RemoteClientSettingsSection(laterHostId, RemoteClientSpeed(L"Early Peer", L"1")))),
        L"speeds for a host which is not there are accepted");

    VERIFY_IS_TRUE(IsSuccess(Send(L"{\"create\":{\"hosts\":{\"" + laterHostId +
        L"\":{\"name\":\"Later Host\",\"port\":\"auto\",\"advertise\":false,\"enabled\":false}}}}")), L"the host is created after them");

    auto const laterHost = FindHost(laterHostId);

    RemoveHost(laterHostId);

    VERIFY_IS_TRUE(laterHost != nullptr && laterHost.GetNamedArray(L"remoteClientSettings").Size() == 0,
        L"and starts with no speeds for its remotes");
}
