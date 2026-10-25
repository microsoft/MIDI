// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include "midi_dnssd_advertiser.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace NetworkMidiTest;

namespace
{
    constexpr auto InvitationTimeout{ std::chrono::seconds(15) };
    constexpr auto ReplyTimeout{ std::chrono::seconds(10) };
    constexpr auto StateTimeout{ std::chrono::seconds(10) };

    // mDNS: a new service is seen within a few seconds, and so is a goodbye
    constexpr auto AdvertisedInvitationTimeout{ std::chrono::seconds(30) };
    constexpr auto GoodbyeSettleTime{ std::chrono::seconds(5) };

    // Several scans at the interval below, so an entry which was going to retry would have
    constexpr auto QuietPeriod{ std::chrono::seconds(4) };

    constexpr uint32_t ScanIntervalMilliseconds{ 500 };
    constexpr uint32_t PingIntervalMilliseconds{ 60000 };

    // NETWORK_ERROR_CODE_INVITATION_ENDED_BY_HOST, MidiNetworkClientConnectErrorCode.InvitationEndedByHost
    constexpr double EndedByHostErrorCode{ 0x78 };

    bool StartTransport(_In_ InProcessTransport& transport)
    {
        if (!transport.Start()) return false;

        auto const response = transport.Configure(
            L"{\"transportSettings\":{\"outboundPingInterval\":" + std::to_wstring(PingIntervalMilliseconds) +
            L",\"directConnectionScanInterval\":" + std::to_wstring(ScanIntervalMilliseconds) + L"}}");

        return response.has_value();
    }

    // The transport's settings outlive the transport in this process
    void RestoreDefaultSettings(_In_ InProcessTransport& transport)
    {
        transport.Configure(
            L"{\"transportSettings\":{\"outboundPingInterval\":2000,\"directConnectionScanInterval\":20000}}");
    }

    std::vector<uint8_t> Accepted(_In_ std::string const& name, _In_ std::string const& productInstanceId)
    {
        PacketBuilder builder;
        builder.StartPacket().AddInvitationReplyAccepted(name, productInstanceId);
        return builder.Bytes();
    }

    std::vector<uint8_t> ByeFor(_In_ ByeReason const reason)
    {
        PacketBuilder builder;
        builder.StartPacket().AddBye(reason, "Ended by the test.");
        return builder.Bytes();
    }

    // The first invitation, from any port
    std::optional<RawUdpPeer::Received> WaitForInvitation(_In_ RawUdpPeer& device, _In_ std::chrono::milliseconds const timeout)
    {
        return device.WaitFor(CommandCode::Invitation, 0, timeout);
    }

    size_t CountInvitations(_In_ std::vector<RawUdpPeer::Received> const& received)
    {
        size_t count{ 0 };

        for (auto const& entry : received)
        {
            count += entry.Packet.Count(CommandCode::Invitation);
        }

        return count;
    }

    std::optional<winrt::Windows::Data::Json::JsonObject> WaitForClientEntry(
        _In_ InProcessTransport& transport,
        _In_ std::wstring const& entryIdentifier,
        _In_ std::function<bool(winrt::Windows::Data::Json::JsonObject const&)> const& condition)
    {
        auto const deadline = std::chrono::steady_clock::now() + StateTimeout;
        std::optional<winrt::Windows::Data::Json::JsonObject> entry{};

        while (std::chrono::steady_clock::now() < deadline)
        {
            entry = transport.FindClientEntry(entryIdentifier);

            if (entry.has_value() && condition(entry.value()))
            {
                return entry;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (entry.has_value())
        {
            Log::Comment(String().Format(L"Last state of %s: %s", entryIdentifier.c_str(), entry->Stringify().c_str()));
        }

        return std::nullopt;
    }

    bool IsConnected(_In_ winrt::Windows::Data::Json::JsonObject const& entry)
    {
        return entry.GetNamedBoolean(L"sessionActive", false);
    }

    bool IsEndedByHost(_In_ winrt::Windows::Data::Json::JsonObject const& entry)
    {
        return entry.GetNamedString(L"entryState", L"") == L"failed" &&
            entry.GetNamedNumber(L"lastErrorCode", 0) == EndedByHostErrorCode &&
            !entry.GetNamedBoolean(L"sessionActive", false);
    }

    // Sends the host's Bye and waits for the client's Bye Reply. Returns everything which arrived
    // meanwhile, so an invitation sent straight after the Bye Reply is counted, not dropped.
    std::vector<RawUdpPeer::Received> EndFromTheHost(
        _In_ RawUdpPeer& device,
        _In_ RawUdpPeer::Received const& session,
        _In_ ByeReason const reason)
    {
        std::vector<RawUdpPeer::Received> arrived{};

        VERIFY_IS_TRUE(device.SendToSender(session, ByeFor(reason)));

        bool answered{ false };
        auto const deadline = std::chrono::steady_clock::now() + ReplyTimeout;

        while (!answered && std::chrono::steady_clock::now() < deadline)
        {
            for (auto& entry : device.ReceiveFor(std::chrono::milliseconds(100)))
            {
                answered = answered || (entry.FromPort == session.FromPort && entry.Packet.Contains(CommandCode::ByeReply));
                arrived.push_back(std::move(entry));
            }
        }

        VERIFY_IS_TRUE(answered, L"The client answered the Bye");

        return arrived;
    }

    // Accepts an invitation and waits until the entry has a session
    bool AcceptAndWaitForSession(
        _In_ InProcessTransport& transport,
        _In_ RawUdpPeer& device,
        _In_ RawUdpPeer::Received const& invitation,
        _In_ std::wstring const& clientEntry,
        _In_ std::string const& name,
        _In_ std::string const& productInstanceId)
    {
        if (!device.SendToSender(invitation, Accepted(name, productInstanceId))) return false;

        return WaitForClientEntry(transport, clientEntry, IsConnected).has_value();
    }

    // Connected by address. The host ends the session for the reason given. Returns what arrived
    // while the client answered.
    std::vector<RawUdpPeer::Received> ConnectDirectThenEndFromTheHost(
        _In_ InProcessTransport& transport,
        _In_ RawUdpPeer& device,
        _In_ std::wstring const& clientEntry,
        _In_ ByeReason const reason,
        _In_ std::string const& name,
        _In_ std::string const& productInstanceId)
    {
        VERIFY_IS_TRUE(transport.ConnectDirect(clientEntry, L"127.0.0.1", device.Port()));

        auto const invitation = WaitForInvitation(device, InvitationTimeout);
        VERIFY_IS_TRUE(invitation.has_value(), L"The client entry invited the device");

        if (!invitation.has_value()) return {};

        VERIFY_IS_TRUE(AcceptAndWaitForSession(transport, device, invitation.value(), clientEntry, name, productInstanceId), L"The client entry is connected");

        return EndFromTheHost(device, invitation.value(), reason);
    }

    void VerifyStopsUntilTryAgain(_In_ ByeReason const reason, _In_ std::string const& name, _In_ std::string const& productInstanceId)
    {
        InProcessTransport transport;
        VERIFY_IS_TRUE(StartTransport(transport), L"The transport started in the test process");
        auto restoreSettings = wil::scope_exit([&transport]() { RestoreDefaultSettings(transport); });

        RawUdpPeer device;
        VERIFY_IS_TRUE(device.Open());

        auto const clientEntry = MakeEntryIdentifier();

        auto const afterBye = ConnectDirectThenEndFromTheHost(transport, device, clientEntry, reason, name, productInstanceId);

        VERIFY_IS_TRUE(WaitForClientEntry(transport, clientEntry, IsEndedByHost).has_value(), L"The entry stopped, ended by the host");

        auto const quiet = device.ReceiveFor(QuietPeriod);
        VERIFY_ARE_EQUAL(static_cast<size_t>(0), CountInvitations(afterBye) + CountInvitations(quiet), L"No new invitation after the host ended the session");

        // Try again: the app connects the same entry again
        VERIFY_IS_TRUE(transport.ConnectDirect(clientEntry, L"127.0.0.1", device.Port()));

        auto const again = WaitForInvitation(device, InvitationTimeout);
        VERIFY_IS_TRUE(again.has_value(), L"Try again invites the device again");

        if (again.has_value())
        {
            VERIFY_IS_TRUE(AcceptAndWaitForSession(transport, device, again.value(), clientEntry, name, productInstanceId), L"Connected again");
        }

        transport.DisconnectClient(clientEntry);
    }

    std::wstring LocalHostName()
    {
        wchar_t name[MAX_COMPUTERNAME_LENGTH + 1]{};
        DWORD size{ ARRAYSIZE(name) };

        if (!GetComputerNameExW(ComputerNameDnsHostname, name, &size)) return {};

        return std::wstring{ name } + L".local";
    }
}


void NetworkMidiRemoteByeTests::ClientStopsWhenTheHostsUserEndsTheSession()
{
    VerifyStopsUntilTryAgain(ByeReason::UserTerminated, "Remote Bye User Device", "remote-bye-user-device");
}


void NetworkMidiRemoteByeTests::ClientStopsWhenTheHostPowersDown()
{
    VerifyStopsUntilTryAgain(ByeReason::PowerDown, "Remote Bye Power Device", "remote-bye-power-device");
}


void NetworkMidiRemoteByeTests::ClientReconnectsAfterOtherByeReasons()
{
    InProcessTransport transport;
    VERIFY_IS_TRUE(StartTransport(transport), L"The transport started in the test process");
    auto restoreSettings = wil::scope_exit([&transport]() { RestoreDefaultSettings(transport); });

    std::string const name{ "Remote Bye Other Device" };
    std::string const productInstanceId{ "remote-bye-other-device" };

    RawUdpPeer device;
    VERIFY_IS_TRUE(device.Open());

    auto const clientEntry = MakeEntryIdentifier();

    auto const afterBye = ConnectDirectThenEndFromTheHost(transport, device, clientEntry, ByeReason::Undefined, name, productInstanceId);

    auto const invitedAgain = CountInvitations(afterBye) > 0 || WaitForInvitation(device, InvitationTimeout).has_value();
    VERIFY_IS_TRUE(invitedAgain, L"Any other reason invites the device again by itself");

    transport.DisconnectClient(clientEntry);
}


void NetworkMidiRemoteByeTests::ClientConnectsAgainWhenTheHostAdvertisesAgain()
{
    InProcessTransport transport;
    VERIFY_IS_TRUE(StartTransport(transport), L"The transport started in the test process");
    auto restoreSettings = wil::scope_exit([&transport]() { RestoreDefaultSettings(transport); });

    auto const hostName = LocalHostName();
    VERIFY_IS_FALSE(hostName.empty(), L"This PC's host name");

    // On every address, because the transport invites a host at the addresses mDNS gives for it
    RawUdpPeer device;
    VERIFY_IS_TRUE(device.Open(true));

    auto const suffix = MakeEntryIdentifier().substr(1, 8);
    std::string const name{ "Remote Bye Advertised Device" };
    std::string const productInstanceId{ "remote-bye-advertised-" + winrt::to_string(suffix) };
    std::wstring const wideProductInstanceId{ productInstanceId.begin(), productInstanceId.end() };
    std::wstring const instanceLabel{ L"nm2-remote-bye-" + suffix };

    auto advertise = [&]()
        {
            auto advertisement = std::make_unique<::WindowsMidiServicesInternal::MidiDnssdAdvertiser>();

            auto const hr = advertisement->Register(
                instanceLabel,
                L"_midi2._udp.local",
                hostName,
                device.Port(),
                {
                    { L"UMPEndpointName", std::wstring{ name.begin(), name.end() } },
                    { L"ProductInstanceId", wideProductInstanceId }
                },
                ::WindowsMidiServicesInternal::MidiDnssdRegistrationTimeoutMilliseconds,
                std::stop_token{ });

            Log::Comment(String().Format(L"Registered %s on %s: 0x%08X", instanceLabel.c_str(), hostName.c_str(), hr));

            return SUCCEEDED(hr) ? std::move(advertisement) : nullptr;
        };

    auto advertisement = advertise();

    if (advertisement == nullptr)
    {
        Log::Result(TestResults::Skipped, L"This PC can't register a DNS-SD service, so the advertisement can't be taken away and put back.");
        return;
    }

    // Matched by the product instance id the advertisement carries
    auto const clientEntry = MakeEntryIdentifier();

    auto const response = transport.Configure(
        L"{\"transportCommand\":{\"commandName\":\"connectMdns\",\"commandArguments\":{"
        L"\"entryIdentifier\":\"" + clientEntry + L"\","
        L"\"matchId\":\"" + wideProductInstanceId + L"\","
        L"\"umpEndpointName\":\"In Process Test Client\"}}}");

    VERIFY_IS_TRUE(response.has_value() && response->GetNamedBoolean(L"success", false), L"The mDNS client entry was created");

    auto const invitation = WaitForInvitation(device, AdvertisedInvitationTimeout);
    VERIFY_IS_TRUE(invitation.has_value(), L"The client entry found the advertisement and invited the device");

    if (!invitation.has_value()) return;

    VERIFY_IS_TRUE(AcceptAndWaitForSession(transport, device, invitation.value(), clientEntry, name, productInstanceId), L"The client entry is connected");

    auto const afterBye = EndFromTheHost(device, invitation.value(), ByeReason::UserTerminated);

    VERIFY_IS_TRUE(WaitForClientEntry(transport, clientEntry, IsEndedByHost).has_value(), L"The entry stopped, ended by the host");

    // still advertised, and announced again, which isn't advertising again
    auto const quiet = device.ReceiveFor(QuietPeriod);
    VERIFY_ARE_EQUAL(static_cast<size_t>(0), CountInvitations(afterBye) + CountInvitations(quiet), L"No new invitation while the host stays advertised");

    // The host leaves the network and comes back
    advertisement->Unregister();
    advertisement.reset();

    auto const gone = device.ReceiveFor(GoodbyeSettleTime);
    VERIFY_ARE_EQUAL(static_cast<size_t>(0), CountInvitations(gone), L"No invitation while the host is gone");

    advertisement = advertise();
    VERIFY_IS_NOT_NULL(advertisement.get(), L"Advertised again");

    auto const again = WaitForInvitation(device, AdvertisedInvitationTimeout);
    VERIFY_IS_TRUE(again.has_value(), L"The host advertising again brought the entry back");

    if (again.has_value())
    {
        VERIFY_IS_TRUE(AcceptAndWaitForSession(transport, device, again.value(), clientEntry, name, productInstanceId), L"Connected again");
    }

    transport.DisconnectClient(clientEntry);

    if (advertisement != nullptr)
    {
        advertisement->Unregister();
    }
}
