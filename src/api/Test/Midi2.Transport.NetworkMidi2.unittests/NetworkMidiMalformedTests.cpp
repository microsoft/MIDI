// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include <random>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace NetworkMidiTest;

// A missing host is a connection failure, not a skip. EnsureHostAvailable logs the error.
#define REQUIRE_HOST() \
    if (!ProtocolTestContext::Current().EnsureHostAvailable()) { return; }

// historical name, still used by the tests below
#define SKIP_IF_NO_HOST() REQUIRE_HOST()

namespace
{
    constexpr uint32_t SampleMidi1NoteOn{ 0x20903C40 };

    // The single most important assertion in this file. Whatever we just sent, the host has to
    // still be there. An invitation is used as the probe because the host drops non-invitation
    // commands from remotes it has no session with, so a bare Ping proves nothing.
    void VerifyServiceStillResponds(_In_ std::wstring const& afterWhat)
    {
        auto& context = ProtocolTestContext::Current();

        // Retry a couple of times: a single dropped datagram on a busy machine is not a crash.
        bool responded{ false };

        auto started = std::chrono::steady_clock::now();

        for (int attempt = 0; attempt < 3 && !responded; attempt++)
        {
            UdpTestClient client;
            VERIFY_IS_TRUE(client.Open(context.Host()));

            auto attemptStarted = std::chrono::steady_clock::now();

            // A distinct identity per attempt. Reusing one name means that once the first
            // attempt's endpoint is eventually created, every retry collides with it and is
            // refused, so the retries could never succeed.
            //
            // The timeout is long because these tests deliberately leave the service with a lot
            // of endpoints to create and tear down, and that work is serialized. The question
            // here is whether it recovers at all; how long it takes is reported separately.
            responded = EstablishSession(
                client,
                context.MakeUniqueEndpointName("Live" + std::to_string(attempt)),
                context.MakeUniqueProductInstanceId("L" + std::to_string(attempt)),
                std::chrono::milliseconds(45000));

            auto attemptMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - attemptStarted).count();

            // Which attempt succeeded matters more than the total. A first attempt that times
            // out and a second that returns immediately says the host ignored the first
            // invitation outright, which a single total would hide.
            Log::Comment(String().Format(
                L"Liveness attempt %d after %s: %s after %lldms",
                attempt + 1,
                afterWhat.c_str(),
                responded ? L"accepted" : L"no reply",
                static_cast<long long>(attemptMilliseconds)));

            if (responded)
            {
                EndSession(client);
            }
        }

        WarnIfSlowerThan(
            L"Recovery after " + afterWhat,
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started),
            std::chrono::milliseconds(10000));

        if (!responded)
        {
            Log::Error(String().Format(L"Service stopped responding after %s", afterWhat.c_str()));
        }

        VERIFY_IS_TRUE(responded, L"Service is still responding");
    }

    void SendRaw(_In_ std::vector<uint8_t> const& bytes)
    {
        auto& context = ProtocolTestContext::Current();

        UdpTestClient client;

        if (client.Open(context.Host()))
        {
            client.Send(bytes);

            // absorb whatever comes back, if anything
            client.ReceivePacket(std::chrono::milliseconds(500));
            client.Close();
        }
    }
}


void NetworkMidiMalformedTests::BadSignatureIsIgnored()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"5.3 - A receiver shall verify the first 32-bit word. If verification fails, the rest of the packet shall be ignored.");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;
    VERIFY_IS_TRUE(client.Open(context.Host()));

    // right shape, wrong signature, carrying a command which would otherwise be answered
    std::vector<uint8_t> bytes{ 0x4D, 0x49, 0x44, 0x00 };       // "MID\0"

    PacketBuilder inner;
    inner.AddPing(0x11223344);

    bytes.insert(bytes.end(), inner.Bytes().begin(), inner.Bytes().end());

    VERIFY_IS_TRUE(client.Send(bytes));

    auto reply = client.ReceivePacket(std::chrono::milliseconds(2000));

    if (reply.has_value())
    {
        LogPacket(L"Unexpected response to a bad signature:", reply.value());
    }

    VERIFY_IS_FALSE(reply.has_value(), L"Packet with an invalid signature produced no response");

    client.Close();

    VerifyServiceStillResponds(L"a packet with an invalid signature");
}

void NetworkMidiMalformedTests::EmptyAndRuntDatagramsAreIgnored()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"5.3 - Datagrams too short to contain a signature and a command header cannot be valid");

    // zero bytes, then every length shorter than a signature plus one command header
    SendRaw({ });
    SendRaw({ 0x4D });
    SendRaw({ 0x4D, 0x49 });
    SendRaw({ 0x4D, 0x49, 0x44 });
    SendRaw({ 0x4D, 0x49, 0x44, 0x49 });                            // signature only
    SendRaw({ 0x4D, 0x49, 0x44, 0x49, 0x20 });                      // signature plus one byte
    SendRaw({ 0x4D, 0x49, 0x44, 0x49, 0x20, 0x01 });                // signature plus two bytes
    SendRaw({ 0x4D, 0x49, 0x44, 0x49, 0x20, 0x01, 0x00 });          // signature plus three bytes

    VerifyServiceStillResponds(L"empty and runt datagrams");
}

void NetworkMidiMalformedTests::CommandHeaderSplitAcrossTheEndOfTheDatagram()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"Robustness - a partial command header at the end of a datagram must not be read past");

    auto& context = ProtocolTestContext::Current();

    PacketBuilder builder;
    builder.StartPacket().AddPing(0x01020304);

    // chop the datagram at every offset inside the trailing command
    auto full = builder.Bytes();

    for (size_t length = 5; length < full.size(); length++)
    {
        std::vector<uint8_t> truncated(full.begin(), full.begin() + length);

        SendRaw(truncated);
    }

    VerifyServiceStillResponds(L"command headers truncated at every offset");
}


void NetworkMidiMalformedTests::PayloadLengthLongerThanDatagramIsRejected()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"Robustness - a Command Payload Length larger than the datagram must not cause a read past the buffer");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;
    VERIFY_IS_TRUE(client.Open(context.Host()));

    // claims 255 words of payload, supplies none
    PacketBuilder builder;
    builder.StartPacket().AddCommandHeader(CommandCode::UmpData, 255, static_cast<uint16_t>(1));

    VERIFY_IS_TRUE(client.Send(builder));

    client.ReceivePacket(std::chrono::milliseconds(1000));
    client.Close();

    // the same lie on a Ping, which does have a handler that reads a payload word
    {
        PacketBuilder ping;
        ping.StartPacket().AddCommandHeader(CommandCode::Ping, 255, static_cast<uint16_t>(0));

        SendRaw(ping.Bytes());
    }

    // and on an invitation, where the handler reads two strings
    {
        PacketBuilder invitation;
        invitation.StartPacket().AddCommandHeader(CommandCode::Invitation, 255, 10, 0);

        SendRaw(invitation.Bytes());
    }

    VerifyServiceStillResponds(L"commands claiming a payload longer than the datagram");
}

void NetworkMidiMalformedTests::MaximumPayloadLengthWithNoPayload()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"Robustness - the payload length field is one byte, so 255 words is the maximum a peer can claim");

    for (uint8_t code : { static_cast<uint8_t>(CommandCode::UmpData),
                          static_cast<uint8_t>(CommandCode::Bye),
                          static_cast<uint8_t>(CommandCode::Nak),
                          static_cast<uint8_t>(CommandCode::RetransmitRequest),
                          static_cast<uint8_t>(CommandCode::RetransmitError),
                          static_cast<uint8_t>(CommandCode::SessionReset),
                          static_cast<uint8_t>(CommandCode::InvitationReplyAccepted) })
    {
        PacketBuilder builder;
        builder.StartPacket().AddCommandHeader(static_cast<CommandCode>(code), 255, static_cast<uint16_t>(0xFFFF));

        SendRaw(builder.Bytes());
    }

    VerifyServiceStillResponds(L"every command code claiming the maximum payload length with no payload");
}

void NetworkMidiMalformedTests::TrailingBytesAfterTheLastCommand()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"Robustness - trailing bytes which are not a whole command must be discarded, not parsed");

    PacketBuilder builder;
    builder.StartPacket().AddPing(0x0BADF00D);

    // one, two and three stray bytes past the end of a valid command
    for (size_t extra = 1; extra <= 3; extra++)
    {
        auto bytes = builder.Bytes();

        for (size_t i = 0; i < extra; i++)
        {
            bytes.push_back(0xFF);
        }

        SendRaw(bytes);
    }

    VerifyServiceStillResponds(L"trailing partial bytes after a valid command");
}


void NetworkMidiMalformedTests::InvitationNameLengthExceedsPayloadLength()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"6.4 - The endpoint name word count is a portion of the payload. A count larger than the payload would underflow the product instance id length.");

    // payload is 2 words, but the name claims 200 words. The subtraction which derives the
    // product instance id length underflows if this is not checked.
    PacketBuilder builder;
    builder.StartPacket()
        .AddCommandHeader(CommandCode::Invitation, 2, 200, 0)
        .AddUInt32(0x41414141)
        .AddUInt32(0x42424242);

    SendRaw(builder.Bytes());

    // and the boundary case, one word more than the payload holds
    PacketBuilder boundary;
    boundary.StartPacket()
        .AddCommandHeader(CommandCode::Invitation, 2, 3, 0)
        .AddUInt32(0x41414141)
        .AddUInt32(0x42424242);

    SendRaw(boundary.Bytes());

    // same shape on the reply command, which the client role parses
    PacketBuilder reply;
    reply.StartPacket()
        .AddCommandHeader(CommandCode::InvitationReplyAccepted, 2, 200, 0)
        .AddUInt32(0x41414141)
        .AddUInt32(0x42424242);

    SendRaw(reply.Bytes());

    VerifyServiceStillResponds(L"an invitation whose name length exceeds its payload");
}

void NetworkMidiMalformedTests::InvitationNameLengthIsMaximum()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"6.4 - Boundary: name length equal to the whole payload leaves a zero length product instance id");

    PacketBuilder builder;
    builder.StartPacket()
        .AddCommandHeader(CommandCode::Invitation, 2, 2, 0)
        .AddUInt32(0x54455354)
        .AddUInt32(0x54455354);

    SendRaw(builder.Bytes());

    // and zero name length, so the entire payload is the product instance id
    PacketBuilder zeroName;
    zeroName.StartPacket()
        .AddCommandHeader(CommandCode::Invitation, 2, 0, 0)
        .AddUInt32(0x54455354)
        .AddUInt32(0x54455354);

    SendRaw(zeroName.Bytes());

    VerifyServiceStillResponds(L"invitations at the name length boundaries");
}

void NetworkMidiMalformedTests::InvitationWithOversizedStrings()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"6.4 - UMP Endpoint Name is at most 98 bytes and Product Instance Id at most 42. Longer values must not overrun anything.");

    std::string longName(240, 'N');
    std::string longProductId(240, 'P');

    PacketBuilder builder;
    builder.StartPacket().AddInvitation(longName, longProductId);

    SendRaw(builder.Bytes());

    VerifyServiceStillResponds(L"an invitation with strings far over the specified maximum");
}

void NetworkMidiMalformedTests::InvitationWithInvalidUtf8()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"5.3 - The UMP Endpoint Name is UTF-8. Invalid sequences must be handled without throwing out of the parser.");

    auto& context = ProtocolTestContext::Current();

    // a lone continuation byte, a truncated multi-byte sequence, and an overlong encoding
    std::vector<std::vector<uint8_t>> badNames{
        { 0x80, 0x80, 0x80, 0x80 },
        { 0xE2, 0x82 },
        { 0xC0, 0xAF },
        { 0xF0, 0x9F, 0x98 },
        { 0xFF, 0xFE, 0xFD, 0xFC },
    };

    for (auto const& badName : badNames)
    {
        auto nameWords = PacketBuilder::PaddedWordCount(badName.size());

        PacketBuilder builder;
        builder.StartPacket()
            .AddCommandHeader(CommandCode::Invitation, static_cast<uint8_t>(nameWords + 1), static_cast<uint8_t>(nameWords), 0);

        auto padded = badName;
        padded.resize(static_cast<size_t>(nameWords) * sizeof(uint32_t), 0);

        builder.AddRawBytes(padded);
        builder.AddPaddedString(context.MakeUniqueProductInstanceId("BadUtf8"));

        SendRaw(builder.Bytes());
    }

    VerifyServiceStillResponds(L"invitations carrying invalid UTF-8");
}

void NetworkMidiMalformedTests::InvitationWithEmbeddedNulls()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"5.3 - Strings are zero padded to a word boundary, so trailing nulls are normal and embedded nulls must not corrupt parsing");

    PacketBuilder builder;
    builder.StartPacket()
        .AddCommandHeader(CommandCode::Invitation, 4, 2, 0)
        .AddRawBytes({ 'A', 0x00, 'B', 0x00, 'C', 0x00, 0x00, 0x00 })
        .AddRawBytes({ 'I', 'D', 0x00, 'X', 0x00, 0x00, 0x00, 0x00 });

    SendRaw(builder.Bytes());

    VerifyServiceStillResponds(L"an invitation with embedded nulls in both strings");
}


void NetworkMidiMalformedTests::UmpMessageClaimsMoreWordsThanSent()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"7.1 - The UMP message type nibble implies the message length. A type claiming more words than the command carries must not be read past.");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;

    if (!EstablishSession(
        client,
        context.MakeUniqueEndpointName("OverRead"),
        context.MakeUniqueProductInstanceId("OR")))
    {
        Log::Error(L"Could not establish a session for the over-read test.");

        return;
    }

    client.DrainPending();

    // Message type 5 is a 128-bit message, so four words. We send one word and claim a payload
    // of one word, which is internally consistent at the command level but not at the UMP level.
    // Reading the declared message length would run twelve bytes past the buffer.
    uint16_t sequence{ 1 };

    for (uint8_t messageType : { 0x5, 0xD, 0xE, 0xF, 0xB, 0xC, 0x3, 0x4 })
    {
        uint32_t firstWord = (static_cast<uint32_t>(messageType) << 28) | 0x0BADF00D;

        PacketBuilder builder;
        builder.StartPacket().AddUmpData(sequence++, { firstWord });

        VERIFY_IS_TRUE(client.Send(builder));

        Sleep(60);
    }

    // and the same with two words present where four are implied
    {
        uint32_t firstWord = (static_cast<uint32_t>(0x5) << 28) | 0x00000001;

        PacketBuilder builder;
        builder.StartPacket().AddUmpData(sequence++, { firstWord, 0xAAAAAAAA });

        VERIFY_IS_TRUE(client.Send(builder));
    }

    client.DrainPending();
    client.Close();

    VerifyServiceStillResponds(L"UMP messages claiming more words than were sent");
}

void NetworkMidiMalformedTests::UmpDataWithTruncatedFinalMessage()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"7.1 - A UMP Data Command carrying a partial final message must have the remainder discarded rather than delivered");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;

    if (!EstablishSession(
        client,
        context.MakeUniqueEndpointName("Truncated"),
        context.MakeUniqueProductInstanceId("TR")))
    {
        Log::Error(L"Could not establish a session for the truncated message test.");

        return;
    }

    client.DrainPending();

    // one complete 32-bit message followed by the first word only of a 128-bit message
    uint32_t completeMessage = SampleMidi1NoteOn;
    uint32_t partial128 = (static_cast<uint32_t>(0x5) << 28);

    PacketBuilder builder;
    builder.StartPacket().AddUmpData(1, { completeMessage, partial128 });

    VERIFY_IS_TRUE(client.Send(builder));

    Sleep(500);

    client.DrainPending();
    client.Close();

    VerifyServiceStillResponds(L"a UMP Data command ending in a partial message");
}


void NetworkMidiMalformedTests::ValidCommandAfterUnknownCommandIsStillProcessed()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"Robustness - an unhandled command's payload must be skipped so the following command is parsed from the right offset");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;

    // A session is needed first: the host drops a whole datagram whose first command from an
    // unknown remote is not an invitation, so the parsing behaviour would never be reached.
    VERIFY_IS_TRUE(EstablishSession(
        client,
        context.MakeUniqueEndpointName("AfterUnknown"),
        context.MakeUniqueProductInstanceId("AU")));

    client.DrainPending();

    // an unknown command with a real payload, then a Ping. If the payload is not skipped, the
    // Ping is never seen because parsing resumes in the middle of the unknown payload.
    PacketBuilder builder;
    builder.StartPacket()
        .AddCommandHeader(static_cast<CommandCode>(0x7D), 3, static_cast<uint16_t>(0))
        .AddUInt32(0xDEADBEEF)
        .AddUInt32(0xFEEDFACE)
        .AddUInt32(0x12345678)
        .AddPing(0xC0FFEE00);

    VERIFY_IS_TRUE(client.Send(builder));

    auto reply = client.WaitForCommand(CommandCode::PingReply, std::chrono::milliseconds(5000));

    if (!reply.has_value())
    {
        LogNoPacket(L"PingReply following an unknown command:");
    }
    else
    {
        LogPacket(L"Response to unknown command followed by Ping:", reply.value());

        auto pingReply = reply->Find(CommandCode::PingReply);
        VERIFY_IS_NOT_NULL(pingReply);
        VERIFY_ARE_EQUAL(pingReply->GetPayloadUInt32(0), static_cast<uint32_t>(0xC0FFEE00));
    }

    VERIFY_IS_TRUE(reply.has_value(), L"The Ping after an unknown command was found and answered");

    EndSession(client);
    client.Close();
}

void NetworkMidiMalformedTests::ByeFollowedByMoreCommandsDoesNotMisparse()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"Robustness - a Bye carries reason text, whose words must be consumed before the next command is read");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;

    if (!EstablishSession(
        client,
        context.MakeUniqueEndpointName("ByeCompound"),
        context.MakeUniqueProductInstanceId("BC")))
    {
        Log::Error(L"Could not establish a session for the compound Bye test.");

        return;
    }

    client.DrainPending();

    // Bye with a long reason string, then a Ping in the same datagram. Misparsing the Bye text
    // would make the parser read the text as command headers.
    PacketBuilder builder;
    builder.StartPacket()
        .AddBye(ByeReason::UserTerminated, "This reason text is deliberately long so it occupies several words of payload.")
        .AddPing(0xBEEFCAFE);

    VERIFY_IS_TRUE(client.Send(builder));

    Sleep(500);

    client.DrainPending();
    client.Close();

    VerifyServiceStillResponds(L"a Bye with long reason text followed by another command");
}


void NetworkMidiMalformedTests::OversizedDatagramIsHandled()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"5.2 - UDP packets should not exceed 1400 bytes. A larger one must be handled or dropped, never overrun a buffer.");

    // well past the recommended maximum, full of valid Ping commands
    PacketBuilder builder;
    builder.StartPacket();

    while (builder.Size() < 3000)
    {
        builder.AddPing(static_cast<uint32_t>(builder.Size()));
    }

    Log::Comment(String().Format(L"Sending an oversized datagram of %zu bytes", builder.Size()));

    SendRaw(builder.Bytes());

    // and one which is mostly a single enormous declared payload
    PacketBuilder single;
    single.StartPacket().AddCommandHeader(CommandCode::UmpData, 255, static_cast<uint16_t>(1));

    for (int i = 0; i < 255; i++)
    {
        single.AddUInt32(0x20903C40);
    }

    SendRaw(single.Bytes());

    VerifyServiceStillResponds(L"oversized datagrams");
}

void NetworkMidiMalformedTests::RapidInvitationsFromManyPortsAreBounded()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"Robustness - a host must bound the resources it allocates for unestablished sessions, since a source address is trivially forged");

    auto& context = ProtocolTestContext::Current();

    // Each client is a distinct source port, which to the host looks like a distinct remote.
    // The host should not allocate without limit.
    constexpr int clientCount{ 40 };

    std::vector<std::unique_ptr<UdpTestClient>> clients;

    for (int i = 0; i < clientCount; i++)
    {
        auto client = std::make_unique<UdpTestClient>();

        if (!client->Open(context.Host()))
        {
            continue;
        }

        PacketBuilder builder;
        builder.StartPacket().AddInvitation(
            context.MakeUniqueEndpointName("Flood" + std::to_string(i)),
            context.MakeUniqueProductInstanceId("F" + std::to_string(i)));

        client->Send(builder);

        clients.push_back(std::move(client));
    }

    Log::Comment(String().Format(L"Sent invitations from %zu distinct source ports", clients.size()));

    // Let the host work through them, then tear every one down so the run leaves nothing behind.
    Sleep(2000);

    for (auto& client : clients)
    {
        PacketBuilder bye;
        bye.StartPacket().AddBye(ByeReason::UserTerminated, "Flood test cleanup.");

        client->Send(bye);
    }

    Sleep(1000);

    clients.clear();

    VerifyServiceStillResponds(L"a burst of invitations from many source ports");
}


void NetworkMidiMalformedTests::ServiceSurvivesRandomFuzzing()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"Robustness - arbitrary bytes behind a valid signature must never crash or wedge the service");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;
    VERIFY_IS_TRUE(client.Open(context.Host()));

    // Deterministic seed so a failure can be reproduced exactly.
    constexpr unsigned int seed{ 0x4D494449 };
    std::mt19937 rng{ seed };
    std::uniform_int_distribution<int> byteDistribution{ 0, 255 };
    std::uniform_int_distribution<int> lengthDistribution{ 4, 600 };

    Log::Comment(String().Format(L"Fuzzing with seed 0x%08X", seed));

    constexpr int iterations{ 300 };

    for (int i = 0; i < iterations; i++)
    {
        std::vector<uint8_t> bytes;

        // Half carry a valid signature so they reach the command parser, half do not so the
        // signature check itself is exercised.
        if ((i % 2) == 0)
        {
            bytes = { 0x4D, 0x49, 0x44, 0x49 };
        }

        auto length = lengthDistribution(rng);

        for (int b = 0; b < length; b++)
        {
            bytes.push_back(static_cast<uint8_t>(byteDistribution(rng)));
        }

        client.Send(bytes);

        // drain anything the host sends back so the receive buffer does not fill
        client.Receive(std::chrono::milliseconds(1));
    }

    client.Close();

    VerifyServiceStillResponds(L"fuzzed datagrams");
}
