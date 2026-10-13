// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"
#include "MidiRtpApiTests.h"

#include <winrt/Windows.Devices.Midi2.Enumeration.h>

using namespace WEX::Logging;
using namespace WEX::Common;
using namespace winrt::Windows::Devices::Midi2;

namespace
{
    constexpr uint32_t KeptServiceWaitMilliseconds{ 5000 };

    // a remote that does not answer is asked for about twelve seconds, then again 15 seconds later
    constexpr uint32_t KeptReconnectWaitMilliseconds{ 30000 };

    bool KeptTransportIsAvailable()
    {
        try
        {
            return MidiRtpTransportManager::IsTransportAvailable();
        }
        catch (...)
        {
            return false;
        }
    }

    MidiRtpConfiguredClient FindClient(_In_ winrt::guid const& clientId)
    {
        for (auto const& client : MidiRtpTransportManager::GetConfiguredClients())
        {
            if (client.ClientId() == clientId) return client;
        }

        return nullptr;
    }

    bool IsConnected(_In_ winrt::guid const& clientId)
    {
        auto const client = FindClient(clientId);
        auto const connection = client == nullptr ? nullptr : client.Connection();

        return connection != nullptr && connection.IsConnected();
    }

    bool EndpointIsPresent(_In_ winrt::hstring const& endpointDeviceId)
    {
        return !endpointDeviceId.empty() &&
            Enumeration::MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(endpointDeviceId) != nullptr;
    }

    bool Contains(_In_ std::vector<uint8_t> const& bytes, _In_ std::vector<uint8_t> const& wanted)
    {
        return std::search(bytes.begin(), bytes.end(), wanted.begin(), wanted.end()) != bytes.end();
    }
}

// The endpoint of a client that keeps it is there before its remote answers, carries on while the
// remote comes and goes, and goes when the client is disconnected
void MidiRtpApiTests::TestKeptClientEndpointOutlivesItsRemote()
{
    if (!KeptTransportIsAvailable())
    {
        Log::Result(TestResults::Skipped, L"The RTP-MIDI transport is not installed on this PC.");
        return;
    }

    // the port the remote will have later, found by letting a first remote take it and go
    uint16_t port{ 0 };
    {
        RtpMidiTest::Peer probe("SDK Port Probe", true);
        VERIFY_IS_TRUE(probe.Start(), L"a test remote binds");

        port = probe.ControlPort();
        probe.Stop();
    }

    MidiRtpClientMatchCriteria criteria;
    criteria.DirectHostNameOrIPAddress(L"::1");
    criteria.DirectPort(port);

    MidiRtpClientConnectConfig config;
    config.MatchCriteria(criteria);
    config.CustomEndpointName(L"SDK Kept Endpoint");
    config.OfflineRetentionSeconds(static_cast<int32_t>(MidiRtpOfflineRetention::KeepAlways));

    auto const clientId = config.ClientId();

    auto disconnectClient = wil::scope_exit([&]
        {
            try
            {
                MidiRtpTransportManager::DisconnectRtpClientAsync(MidiRtpClientDisconnectConfig(clientId)).get();
            }
            catch (...)
            {
            }
        });

    auto const created = MidiRtpTransportManager::ConnectRtpClientAsync(config).get();
    VERIFY_IS_TRUE(created.Success(), created.ErrorMessage().c_str());

    auto const listed = FindClient(clientId);
    VERIFY_IS_TRUE(listed != nullptr, L"the service has the client");

    if (listed.OfflineRetentionSeconds() != static_cast<int32_t>(MidiRtpOfflineRetention::KeepAlways))
    {
        Log::Result(TestResults::Skipped, L"The RTP-MIDI transport on this PC is older than keeping endpoints.");
        return;
    }

    winrt::hstring endpointId{};

    VERIFY_IS_TRUE(RtpMidiTest::WaitFor([&]()
        {
            auto const client = FindClient(clientId);
            endpointId = client == nullptr ? winrt::hstring{} : client.EndpointDeviceId();
            return !endpointId.empty();
        }, KeptServiceWaitMilliseconds), L"the client has an endpoint before anything answers");

    VERIFY_IS_TRUE(EndpointIsPresent(endpointId), L"and Windows lists it");
    VERIFY_IS_FALSE(IsConnected(clientId), L"while there is no connection");

    // declared before the session, so the session is closed before these go away
    std::mutex receivedLock;
    std::vector<uint32_t> receivedWords{};

    auto session = MidiSession::Create(L"MidiApiTest kept endpoint");
    VERIFY_IS_TRUE(session != nullptr);

    if (session == nullptr)
    {
        return;
    }

    auto closeSession = wil::scope_exit([&] { session.Close(); });

    auto endpoint = session.CreateEndpointConnection(endpointId);

    endpoint.MessageReceived([&](auto const&, MidiMessageReceivedEventArgs const& args)
        {
            auto lock = std::scoped_lock{ receivedLock };
            receivedWords.push_back(args.PeekFirstWord());
        });

    VERIFY_IS_TRUE(endpoint.Open(), L"an app can open it while the remote is away");

    auto const awayStart = std::chrono::steady_clock::now();

    for (uint32_t i = 0; i < 10; i++)
    {
        VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(endpoint.SendSingleMessageWords(0, 0x20803C00)), L"and send to it");
    }

    auto const awayMilliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - awayStart).count();
    Log::Comment(String().Format(L"10 messages sent while the remote was away in %.0f ms", awayMilliseconds));
    VERIFY_IS_LESS_THAN(awayMilliseconds, 1000.0, L"without being held up");

    RtpMidiTest::Peer remote("SDK Kept Remote", true);
    VERIFY_IS_TRUE(remote.Start(), L"the remote binds");
    VERIFY_ARE_EQUAL(static_cast<uint32_t>(remote.ControlPort()), static_cast<uint32_t>(port), L"on the port the client is set up for");

    // asks for a try now, rather than at the next retry
    MidiRtpTransportManager::ReconnectRtpClientAsync(clientId).get();

    VERIFY_IS_TRUE(RtpMidiTest::WaitFor([&]() { return IsConnected(clientId); }, KeptReconnectWaitMilliseconds), L"the client connects once the remote answers");

    VERIFY_IS_TRUE(FindClient(clientId).Connection().EndpointDeviceId() == endpointId, L"through the endpoint it already had");
    VERIFY_IS_TRUE(FindClient(clientId).EndpointDeviceId() == endpointId);

    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(endpoint.SendSingleMessageWords(0, 0x20903D64)));
    VERIFY_IS_TRUE(RtpMidiTest::WaitFor([&]() { return Contains(remote.Received(), { 0x90, 0x3D, 0x64 }); }, KeptServiceWaitMilliseconds),
        L"the connection the app opened earlier reaches the remote");
    VERIFY_IS_FALSE(Contains(remote.Received(), { 0x80, 0x3C, 0x00 }), L"what was sent while the remote was away is not delivered late");

    remote.Send({ 0x90, 0x40, 0x7F });

    VERIFY_IS_TRUE(RtpMidiTest::WaitFor([&]()
        {
            auto lock = std::scoped_lock{ receivedLock };
            return std::find(receivedWords.begin(), receivedWords.end(), 0x2090407Fu) != receivedWords.end();
        }, KeptServiceWaitMilliseconds), L"and the remote reaches the app");

    remote.Stop();

    VERIFY_IS_TRUE(RtpMidiTest::WaitFor([&]() { return !IsConnected(clientId); }, KeptServiceWaitMilliseconds), L"the connection ends when the remote says goodbye");
    VERIFY_IS_TRUE(FindClient(clientId).EndpointDeviceId() == endpointId, L"the endpoint stays");
    VERIFY_IS_TRUE(EndpointIsPresent(endpointId));
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(endpoint.SendSingleMessageWords(0, 0x20803D00)), L"and still takes messages");

    closeSession.reset();

    auto const disconnected = MidiRtpTransportManager::DisconnectRtpClientAsync(MidiRtpClientDisconnectConfig(clientId)).get();
    VERIFY_IS_TRUE(disconnected.Success(), disconnected.ErrorMessage().c_str());

    disconnectClient.release();

    VERIFY_IS_TRUE(RtpMidiTest::WaitFor([&]() { return !EndpointIsPresent(endpointId); }, KeptServiceWaitMilliseconds),
        L"disconnecting the client removes its endpoint");
}
