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

// A missing host is a connection failure, not a skip. EnsureHostAvailable logs the error.
#define REQUIRE_HOST() \
    if (!ProtocolTestContext::Current().EnsureHostAvailable()) { return; }

// historical name, still used by the tests below
#define SKIP_IF_NO_HOST() REQUIRE_HOST()

namespace
{
    // A single 32-bit UMP message (type 2, MIDI 1.0 channel voice: note on, ch 1, note 60)
    constexpr uint32_t SampleMidi1NoteOn{ 0x20903C40 };

    // Sends a command outside any session and verifies the host answers Bye 0x05.
    void VerifyRefusedOutsideSession(
        _In_ std::wstring const& description,
        _In_ PacketBuilder& builder)
    {
        auto& context = ProtocolTestContext::Current();

        UdpTestClient client;
        VERIFY_IS_TRUE(client.Open(context.Host()));

        VERIFY_IS_TRUE(client.Send(builder));

        auto reply = client.WaitForCommand(CommandCode::Bye, std::chrono::milliseconds(5000));

        if (!reply.has_value())
        {
            LogNoPacket(description + L":");
        }
        else
        {
            LogPacket(description + L":", reply.value());
        }

        VERIFY_IS_TRUE(reply.has_value(), L"Host answered with a Bye");

        auto bye = reply->Find(CommandCode::Bye);
        VERIFY_IS_NOT_NULL(bye);

        VERIFY_ARE_EQUAL(
            static_cast<int>(bye->GetByeReason()),
            static_cast<int>(ByeReason::SessionNotEstablished),
            L"Bye reason is 0x05 Session Not Established");

        client.Close();
    }
}


void NetworkMidiErrorTests::UmpDataOutsideSessionIsRefused()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"7.1 - If a Device receives the UMP Data Command when not in an Established Session, then it shall respond with a Bye Command, reason 0x05 (Session not Established)");

    PacketBuilder builder;
    builder.StartPacket().AddUmpData(1, { SampleMidi1NoteOn });

    VerifyRefusedOutsideSession(L"UMP Data outside session", builder);
}

void NetworkMidiErrorTests::RetransmitRequestOutsideSessionIsRefused()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"7.2.3 - If a Device receives the Retransmit Request Command outside of an Established Session, then it shall respond with a Bye Command with reason 0x05 (Session Not Established)");

    PacketBuilder builder;
    builder.StartPacket().AddRetransmitRequest(1, 0);

    VerifyRefusedOutsideSession(L"Retransmit Request outside session", builder);
}

void NetworkMidiErrorTests::RetransmitErrorOutsideSessionIsRefused()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"7.2.4 - If a Device receives the Retransmit Error Command outside of an Established Session, then it shall respond with a Bye Command, reason 0x05 (Session Not Established)");

    PacketBuilder builder;
    builder.StartPacket()
        .AddCommandHeader(CommandCode::RetransmitError, 1, static_cast<uint8_t>(RetransmitErrorReason::DataNotAvailable), 0)
        .AddUInt16(1)
        .AddUInt16(0);

    VerifyRefusedOutsideSession(L"Retransmit Error outside session", builder);
}

void NetworkMidiErrorTests::SessionResetOutsideSessionIsRefused()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"6.13 - If the receiver of a Session Reset Command is not in an active Session with the Sender, it shall send a Bye Command with reason 0x05 (Session Not Established)");

    PacketBuilder builder;
    builder.StartPacket().AddSessionReset();

    VerifyRefusedOutsideSession(L"Session Reset outside session", builder);
}

void NetworkMidiErrorTests::SessionResetReplyOutsideSessionIsRefused()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"6.14 - If the receiver is not in an active Session with the sender, it shall send a Bye Command with reason 0x05 (Session Not Established)");

    PacketBuilder builder;
    builder.StartPacket().AddSessionResetReply();

    VerifyRefusedOutsideSession(L"Session Reset Reply outside session", builder);
}

void NetworkMidiErrorTests::RefusalIsNotRepeatedForEveryCommandInOneDatagram()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"Robustness - a datagram full of out-of-session commands must not produce a Bye per command, which would be an amplification vector");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;
    VERIFY_IS_TRUE(client.Open(context.Host()));

    // ten UMP Data commands, none of which belong to a session
    PacketBuilder builder;
    builder.StartPacket();

    for (uint16_t i = 1; i <= 10; i++)
    {
        builder.AddUmpData(i, { SampleMidi1NoteOn });
    }

    VERIFY_IS_TRUE(client.Send(builder));

    size_t byeCount{ 0 };

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(3000);

    while (std::chrono::steady_clock::now() < deadline)
    {
        auto packet = client.ReceivePacket(std::chrono::milliseconds(500));

        if (!packet.has_value())
        {
            continue;
        }

        LogPacket(L"Response:", packet.value());

        byeCount += packet->Count(CommandCode::Bye);
    }

    Log::Comment(String().Format(L"Bye commands received for a datagram of 10 out-of-session commands: %zu", byeCount));

    VERIFY_IS_GREATER_THAN(byeCount, static_cast<size_t>(0), L"Host answered at least once");
    VERIFY_IS_LESS_THAN_OR_EQUAL(byeCount, static_cast<size_t>(2), L"Host did not answer once per command");

    client.Close();
}


void NetworkMidiErrorTests::SequenceGapTriggersRetransmitRequest()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"7.2 - A Device may send a Retransmit Request Command to ask the receiving entity to resend UMP Data Command(s) identified by Sequence Number");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;

    VERIFY_IS_TRUE(EstablishSession(
        client,
        context.MakeUniqueEndpointName("Gap"),
        context.MakeUniqueProductInstanceId("G")));

    client.DrainPending();

    // establish the sequence baseline
    {
        PacketBuilder builder;
        builder.StartPacket().AddUmpData(1, { SampleMidi1NoteOn });

        VERIFY_IS_TRUE(client.Send(builder));
    }

    Sleep(300);

    // jump well past the forward error correction window so the host cannot recover on its own
    {
        PacketBuilder builder;
        builder.StartPacket().AddUmpData(500, { SampleMidi1NoteOn });

        VERIFY_IS_TRUE(client.Send(builder));
    }

    auto request = client.WaitForCommand(CommandCode::RetransmitRequest, std::chrono::milliseconds(5000));

    if (!request.has_value())
    {
        LogNoPacket(L"RetransmitRequest after a sequence gap:");
    }
    else
    {
        LogPacket(L"RetransmitRequest after a sequence gap:", request.value());

        auto command = request->Find(CommandCode::RetransmitRequest);
        VERIFY_IS_NOT_NULL(command);

        Log::Comment(String().Format(
            L"Host asked for retransmit starting at sequence %u",
            static_cast<unsigned>(command->GetSequenceNumber())));
    }

    VERIFY_IS_TRUE(request.has_value(), L"Host asked for the missing packets");

    EndSession(client);
}

void NetworkMidiErrorTests::RetransmitRequestStopsAfterNakCommandNotSupported()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"7.2.3 - If a Device does not implement the Retransmit mechanism, it shall reply with NAK reason 0x01. The remote Device should not send Retransmit Request Commands after that. (issue #1003)");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;

    VERIFY_IS_TRUE(EstablishSession(
        client,
        context.MakeUniqueEndpointName("NakRetransmit"),
        context.MakeUniqueProductInstanceId("NR")));

    client.DrainPending();

    {
        PacketBuilder builder;
        builder.StartPacket().AddUmpData(1, { SampleMidi1NoteOn });
        client.Send(builder);
    }

    Sleep(300);

    {
        PacketBuilder builder;
        builder.StartPacket().AddUmpData(500, { SampleMidi1NoteOn });
        client.Send(builder);
    }

    auto request = client.WaitForCommand(CommandCode::RetransmitRequest, std::chrono::milliseconds(5000));

    if (!request.has_value())
    {
        // Spec 7.2: a receiver which detects a gap asks for the missing packets. Without that
        // there is nothing to NAK, so this test cannot verify anything and must not pass.
        Log::Error(L"Host did not request a retransmit after a 499 packet sequence gap.");

        EndSession(client);

        return;
    }

    auto command = request->Find(CommandCode::RetransmitRequest);
    VERIFY_IS_NOT_NULL(command);

    // Tell the host we do not implement retransmit at all
    {
        PacketBuilder builder;
        builder.StartPacket().AddNak(command->HeaderWord, NakReason::CommandNotSupported, "Not supported by test client.");

        VERIFY_IS_TRUE(client.Send(builder));
    }

    client.ClearHistory();

    // A request already on the wire when we sent the NAK is not a violation: the spec's "should
    // not send Retransmit Request Commands after that" means after the NAK has been processed.
    // The host retries a request several times, so one can easily be in flight.
    client.DrainPending(std::chrono::milliseconds(500));

    // Keep sending data with an advancing sequence. The host should accept the loss and stop
    // asking rather than demanding the gap be filled forever.
    size_t furtherRequests{ 0 };

    for (uint16_t sequence = 501; sequence < 511; sequence++)
    {
        PacketBuilder builder;
        builder.StartPacket().AddUmpData(sequence, { SampleMidi1NoteOn });
        client.Send(builder);

        auto packet = client.ReceivePacket(std::chrono::milliseconds(400));

        if (packet.has_value())
        {
            furtherRequests += packet->Count(CommandCode::RetransmitRequest);

            if (packet->Contains(CommandCode::RetransmitRequest))
            {
                LogPacket(L"Unexpected further retransmit request:", packet.value());
            }
        }
    }

    Log::Comment(String().Format(L"Retransmit requests after NAK CommandNotSupported: %zu", furtherRequests));

    VERIFY_ARE_EQUAL(furtherRequests, static_cast<size_t>(0), L"Host stopped asking after being told retransmit is unsupported");

    EndSession(client);
}

void NetworkMidiErrorTests::RetransmitErrorEndsTheRequestsAndSessionSurvives()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"7.2.4 - A Retransmit Error tells the requester the data is gone. The session must continue rather than stall. (issue #1003)");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;

    VERIFY_IS_TRUE(EstablishSession(
        client,
        context.MakeUniqueEndpointName("RetransmitError"),
        context.MakeUniqueProductInstanceId("RE")));

    client.DrainPending();

    {
        PacketBuilder builder;
        builder.StartPacket().AddUmpData(1, { SampleMidi1NoteOn });
        client.Send(builder);
    }

    Sleep(300);

    {
        PacketBuilder builder;
        builder.StartPacket().AddUmpData(400, { SampleMidi1NoteOn });
        client.Send(builder);
    }

    auto request = client.WaitForCommand(CommandCode::RetransmitRequest, std::chrono::milliseconds(5000));

    if (request.has_value())
    {
        PacketBuilder builder;
        builder.StartPacket()
            .AddCommandHeader(CommandCode::RetransmitError, 1, static_cast<uint8_t>(RetransmitErrorReason::DataNotAvailable), 0)
            .AddUInt16(400)
            .AddUInt16(0);

        VERIFY_IS_TRUE(client.Send(builder));
    }

    client.DrainPending();

    // The session must still be alive: a ping should still be answered
    PacketBuilder ping;
    ping.StartPacket().AddPing(0xDEADBEEF);

    VERIFY_IS_TRUE(client.Send(ping));

    auto reply = client.WaitForCommand(CommandCode::PingReply, std::chrono::milliseconds(5000));

    if (!reply.has_value())
    {
        LogNoPacket(L"PingReply after Retransmit Error:");
    }

    VERIFY_IS_TRUE(reply.has_value(), L"Session survived an unfulfillable retransmit");

    EndSession(client);
}

void NetworkMidiErrorTests::SessionSurvivesUnrecoverableGap()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"Issue #1003 - after a gap which cannot be recovered, the host must resynchronize and keep accepting data rather than stalling forever");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;

    VERIFY_IS_TRUE(EstablishSession(
        client,
        context.MakeUniqueEndpointName("Unrecoverable"),
        context.MakeUniqueProductInstanceId("U")));

    client.DrainPending();

    {
        PacketBuilder builder;
        builder.StartPacket().AddUmpData(1, { SampleMidi1NoteOn });
        client.Send(builder);
    }

    Sleep(300);

    // Big gap, and we never satisfy any retransmit request. The host should give up and move on.
    uint16_t sequence{ 1000 };

    for (int i = 0; i < 12; i++)
    {
        PacketBuilder builder;
        builder.StartPacket().AddUmpData(sequence++, { SampleMidi1NoteOn });
        client.Send(builder);

        Sleep(250);

        // absorb whatever the host sends back, including retransmit requests we refuse to honour
        client.ReceivePacket(std::chrono::milliseconds(50));
    }

    client.DrainPending();

    // The session must still be usable
    PacketBuilder ping;
    ping.StartPacket().AddPing(0x600DF00D);

    VERIFY_IS_TRUE(client.Send(ping));

    auto reply = client.WaitForCommand(CommandCode::PingReply, std::chrono::milliseconds(5000));

    if (!reply.has_value())
    {
        LogNoPacket(L"PingReply after an unrecoverable gap:");
    }

    VERIFY_IS_TRUE(reply.has_value(), L"Session still alive after an unrecoverable gap");

    EndSession(client);
}

void NetworkMidiErrorTests::DuplicateSequenceNumbersAreIgnored()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"7.1 - The underlying transport might cause duplication of UDP packets, and FEC uses duplicate UMP Data Commands. Receivers ignore already-processed Sequence Numbers.");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;

    VERIFY_IS_TRUE(EstablishSession(
        client,
        context.MakeUniqueEndpointName("Duplicate"),
        context.MakeUniqueProductInstanceId("D")));

    client.DrainPending();

    // The same sequence number several times over, which must not be treated as a gap.
    // Spec 5.6: the first UMP Data Command of a session shall use Sequence Number 0x0000,
    // so starting anywhere else would legitimately look like a gap.
    for (int i = 0; i < 5; i++)
    {
        PacketBuilder builder;
        builder.StartPacket().AddUmpData(0, { SampleMidi1NoteOn });

        VERIFY_IS_TRUE(client.Send(builder));

        Sleep(100);
    }

    size_t retransmitRequests{ 0 };

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);

    while (std::chrono::steady_clock::now() < deadline)
    {
        auto packet = client.ReceivePacket(std::chrono::milliseconds(400));

        if (packet.has_value())
        {
            retransmitRequests += packet->Count(CommandCode::RetransmitRequest);
        }
    }

    VERIFY_ARE_EQUAL(retransmitRequests, static_cast<size_t>(0), L"Repeated sequence numbers did not look like a gap");

    EndSession(client);
}

void NetworkMidiErrorTests::EmptyUmpDataIsAcceptedAsKeepAlive()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"7.1 - A UMP Data Command with no UMP words is valid and still carries a Sequence Number");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;

    VERIFY_IS_TRUE(EstablishSession(
        client,
        context.MakeUniqueEndpointName("EmptyUmp"),
        context.MakeUniqueProductInstanceId("E")));

    client.DrainPending();

    // sequences 0..4, all empty, then real data at 5. If empty commands did not advance the
    // sequence, the host would see 5 as a gap. Spec 5.6: a session's first UMP Data Command
    // shall use Sequence Number 0x0000.
    for (uint16_t sequence = 0; sequence <= 4; sequence++)
    {
        PacketBuilder builder;
        builder.StartPacket().AddUmpData(sequence, { });

        VERIFY_IS_TRUE(client.Send(builder));

        Sleep(80);
    }

    {
        PacketBuilder builder;
        builder.StartPacket().AddUmpData(5, { SampleMidi1NoteOn });

        VERIFY_IS_TRUE(client.Send(builder));
    }

    size_t retransmitRequests{ 0 };

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);

    while (std::chrono::steady_clock::now() < deadline)
    {
        auto packet = client.ReceivePacket(std::chrono::milliseconds(400));

        if (packet.has_value())
        {
            retransmitRequests += packet->Count(CommandCode::RetransmitRequest);
        }
    }

    VERIFY_ARE_EQUAL(retransmitRequests, static_cast<size_t>(0), L"Empty UMP Data advanced the sequence like any other command");

    EndSession(client);
}


void NetworkMidiErrorTests::SessionResetIsAcknowledged()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"6.13 - A Session Reset within an established session shall be acknowledged with a Session Reset Reply");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;

    VERIFY_IS_TRUE(EstablishSession(
        client,
        context.MakeUniqueEndpointName("Reset"),
        context.MakeUniqueProductInstanceId("S")));

    client.DrainPending();

    PacketBuilder builder;
    builder.StartPacket().AddSessionReset();

    VERIFY_IS_TRUE(client.Send(builder));

    auto reply = client.WaitForCommand(CommandCode::SessionResetReply, std::chrono::milliseconds(5000));

    if (!reply.has_value())
    {
        LogNoPacket(L"SessionResetReply:");
    }

    VERIFY_IS_TRUE(reply.has_value(), L"Host acknowledged the session reset");

    // after a reset the sequence starts over, so sequence 1 must not look like a duplicate
    client.DrainPending();

    {
        PacketBuilder data;
        data.StartPacket().AddUmpData(0, { SampleMidi1NoteOn });

        VERIFY_IS_TRUE(client.Send(data));
    }

    size_t retransmitRequests{ 0 };

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1500);

    while (std::chrono::steady_clock::now() < deadline)
    {
        auto packet = client.ReceivePacket(std::chrono::milliseconds(400));

        if (packet.has_value())
        {
            retransmitRequests += packet->Count(CommandCode::RetransmitRequest);
        }
    }

    VERIFY_ARE_EQUAL(retransmitRequests, static_cast<size_t>(0), L"Sequence numbering restarted cleanly after the reset");

    EndSession(client);
}


void NetworkMidiErrorTests::UnknownCommandCodeIsNaked()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"6.15 - An unrecognized Command Code should be answered with a NAK, reason 0x01 (Command not supported)");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;

    VERIFY_IS_TRUE(EstablishSession(
        client,
        context.MakeUniqueEndpointName("Unknown"),
        context.MakeUniqueProductInstanceId("X")));

    client.DrainPending();

    // 0x7E is not assigned by the specification
    PacketBuilder builder;
    builder.StartPacket().AddCommandHeader(static_cast<CommandCode>(0x7E), 0, static_cast<uint16_t>(0));

    VERIFY_IS_TRUE(client.Send(builder));

    auto reply = client.WaitForCommand(CommandCode::Nak, std::chrono::milliseconds(5000));

    if (!reply.has_value())
    {
        LogNoPacket(L"NAK for an unknown command code:");
    }
    else
    {
        LogPacket(L"NAK for an unknown command code:", reply.value());

        auto nak = reply->Find(CommandCode::Nak);
        VERIFY_IS_NOT_NULL(nak);

        VERIFY_ARE_EQUAL(
            static_cast<int>(nak->GetNakReason()),
            static_cast<int>(NakReason::CommandNotSupported),
            L"NAK reason is 0x01 Command Not Supported");

        // the NAK payload starts with the original command header word
        VERIFY_IS_GREATER_THAN_OR_EQUAL(nak->PayloadLengthWords, static_cast<uint8_t>(1));

        auto echoed = nak->GetPayloadUInt32(0);

        Log::Comment(String().Format(L"NAK echoed original command header 0x%08X", echoed));

        VERIFY_ARE_EQUAL(
            static_cast<uint8_t>((echoed >> 24) & 0xFF),
            static_cast<uint8_t>(0x7E),
            L"NAK echoed the offending command code");
    }

    VERIFY_IS_TRUE(reply.has_value());

    EndSession(client);
}


void NetworkMidiErrorTests::RetransmitRequestForUnknownSequenceIsAnswered()
{
    SKIP_IF_NO_HOST();

    LogSpecRequirement(L"7.2 - If the requested Sequence Numbers are not available, the Device should send a Retransmit Error. If it does not implement retransmit at all, it replies NAK reason 0x01.");

    auto& context = ProtocolTestContext::Current();

    UdpTestClient client;

    VERIFY_IS_TRUE(EstablishSession(
        client,
        context.MakeUniqueEndpointName("AskHost"),
        context.MakeUniqueProductInstanceId("AH")));

    client.DrainPending();

    // ask the host for data it certainly never sent
    PacketBuilder builder;
    builder.StartPacket().AddRetransmitRequest(50000, 4);

    VERIFY_IS_TRUE(client.Send(builder));

    bool answered{ false };

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(5000);

    while (std::chrono::steady_clock::now() < deadline && !answered)
    {
        auto packet = client.ReceivePacket(std::chrono::milliseconds(500));

        if (!packet.has_value())
        {
            continue;
        }

        if (packet->Contains(CommandCode::RetransmitError) || packet->Contains(CommandCode::Nak))
        {
            LogPacket(L"Answer to an impossible retransmit request:", packet.value());

            answered = true;
        }
    }

    VERIFY_IS_TRUE(answered, L"Host answered with a Retransmit Error or a NAK rather than ignoring the request");

    EndSession(client);
}
