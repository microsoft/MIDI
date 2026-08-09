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

#define SKIP_IF_NO_HOST() \
    if (!ProtocolTestContext::Current().EnsureHostAvailable()) { return; }


void NetworkMidiSessionTests::HostIsDiscoverable()
{
    SKIP_IF_NO_HOST();

    auto const& host = ProtocolTestContext::Current().Host();

    VERIFY_IS_FALSE(host.HostNameOrAddress.empty());
    VERIFY_IS_GREATER_THAN(host.Port, static_cast<uint16_t>(0));

    // A host which cannot be reached at all makes every later failure ambiguous, so prove
    // reachability here before anything else runs. An invitation is used rather than a Ping
    // because the host deliberately drops non-invitation commands from unknown remotes.
    UdpTestClient client;
    VERIFY_IS_TRUE(client.Open(host));

    bool established = EstablishSession(
        client,
        ProtocolTestContext::Current().MakeUniqueEndpointName("Discover"),
        ProtocolTestContext::Current().MakeUniqueProductInstanceId("D"));

    VERIFY_IS_TRUE(established, L"Host accepted an invitation, so it is reachable");

    EndSession(client);
}

void NetworkMidiSessionTests::HostAdvertisesRequiredTxtRecords()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"4.4 - Every Device shall include a TXT record providing UMPEndpointName and ProductInstanceId");

    auto const& host = ProtocolTestContext::Current().Host();

    if (host.DiscoveredVia.find(L"mDNS") == std::wstring::npos)
    {
        Log::Result(TestResults::Skipped, L"Host was supplied by environment override, so there are no TXT records to check.");

        return;
    }

    VERIFY_IS_FALSE(host.AdvertisedEndpointName.empty(), L"UMPEndpointName TXT record is present");
    VERIFY_IS_FALSE(host.AdvertisedProductInstanceId.empty(), L"ProductInstanceId TXT record is present");

    // Spec 4.4: ASCII ordinals 32-126 only, up to 42 bytes
    VERIFY_IS_LESS_THAN_OR_EQUAL(host.AdvertisedProductInstanceId.length(), MaxProductInstanceIdBytes);

    for (auto const& ch : host.AdvertisedProductInstanceId)
    {
        VERIFY_IS_TRUE(ch >= 32 && ch <= 126, L"Product Instance Id character is in ASCII range 32-126");
    }
}


void NetworkMidiSessionTests::InvitationIsAccepted()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"6.4 - If a Host receives an Invitation Command, the Host shall respond with Invitation Reply: Accepted, a Bye, or an authentication reply");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;

    auto established = EstablishSession(
        client,
        context.MakeUniqueEndpointName("Accept"),
        context.MakeUniqueProductInstanceId("A"));

    VERIFY_IS_TRUE(established);

    EndSession(client);
}

void NetworkMidiSessionTests::InvitationReplyCarriesNameAndProductInstanceId()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"6.5 - The Host shall provide a name and a Product Instance Id to the Client in Invitation Reply: Accepted");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;
    VERIFY_IS_TRUE(client.Open(context.Host()));

    PacketBuilder builder;
    builder.StartPacket().AddInvitation(
        context.MakeUniqueEndpointName("Names"),
        context.MakeUniqueProductInstanceId("N"));

    VERIFY_IS_TRUE(client.Send(builder));

    auto reply = client.WaitForCommand(CommandCode::InvitationReplyAccepted, std::chrono::milliseconds(5000));

    if (!reply.has_value())
    {
        LogNoPacket(L"InvitationReplyAccepted:");
    }

    VERIFY_IS_TRUE(reply.has_value());

    auto accepted = reply->Find(CommandCode::InvitationReplyAccepted);
    VERIFY_IS_NOT_NULL(accepted);

    // csd1 is the endpoint name length in words, the remainder of the payload is the product id
    size_t nameBytes = static_cast<size_t>(accepted->CommandSpecificData1) * sizeof(uint32_t);
    size_t payloadBytes = static_cast<size_t>(accepted->PayloadLengthWords) * sizeof(uint32_t);

    VERIFY_IS_LESS_THAN_OR_EQUAL(nameBytes, payloadBytes, L"Endpoint name length fits inside the payload");

    auto endpointName = accepted->GetPayloadString(0, nameBytes);
    auto productInstanceId = accepted->GetPayloadString(nameBytes, payloadBytes - nameBytes);

    Log::Comment(String().Format(
        L"Host identified itself as name='%S' productInstanceId='%S'",
        endpointName.c_str(),
        productInstanceId.c_str()));

    VERIFY_IS_FALSE(endpointName.empty(), L"Host supplied a UMP Endpoint Name");
    VERIFY_IS_FALSE(productInstanceId.empty(), L"Host supplied a Product Instance Id");

    VERIFY_IS_LESS_THAN_OR_EQUAL(endpointName.length(), MaxEndpointNameBytes);
    VERIFY_IS_LESS_THAN_OR_EQUAL(productInstanceId.length(), MaxProductInstanceIdBytes);

    EndSession(client);
}

void NetworkMidiSessionTests::RepeatInvitationOnEstablishedSessionIsAccepted()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"6.4 - If a Host receives an Invitation from a remote Client with which it is already in a Session, then it shall respond with Invitation Reply: Accepted");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;

    VERIFY_IS_TRUE(EstablishSession(
        client,
        context.MakeUniqueEndpointName("Repeat"),
        context.MakeUniqueProductInstanceId("R")));

    client.DrainPending();

    // same source port, same identity, so this is the already-in-session case
    PacketBuilder builder;
    builder.StartPacket().AddInvitation(
        context.MakeUniqueEndpointName("Repeat"),
        context.MakeUniqueProductInstanceId("R"));

    VERIFY_IS_TRUE(client.Send(builder));

    auto reply = client.WaitForCommand(CommandCode::InvitationReplyAccepted, std::chrono::milliseconds(5000));

    if (!reply.has_value())
    {
        LogNoPacket(L"Second InvitationReplyAccepted:");
    }

    VERIFY_IS_TRUE(reply.has_value(), L"Host re-accepted rather than ignoring or refusing");

    EndSession(client);
}

void NetworkMidiSessionTests::InvitationWithoutProductInstanceIdIsRefused()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"6.4 - The Client shall provide a Product Instance Id to the Host. Windows refuses an invitation which omits it rather than inventing an identity.");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;
    VERIFY_IS_TRUE(client.Open(context.Host()));

    // name present, product instance id absent
    PacketBuilder builder;
    builder.StartPacket().AddInvitation(context.MakeUniqueEndpointName("NoProduct"), "");

    VERIFY_IS_TRUE(client.Send(builder));

    auto reply = client.ReceivePacket(std::chrono::milliseconds(4000));

    if (reply.has_value())
    {
        LogPacket(L"Response to invitation with no Product Instance Id:", reply.value());

        VERIFY_IS_FALSE(
            reply->Contains(CommandCode::InvitationReplyAccepted),
            L"Host did not accept an invitation lacking a Product Instance Id");
    }
    else
    {
        LogNoPacket(L"Response to invitation with no Product Instance Id:");
    }

    client.Close();
}


void NetworkMidiSessionTests::PingIsAnswered()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"6.11 - A Device receiving a Ping Command shall reply with a Ping Reply Command");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;
    VERIFY_IS_TRUE(client.Open(context.Host()));

    // The host drops non-invitation commands from a remote it has no session with.
    VERIFY_IS_TRUE(EstablishSession(
        client,
        context.MakeUniqueEndpointName("Ping"),
        context.MakeUniqueProductInstanceId("P")));

    PacketBuilder builder;
    builder.StartPacket().AddPing(0x12345678);

    VERIFY_IS_TRUE(client.Send(builder));

    auto reply = client.WaitForCommand(CommandCode::PingReply, std::chrono::milliseconds(5000));

    if (!reply.has_value())
    {
        LogNoPacket(L"PingReply:");
    }

    VERIFY_IS_TRUE(reply.has_value());

    EndSession(client);
    client.Close();
}

void NetworkMidiSessionTests::PingReplyEchoesTheSamePingId()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"6.11 - The Ping Reply carries back the same Ping Id supplied in the Ping");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;
    VERIFY_IS_TRUE(client.Open(context.Host()));

    // The host drops non-invitation commands from a remote it has no session with.
    VERIFY_IS_TRUE(EstablishSession(
        client,
        context.MakeUniqueEndpointName("PingId"),
        context.MakeUniqueProductInstanceId("PI")));

    const uint32_t pingId{ 0xA5A5F00D };

    PacketBuilder builder;
    builder.StartPacket().AddPing(pingId);

    VERIFY_IS_TRUE(client.Send(builder));

    auto reply = client.WaitForCommand(CommandCode::PingReply, std::chrono::milliseconds(5000));

    VERIFY_IS_TRUE(reply.has_value());

    auto pingReply = reply->Find(CommandCode::PingReply);
    VERIFY_IS_NOT_NULL(pingReply);

    VERIFY_ARE_EQUAL(pingReply->PayloadLengthWords, static_cast<uint8_t>(1), L"Ping Reply payload is one word");
    VERIFY_ARE_EQUAL(pingReply->GetPayloadUInt32(0), pingId, L"Ping Reply echoed the Ping Id");

    EndSession(client);
    client.Close();
}

void NetworkMidiSessionTests::HostSendsPingsToKeepSessionAlive()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"6.11 - A Device may send Ping Commands to verify the remote is still responsive");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;

    VERIFY_IS_TRUE(EstablishSession(
        client,
        context.MakeUniqueEndpointName("Keepalive"),
        context.MakeUniqueProductInstanceId("K")));

    client.DrainPending();

    // The host pings an idle session. Answer them so the session is not torn down, and confirm
    // at least one arrives.
    bool sawPing{ false };

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(8000);

    while (std::chrono::steady_clock::now() < deadline && !sawPing)
    {
        auto packet = client.ReceivePacket(std::chrono::milliseconds(1000));

        if (!packet.has_value())
        {
            continue;
        }

        auto ping = packet->Find(CommandCode::Ping);

        if (ping != nullptr)
        {
            sawPing = true;

            PacketBuilder reply;
            reply.StartPacket().AddPingReply(ping->GetPayloadUInt32(0));

            client.Send(reply);
        }
    }

    VERIFY_IS_TRUE(sawPing, L"Host pinged the idle session");

    EndSession(client);
}


void NetworkMidiSessionTests::ByeIsAnsweredWithByeReply()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"6.16 - A Device receiving a Bye Command shall reply with a Bye Reply Command");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;

    VERIFY_IS_TRUE(EstablishSession(
        client,
        context.MakeUniqueEndpointName("Bye"),
        context.MakeUniqueProductInstanceId("B")));

    client.DrainPending();

    PacketBuilder builder;
    builder.StartPacket().AddBye(ByeReason::UserTerminated, "Goodbye.");

    VERIFY_IS_TRUE(client.Send(builder));

    auto reply = client.WaitForCommand(CommandCode::ByeReply, std::chrono::milliseconds(5000));

    if (!reply.has_value())
    {
        LogNoPacket(L"ByeReply:");
    }

    VERIFY_IS_TRUE(reply.has_value());

    client.Close();
}

void NetworkMidiSessionTests::SessionCanBeReestablishedAfterBye()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"Reconnect - a client which said Bye can invite again and be accepted (regression cover for issue #747)");

    auto& context = ProtocolTestContext::Current();

    auto endpointName = context.MakeUniqueEndpointName("Reconnect");
    auto productInstanceId = context.MakeUniqueProductInstanceId("C");

    {
        UdpTestClient first;

        VERIFY_IS_TRUE(EstablishSession(first, endpointName, productInstanceId), L"First session established");

        EndSession(first);
    }

    // Give the host a moment to finish tearing the first session down
    Sleep(1000);

    {
        // A new socket means a new source port, which is what a real client does. Same identity,
        // so this also proves the endpoint identity survives a reconnect.
        UdpTestClient second;

        VERIFY_IS_TRUE(
            EstablishSession(second, endpointName, productInstanceId),
            L"Second session established with the same identity from a new source port");

        EndSession(second);
    }
}
