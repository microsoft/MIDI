// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "BleMidi2JrTests.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    // 10 MHz, the usual QPC rate. One JR tick (32 microseconds) is exactly 320 local ticks, and
    // 1,000 JR ticks are exactly 32 ms.
    constexpr uint64_t LocalTicksPerSecond = 10'000'000;
    constexpr uint64_t LocalTicksPerJrTick = 320;
    constexpr uint64_t LocalTicksPerMillisecond = 10'000;

    constexpr uint64_t BaseArrival = 10'000'000'000;

    constexpr uint32_t NoteOn = 0x20903C7F;
    constexpr uint32_t NoteOff = 0x20803C00;

    uint32_t Clock(uint16_t const jrTime) { return MidiBleJr::BuildJrClock(jrTime); }
    uint32_t Stamp(uint16_t const jrTime) { return MidiBleJr::BuildJrTimestamp(jrTime); }

    std::vector<MidiBleJr::TimestampedRun> Decode(
        MidiBleJr::IncomingDecoder& decoder,
        std::vector<uint32_t> const& words,
        uint64_t const arrival)
    {
        std::vector<MidiBleJr::TimestampedRun> runs;

        decoder.DecodePayload(words.data(), words.size(), arrival, LocalTicksPerSecond, runs);

        return runs;
    }

    std::vector<uint8_t> Bytes(std::vector<uint32_t> const& words)
    {
        std::vector<uint8_t> bytes;

        for (auto const word : words)
        {
            MidiBleJr::OutgoingPacketBuilder::AppendWordBigEndian(bytes, word);
        }

        return bytes;
    }

    void VerifyPacket(std::vector<uint32_t> const& expectedWords, std::vector<uint8_t> const& actual)
    {
        auto const expected = Bytes(expectedWords);

        VERIFY_ARE_EQUAL(expected.size(), actual.size());

        for (size_t i = 0; i < expected.size() && i < actual.size(); i++)
        {
            VERIFY_ARE_EQUAL(expected[i], actual[i]);
        }
    }
}


void BleMidi2JrTests::TestJrMessagesAreRecognized()
{
    VERIFY_IS_TRUE(MidiBleJr::IsJrClock(0x00101234));
    VERIFY_IS_FALSE(MidiBleJr::IsJrTimestamp(0x00101234));

    VERIFY_IS_TRUE(MidiBleJr::IsJrTimestamp(0x00205678));
    VERIFY_IS_FALSE(MidiBleJr::IsJrClock(0x00205678));

    // NOOP, Delta Clockstamp and anything outside message type 0 are not JR
    VERIFY_IS_FALSE(MidiBleJr::IsJrMessage(0x00000000));
    VERIFY_IS_FALSE(MidiBleJr::IsJrMessage(0x00400010));
    VERIFY_IS_FALSE(MidiBleJr::IsJrMessage(0x20103C7F));

    VERIFY_ARE_EQUAL(static_cast<uint16_t>(0x1234), MidiBleJr::GetJrTime(0x00101234));
    VERIFY_ARE_EQUAL(static_cast<uint32_t>(0x00101234), MidiBleJr::BuildJrClock(0x1234));
    VERIFY_ARE_EQUAL(static_cast<uint32_t>(0x00205678), MidiBleJr::BuildJrTimestamp(0x5678));
}

void BleMidi2JrTests::TestStreamConfigurationNotificationIsParsed()
{
    // MIDI 2.0 Protocol, receives JR, sends JR
    VERIFY_IS_TRUE(MidiBleJr::IsStreamConfigurationNotification(0xF0060203));

    auto both = MidiBleJr::ParseStreamConfigurationNotification(0xF0060203);
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(0x02), both.Protocol);
    VERIFY_IS_TRUE(both.EndpointReceivesJr);
    VERIFY_IS_TRUE(both.EndpointSendsJr);

    auto sendsOnly = MidiBleJr::ParseStreamConfigurationNotification(0xF0060101);
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(0x01), sendsOnly.Protocol);
    VERIFY_IS_FALSE(sendsOnly.EndpointReceivesJr);
    VERIFY_IS_TRUE(sendsOnly.EndpointSendsJr);

    auto receivesOnly = MidiBleJr::ParseStreamConfigurationNotification(0xF0060202);
    VERIFY_IS_TRUE(receivesOnly.EndpointReceivesJr);
    VERIFY_IS_FALSE(receivesOnly.EndpointSendsJr);

    // a Stream Configuration Request is the other side asking, not the endpoint declaring
    VERIFY_IS_FALSE(MidiBleJr::IsStreamConfigurationNotification(0xF0050203));

    // nor is an Endpoint Info Notification
    VERIFY_IS_FALSE(MidiBleJr::IsStreamConfigurationNotification(0xF0010101));
}

void BleMidi2JrTests::TestLocalTimeConvertsToJrTime()
{
    VERIFY_ARE_EQUAL(static_cast<uint16_t>(0), MidiBleJr::LocalTimestampToJrTime(0, LocalTicksPerSecond));
    VERIFY_ARE_EQUAL(static_cast<uint16_t>(1), MidiBleJr::LocalTimestampToJrTime(LocalTicksPerJrTick, LocalTicksPerSecond));
    VERIFY_ARE_EQUAL(static_cast<uint16_t>(31250), MidiBleJr::LocalTimestampToJrTime(LocalTicksPerSecond, LocalTicksPerSecond));

    // JR time wraps every 65,536 ticks, which is 2.09712 seconds
    VERIFY_ARE_EQUAL(static_cast<uint16_t>(0), MidiBleJr::LocalTimestampToJrTime(65536 * LocalTicksPerJrTick, LocalTicksPerSecond));

    // an uptime of a year does not overflow
    uint64_t const aYear = LocalTicksPerSecond * 60 * 60 * 24 * 365;
    VERIFY_ARE_EQUAL(
        static_cast<uint16_t>((aYear / LocalTicksPerJrTick) & 0xFFFF),
        MidiBleJr::LocalTimestampToJrTime(aYear, LocalTicksPerSecond));

    VERIFY_ARE_EQUAL(static_cast<int64_t>(LocalTicksPerSecond), MidiBleJr::JrTicksToLocalTicks(31250, LocalTicksPerSecond));
}


void BleMidi2JrTests::TestDecoderRemovesJrMessages()
{
    MidiBleJr::IncomingDecoder decoder;

    // a 64-bit MIDI 2.0 note on between the JR messages
    std::vector<uint32_t> payload{ Clock(1000), Stamp(1000), 0x40903C00, 0xFFFF0000, Stamp(1000), NoteOff, Clock(1000) };

    auto runs = Decode(decoder, payload, BaseArrival);

    std::vector<uint32_t> delivered;

    for (auto const& run : runs)
    {
        delivered.insert(delivered.end(), run.Words.begin(), run.Words.end());
    }

    VERIFY_ARE_EQUAL(static_cast<size_t>(3), delivered.size());
    VERIFY_ARE_EQUAL(static_cast<uint32_t>(0x40903C00), delivered[0]);
    VERIFY_ARE_EQUAL(static_cast<uint32_t>(0xFFFF0000), delivered[1]);
    VERIFY_ARE_EQUAL(NoteOff, delivered[2]);

    // a payload of nothing but JR delivers nothing
    runs = Decode(decoder, { Clock(1100) }, BaseArrival + 32 * LocalTicksPerMillisecond);
    VERIFY_IS_TRUE(runs.empty());
}

void BleMidi2JrTests::TestDecoderUsesArrivalTimeWithoutAClock()
{
    // Most devices send no JR before negotiation, and some never will
    MidiBleJr::IncomingDecoder decoder;

    auto runs = Decode(decoder, { NoteOn }, BaseArrival);

    VERIFY_ARE_EQUAL(static_cast<size_t>(1), runs.size());
    VERIFY_ARE_EQUAL(BaseArrival, runs[0].Timestamp);
    VERIFY_IS_FALSE(decoder.HaveSeenClock());
    VERIFY_IS_FALSE(decoder.ClockIsUsable());

    // a JR Timestamp is no use without a clock to place it against
    runs = Decode(decoder, { Stamp(5000), NoteOff }, BaseArrival + 1000);

    VERIFY_ARE_EQUAL(static_cast<size_t>(1), runs.size());
    VERIFY_ARE_EQUAL(BaseArrival + 1000, runs[0].Timestamp);
}

void BleMidi2JrTests::TestDecoderPlacesMessagesByJrTimestamp()
{
    MidiBleJr::IncomingDecoder decoder;

    // JR time 1000 arrives at BaseArrival, so that is the offset between the two clocks
    VERIFY_IS_TRUE(Decode(decoder, { Clock(1000) }, BaseArrival).empty());

    // 2,000 JR ticks (64 ms) later by both clocks, with the clock at the end of the packet
    auto runs = Decode(
        decoder,
        { Stamp(2000), NoteOn, Stamp(2500), NoteOff, Clock(3000) },
        BaseArrival + 2000 * LocalTicksPerJrTick);

    VERIFY_IS_TRUE(decoder.ClockIsUsable());
    VERIFY_ARE_EQUAL(static_cast<size_t>(2), runs.size());

    VERIFY_ARE_EQUAL(BaseArrival + 1000 * LocalTicksPerJrTick, runs[0].Timestamp);
    VERIFY_ARE_EQUAL(NoteOn, runs[0].Words[0]);

    VERIFY_ARE_EQUAL(BaseArrival + 1500 * LocalTicksPerJrTick, runs[1].Timestamp);
    VERIFY_ARE_EQUAL(NoteOff, runs[1].Words[0]);

    // the 500 tick (16 ms) spacing the sender meant is kept
    VERIFY_ARE_EQUAL(500 * LocalTicksPerJrTick, runs[1].Timestamp - runs[0].Timestamp);
}

void BleMidi2JrTests::TestDecoderSharesATimestampAcrossSimultaneousMessages()
{
    MidiBleJr::IncomingDecoder decoder;

    Decode(decoder, { Clock(1000) }, BaseArrival);

    auto runs = Decode(decoder, { Stamp(1100), NoteOn, NoteOff, Clock(1200) }, BaseArrival + 200 * LocalTicksPerJrTick);

    VERIFY_ARE_EQUAL(static_cast<size_t>(1), runs.size());
    VERIFY_ARE_EQUAL(static_cast<size_t>(2), runs[0].Words.size());
    VERIFY_ARE_EQUAL(BaseArrival + 100 * LocalTicksPerJrTick, runs[0].Timestamp);
}

void BleMidi2JrTests::TestDecoderNeverReturnsAFutureTimestamp()
{
    MidiBleJr::IncomingDecoder decoder;

    Decode(decoder, { Clock(1000) }, BaseArrival);

    // stamped a full second ahead of the clock
    auto const arrival = BaseArrival + 32 * LocalTicksPerMillisecond;
    auto runs = Decode(decoder, { Stamp(1000 + 31250), NoteOn }, arrival);

    VERIFY_ARE_EQUAL(static_cast<size_t>(1), runs.size());
    VERIFY_ARE_EQUAL(arrival, runs[0].Timestamp);
}

void BleMidi2JrTests::TestDecoderFollowsTheClockAcrossAWrap()
{
    MidiBleJr::IncomingDecoder decoder;

    Decode(decoder, { Clock(65000) }, BaseArrival);

    // 1,036 ticks later the 16-bit time has wrapped to 500
    auto runs = Decode(
        decoder,
        { Stamp(65500), NoteOn, Stamp(400), NoteOff, Clock(500) },
        BaseArrival + 1036 * LocalTicksPerJrTick);

    VERIFY_ARE_EQUAL(static_cast<size_t>(2), runs.size());
    VERIFY_ARE_EQUAL(BaseArrival + 500 * LocalTicksPerJrTick, runs[0].Timestamp);
    VERIFY_ARE_EQUAL(BaseArrival + 936 * LocalTicksPerJrTick, runs[1].Timestamp);
}

void BleMidi2JrTests::TestDecoderFallsBackToArrivalWhenTheClockStops()
{
    MidiBleJr::IncomingDecoder decoder;

    // the same JR Clock value, 100 ms apart, three times over
    for (uint64_t i = 0; i < 4; i++)
    {
        Decode(decoder, { Clock(1000) }, BaseArrival + i * 100 * LocalTicksPerMillisecond);
    }

    VERIFY_IS_TRUE(decoder.ClockIsStalled());
    VERIFY_IS_FALSE(decoder.ClockIsUsable());

    auto const arrival = BaseArrival + 350 * LocalTicksPerMillisecond;
    auto runs = Decode(decoder, { Stamp(1000), NoteOn }, arrival);

    VERIFY_ARE_EQUAL(arrival, runs[0].Timestamp);

    // and it is trusted again once it moves
    Decode(decoder, { Clock(1000 + 3125) }, arrival + 100 * LocalTicksPerMillisecond);

    VERIFY_IS_FALSE(decoder.ClockIsStalled());
    VERIFY_IS_TRUE(decoder.ClockIsUsable());
}

void BleMidi2JrTests::TestDecoderFallsBackToArrivalWhenTheClockGoesQuiet()
{
    MidiBleJr::IncomingDecoder decoder;

    Decode(decoder, { Clock(1000) }, BaseArrival);

    // two seconds with no clock, when one is owed every 250 ms
    auto const arrival = BaseArrival + 2000 * LocalTicksPerMillisecond;
    auto runs = Decode(decoder, { Stamp(1000), NoteOn }, arrival);

    VERIFY_IS_TRUE(decoder.ClockIsStale());
    VERIFY_IS_FALSE(decoder.ClockIsUsable());
    VERIFY_ARE_EQUAL(arrival, runs[0].Timestamp);
}

void BleMidi2JrTests::TestDecoderCarriesAnUnusedTimestampIntoTheNextPayload()
{
    // A sender out of room defers the message, so its timestamp can end the previous packet
    MidiBleJr::IncomingDecoder decoder;

    VERIFY_IS_TRUE(Decode(decoder, { Clock(1000), Stamp(1500) }, BaseArrival).empty());

    auto runs = Decode(decoder, { NoteOn }, BaseArrival + 1000 * LocalTicksPerJrTick);

    VERIFY_ARE_EQUAL(static_cast<size_t>(1), runs.size());
    VERIFY_ARE_EQUAL(BaseArrival + 500 * LocalTicksPerJrTick, runs[0].Timestamp);
}

void BleMidi2JrTests::TestDecoderDoesNotCarryAUsedTimestamp()
{
    MidiBleJr::IncomingDecoder decoder;

    auto runs = Decode(decoder, { Clock(1000), Stamp(1000), NoteOn }, BaseArrival);
    VERIFY_ARE_EQUAL(BaseArrival, runs[0].Timestamp);

    // no timestamp of its own, so it is placed when it arrived and not pinned to the last one
    auto const arrival = BaseArrival + 1000 * LocalTicksPerJrTick;
    runs = Decode(decoder, { NoteOff }, arrival);

    VERIFY_ARE_EQUAL(arrival, runs[0].Timestamp);
}

void BleMidi2JrTests::TestDecoderResetForgetsTheClock()
{
    MidiBleJr::IncomingDecoder decoder;

    Decode(decoder, { Clock(1000) }, BaseArrival);
    VERIFY_IS_TRUE(decoder.HaveSeenClock());

    decoder.Reset();

    VERIFY_IS_FALSE(decoder.HaveSeenClock());
    VERIFY_IS_FALSE(decoder.ClockIsUsable());

    auto const arrival = BaseArrival + 10 * LocalTicksPerMillisecond;
    auto runs = Decode(decoder, { Stamp(1000), NoteOn }, arrival);

    VERIFY_ARE_EQUAL(arrival, runs[0].Timestamp);
}


void BleMidi2JrTests::TestBuilderWithoutJrPacksMessagesUnchanged()
{
    MidiBleJr::OutgoingPacketBuilder builder;
    builder.SetMaxPayloadByteCount(64);
    builder.SetJrEnabled(false);

    VERIFY_IS_TRUE(builder.AppendMessage(&NoteOn, 1, 100));
    VERIFY_IS_TRUE(builder.AppendMessage(&NoteOff, 1, 200));

    auto packets = builder.TakePackets();

    VERIFY_ARE_EQUAL(static_cast<size_t>(1), packets.size());
    VerifyPacket({ NoteOn, NoteOff }, packets[0]);
}

void BleMidi2JrTests::TestBuilderDropsApplicationJrMessages()
{
    for (auto const jrEnabled : { false, true })
    {
        MidiBleJr::OutgoingPacketBuilder builder;
        builder.SetMaxPayloadByteCount(64);
        builder.SetJrEnabled(jrEnabled);

        auto const clock = Clock(1234);
        auto const stamp = Stamp(1234);

        VERIFY_IS_FALSE(builder.AppendMessage(&clock, 1, 100));
        VERIFY_IS_FALSE(builder.AppendMessage(&stamp, 1, 100));

        VERIFY_IS_TRUE(builder.TakePackets().empty());
    }
}

void BleMidi2JrTests::TestBuilderSharesOneTimestampWithinATick()
{
    MidiBleJr::OutgoingPacketBuilder builder;
    builder.SetMaxPayloadByteCount(64);
    builder.SetJrEnabled(true);

    builder.AppendMessage(&NoteOn, 1, 700);
    builder.AppendMessage(&NoteOff, 1, 700);

    auto packets = builder.TakePackets();

    // one timestamp for the pair, and the clock at the end carries the same time
    VERIFY_ARE_EQUAL(static_cast<size_t>(1), packets.size());
    VerifyPacket({ Stamp(700), NoteOn, NoteOff, Clock(700) }, packets[0]);
}

void BleMidi2JrTests::TestBuilderTimestampsEachMessageInADifferentTick()
{
    MidiBleJr::OutgoingPacketBuilder builder;
    builder.SetMaxPayloadByteCount(64);
    builder.SetJrEnabled(true);

    builder.AppendMessage(&NoteOn, 1, 700);
    builder.AppendMessage(&NoteOff, 1, 750);

    auto packets = builder.TakePackets();

    VERIFY_ARE_EQUAL(static_cast<size_t>(1), packets.size());
    VerifyPacket({ Stamp(700), NoteOn, Stamp(750), NoteOff, Clock(750) }, packets[0]);
}

void BleMidi2JrTests::TestBuilderKeepsRoomForTheClock()
{
    MidiBleJr::OutgoingPacketBuilder builder;
    builder.SetMaxPayloadByteCount(16);
    builder.SetJrEnabled(true);

    uint32_t const midi2NoteOn[]{ 0x40903C00, 0xFFFF0000 };

    builder.AppendMessage(midi2NoteOn, 2, 900);
    builder.AppendMessage(midi2NoteOn, 2, 900);

    auto packets = builder.TakePackets();

    // Each packet holds one 8-byte message, its timestamp and the clock. The second message
    // starts a new packet, so it gets a timestamp of its own there.
    VERIFY_ARE_EQUAL(static_cast<size_t>(2), packets.size());
    VerifyPacket({ Stamp(900), midi2NoteOn[0], midi2NoteOn[1], Clock(900) }, packets[0]);
    VerifyPacket({ Stamp(900), midi2NoteOn[0], midi2NoteOn[1], Clock(900) }, packets[1]);
}

void BleMidi2JrTests::TestBuilderSendsTheClockSeparatelyWhenItCannotFit()
{
    MidiBleJr::OutgoingPacketBuilder builder;
    builder.SetMaxPayloadByteCount(12);
    builder.SetJrEnabled(true);

    uint32_t const midi2NoteOn[]{ 0x40903C00, 0xFFFF0000 };

    builder.AppendMessage(midi2NoteOn, 2, 900);

    auto packets = builder.TakePackets();

    VERIFY_ARE_EQUAL(static_cast<size_t>(2), packets.size());
    VerifyPacket({ Stamp(900), midi2NoteOn[0], midi2NoteOn[1] }, packets[0]);
    VerifyPacket({ Clock(900) }, packets[1]);
}

void BleMidi2JrTests::TestClockOnlyPacket()
{
    VerifyPacket({ Clock(0xBEEF) }, MidiBleJr::OutgoingPacketBuilder::BuildClockOnlyPacket(0xBEEF));
}
