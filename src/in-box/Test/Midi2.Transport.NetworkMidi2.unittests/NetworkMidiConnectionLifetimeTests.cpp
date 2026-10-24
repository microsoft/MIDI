// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// White-box tests of the transport's connection class. Unlike the rest of this project, this file
// is compiled against the transport's own headers and linked with its object files (see the
// project file), because what it checks can't be reached from the protocol. It does not use this
// project's precompiled header.

#include "..\..\Transport\UdpNetworkMidi2Transport\pch.h"

#include <WexTestClass.h>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    // A host connection which can be put into a session without an endpoint manager, and which
    // records the thread it was destroyed on
    class LifetimeTestConnection : public MidiNetworkHostConnection
    {
    public:
        ~LifetimeTestConnection() override
        {
            if (DestroyedOnThread != nullptr)
            {
                DestroyedOnThread->store(GetCurrentThreadId());
            }
        }

        std::atomic<DWORD>* DestroyedOnThread{ nullptr };

        DWORD WatchdogThreadId()
        {
            return ::GetThreadId(m_connectionWatcherThread.native_handle());
        }

        // What CompleteHostSessionAfterEndpointCreated does, without creating an endpoint
        HRESULT EnterSession(_In_ std::wstring const& endpointDeviceInterfaceId)
        {
            auto lock = m_sessionLock.lock();

            RETURN_IF_FAILED(TransportState::Current().AssociateMidiEndpointWithConnection(
                endpointDeviceInterfaceId,
                m_configIdentifier,
                m_remoteHostName,
                winrt::hstring{ m_remotePort },
                this));

            m_sessionEndpointDeviceInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);
            m_sessionEverEstablished = true;
            m_sessionActive = true;

            return S_OK;
        }
    };

    // Stands in for a remote which has gone quiet: bound, so the pings it is sent don't come back
    // as port-unreachable errors, and never answering
    struct SilentRemote
    {
        SOCKET Socket{ INVALID_SOCKET };
        uint16_t Port{ 0 };

        bool Open()
        {
            Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

            if (Socket == INVALID_SOCKET) return false;

            sockaddr_in local{};
            local.sin_family = AF_INET;
            inet_pton(AF_INET, "127.0.0.1", &local.sin_addr);

            if (bind(Socket, reinterpret_cast<sockaddr*>(&local), sizeof(local)) != 0) return false;

            int length = sizeof(local);

            if (getsockname(Socket, reinterpret_cast<sockaddr*>(&local), &length) != 0) return false;

            Port = ntohs(local.sin_port);

            return true;
        }

        ~SilentRemote()
        {
            if (Socket != INVALID_SOCKET) closesocket(Socket);
        }
    };
}


class NetworkMidiConnectionLifetimeTests
    : public WEX::TestClass<NetworkMidiConnectionLifetimeTests>
{
public:

    BEGIN_TEST_CLASS(NetworkMidiConnectionLifetimeTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.NetworkMidiTransport.dll")
    END_TEST_CLASS()

    // A connection whose only remaining reference is its session's, as one replaced in the
    // connection map was. Its watchdog ends the session when the remote stops answering pings,
    // which lets that last reference go on the watchdog thread itself. The connection is then
    // destroyed on the thread it has to join, which used to end the process.
    TEST_METHOD(ConnectionReleasedOnItsOwnWatchdogThreadDoesNotEndTheProcess);

    // A host's list of recent refusals stays small, keeps one entry per remote with its latest reason, newest
    // first, and forgets old ones. The remote's identity is unverified, so the list must not grow.
    TEST_METHOD(RecentRefusalsAreBoundedDeduplicatedAndExpire);
};


void NetworkMidiConnectionLifetimeTests::RecentRefusalsAreBoundedDeduplicatedAndExpire()
{
    MidiNetworkRecentRefusals refusals{ 3, std::chrono::milliseconds(1000) };

    auto const start = std::chrono::steady_clock::now();

    auto refusal = [&start](std::wstring const& name, MidiNetworkSessionRefusalReason const reason, int const atMilliseconds)
        {
            MidiNetworkRecentRefusal entry{};
            entry.UmpEndpointName = name;
            entry.ProductInstanceId = name + L"-piid";
            entry.Reason = reason;
            entry.When = start + std::chrono::milliseconds(atMilliseconds);
            return entry;
        };

    auto names = [&refusals, &start](int const atMilliseconds)
        {
            std::wstring result{};

            for (auto const& entry : refusals.Get(start + std::chrono::milliseconds(atMilliseconds)))
            {
                result += (result.empty() ? L"" : L",") + entry.UmpEndpointName;
            }

            return result;
        };

    refusals.Add(refusal(L"a", MidiNetworkSessionRefusalReason::AlreadyConnected, 0));
    refusals.Add(refusal(L"b", MidiNetworkSessionRefusalReason::AlreadyConnected, 100));
    refusals.Add(refusal(L"c", MidiNetworkSessionRefusalReason::ConnectedInOtherRole, 200));

    VERIFY_ARE_EQUAL(std::wstring{ L"c,b,a" }, names(300), L"newest first");

    // a repeat refreshes its entry rather than adding one
    refusals.Add(refusal(L"a", MidiNetworkSessionRefusalReason::AlreadyConnected, 400));
    VERIFY_ARE_EQUAL(std::wstring{ L"a,c,b" }, names(400), L"a repeat moves to the front");

    // the same remote for the other reason replaces its entry and takes the new reason
    refusals.Add(refusal(L"a", MidiNetworkSessionRefusalReason::ConnectedInOtherRole, 500));
    VERIFY_ARE_EQUAL(std::wstring{ L"a,c,b" }, names(500), L"one entry per remote");

    {
        auto const current = refusals.Get(start + std::chrono::milliseconds(500));
        VERIFY_IS_TRUE(!current.empty() && current.front().Reason == MidiNetworkSessionRefusalReason::ConnectedInOtherRole, L"the latest reason");
    }

    refusals.Add(refusal(L"d", MidiNetworkSessionRefusalReason::AlreadyConnected, 550));
    VERIFY_ARE_EQUAL(std::wstring{ L"d,a,c" }, names(550), L"capped at three, oldest dropped");

    // a flood of new identities never holds more than the cap
    for (int i = 0; i < 100; i++)
    {
        refusals.Add(refusal(L"flood" + std::to_wstring(i), MidiNetworkSessionRefusalReason::AlreadyConnected, 600));
    }

    VERIFY_ARE_EQUAL(static_cast<size_t>(3), refusals.Get(start + std::chrono::milliseconds(600)).size(), L"never more than the cap");

    // all of those were added at 600 ms and live for 1000 ms
    VERIFY_ARE_EQUAL(static_cast<size_t>(3), refusals.Get(start + std::chrono::milliseconds(1599)).size(), L"not expired yet");
    VERIFY_ARE_EQUAL(static_cast<size_t>(0), refusals.Get(start + std::chrono::milliseconds(1600)).size(), L"expired");
}


void NetworkMidiConnectionLifetimeTests::ConnectionReleasedOnItsOwnWatchdogThreadDoesNotEndTheProcess()
{
    WSADATA wsaData{};
    VERIFY_ARE_EQUAL(0, WSAStartup(MAKEWORD(2, 2), &wsaData));
    auto cleanupWinsock = wil::scope_exit([]() { WSACleanup(); });

    // Kept for the rest of the process, as in the service. The linked transport code caches WinRT
    // activation factories, which go stale if the last apartment goes away between test methods.
    static CO_MTA_USAGE_COOKIE mtaUsage{ nullptr };

    if (mtaUsage == nullptr)
    {
        VERIFY_SUCCEEDED(CoIncrementMTAUsage(&mtaUsage));
    }

    auto& state = TransportState::Current();

    // The fastest watchdog the settings allow, so five missed pings take about a second
    auto const previousPingInterval = state.TransportSettings.OutboundPingInterval.load();
    state.TransportSettings.OutboundPingInterval = MIDI_NETWORK_OUTBOUND_PING_INTERVAL_LOWER_BOUND;
    auto restorePingInterval = wil::scope_exit([&]() { state.TransportSettings.OutboundPingInterval = previousPingInterval; });

    SilentRemote remote;
    VERIFY_IS_TRUE(remote.Open());

    winrt::Windows::Networking::Sockets::DatagramSocket socket;
    socket.BindEndpointAsync(winrt::Windows::Networking::HostName{ L"127.0.0.1" }, L"").get();

    winrt::guid entryIdentifier{};
    VERIFY_SUCCEEDED(CoCreateGuid(reinterpret_cast<GUID*>(&entryIdentifier)));

    winrt::Windows::Networking::HostName const remoteHostName{ L"127.0.0.1" };
    winrt::hstring const remotePort{ winrt::to_hstring(remote.Port) };

    auto connection = std::make_shared<LifetimeTestConnection>();

    VERIFY_SUCCEEDED(connection->Initialize(
        entryIdentifier,
        L"SWD\\MIDISRV\\NM2LIFETIMETEST",
        socket,
        remoteHostName,
        remotePort,
        nullptr,
        L"Lifetime Test",
        L"lifetime-test",
        10,
        0,
        true,
        0));

    auto registered = state.AddNetworkConnectionIfAbsent(remoteHostName, remotePort, connection);
    VERIFY_IS_TRUE(registered == connection, L"The connection is registered");
    registered.reset();

    VERIFY_SUCCEEDED(connection->EnterSession(L"\\\\?\\swd#midisrv#nm2lifetimetest#{e7cce071-3c03-423f-88d3-f1045d756eb3}"));

    // What a connection replaced in the map was left with: only its session's reference
    auto detached = state.DetachNetworkConnection(entryIdentifier, remoteHostName, remotePort, connection.get());
    VERIFY_IS_TRUE(detached == connection, L"The connection map let go of it");
    detached.reset();

    std::atomic<DWORD> destroyedOnThread{ 0 };
    connection->DestroyedOnThread = &destroyedOnThread;

    auto const watchdogThreadId = connection->WatchdogThreadId();
    VERIFY_ARE_NOT_EQUAL(static_cast<DWORD>(0), watchdogThreadId, L"The watchdog is running");

    std::weak_ptr<MidiNetworkConnection> weakConnection{ connection };

    connection.reset();

    // Nothing but the session holds it now
    auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);

    while (!weakConnection.expired() && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    VERIFY_IS_TRUE(weakConnection.expired(), L"The watchdog ended the session and the connection was destroyed");

    Log::Comment(String().Format(L"Watchdog thread %u, destroyed on thread %u", watchdogThreadId, destroyedOnThread.load()));

    VERIFY_ARE_EQUAL(watchdogThreadId, destroyedOnThread.load(), L"The last reference went on the watchdog thread itself");

    // The detached watchdog thread finishes on its own. Reaching here at all is the real result.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}
