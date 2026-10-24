// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace NetworkMidiTest;

namespace
{
    constexpr auto InvitationTimeout{ std::chrono::seconds(15) };
    constexpr auto ReplyTimeout{ std::chrono::seconds(10) };
    constexpr auto StateTimeout{ std::chrono::seconds(10) };

    // How long a waiting entry is watched for invitations. Several scans at the interval below.
    constexpr auto QuietPeriod{ std::chrono::seconds(4) };

    // Fast scans, so an entry which was going to retry on the timer would do it during the quiet
    // period, and slow pings, so no session times out while a test runs
    constexpr uint32_t ScanIntervalMilliseconds{ 500 };
    constexpr uint32_t PingIntervalMilliseconds{ 60000 };

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

    std::vector<uint8_t> Invitation(_In_ std::string const& name, _In_ std::string const& productInstanceId)
    {
        PacketBuilder builder;
        builder.StartPacket().AddInvitation(name, productInstanceId);
        return builder.Bytes();
    }

    std::vector<uint8_t> Accepted(_In_ std::string const& name, _In_ std::string const& productInstanceId)
    {
        PacketBuilder builder;
        builder.StartPacket().AddInvitationReplyAccepted(name, productInstanceId);
        return builder.Bytes();
    }

    std::vector<uint8_t> Bye()
    {
        PacketBuilder builder;
        builder.StartPacket().AddBye(ByeReason::UserTerminated, "Test cleanup.");
        return builder.Bytes();
    }

    // The first invitation from a port not in the list. Every port but a host's is a client here.
    std::optional<RawUdpPeer::Received> WaitForInvitation(
        _In_ RawUdpPeer& device,
        _In_ std::vector<uint16_t> const& notFrom,
        _In_ std::chrono::milliseconds const timeout)
    {
        auto const deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline)
        {
            for (auto& entry : device.ReceiveFor(std::chrono::milliseconds(100)))
            {
                if (entry.Packet.Contains(CommandCode::Invitation) &&
                    std::find(notFrom.begin(), notFrom.end(), entry.FromPort) == notFrom.end())
                {
                    return entry;
                }
            }
        }

        return std::nullopt;
    }

    size_t CountInvitations(_In_ std::vector<RawUdpPeer::Received> const& received, _In_ uint16_t const notFrom)
    {
        size_t count{ 0 };

        for (auto const& entry : received)
        {
            if (entry.FromPort != notFrom)
            {
                count += entry.Packet.Count(CommandCode::Invitation);
            }
        }

        return count;
    }

    // The first invitation from each of count different ports. Two clients starting together send
    // theirs together, so they're collected in one go rather than one at a time.
    std::vector<RawUdpPeer::Received> WaitForInvitations(
        _In_ RawUdpPeer& device,
        _In_ size_t const count,
        _In_ std::chrono::milliseconds const timeout)
    {
        std::vector<RawUdpPeer::Received> found{};

        auto const deadline = std::chrono::steady_clock::now() + timeout;

        while (found.size() < count && std::chrono::steady_clock::now() < deadline)
        {
            for (auto& entry : device.ReceiveFor(std::chrono::milliseconds(100)))
            {
                if (entry.Packet.Contains(CommandCode::Invitation) &&
                    std::none_of(found.begin(), found.end(), [&entry](auto const& existing) { return existing.FromPort == entry.FromPort; }))
                {
                    found.push_back(entry);
                }
            }
        }

        return found;
    }

    bool SameIdentifier(_In_ std::wstring const& left, _In_ std::wstring const& right)
    {
        return _wcsicmp(left.c_str(), right.c_str()) == 0;
    }

    // Polls a client entry until the condition holds, and returns the last copy seen
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

    bool IsWaitingFor(_In_ winrt::Windows::Data::Json::JsonObject const& entry, _In_ std::wstring const& blockingEntry, _In_ bool const blockingIsHost)
    {
        return SameIdentifier(std::wstring{ entry.GetNamedString(L"waitingForEntryIdentifier", L"") }, blockingEntry) &&
            entry.GetNamedBoolean(L"waitingForHostSession", !blockingIsHost) == blockingIsHost &&
            entry.GetNamedString(L"entryState", L"") == L"pending" &&
            !entry.GetNamedBoolean(L"sessionActive", false);
    }

    bool IsConnected(_In_ winrt::Windows::Data::Json::JsonObject const& entry)
    {
        return entry.GetNamedBoolean(L"sessionActive", false) && !entry.HasKey(L"waitingForEntryIdentifier");
    }

    winrt::Windows::Data::Json::JsonArray RecentRefusals(_In_ InProcessTransport& transport, _In_ std::wstring const& hostEntry)
    {
        auto const host = transport.FindHostEntry(hostEntry);

        if (!host.has_value()) return winrt::Windows::Data::Json::JsonArray{};

        auto refusals = host->GetNamedArray(L"recentRefusals", nullptr);

        return refusals != nullptr ? refusals : winrt::Windows::Data::Json::JsonArray{};
    }

    winrt::Windows::Data::Json::JsonArray WaitForRefusalCount(_In_ InProcessTransport& transport, _In_ std::wstring const& hostEntry, _In_ uint32_t const count)
    {
        auto const deadline = std::chrono::steady_clock::now() + StateTimeout;
        auto refusals = RecentRefusals(transport, hostEntry);

        while (refusals.Size() < count && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            refusals = RecentRefusals(transport, hostEntry);
        }

        Log::Comment(String().Format(L"Recent refusals: %s", refusals.Stringify().c_str()));

        return refusals;
    }
}


void NetworkMidiSessionRefusalTests::ClientWaitsWhileTheRemoteIsAClientOfAHostHere()
{
    InProcessTransport transport;
    VERIFY_IS_TRUE(StartTransport(transport), L"The transport started in the test process");
    auto restoreSettings = wil::scope_exit([&transport]() { RestoreDefaultSettings(transport); });

    auto const hostEntry = MakeEntryIdentifier();
    auto const hostPort = transport.CreateHostAndWait(hostEntry, L"nm2-refusal-other-role", false);
    VERIFY_ARE_NOT_EQUAL(static_cast<uint16_t>(0), hostPort, L"The host started");

    std::string const name{ "Refusal Device Both Ways" };
    std::string const productInstanceId{ "refusal-device-both-ways" };

    RawUdpPeer device;
    VERIFY_IS_TRUE(device.Open());

    VERIFY_IS_TRUE(device.SendTo(hostPort, Invitation(name, productInstanceId)));
    VERIFY_IS_TRUE(device.WaitFor(CommandCode::InvitationReplyAccepted, hostPort, ReplyTimeout).has_value(), L"The device is a client of the host here");

    auto const clientEntry = MakeEntryIdentifier();
    VERIFY_IS_TRUE(transport.ConnectDirect(clientEntry, L"127.0.0.1", device.Port()));

    auto const invitation = WaitForInvitation(device, { hostPort }, InvitationTimeout);
    VERIFY_IS_TRUE(invitation.has_value(), L"The client entry invited the device");

    if (!invitation.has_value()) return;

    VERIFY_IS_TRUE(device.SendTo(invitation->FromPort, Accepted(name, productInstanceId)));
    VERIFY_IS_TRUE(device.WaitFor(CommandCode::Bye, invitation->FromPort, ReplyTimeout).has_value(), L"The client declined the session it was given");

    auto const waiting = WaitForClientEntry(transport, clientEntry, [&hostEntry](auto const& entry) { return IsWaitingFor(entry, hostEntry, true); });
    VERIFY_IS_TRUE(waiting.has_value(), L"The client entry says it waits for the device's session with the host here");

    auto const quiet = device.ReceiveFor(QuietPeriod);
    VERIFY_ARE_EQUAL(static_cast<size_t>(0), CountInvitations(quiet, hostPort), L"The client entry stopped inviting while that session is up");

    // The device leaves the host here
    auto const ended = std::chrono::steady_clock::now();

    VERIFY_IS_TRUE(device.SendTo(hostPort, Bye()));

    auto const again = WaitForInvitation(device, { hostPort }, std::chrono::seconds(5));

    Log::Comment(String().Format(L"Invited again %lld ms after the other session ended",
        static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - ended).count())));

    VERIFY_IS_TRUE(again.has_value(), L"The client entry invited the device again once the other session ended");

    if (again.has_value())
    {
        VERIFY_IS_TRUE(device.SendTo(again->FromPort, Accepted(name, productInstanceId)));

        VERIFY_IS_TRUE(WaitForClientEntry(transport, clientEntry, IsConnected).has_value(), L"The client entry is connected, and waits for nothing");
    }

    transport.DisconnectClient(clientEntry);
    transport.RemoveHost(hostEntry);
}


void NetworkMidiSessionRefusalTests::ClientWaitsWhileAnotherClientEntryIsConnectedToTheRemote()
{
    InProcessTransport transport;
    VERIFY_IS_TRUE(StartTransport(transport), L"The transport started in the test process");
    auto restoreSettings = wil::scope_exit([&transport]() { RestoreDefaultSettings(transport); });

    std::string const name{ "Refusal Device Two Entries" };
    std::string const productInstanceId{ "refusal-device-two-entries" };

    RawUdpPeer device;
    VERIFY_IS_TRUE(device.Open());

    auto const firstEntry = MakeEntryIdentifier();
    auto const secondEntry = MakeEntryIdentifier();

    VERIFY_IS_TRUE(transport.ConnectDirect(firstEntry, L"127.0.0.1", device.Port()));
    VERIFY_IS_TRUE(transport.ConnectDirect(secondEntry, L"127.0.0.1", device.Port()));

    auto const invitations = WaitForInvitations(device, 2, InvitationTimeout);
    VERIFY_ARE_EQUAL(static_cast<size_t>(2), invitations.size(), L"Both entries invited the device");

    if (invitations.size() < 2) return;

    auto const& first = invitations[0];
    auto const& second = invitations[1];

    VERIFY_IS_TRUE(device.SendTo(first.FromPort, Accepted(name, productInstanceId)));

    // Whichever entry that was is connected before the other is answered
    std::wstring connectedEntry{};
    std::wstring otherEntry{};

    auto const deadline = std::chrono::steady_clock::now() + StateTimeout;

    while (connectedEntry.empty() && std::chrono::steady_clock::now() < deadline)
    {
        for (auto const& candidate : { firstEntry, secondEntry })
        {
            if (auto entry = transport.FindClientEntry(candidate); entry.has_value() && entry->GetNamedBoolean(L"sessionActive", false))
            {
                connectedEntry = candidate;
                otherEntry = (candidate == firstEntry) ? secondEntry : firstEntry;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    VERIFY_IS_FALSE(connectedEntry.empty(), L"The first entry to be answered connected");

    if (connectedEntry.empty()) return;

    VERIFY_IS_TRUE(device.SendTo(second.FromPort, Accepted(name, productInstanceId)));
    VERIFY_IS_TRUE(device.WaitFor(CommandCode::Bye, second.FromPort, ReplyTimeout).has_value(), L"The other entry declined the second session");

    auto const waiting = WaitForClientEntry(transport, otherEntry, [&connectedEntry](auto const& entry) { return IsWaitingFor(entry, connectedEntry, false); });
    VERIFY_IS_TRUE(waiting.has_value(), L"The other entry says it waits for the connected entry");

    auto const quiet = device.ReceiveFor(QuietPeriod);
    VERIFY_ARE_EQUAL(static_cast<size_t>(0), CountInvitations(quiet, 0), L"The waiting entry stopped inviting");

    transport.DisconnectClient(connectedEntry);

    auto const again = WaitForInvitation(device, { first.FromPort, second.FromPort }, std::chrono::seconds(8));
    VERIFY_IS_TRUE(again.has_value(), L"The waiting entry invited the device once the other entry disconnected");

    if (again.has_value())
    {
        VERIFY_IS_TRUE(device.SendTo(again->FromPort, Accepted(name, productInstanceId)));

        VERIFY_IS_TRUE(WaitForClientEntry(transport, otherEntry, IsConnected).has_value(), L"The waiting entry is connected now");
    }

    transport.DisconnectClient(otherEntry);
}


void NetworkMidiSessionRefusalTests::HostListsTheRemotesItRefused()
{
    InProcessTransport transport;
    VERIFY_IS_TRUE(StartTransport(transport), L"The transport started in the test process");
    auto restoreSettings = wil::scope_exit([&transport]() { RestoreDefaultSettings(transport); });

    auto const hostEntry = MakeEntryIdentifier();
    auto const hostPort = transport.CreateHostAndWait(hostEntry, L"nm2-refusal-host-list", false);
    VERIFY_ARE_NOT_EQUAL(static_cast<uint16_t>(0), hostPort, L"The host started");

    VERIFY_ARE_EQUAL(0u, RecentRefusals(transport, hostEntry).Size(), L"A new host has refused nobody");

    // The same device twice, from two sockets
    std::string const twiceName{ "Refusal Device Twice" };
    std::string const twiceProductInstanceId{ "refusal-device-twice" };

    RawUdpPeer firstSocket;
    RawUdpPeer secondSocket;
    VERIFY_IS_TRUE(firstSocket.Open());
    VERIFY_IS_TRUE(secondSocket.Open());

    VERIFY_IS_TRUE(firstSocket.SendTo(hostPort, Invitation(twiceName, twiceProductInstanceId)));
    VERIFY_IS_TRUE(firstSocket.WaitFor(CommandCode::InvitationReplyAccepted, hostPort, ReplyTimeout).has_value(), L"The first session opened");

    VERIFY_IS_TRUE(secondSocket.SendTo(hostPort, Invitation(twiceName, twiceProductInstanceId)));
    VERIFY_IS_TRUE(secondSocket.WaitFor(CommandCode::Bye, hostPort, ReplyTimeout).has_value(), L"The second session was refused");

    auto refusals = WaitForRefusalCount(transport, hostEntry, 1);
    VERIFY_ARE_EQUAL(1u, refusals.Size());

    if (refusals.Size() >= 1)
    {
        auto const refusal = refusals.GetObjectAt(0);
        VERIFY_ARE_EQUAL(std::wstring{ L"Refusal Device Twice" }, std::wstring{ refusal.GetNamedString(L"umpEndpointName", L"") });
        VERIFY_ARE_EQUAL(std::wstring{ L"alreadyConnected" }, std::wstring{ refusal.GetNamedString(L"reason", L"") });
        VERIFY_IS_FALSE(refusal.GetNamedString(L"refusalTime", L"").empty(), L"The refusal says when");
    }

    // A device this PC is a client of
    std::string const hostName{ "Refusal Device Is A Host" };
    std::string const hostProductInstanceId{ "refusal-device-is-a-host" };

    RawUdpPeer remoteHost;
    VERIFY_IS_TRUE(remoteHost.Open());

    auto const clientEntry = MakeEntryIdentifier();
    VERIFY_IS_TRUE(transport.ConnectDirect(clientEntry, L"127.0.0.1", remoteHost.Port()));

    auto const invitation = WaitForInvitation(remoteHost, { hostPort }, InvitationTimeout);
    VERIFY_IS_TRUE(invitation.has_value(), L"The client entry invited the device");

    if (invitation.has_value())
    {
        VERIFY_IS_TRUE(remoteHost.SendTo(invitation->FromPort, Accepted(hostName, hostProductInstanceId)));
        VERIFY_IS_TRUE(WaitForClientEntry(transport, clientEntry, IsConnected).has_value(), L"This PC is a client of the device");

        VERIFY_IS_TRUE(remoteHost.SendTo(hostPort, Invitation(hostName, hostProductInstanceId)));
        VERIFY_IS_TRUE(remoteHost.WaitFor(CommandCode::Bye, hostPort, ReplyTimeout).has_value(), L"The host refused it");

        refusals = WaitForRefusalCount(transport, hostEntry, 2);
        VERIFY_ARE_EQUAL(2u, refusals.Size());

        if (refusals.Size() >= 2)
        {
            VERIFY_ARE_EQUAL(std::wstring{ L"Refusal Device Is A Host" }, std::wstring{ refusals.GetObjectAt(0).GetNamedString(L"umpEndpointName", L"") }, L"Newest first");
            VERIFY_ARE_EQUAL(std::wstring{ L"connectedInOtherRole" }, std::wstring{ refusals.GetObjectAt(0).GetNamedString(L"reason", L"") });
            VERIFY_ARE_EQUAL(std::wstring{ L"Refusal Device Twice" }, std::wstring{ refusals.GetObjectAt(1).GetNamedString(L"umpEndpointName", L"") });
        }
    }

    // The same device refused again is one entry, moved to the front. Past the reply limit for a
    // remote with no session, so the refusal goes out.
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    VERIFY_IS_TRUE(secondSocket.SendTo(hostPort, Invitation(twiceName, twiceProductInstanceId)));
    VERIFY_IS_TRUE(secondSocket.WaitFor(CommandCode::Bye, hostPort, ReplyTimeout).has_value(), L"The second session was refused again");

    auto const deadline = std::chrono::steady_clock::now() + StateTimeout;

    do
    {
        refusals = RecentRefusals(transport, hostEntry);

        if (refusals.Size() > 0 && refusals.GetObjectAt(0).GetNamedString(L"umpEndpointName", L"") == L"Refusal Device Twice")
        {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } while (std::chrono::steady_clock::now() < deadline);

    Log::Comment(String().Format(L"Recent refusals: %s", refusals.Stringify().c_str()));

    VERIFY_ARE_EQUAL(invitation.has_value() ? 2u : 1u, refusals.Size(), L"A repeat is not a second entry");
    VERIFY_ARE_EQUAL(std::wstring{ L"Refusal Device Twice" }, std::wstring{ refusals.GetObjectAt(0).GetNamedString(L"umpEndpointName", L"") }, L"The repeat moved to the front");

    // A device refused for the other reason is still one entry, with the latest reason. This PC
    // stops being its client, it connects to the host, and then tries a second session.
    if (invitation.has_value())
    {
        transport.DisconnectClient(clientEntry);
        VERIFY_IS_TRUE(remoteHost.WaitFor(CommandCode::Bye, invitation->FromPort, ReplyTimeout).has_value(), L"This PC ended its session with the device");

        RawUdpPeer thirdSocket;
        RawUdpPeer fourthSocket;
        VERIFY_IS_TRUE(thirdSocket.Open());
        VERIFY_IS_TRUE(fourthSocket.Open());

        VERIFY_IS_TRUE(thirdSocket.SendTo(hostPort, Invitation(hostName, hostProductInstanceId)));
        VERIFY_IS_TRUE(thirdSocket.WaitFor(CommandCode::InvitationReplyAccepted, hostPort, ReplyTimeout).has_value(), L"The device connected to the host");

        VERIFY_IS_TRUE(fourthSocket.SendTo(hostPort, Invitation(hostName, hostProductInstanceId)));
        VERIFY_IS_TRUE(fourthSocket.WaitFor(CommandCode::Bye, hostPort, ReplyTimeout).has_value(), L"Its second session was refused");

        auto const reasonDeadline = std::chrono::steady_clock::now() + StateTimeout;

        do
        {
            refusals = RecentRefusals(transport, hostEntry);

            if (refusals.Size() > 0 &&
                refusals.GetObjectAt(0).GetNamedString(L"umpEndpointName", L"") == L"Refusal Device Is A Host" &&
                refusals.GetObjectAt(0).GetNamedString(L"reason", L"") == L"alreadyConnected")
            {
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } while (std::chrono::steady_clock::now() < reasonDeadline);

        Log::Comment(String().Format(L"Recent refusals: %s", refusals.Stringify().c_str()));

        VERIFY_ARE_EQUAL(2u, refusals.Size(), L"Another reason is not another entry");

        if (refusals.Size() >= 1)
        {
            VERIFY_ARE_EQUAL(std::wstring{ L"Refusal Device Is A Host" }, std::wstring{ refusals.GetObjectAt(0).GetNamedString(L"umpEndpointName", L"") }, L"Moved to the front");
            VERIFY_ARE_EQUAL(std::wstring{ L"alreadyConnected" }, std::wstring{ refusals.GetObjectAt(0).GetNamedString(L"reason", L"") }, L"The latest reason");
        }

        VERIFY_IS_TRUE(thirdSocket.SendTo(hostPort, Bye()));
    }

    VERIFY_IS_TRUE(firstSocket.SendTo(hostPort, Bye()));
    transport.DisconnectClient(clientEntry);
    transport.RemoveHost(hostEntry);
}
