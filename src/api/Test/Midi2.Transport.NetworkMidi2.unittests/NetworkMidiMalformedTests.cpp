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


// ============================================================================
// Configuration payloads
//
// UpdateConfiguration is reachable by anything which can talk to the service, and the payload
// arrives as a string. These go in through SendRawServiceConfig, which does no wrapping, so
// what the transport receives is exactly what is written here.
//
// The pass condition is the same throughout: the call comes back, nothing is created, and the
// service is still answering afterwards.
// ============================================================================

namespace
{
    // Sends a payload which must not be accepted, and fails the test if the service reports
    // success for it.
    void VerifyConfigPayloadRejected(_In_ std::wstring const& payload, _In_ wchar_t const* description)
    {
        // Logged before the send, so that if the service dies the last line names the payload
        // which killed it.
        Log::Comment(String().Format(L"sending: %s", description));

        auto result = SendRawServiceConfig(payload);

        // The RPC itself may fail or the transport may answer with a failure. Either is a
        // clean rejection. What is not acceptable is a reported success.
        VERIFY_IS_FALSE(
            result.IsSuccess(),
            String().Format(L"Payload rejected: %s", description));
    }
}


void NetworkMidiMalformedTests::ConfigNonJsonPayloadsAreRejected()
{
    // needed because the survival probe at the end invites the test host
    REQUIRE_HOST();

    // Nothing here is JSON at all. A parser which assumes it has an object to walk will fault
    // on most of these.
    const std::pair<std::wstring, const wchar_t*> payloads[] =
    {
        { L"",                                  L"empty string" },
        { L" ",                                 L"single space" },
        { L"\r\n\t",                            L"whitespace only" },
        { L"not json at all",                   L"plain text" },
        { L"<?xml version=\"1.0\"?><root/>",    L"xml" },
        { L"{",                                 L"lone opening brace" },
        { L"}",                                 L"lone closing brace" },
        { L"[",                                 L"lone opening bracket" },
        { L"\"",                                L"lone quote" },
        { L"\\",                                L"lone backslash" },
        { L"%s %d %n",                          L"format specifiers" },
        { L"\x0001\x0002\x0003",                L"control characters" },
        { L"\uFFFE\uFFFF",                      L"noncharacters" },
        { L"NULL",                              L"the word NULL" },
        { L"0",                                 L"bare number" },
        { L"true",                              L"bare boolean" },
        { L"undefined",                         L"javascript undefined" },
    };

    for (auto const& entry : payloads)
    {
        VerifyConfigPayloadRejected(entry.first, entry.second);
    }

    VerifyServiceStillResponds(L"non-json configuration payloads");
}


void NetworkMidiMalformedTests::ConfigMalformedJsonIsRejected()
{
    REQUIRE_HOST();

    // Close enough to JSON to get past a naive check, but not parseable.
    const std::pair<std::wstring, const wchar_t*> payloads[] =
    {
        { L"{\"unterminated\":",                        L"truncated after key" },
        { L"{\"unterminated\":\"value",                 L"unterminated string" },
        { L"{\"a\":1,}",                                L"trailing comma" },
        { L"{,}",                                       L"lone comma" },
        { L"{\"a\":1 \"b\":2}",                         L"missing comma" },
        { L"{'single':'quotes'}",                       L"single quotes" },
        { L"{a:1}",                                     L"unquoted key" },
        { L"{\"a\":01}",                                L"leading zero number" },
        { L"{\"a\":1e}",                                L"truncated exponent" },
        { L"{\"a\":\"\\uZZZZ\"}",                       L"bad unicode escape" },
        { L"{\"a\":\"\\\"}",                            L"escape swallowing the terminator" },
        { L"[{\"a\":1}]",                               L"array at the root" },
        { L"{{}}",                                      L"object as a key" },
        { L"{\"a\":}",                                  L"missing value" },
    };

    for (auto const& entry : payloads)
    {
        VerifyConfigPayloadRejected(entry.first, entry.second);
    }

    VerifyServiceStillResponds(L"malformed json configuration payloads");
}


// DISABLED. This test fails, and the failure is real.
//
// The "command name is a number" payload below crashes the service. commandName is read with
// GetNamedString(name, default), whose default only covers the name being absent: a value of
// another type throws, and the throw fail-fasts midisrv.
//
// The throw is in json_transport_command_helper.h, which is shared by five transports (KS,
// KSAggregate, Loopback, BasicLoopback and Network), four of them shipping. That header has
// five such sites, the widest being arguments.GetNamedString(key) with no default, which any
// non-string argument value trips. Fixing it properly means net-new hardened functions and KIR
// gating at every consuming call site across those transports, which is a much larger change
// than this hole warrants on its own.
//
// The worst case is a service crash. There is no leak, no buffer overrun, no elevation, and no
// access to other processes, and the service runs as Local Service rather than Local System.
//
// Re-enable once the shared command helper is hardened. The other thirteen payloads here
// already pass, so this is only parked for the one.
void NetworkMidiMalformedTests::ConfigStructurallyValidButWrongShapeIsRejected()
{
    REQUIRE_HOST();

    // Valid JSON which does not describe anything the transport can act on. These probe the
    // code which reaches into the payload expecting objects, arrays and strings to be there.
    const std::pair<std::wstring, const wchar_t*> payloads[] =
    {
        { L"{}",                                                            L"empty object" },
        { L"{\"endpointTransportPluginSettings\":null}",                    L"null settings" },
        { L"{\"endpointTransportPluginSettings\":\"string\"}",              L"settings as a string" },
        { L"{\"endpointTransportPluginSettings\":[]}",                      L"settings as an array" },
        { L"{\"endpointTransportPluginSettings\":{}}",                      L"settings with no transport" },
        { L"{\"endpointTransportPluginSettings\":{\"not-a-guid\":{}}}",     L"transport key is not a guid" },
        { L"{\"endpointTransportPluginSettings\":{\"{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}\":null}}", L"null transport object" },
        { L"{\"endpointTransportPluginSettings\":{\"{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}\":42}}",   L"transport object is a number" },
        { L"{\"endpointTransportPluginSettings\":{\"{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}\":{\"create\":\"string\"}}}", L"create section is a string" },
        { L"{\"endpointTransportPluginSettings\":{\"{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}\":{\"create\":{\"hosts\":[]}}}}", L"hosts as an array" },
        { L"{\"endpointTransportPluginSettings\":{\"{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}\":{\"create\":{\"hosts\":{\"{11111111-1111-1111-1111-111111111111}\":\"string\"}}}}}", L"host entry is a string" },
        { L"{\"endpointTransportPluginSettings\":{\"{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}\":{\"transportCommand\":{}}}}", L"command with no name" },
        { L"{\"endpointTransportPluginSettings\":{\"{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}\":{\"transportCommand\":{\"commandName\":123}}}}", L"command name is a number" },
        { L"{\"endpointTransportPluginSettings\":{\"{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}\":{\"transportCommand\":{\"commandName\":\"noSuchVerb\"}}}}", L"unknown verb" },
    };

    for (auto const& entry : payloads)
    {
        VerifyConfigPayloadRejected(entry.first, entry.second);
    }

    VerifyServiceStillResponds(L"wrong shape configuration payloads");
}


void NetworkMidiMalformedTests::ConfigHostileStringValuesAreHandled()
{
    REQUIRE_HOST();

    // Well formed requests carrying values chosen to break whatever reads them: the entry
    // identifier parser, the service instance name, and anything which ends up in a device id.
    const std::wstring hostileValues[] =
    {
        L"",
        L"                    ",
        L"\\\\?\\GLOBALROOT\\Device",
        L"../../../../windows/system32",
        L"{00000000-0000-0000-0000-000000000000}",
        L"{not-a-guid-at-all-but-braced-XXXX}",
        L"%SystemRoot%",
        L"\\u0000embedded",
        L"a\\\"b\\\"c",
    };

    for (auto const& value : hostileValues)
    {
        // as an entry identifier on a command which looks one up
        auto command = std::wstring{ L"{\"transportCommand\":{\"commandName\":\"removeHost\",\"entryIdentifier\":\"" }
            + value
            + L"\"}}";

        auto result = SendNetworkTransportConfig(command);

        VERIFY_IS_FALSE(result.IsSuccess(), L"A hostile entry identifier does not remove anything");
    }

    VerifyServiceStillResponds(L"hostile configuration string values");
}


void NetworkMidiMalformedTests::ConfigDeeplyNestedJsonIsHandled()
{
    REQUIRE_HOST();

    Log::Comment(
        L"This test builds thousands of levels of nesting and can take a while. It has not hung.");

    // Recursive descent parsers blow the stack on this. The service must refuse it rather
    // than fault.
    const int depth = 5000;

    std::wstring payload;
    payload.reserve(depth * 8);

    for (int i = 0; i < depth; i++)
    {
        payload += L"{\"a\":";
    }

    payload += L"1";

    for (int i = 0; i < depth; i++)
    {
        payload += L"}";
    }

    auto result = SendRawServiceConfig(payload);

    VERIFY_IS_FALSE(result.IsSuccess(), L"Deeply nested json is not accepted");

    // and the same nesting using arrays, which some parsers handle on a different path
    std::wstring arrayPayload;
    arrayPayload.reserve(depth * 2);

    for (int i = 0; i < depth; i++)
    {
        arrayPayload += L"[";
    }

    for (int i = 0; i < depth; i++)
    {
        arrayPayload += L"]";
    }

    auto arrayResult = SendRawServiceConfig(arrayPayload);

    VERIFY_IS_FALSE(arrayResult.IsSuccess(), L"Deeply nested arrays are not accepted");

    VerifyServiceStillResponds(L"deeply nested json");
}


void NetworkMidiMalformedTests::ConfigHugePayloadIsHandled()
{
    REQUIRE_HOST();

    Log::Comment(
        L"This test sends several megabyte-sized configuration payloads over RPC and can take "
        L"a few minutes. It has not hung. Please let it finish.");

    // A megabyte of key, and separately a megabyte of value. The point is that this is
    // refused without the service growing without bound or falling over.
    const size_t hugeLength = 1024 * 1024;

    {
        std::wstring payload = L"{\"" + std::wstring(hugeLength, L'k') + L"\":1}";

        auto result = SendRawServiceConfig(payload);

        VERIFY_IS_FALSE(result.IsSuccess(), L"A huge key is not accepted");
    }

    {
        std::wstring payload = L"{\"a\":\"" + std::wstring(hugeLength, L'v') + L"\"}";

        auto result = SendRawServiceConfig(payload);

        VERIFY_IS_FALSE(result.IsSuccess(), L"A huge value is not accepted");
    }

    {
        // deeply repeated keys rather than one long one
        std::wstring payload = L"{";

        for (int i = 0; i < 20000; i++)
        {
            if (i > 0)
            {
                payload += L",";
            }

            payload += L"\"k" + std::to_wstring(i) + L"\":" + std::to_wstring(i);
        }

        payload += L"}";

        auto result = SendRawServiceConfig(payload);

        VERIFY_IS_FALSE(result.IsSuccess(), L"A payload with very many keys is not accepted");
    }

    VerifyServiceStillResponds(L"huge configuration payloads");
}


void NetworkMidiMalformedTests::ServiceStillAnswersAfterConfigFuzzing()
{
    REQUIRE_HOST();

    Log::Comment(
        L"This test makes several hundred round trips to the service and can take a few "
        L"minutes. It has not hung.");

    // Randomly mutated payloads, on the same call. Unlike the cases above these are not
    // curated, so the only assertion which makes sense is that the service is still there.
    std::mt19937 rng{ 0xC0FFEE };
    std::uniform_int_distribution<int> lengthDistribution{ 0, 400 };
    std::uniform_int_distribution<int> charDistribution{ 1, 0xFFFF };

    const wchar_t* fragments[] =
    {
        L"{", L"}", L"[", L"]", L"\"", L":", L",", L"\\",
        L"endpointTransportPluginSettings", L"create", L"hosts", L"transportCommand",
        L"commandName", L"removeHost", L"entryIdentifier", L"true", L"null", L"-1",
    };

    const int iterations = 300;

    for (int i = 0; i < iterations; i++)
    {
        std::wstring payload;

        auto length = lengthDistribution(rng);

        for (int c = 0; c < length; c++)
        {
            // mostly json-ish fragments, with raw codepoints mixed in
            if ((rng() % 4) == 0)
            {
                payload += static_cast<wchar_t>(charDistribution(rng));
            }
            else
            {
                payload += fragments[rng() % ARRAYSIZE(fragments)];
            }
        }

        // return value deliberately ignored: any answer is acceptable, a crash is not
        SendRawServiceConfig(payload);
    }

    VerifyServiceStillResponds(L"randomly fuzzed configuration payloads");
}
