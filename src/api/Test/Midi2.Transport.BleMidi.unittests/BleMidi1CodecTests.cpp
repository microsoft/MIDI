// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "BleMidi1CodecTests.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    std::vector<MidiBleMidi1::DecodedSegment> Decode(
        MidiBleMidi1::PacketDecoder& decoder,
        std::vector<uint8_t> const& packet,
        bool expectedResult = true)
    {
        std::vector<MidiBleMidi1::DecodedSegment> segments;

        auto const result = decoder.DecodePacket(packet.data(), packet.size(), segments);

        VERIFY_ARE_EQUAL(expectedResult, result);

        return segments;
    }

    // Every byte in every segment, in order. Segment boundaries only carry timing, so this is
    // what a decoder consumer ultimately sees.
    std::vector<uint8_t> FlattenBytes(std::vector<MidiBleMidi1::DecodedSegment> const& segments)
    {
        std::vector<uint8_t> bytes;

        for (auto const& segment : segments)
        {
            bytes.insert(bytes.end(), segment.Bytes.begin(), segment.Bytes.end());
        }

        return bytes;
    }

    std::wstring FormatBytes(std::vector<uint8_t> const& bytes)
    {
        std::wstring text;

        for (auto const& b : bytes)
        {
            wchar_t buffer[6]{ };
            swprintf_s(buffer, ARRAYSIZE(buffer), L"%02X ", b);
            text += buffer;
        }

        return text;
    }

    void VerifyBytes(std::vector<uint8_t> const& expected, std::vector<uint8_t> const& actual)
    {
        Log::Comment((L"Expected: " + FormatBytes(expected)).c_str());
        Log::Comment((L"Actual:   " + FormatBytes(actual)).c_str());

        VERIFY_ARE_EQUAL(expected.size(), actual.size());

        for (size_t i = 0; i < expected.size() && i < actual.size(); i++)
        {
            VERIFY_ARE_EQUAL(expected[i], actual[i]);
        }
    }

    constexpr uint8_t Header(uint16_t const timestamp)
    {
        return static_cast<uint8_t>(0x80 | ((timestamp >> 7) & 0x3F));
    }

    constexpr uint8_t Timestamp(uint16_t const timestamp)
    {
        return static_cast<uint8_t>(0x80 | (timestamp & 0x7F));
    }
}


void BleMidi1CodecTests::TestEmptyPayloadIsAccepted()
{
    // the Peripheral answers the initial Characteristic read with a packet that has no payload
    MidiBleMidi1::PacketDecoder decoder;
    std::vector<MidiBleMidi1::DecodedSegment> segments;

    VERIFY_IS_TRUE(decoder.DecodePacket(nullptr, 0, segments));
    VERIFY_IS_TRUE(segments.empty());
}

void BleMidi1CodecTests::TestPacketWithoutHeaderBitIsRejected()
{
    MidiBleMidi1::PacketDecoder decoder;

    // bit 7 of the Header Byte is always set
    auto segments = Decode(decoder, { 0x00, 0x80, 0x90, 0x40, 0x7F }, false);

    VERIFY_IS_TRUE(segments.empty());
}

void BleMidi1CodecTests::TestSingleFullMessageIsDecoded()
{
    MidiBleMidi1::PacketDecoder decoder;

    auto segments = Decode(decoder, { Header(4660), Timestamp(4660), 0x90, 0x40, 0x7F });

    VERIFY_ARE_EQUAL((size_t)1, segments.size());
    VERIFY_ARE_EQUAL((uint32_t)0, segments[0].RelativeMilliseconds);
    VerifyBytes({ 0x90, 0x40, 0x7F }, segments[0].Bytes);
}

void BleMidi1CodecTests::TestTruncatedMessageDoesNotOverrun()
{
    MidiBleMidi1::PacketDecoder decoder;

    // a note on missing its velocity byte
    auto segments = Decode(decoder, { Header(0), Timestamp(0), 0x90, 0x40 });

    VerifyBytes({ 0x90, 0x40 }, FlattenBytes(segments));
}

void BleMidi1CodecTests::TestTrailingTimestampByteIsIgnored()
{
    MidiBleMidi1::PacketDecoder decoder;

    auto segments = Decode(decoder, { Header(0), Timestamp(0), 0x90, 0x40, 0x7F, Timestamp(5) });

    VerifyBytes({ 0x90, 0x40, 0x7F }, FlattenBytes(segments));
}

void BleMidi1CodecTests::TestTimestampSpacingIsRelativeToFirstMessage()
{
    MidiBleMidi1::PacketDecoder decoder;

    auto segments = Decode(decoder, {
        Header(1000),
        Timestamp(1000), 0x90, 0x40, 0x7F,
        Timestamp(1007), 0x90, 0x41, 0x7F });

    VERIFY_ARE_EQUAL((size_t)2, segments.size());
    VERIFY_ARE_EQUAL((uint32_t)0, segments[0].RelativeMilliseconds);
    VERIFY_ARE_EQUAL((uint32_t)7, segments[1].RelativeMilliseconds);
}

void BleMidi1CodecTests::TestTimestampLowWrapIncrementsTimestampHigh()
{
    MidiBleMidi1::PacketDecoder decoder;

    // The increment is never transmitted. A timestampLow smaller than the preceding one is the
    // only signal that the 13 bit value crossed a 128 ms boundary inside this packet.
    auto segments = Decode(decoder, {
        0x80,           // timestampHigh 0
        0xFF, 0x90, 0x40, 0x7F,     // timestampLow 127 -> 127 ms
        0x80, 0x90, 0x41, 0x7F });  // timestampLow 0   -> 128 ms

    VERIFY_ARE_EQUAL((size_t)2, segments.size());
    VERIFY_ARE_EQUAL((uint32_t)0, segments[0].RelativeMilliseconds);
    VERIFY_ARE_EQUAL((uint32_t)1, segments[1].RelativeMilliseconds);
}

void BleMidi1CodecTests::TestRunningStatusWithTimestampIsExpanded()
{
    MidiBleMidi1::PacketDecoder decoder;

    auto segments = Decode(decoder, {
        Header(0),
        Timestamp(0), 0x90, 0x40, 0x7F,
        Timestamp(1), 0x41, 0x7F });

    // the omitted status byte is restored, because everything downstream expects whole messages
    VerifyBytes({ 0x90, 0x40, 0x7F, 0x90, 0x41, 0x7F }, FlattenBytes(segments));
}

void BleMidi1CodecTests::TestRunningStatusWithoutTimestampInheritsTimestamp()
{
    MidiBleMidi1::PacketDecoder decoder;

    auto segments = Decode(decoder, {
        Header(0),
        Timestamp(4), 0x90, 0x40, 0x7F,
        0x41, 0x7F });

    VerifyBytes({ 0x90, 0x40, 0x7F, 0x90, 0x41, 0x7F }, FlattenBytes(segments));

    // no timestamp byte means the timestamp of the most recent preceding message applies
    VERIFY_ARE_EQUAL((size_t)1, segments.size());
}

void BleMidi1CodecTests::TestRunningStatusIsCanceledByEndOfPacket()
{
    MidiBleMidi1::PacketDecoder decoder;

    Decode(decoder, { Header(0), Timestamp(0), 0x90, 0x40, 0x7F });

    // the second packet's data bytes have no status to attach to and are dropped
    auto segments = Decode(decoder, { Header(0), Timestamp(1), 0x41, 0x7F });

    VERIFY_IS_TRUE(FlattenBytes(segments).empty());
}

void BleMidi1CodecTests::TestSystemCommonCancelsRunningStatus()
{
    MidiBleMidi1::PacketDecoder decoder;

    auto segments = Decode(decoder, {
        Header(0),
        Timestamp(0), 0x90, 0x40, 0x7F,
        Timestamp(1), 0xF3, 0x05,       // song select
        Timestamp(2), 0x41, 0x7F });    // nothing to expand this into

    VerifyBytes({ 0x90, 0x40, 0x7F, 0xF3, 0x05 }, FlattenBytes(segments));
}

void BleMidi1CodecTests::TestSystemRealTimeDoesNotCancelRunningStatus()
{
    MidiBleMidi1::PacketDecoder decoder;

    auto segments = Decode(decoder, {
        Header(0),
        Timestamp(0), 0x90, 0x40, 0x7F,
        Timestamp(1), 0xF8,             // timing clock
        Timestamp(2), 0x41, 0x7F });

    VerifyBytes({ 0x90, 0x40, 0x7F, 0xF8, 0x90, 0x41, 0x7F }, FlattenBytes(segments));
}

void BleMidi1CodecTests::TestSysExInSinglePacketIsDecoded()
{
    MidiBleMidi1::PacketDecoder decoder;

    auto segments = Decode(decoder, {
        Header(0),
        Timestamp(0), 0xF0, 0x7E, 0x00, 0x09, 0x01,
        Timestamp(1), 0xF7 });

    VerifyBytes({ 0xF0, 0x7E, 0x00, 0x09, 0x01, 0xF7 }, FlattenBytes(segments));
    VERIFY_IS_FALSE(decoder.IsInSysex());
}

void BleMidi1CodecTests::TestSysExSpanningPacketsIsDecoded()
{
    MidiBleMidi1::PacketDecoder decoder;

    std::vector<uint8_t> bytes;

    auto append = [&bytes](std::vector<MidiBleMidi1::DecodedSegment> const& segments)
        {
            auto flattened = FlattenBytes(segments);
            bytes.insert(bytes.end(), flattened.begin(), flattened.end());
        };

    append(Decode(decoder, { Header(0), Timestamp(0), 0xF0, 0x01, 0x02, 0x03 }));
    VERIFY_IS_TRUE(decoder.IsInSysex());

    // a continuation packet has a header byte but no timestamp byte, which is the only signal
    // that these are SysEx data bytes rather than the start of a message
    append(Decode(decoder, { Header(0), 0x04, 0x05, 0x06 }));
    VERIFY_IS_TRUE(decoder.IsInSysex());

    append(Decode(decoder, { Header(0), 0x07, Timestamp(3), 0xF7 }));
    VERIFY_IS_FALSE(decoder.IsInSysex());

    VerifyBytes({ 0xF0, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0xF7 }, bytes);
}

void BleMidi1CodecTests::TestSystemRealTimeInsideSysExDoesNotEndIt()
{
    MidiBleMidi1::PacketDecoder decoder;

    Decode(decoder, { Header(0), Timestamp(0), 0xF0, 0x01, 0x02 });

    auto segments = Decode(decoder, { Header(0), 0x03, Timestamp(1), 0xFE, 0x04 });

    VerifyBytes({ 0x03, 0xFE, 0x04 }, FlattenBytes(segments));
    VERIFY_IS_TRUE(decoder.IsInSysex());
}

void BleMidi1CodecTests::TestBuilderWritesHeaderAndTimestampBytes()
{
    MidiBleMidi1::PacketBuilder builder;
    builder.SetMaxPacketByteCount(20);

    std::vector<uint8_t> message{ 0x90, 0x40, 0x7F };
    builder.AppendMessage(message.data(), message.size(), 4660);

    auto packets = builder.TakePackets();

    VERIFY_ARE_EQUAL((size_t)1, packets.size());
    VerifyBytes({ Header(4660), Timestamp(4660), 0x90, 0x40, 0x7F }, packets[0]);
}

void BleMidi1CodecTests::TestBuilderPacksMultipleMessagesIntoOnePacket()
{
    MidiBleMidi1::PacketBuilder builder;
    builder.SetMaxPacketByteCount(20);

    std::vector<uint8_t> first{ 0x90, 0x40, 0x7F };
    std::vector<uint8_t> second{ 0x90, 0x41, 0x7F };

    builder.AppendMessage(first.data(), first.size(), 10);
    builder.AppendMessage(second.data(), second.size(), 12);

    auto packets = builder.TakePackets();

    VERIFY_ARE_EQUAL((size_t)1, packets.size());
    VerifyBytes({ Header(10), Timestamp(10), 0x90, 0x40, 0x7F, Timestamp(12), 0x90, 0x41, 0x7F }, packets[0]);
}

void BleMidi1CodecTests::TestBuilderNeverSplitsANonSysExMessage()
{
    MidiBleMidi1::PacketBuilder builder;
    builder.SetMaxPacketByteCount(20);

    std::vector<uint8_t> message{ 0x90, 0x40, 0x7F };

    // header plus four messages is 17 bytes, so the fifth cannot fit
    for (int i = 0; i < 6; i++)
    {
        builder.AppendMessage(message.data(), message.size(), 10);
    }

    auto packets = builder.TakePackets();

    VERIFY_ARE_EQUAL((size_t)2, packets.size());
    VERIFY_ARE_EQUAL((size_t)17, packets[0].size());
    VERIFY_ARE_EQUAL((size_t)9, packets[1].size());

    for (auto const& packet : packets)
    {
        VERIFY_IS_TRUE(packet.size() <= 20);
    }
}

void BleMidi1CodecTests::TestBuilderStartsNewPacketWhenTimestampHighChanges()
{
    MidiBleMidi1::PacketBuilder builder;
    builder.SetMaxPacketByteCount(20);

    std::vector<uint8_t> message{ 0x90, 0x40, 0x7F };

    // a packet carries a single timestampHigh, so crossing a 128 ms boundary needs a new one
    builder.AppendMessage(message.data(), message.size(), 127);
    builder.AppendMessage(message.data(), message.size(), 128);

    auto packets = builder.TakePackets();

    VERIFY_ARE_EQUAL((size_t)2, packets.size());
    VERIFY_ARE_EQUAL(Header(127), packets[0][0]);
    VERIFY_ARE_EQUAL(Header(128), packets[1][0]);
}

void BleMidi1CodecTests::TestBuilderSplitsSysExAcrossPacketsWithoutTimestampBytes()
{
    MidiBleMidi1::PacketBuilder builder;
    builder.SetMaxPacketByteCount(8);

    std::vector<uint8_t> start{ 0xF0, 0x01, 0x02, 0x03, 0x04, 0x05 };
    std::vector<uint8_t> more{ 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B };

    builder.AppendMessage(start.data(), start.size(), 0);
    builder.AppendMessage(more.data(), more.size(), 0);

    auto packets = builder.TakePackets();

    VERIFY_IS_TRUE(packets.size() >= 2);

    // the first packet carries the timestamp byte and the SysEx start byte
    VERIFY_ARE_EQUAL(Header(0), packets[0][0]);
    VERIFY_ARE_EQUAL(Timestamp(0), packets[0][1]);
    VERIFY_ARE_EQUAL((uint8_t)0xF0, packets[0][2]);

    // every continuation packet is a header byte followed immediately by data
    for (size_t i = 1; i < packets.size(); i++)
    {
        VERIFY_ARE_EQUAL(Header(0), packets[i][0]);
        VERIFY_IS_TRUE(packets[i].size() > 1);
        VERIFY_ARE_EQUAL(0, packets[i][1] & 0x80);
    }
}

void BleMidi1CodecTests::TestBuilderGivesSysExEndByteItsOwnTimestampByte()
{
    MidiBleMidi1::PacketBuilder builder;
    builder.SetMaxPacketByteCount(20);

    std::vector<uint8_t> start{ 0xF0, 0x7E, 0x00 };
    std::vector<uint8_t> end{ 0x09, 0x01, 0xF7 };

    builder.AppendMessage(start.data(), start.size(), 3);
    builder.AppendMessage(end.data(), end.size(), 3);

    auto packets = builder.TakePackets();

    VERIFY_ARE_EQUAL((size_t)1, packets.size());
    VerifyBytes({ Header(3), Timestamp(3), 0xF0, 0x7E, 0x00, 0x09, 0x01, Timestamp(3), 0xF7 }, packets[0]);
}

void BleMidi1CodecTests::TestBuilderGivesRealTimeInsideSysExItsOwnTimestampByte()
{
    MidiBleMidi1::PacketBuilder builder;
    builder.SetMaxPacketByteCount(20);

    std::vector<uint8_t> start{ 0xF0, 0x01, 0x02 };
    std::vector<uint8_t> clock{ 0xF8 };
    std::vector<uint8_t> more{ 0x03, 0xF7 };

    builder.AppendMessage(start.data(), start.size(), 1);
    builder.AppendMessage(clock.data(), clock.size(), 2);
    builder.AppendMessage(more.data(), more.size(), 3);

    auto packets = builder.TakePackets();

    VERIFY_ARE_EQUAL((size_t)1, packets.size());
    VerifyBytes({ Header(1), Timestamp(1), 0xF0, 0x01, 0x02, Timestamp(2), 0xF8, 0x03, Timestamp(3), 0xF7 }, packets[0]);
}

void BleMidi1CodecTests::TestRoundTripOfChannelMessages()
{
    MidiBleMidi1::PacketBuilder builder;
    builder.SetMaxPacketByteCount(20);

    std::vector<std::vector<uint8_t>> messages
    {
        { 0x90, 0x40, 0x7F },
        { 0xB0, 0x07, 0x64 },
        { 0xC1, 0x20 },
        { 0xE0, 0x00, 0x40 },
        { 0xF8 },
        { 0x80, 0x40, 0x00 },
    };

    for (auto const& message : messages)
    {
        builder.AppendMessage(message.data(), message.size(), 100);
    }

    std::vector<uint8_t> expected;

    for (auto const& message : messages)
    {
        expected.insert(expected.end(), message.begin(), message.end());
    }

    MidiBleMidi1::PacketDecoder decoder;
    std::vector<uint8_t> actual;

    for (auto const& packet : builder.TakePackets())
    {
        std::vector<MidiBleMidi1::DecodedSegment> segments;
        VERIFY_IS_TRUE(decoder.DecodePacket(packet.data(), packet.size(), segments));

        auto flattened = FlattenBytes(segments);
        actual.insert(actual.end(), flattened.begin(), flattened.end());
    }

    VerifyBytes(expected, actual);
}

void BleMidi1CodecTests::TestRoundTripOfSysExAcrossPackets()
{
    MidiBleMidi1::PacketBuilder builder;

    // small enough to force several continuation packets
    builder.SetMaxPacketByteCount(10);

    std::vector<uint8_t> expected;
    expected.push_back(0xF0);

    for (uint8_t i = 0; i < 40; i++)
    {
        expected.push_back(i);
    }

    expected.push_back(0xF7);

    // sent the way the UMP translator produces it: a few bytes at a time
    for (size_t i = 0; i < expected.size(); i += 6)
    {
        auto const count = (std::min)((size_t)6, expected.size() - i);
        builder.AppendMessage(expected.data() + i, count, 55);
    }

    MidiBleMidi1::PacketDecoder decoder;
    std::vector<uint8_t> actual;

    for (auto const& packet : builder.TakePackets())
    {
        VERIFY_IS_TRUE(packet.size() <= 10);

        std::vector<MidiBleMidi1::DecodedSegment> segments;
        VERIFY_IS_TRUE(decoder.DecodePacket(packet.data(), packet.size(), segments));

        auto flattened = FlattenBytes(segments);
        actual.insert(actual.end(), flattened.begin(), flattened.end());
    }

    VerifyBytes(expected, actual);
    VERIFY_IS_FALSE(decoder.IsInSysex());
}
