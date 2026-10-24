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
    constexpr auto ClientInvitationTimeout{ std::chrono::seconds(15) };
    constexpr auto ReplyTimeout{ std::chrono::seconds(5) };

    // A remote with no session gets at most one reply a second from a connection
    constexpr auto PastTheReplyRateLimit{ std::chrono::milliseconds(1500) };

    size_t CountCommands(_In_ std::vector<RawUdpPeer::Received> const& received, _In_ uint16_t const fromPort)
    {
        size_t count{ 0 };

        for (auto const& entry : received)
        {
            if (entry.FromPort == fromPort)
            {
                count += entry.Packet.Commands.size();
            }
        }

        return count;
    }
}


void NetworkMidiInProcessTests::HostAndClientForOneRemoteAddressAndPortAreSeparateConnections()
{
    InProcessTransport transport;
    VERIFY_IS_TRUE(transport.Start(), L"The transport started in the test process");

    auto const hostEntry = MakeEntryIdentifier();
    auto const hostPort = transport.CreateHostAndWait(hostEntry, L"nm2-inprocess-both-roles", true);

    VERIFY_ARE_NOT_EQUAL(static_cast<uint16_t>(0), hostPort, L"The host started");

    // Both orders: the remote invites the host here before the client here invites it, and after
    for (bool const hostConnectionFirst : { true, false })
    {
        Log::Comment(hostConnectionFirst ?
            L"The remote invites the host here first, then the client here invites the remote" :
            L"The client here invites the remote first, then the remote invites the host here");

        RawUdpPeer remote;
        VERIFY_IS_TRUE(remote.Open());

        auto const clientEntry = MakeEntryIdentifier();
        std::string const remoteName = hostConnectionFirst ? "Both Roles Remote A" : "Both Roles Remote B";
        std::string const remoteProductInstanceId = hostConnectionFirst ? "both-roles-a" : "both-roles-b";

        PacketBuilder invitation;
        invitation.StartPacket().AddInvitation(remoteName, remoteProductInstanceId);

        uint16_t clientPort{ 0 };

        auto startClient = [&]()
            {
                VERIFY_IS_TRUE(transport.ConnectDirect(clientEntry, L"127.0.0.1", remote.Port()), L"The client entry was accepted");

                auto clientInvitation = remote.WaitFor(CommandCode::Invitation, 0, ClientInvitationTimeout);

                VERIFY_IS_TRUE(clientInvitation.has_value(), L"The client here invited the remote");

                if (clientInvitation.has_value())
                {
                    clientPort = clientInvitation->FromPort;
                }
            };

        if (!hostConnectionFirst)
        {
            startClient();
        }

        VERIFY_IS_TRUE(remote.SendTo(hostPort, invitation.Bytes()));

        auto firstReply = remote.WaitFor(CommandCode::InvitationReplyPending, hostPort, ReplyTimeout);

        VERIFY_IS_TRUE(firstReply.has_value(), L"The host here answered the remote's invitation from the host's port");

        if (hostConnectionFirst)
        {
            startClient();
        }

        VERIFY_ARE_NOT_EQUAL(hostPort, clientPort, L"The client has a socket of its own");

        // Now that both connections exist. A host connection which was replaced, or a datagram
        // routed to the client's connection instead, is answered from the client's port with a
        // NAK, or not at all.
        std::this_thread::sleep_for(PastTheReplyRateLimit);

        VERIFY_IS_TRUE(remote.SendTo(hostPort, invitation.Bytes()));

        auto secondReply = remote.WaitFor(CommandCode::InvitationReplyPending, hostPort, ReplyTimeout);

        VERIFY_IS_TRUE(secondReply.has_value(), L"The host connection is still there, and still gets the remote's datagrams");

        transport.DisconnectClient(clientEntry);
    }

    transport.RemoveHost(hostEntry);
    transport.Shutdown();
}


void NetworkMidiInProcessTests::DatagramOfEmptyInvitationsGetsAtMostOneReply()
{
    InProcessTransport transport;
    VERIFY_IS_TRUE(transport.Start(), L"The transport started in the test process");

    auto const hostEntry = MakeEntryIdentifier();
    auto const hostPort = transport.CreateHostAndWait(hostEntry, L"nm2-inprocess-empty-invitations", true);

    VERIFY_ARE_NOT_EQUAL(static_cast<uint16_t>(0), hostPort, L"The host started");

    RawUdpPeer remote;
    VERIFY_IS_TRUE(remote.Open());

    // An invitation with no name and no product instance id is refused with a Bye. Each of these
    // used to get one.
    constexpr size_t InvitationCount{ 32 };

    PacketBuilder flood;
    flood.StartPacket();

    for (size_t i = 0; i < InvitationCount; i++)
    {
        flood.AddCommandHeader(CommandCode::Invitation, 0, 0, 0);
    }

    VERIFY_IS_TRUE(flood.Size() <= MaxRecommendedUdpPayloadBytes, L"The datagram is within the spec's size");

    // twice, well inside the rate limiter's one second
    VERIFY_IS_TRUE(remote.SendTo(hostPort, flood.Bytes()));
    VERIFY_IS_TRUE(remote.SendTo(hostPort, flood.Bytes()));

    auto const received = remote.ReceiveFor(std::chrono::milliseconds(2000));
    auto const replies = CountCommands(received, hostPort);

    Log::Comment(String().Format(L"%zu invitations in each of 2 datagrams got %zu replies in %zu datagrams",
        InvitationCount, replies, received.size()));

    for (auto const& entry : received)
    {
        Log::Comment(String().Format(L"from %u: %s", entry.FromPort, DescribePacket(entry.Packet).c_str()));
    }

    VERIFY_IS_TRUE(replies <= 1, L"A remote with no session gets at most one reply, however many commands it sends");

    // The host still answers a real invitation from someone else
    RawUdpPeer other;
    VERIFY_IS_TRUE(other.Open());

    PacketBuilder invitation;
    invitation.StartPacket().AddInvitation("Empty Invitations Bystander", "empty-invitations-bystander");

    VERIFY_IS_TRUE(other.SendTo(hostPort, invitation.Bytes()));
    VERIFY_IS_TRUE(other.WaitFor(CommandCode::InvitationReplyPending, hostPort, ReplyTimeout).has_value(), L"The host still answers other remotes");

    transport.RemoveHost(hostEntry);
    transport.Shutdown();
}


void NetworkMidiInProcessTests::OversizedDatagramGetsNoReply()
{
    InProcessTransport transport;
    VERIFY_IS_TRUE(transport.Start(), L"The transport started in the test process");

    auto const hostEntry = MakeEntryIdentifier();
    auto const hostPort = transport.CreateHostAndWait(hostEntry, L"nm2-inprocess-oversized", true);

    VERIFY_ARE_NOT_EQUAL(static_cast<uint16_t>(0), hostPort, L"The host started");

    RawUdpPeer remote;
    VERIFY_IS_TRUE(remote.Open());

    // A real invitation, then pings until the datagram is past 1400 bytes
    PacketBuilder oversized;
    oversized.StartPacket().AddInvitation("Oversized Remote", "oversized-remote");

    while (oversized.Size() <= MaxRecommendedUdpPayloadBytes)
    {
        oversized.AddPing(static_cast<uint32_t>(oversized.Size()));
    }

    Log::Comment(String().Format(L"Sending %zu bytes", oversized.Size()));

    VERIFY_IS_TRUE(remote.SendTo(hostPort, oversized.Bytes()));

    auto const received = remote.ReceiveFor(std::chrono::milliseconds(1500));

    VERIFY_ARE_EQUAL(static_cast<size_t>(0), CountCommands(received, hostPort), L"An oversized datagram gets no reply");

    // The same invitation on its own is answered, so the silence was about the size
    PacketBuilder invitation;
    invitation.StartPacket().AddInvitation("Oversized Remote", "oversized-remote");

    VERIFY_IS_TRUE(remote.SendTo(hostPort, invitation.Bytes()));
    VERIFY_IS_TRUE(remote.WaitFor(CommandCode::InvitationReplyPending, hostPort, ReplyTimeout).has_value(), L"The same invitation on its own is answered");

    transport.RemoveHost(hostEntry);
    transport.Shutdown();
}
