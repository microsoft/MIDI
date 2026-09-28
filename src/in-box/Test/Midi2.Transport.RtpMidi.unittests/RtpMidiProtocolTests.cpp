// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "RtpMidiProtocolTests.h"

#include <cstdlib>
#include <deque>
#include <iterator>
#include <random>

using namespace RtpMidi;

#define CHECK(condition) VERIFY_IS_TRUE((condition))

#define CHECK_EQ(actual, expected) \
    do { auto const a_ = (actual); auto const e_ = (expected); VERIFY_IS_TRUE(a_ == e_, WEX::Common::String().Format(L"%hs == %hs, got %lld expected %lld", #actual, #expected, static_cast<long long>(a_), static_cast<long long>(e_))); } while (0)

namespace RtpMidiProtocolCases
{
    std::vector<uint8_t> Packet(uint16_t sequence, uint32_t timestamp, uint32_t ssrc, std::vector<uint8_t> const& payload, bool marker = false, uint8_t payloadType = RtpMidiPayloadType)
    {
        std::vector<uint8_t> out;
        PutU8(out, 0x80);
        PutU8(out, static_cast<uint8_t>((marker ? 0x80 : 0x00) | payloadType));
        PutU16(out, sequence);
        PutU32(out, timestamp);
        PutU32(out, ssrc);
        out.insert(out.end(), payload.begin(), payload.end());
        return out;
    }

    std::vector<uint8_t> Concatenate(std::vector<MidiEvent> const& events)
    {
        std::vector<uint8_t> out;
        for (auto const& event : events) out.insert(out.end(), event.Bytes.begin(), event.Bytes.end());
        return out;
    }

    // ------------------------------------------------------------------------------------------
    // AppleMIDI session messages
    // ------------------------------------------------------------------------------------------

    void TestInvitationRoundTrip()
    {
        std::string const name = "Pete\xE2\x80\x99s MacBook Pro";    // curly apostrophe, as macOS names it
        auto const bytes = BuildInvitation(AppleMidiCommand::Invitation, 0x11223344, 0xAABBCCDD, name);

        CHECK_EQ(bytes.size(), 16 + name.size() + 1);
        CHECK_EQ(bytes[0], 0xFF);
        CHECK_EQ(bytes[2], 'I');
        CHECK_EQ(bytes[3], 'N');

        auto const parsed = ParseInvitation(bytes.data(), bytes.size());
        CHECK(parsed.has_value());
        CHECK(parsed->Command == AppleMidiCommand::Invitation);
        CHECK_EQ(parsed->ProtocolVersion, 2u);
        CHECK_EQ(parsed->InitiatorToken, 0x11223344u);
        CHECK_EQ(parsed->Ssrc, 0xAABBCCDDu);
        CHECK(parsed->Name == name);

        auto const bye = BuildInvitation(AppleMidiCommand::EndSession, 1, 2, "ignored");
        CHECK_EQ(bye.size(), 16u);

        CHECK(!ParseInvitation(bytes.data(), 15).has_value());
    }

    void TestInvitationNameHandling()
    {
        auto bytes = BuildInvitation(AppleMidiCommand::Invitation, 1, 2, "abc");
        bytes.pop_back();   // no terminator
        auto parsed = ParseInvitation(bytes.data(), bytes.size());
        CHECK(parsed.has_value() && parsed->Name == "abc");

        // control characters and a broken UTF-8 sequence are dropped
        std::string dirty = "ok\x01" "\x1B" "go\xC3" "!" "\xE2\x82";
        auto dirtyBytes = BuildInvitation(AppleMidiCommand::Invitation, 1, 2, dirty);
        parsed = ParseInvitation(dirtyBytes.data(), dirtyBytes.size());
        CHECK(parsed.has_value() && parsed->Name == "okgo!");

        std::string const longName(1000, 'x');
        auto longBytes = BuildInvitation(AppleMidiCommand::Invitation, 1, 2, longName);
        CHECK(longBytes.size() <= 16 + AppleMidiMaxNameBytes);
    }

    void TestSynchronizationAndFeedback()
    {
        auto const ck = BuildSynchronization(0x01020304, 1, { 0x1111111122222222ull, 0x3333333344444444ull, 0 });
        CHECK_EQ(ck.size(), 36u);

        auto const parsed = ParseSynchronization(ck.data(), ck.size());
        CHECK(parsed.has_value());
        CHECK_EQ(parsed->Ssrc, 0x01020304u);
        CHECK_EQ(parsed->Count, 1);
        CHECK(parsed->Timestamps[0] == 0x1111111122222222ull);
        CHECK(parsed->Timestamps[1] == 0x3333333344444444ull);

        auto bad = ck;
        bad[8] = 3;
        CHECK(!ParseSynchronization(bad.data(), bad.size()).has_value());
        CHECK(!ParseSynchronization(ck.data(), 35).has_value());

        auto const rs = BuildReceiverFeedback(7, 0x00012345);
        auto const feedback = ParseReceiverFeedback(rs.data(), rs.size());
        CHECK(feedback.has_value() && feedback->SequenceField == 0x00012345u);

        AppleMidiCommand command{};
        uint8_t const unknown[] = { 0xFF, 0xFF, 'Z', 'Z', 0, 0, 0, 0 };
        CHECK(!TryGetAppleMidiCommand(unknown, sizeof(unknown), command));

        auto const rtp = Packet(1, 2, 3, { 0x00 });
        CHECK(!TryGetAppleMidiCommand(rtp.data(), rtp.size(), command));
    }

    // ------------------------------------------------------------------------------------------
    // RTP header
    // ------------------------------------------------------------------------------------------

    void TestRtpHeader()
    {
        auto const basic = Packet(0x1234, 0xDEADBEEF, 0xCAFEF00D, { 0x03, 0x90, 0x3C, 0x64 }, true);

        RtpHeader header{};
        CHECK(ParseRtpHeader(basic.data(), basic.size(), header));
        CHECK(header.Marker);
        CHECK_EQ(header.PayloadType, RtpMidiPayloadType);
        CHECK_EQ(header.SequenceNumber, 0x1234);
        CHECK_EQ(header.Timestamp, 0xDEADBEEFu);
        CHECK_EQ(header.Ssrc, 0xCAFEF00Du);
        CHECK_EQ(header.PayloadOffset, 12u);
        CHECK_EQ(header.PayloadSize, 4u);

        // two CSRCs, a one-word extension, three bytes of padding
        std::vector<uint8_t> complex = { 0xB2, 0x61, 0, 1, 0, 0, 0, 2, 0, 0, 0, 3 };
        complex.insert(complex.end(), { 1, 1, 1, 1, 2, 2, 2, 2 });
        complex.insert(complex.end(), { 0xBE, 0xDE, 0x00, 0x01, 9, 9, 9, 9 });
        complex.insert(complex.end(), { 0x03, 0x90, 0x3C, 0x64 });
        complex.insert(complex.end(), { 0, 0, 3 });

        CHECK(ParseRtpHeader(complex.data(), complex.size(), header));
        CHECK_EQ(header.PayloadOffset, 28u);
        CHECK_EQ(header.PayloadSize, 4u);

        auto badVersion = basic;
        badVersion[0] = 0x40;
        CHECK(!ParseRtpHeader(badVersion.data(), badVersion.size(), header));

        auto badPadding = basic;
        badPadding[0] |= 0x20;
        badPadding.back() = 200;
        CHECK(!ParseRtpHeader(badPadding.data(), badPadding.size(), header));

        std::vector<uint8_t> shortCsrc = { 0x8F, 0x61, 0, 1, 0, 0, 0, 2, 0, 0, 0, 3, 1, 2 };
        CHECK(!ParseRtpHeader(shortCsrc.data(), shortCsrc.size(), header));
    }

    // ------------------------------------------------------------------------------------------
    // MIDI command section
    // ------------------------------------------------------------------------------------------

    void TestDecodeRunningStatusAndDeltas()
    {
        CommandSectionDecoder decoder;
        DecodedPacket packet;

        // Note On, delta 10, running status, delta 128 (two octets), running status
        auto const datagram = Packet(1, 1000, 5, { 0x0B, 0x90, 0x3C, 0x64, 0x0A, 0x3E, 0x64, 0x81, 0x00, 0x40, 0x64, 0x00 });
        CHECK(decoder.Decode(datagram.data(), datagram.size(), packet) == DecodeStatus::Ok);

        CHECK_EQ(packet.Events.size(), 3u);
        CHECK_EQ(packet.MalformedCommands, 0u);
        if (packet.Events.size() == 3)
        {
            CHECK(packet.Events[0].Bytes == (std::vector<uint8_t>{ 0x90, 0x3C, 0x64 }));
            CHECK_EQ(packet.Events[0].Timestamp, 1000u);
            CHECK(packet.Events[1].Bytes == (std::vector<uint8_t>{ 0x90, 0x3E, 0x64 }));
            CHECK_EQ(packet.Events[1].Timestamp, 1010u);
            CHECK(packet.Events[2].Bytes == (std::vector<uint8_t>{ 0x90, 0x40, 0x64 }));
            CHECK_EQ(packet.Events[2].Timestamp, 1138u);
        }
    }

    void TestDecodeFirstDeltaAndRealTime()
    {
        CommandSectionDecoder decoder;
        DecodedPacket packet;

        // Z set: the first command has a delta. Real time does not cancel running status.
        auto const datagram = Packet(1, 50, 5, { 0x28, 0x05, 0x90, 0x3C, 0x64, 0x00, 0xF8, 0x00, 0x3E });
        auto withLastByte = datagram;
        withLastByte.push_back(0x64);
        withLastByte[12] = 0x29;

        CHECK(decoder.Decode(withLastByte.data(), withLastByte.size(), packet) == DecodeStatus::Ok);
        CHECK_EQ(packet.Events.size(), 3u);
        if (packet.Events.size() == 3)
        {
            CHECK_EQ(packet.Events[0].Timestamp, 55u);
            CHECK(packet.Events[1].Bytes == (std::vector<uint8_t>{ 0xF8 }));
            CHECK(packet.Events[2].Bytes == (std::vector<uint8_t>{ 0x90, 0x3E, 0x64 }));
        }

        // system common cancels running status, so the data bytes after it are an error
        auto const canceled = Packet(2, 0, 5, { 0x09, 0x90, 0x3C, 0x64, 0x00, 0xF1, 0x20, 0x00, 0x3E, 0x64 });
        CHECK(decoder.Decode(canceled.data(), canceled.size(), packet) == DecodeStatus::Ok);
        CHECK_EQ(packet.Events.size(), 2u);
        CHECK_EQ(packet.MalformedCommands, 1u);

        // a trailing delta with no command after it is legal
        auto const trailing = Packet(3, 0, 5, { 0x04, 0x90, 0x3C, 0x64, 0x05 });
        CHECK(decoder.Decode(trailing.data(), trailing.size(), packet) == DecodeStatus::Ok);
        CHECK_EQ(packet.Events.size(), 1u);
        CHECK_EQ(packet.MalformedCommands, 0u);
    }

    void TestDecodeSysEx()
    {
        CommandSectionDecoder decoder;
        DecodedPacket packet;
        std::vector<uint8_t> stream;

        auto const verbatim = Packet(1, 0, 5, { 0x06, 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 });
        CHECK(decoder.Decode(verbatim.data(), verbatim.size(), packet) == DecodeStatus::Ok);
        CHECK(Concatenate(packet.Events) == (std::vector<uint8_t>{ 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 }));
        CHECK(!decoder.SysExOpen());

        // first, middle and last segments in three packets
        auto const first = Packet(2, 0, 5, { 0x04, 0xF0, 0x01, 0x02, 0xF0 });
        auto const middle = Packet(3, 0, 5, { 0x04, 0xF7, 0x03, 0x04, 0xF0 });
        auto const last = Packet(4, 0, 5, { 0x03, 0xF7, 0x05, 0xF7 });

        decoder.Decode(first.data(), first.size(), packet);
        stream = Concatenate(packet.Events);
        CHECK(decoder.SysExOpen());
        decoder.Decode(middle.data(), middle.size(), packet);
        auto part = Concatenate(packet.Events);
        stream.insert(stream.end(), part.begin(), part.end());
        decoder.Decode(last.data(), last.size(), packet);
        part = Concatenate(packet.Events);
        stream.insert(stream.end(), part.begin(), part.end());

        CHECK(stream == (std::vector<uint8_t>{ 0xF0, 0x01, 0x02, 0x03, 0x04, 0x05, 0xF7 }));
        CHECK(!decoder.SysExOpen());

        // cancel closes the transfer
        decoder.Decode(first.data(), first.size(), packet);
        auto const cancel = Packet(5, 0, 5, { 0x02, 0xF7, 0xF4 });
        decoder.Decode(cancel.data(), cancel.size(), packet);
        CHECK(Concatenate(packet.Events) == (std::vector<uint8_t>{ 0xF7 }));
        CHECK(!decoder.SysExOpen());

        // a dropped F7 is marked with F5 and becomes an ordinary F7
        auto const dropped = Packet(6, 0, 5, { 0x08, 0xF0, 0x01, 0x02, 0xF5, 0x00, 0x90, 0x3C, 0x64 });
        decoder.Decode(dropped.data(), dropped.size(), packet);
        CHECK(Concatenate(packet.Events) == (std::vector<uint8_t>{ 0xF0, 0x01, 0x02, 0xF7, 0x90, 0x3C, 0x64 }));

        // a middle segment with no start is dropped, not delivered
        decoder.Decode(middle.data(), middle.size(), packet);
        CHECK(packet.Events.empty());
        CHECK_EQ(packet.DroppedSysExSegments, 1u);
    }

    void TestDecodeLongHeaderAndJournalOffset()
    {
        CommandSectionDecoder decoder;
        DecodedPacket packet;

        std::vector<uint8_t> list;
        for (int i = 0; i < 100; i++)
        {
            if (i > 0) list.push_back(0x00);
            list.insert(list.end(), { 0x90, static_cast<uint8_t>(i), 0x40 });
        }

        std::vector<uint8_t> payload = { static_cast<uint8_t>(0x80 | 0x40 | (list.size() >> 8)), static_cast<uint8_t>(list.size()) };
        payload.insert(payload.end(), list.begin(), list.end());
        payload.insert(payload.end(), { 0x80, 0x00, 0x05 });   // an empty journal

        auto const datagram = Packet(1, 0, 5, payload);
        CHECK(decoder.Decode(datagram.data(), datagram.size(), packet) == DecodeStatus::Ok);
        CHECK_EQ(packet.Events.size(), 100u);
        CHECK(packet.HasJournal);
        CHECK_EQ(packet.JournalSize, 3u);
        CHECK_EQ(packet.JournalOffset, 12u + 2u + list.size());
    }

    void TestDecodeRejectsMalformed()
    {
        CommandSectionDecoder decoder;
        DecodedPacket packet;

        auto const tooLong = Packet(1, 0, 5, { 0x05, 0x90, 0x3C });
        CHECK(decoder.Decode(tooLong.data(), tooLong.size(), packet) == DecodeStatus::Malformed);

        auto const unterminated = Packet(2, 0, 5, { 0x03, 0xF0, 0x01, 0x02 });
        CHECK(decoder.Decode(unterminated.data(), unterminated.size(), packet) == DecodeStatus::Ok);
        CHECK_EQ(packet.MalformedCommands, 1u);
        CHECK(packet.Events.empty());

        auto const badDelta = Packet(3, 0, 5, { 0x2A, 0x81, 0x81, 0x81, 0x81, 0x01, 0x90, 0x3C, 0x64, 0, 0 });
        CHECK(decoder.Decode(badDelta.data(), badDelta.size(), packet) == DecodeStatus::Ok);
        CHECK_EQ(packet.MalformedCommands, 1u);

        auto const truncated = Packet(4, 0, 5, { 0x02, 0x90, 0x3C });
        CHECK(decoder.Decode(truncated.data(), truncated.size(), packet) == DecodeStatus::Ok);
        CHECK_EQ(packet.MalformedCommands, 1u);

        auto const otherType = Packet(5, 0, 5, { 0x03, 0x90, 0x3C, 0x64 }, false, 96);
        CHECK(decoder.Decode(otherType.data(), otherType.size(), packet) == DecodeStatus::NotRtpMidi);

        auto const dataInStatusPosition = Packet(6, 0, 5, { 0x03, 0x90, 0x3C, 0x94 });
        CHECK(decoder.Decode(dataInStatusPosition.data(), dataInStatusPosition.size(), packet) == DecodeStatus::Ok);
        CHECK_EQ(packet.MalformedCommands, 1u);
    }

    void TestDecoderSurvivesRandomInput()
    {
        std::mt19937 random{ 12345 };
        CommandSectionDecoder decoder;

        for (int i = 0; i < 20000; i++)
        {
            std::vector<uint8_t> payload(random() % 64);
            for (auto& byte : payload) byte = static_cast<uint8_t>(random());

            auto const datagram = Packet(static_cast<uint16_t>(i), 0, 5, payload);
            DecodedPacket packet;
            decoder.Decode(datagram.data(), datagram.size(), packet);

            // every fragment the decoder produces must be something bytestream translation accepts
            for (auto const& event : packet.Events) CHECK(!event.Bytes.empty());

            RecoveryJournal journal;
            ParseRecoveryJournal(payload.data(), payload.size(), journal);

            AppleMidiCommand command{};
            TryGetAppleMidiCommand(payload.data(), payload.size(), command);
            ParseInvitation(payload.data(), payload.size());
            ParseSynchronization(payload.data(), payload.size());
        }
    }

    // ------------------------------------------------------------------------------------------
    // Outgoing MIDI list
    // ------------------------------------------------------------------------------------------

    std::vector<uint8_t> RoundTrip(CommandSectionEncoder& encoder, CommandSectionDecoder& decoder, uint16_t& sequence)
    {
        std::vector<uint8_t> stream;

        for (auto const& list : encoder.TakeLists())
        {
            auto const datagram = BuildRtpMidiPacket(sequence++, 0, 5, list);

            DecodedPacket packet;
            if (decoder.Decode(datagram.data(), datagram.size(), packet) != DecodeStatus::Ok) return {};

            auto const part = Concatenate(packet.Events);
            stream.insert(stream.end(), part.begin(), part.end());
        }

        return stream;
    }

    void TestEncoderBasics()
    {
        CommandSectionEncoder encoder;
        uint8_t const notes[] = { 0x90, 0x3C, 0x64, 0x80, 0x3C, 0x40 };
        encoder.Append(notes, sizeof(notes));

        auto lists = encoder.TakeLists();
        CHECK_EQ(lists.size(), 1u);
        CHECK(!lists.empty() && lists[0] == (std::vector<uint8_t>{ 0x90, 0x3C, 0x64, 0x00, 0x80, 0x3C, 0x40 }));

        // running status in the source comes out with the status restored
        uint8_t const running[] = { 0x90, 0x3C, 0x64, 0x3E, 0x64 };
        encoder.Append(running, sizeof(running));
        lists = encoder.TakeLists();
        CHECK(!lists.empty() && lists[0] == (std::vector<uint8_t>{ 0x90, 0x3C, 0x64, 0x00, 0x90, 0x3E, 0x64 }));

        // header: short form below 16 bytes, M bit clear, long form above
        auto const small = BuildRtpMidiPacket(1, 2, 3, lists[0]);
        CHECK_EQ(small[1], RtpMidiPayloadType);
        CHECK_EQ(small[12], 7);

        std::vector<uint8_t> big(300, 0x40);
        big[0] = 0xF0;
        auto const large = BuildRtpMidiPacket(1, 2, 3, big);
        CHECK_EQ(large[12], 0x81);
        CHECK_EQ(large[13], 0x2C);
    }

    void TestEncoderLargeSysEx()
    {
        CommandSectionEncoder encoder{ 1000 };
        CommandSectionDecoder decoder;
        uint16_t sequence = 0;

        std::vector<uint8_t> sysex(5000);
        sysex.front() = 0xF0;
        for (size_t i = 1; i + 1 < sysex.size(); i++) sysex[i] = static_cast<uint8_t>(i & 0x7F);
        sysex.back() = 0xF7;

        encoder.Append(sysex.data(), sysex.size());

        auto const lists = encoder.TakeLists();
        CHECK(lists.size() >= 5);
        for (auto const& list : lists) CHECK(list.size() <= 1000);

        CommandSectionEncoder again{ 1000 };
        again.Append(sysex.data(), sysex.size());
        CHECK(RoundTrip(again, decoder, sequence) == sysex);
    }

    void TestEncoderStreamsOpenSysEx()
    {
        CommandSectionEncoder encoder;
        CommandSectionDecoder decoder;
        uint16_t sequence = 0;

        std::vector<uint8_t> expected;
        std::vector<uint8_t> received;

        std::vector<uint8_t> piece = { 0xF0 };
        for (int i = 0; i < 100; i++) piece.push_back(static_cast<uint8_t>(i));
        expected.insert(expected.end(), piece.begin(), piece.end());
        encoder.Append(piece.data(), piece.size());
        auto part = RoundTrip(encoder, decoder, sequence);
        received.insert(received.end(), part.begin(), part.end());
        CHECK(decoder.SysExOpen());

        piece.assign(50, 0x11);
        expected.insert(expected.end(), piece.begin(), piece.end());
        encoder.Append(piece.data(), piece.size());
        part = RoundTrip(encoder, decoder, sequence);
        received.insert(received.end(), part.begin(), part.end());

        piece = { 0x22, 0x33, 0xF7 };
        expected.insert(expected.end(), piece.begin(), piece.end());
        encoder.Append(piece.data(), piece.size());
        part = RoundTrip(encoder, decoder, sequence);
        received.insert(received.end(), part.begin(), part.end());

        CHECK(received == expected);
        CHECK(!decoder.SysExOpen());

        // real time inside a SysEx stays in place, which is legal MIDI 1.0
        uint8_t const withClock[] = { 0xF0, 0x01, 0x02, 0xF8, 0x03, 0xF7 };
        encoder.Append(withClock, sizeof(withClock));
        CHECK(RoundTrip(encoder, decoder, sequence) == (std::vector<uint8_t>(std::begin(withClock), std::end(withClock))));

        // any other status ends the SysEx, as it would on a cable
        uint8_t const interrupted[] = { 0xF0, 0x01, 0x02, 0x90, 0x3C, 0x64 };
        encoder.Append(interrupted, sizeof(interrupted));
        CHECK(RoundTrip(encoder, decoder, sequence) == (std::vector<uint8_t>{ 0xF0, 0x01, 0x02, 0xF7, 0x90, 0x3C, 0x64 }));
    }

    // ------------------------------------------------------------------------------------------
    // Recovery journal
    // ------------------------------------------------------------------------------------------

    void TestJournalRoundTrip()
    {
        ChannelJournal piano{};
        piano.Channel = 0;
        piano.NoteOns = { JournalNoteOn{ 60, 100, true, false }, JournalNoteOn{ 67, 90, false, true } };
        piano.NoteOffs = { 62, 64, 127 };
        piano.NoteOffInPreviousPacket = true;

        ChannelJournal drums{};
        drums.Channel = 9;
        drums.HasProgram = true;
        drums.Program = 5;
        drums.HasBank = true;
        drums.BankMsb = 1;
        drums.BankLsb = 2;
        drums.Controllers = { JournalController{ 64, true, 127, false, 0 }, JournalController{ 7, true, 100, false, 0 } };
        drums.HasPitchWheel = true;
        drums.PitchLsb = 0x11;
        drums.PitchMsb = 0x40;

        auto const bytes = BuildRecoveryJournal(0xFFF0, { piano, drums });

        // anything from the previous packet clears S all the way up
        CHECK_EQ(bytes[0] & 0x80, 0);

        RecoveryJournal parsed{};
        CHECK(ParseRecoveryJournal(bytes.data(), bytes.size(), parsed));
        CHECK_EQ(parsed.CheckpointSequence, 0xFFF0);
        CHECK(parsed.HasChannelJournals);
        CHECK_EQ(parsed.Channels.size(), 2u);

        if (parsed.Channels.size() == 2)
        {
            auto const& a = parsed.Channels[0];
            CHECK_EQ(a.Channel, 0);
            CHECK_EQ(a.NoteOns.size(), 2u);
            CHECK(a.NoteOns.size() == 2 && a.NoteOns[0].Note == 60 && a.NoteOns[0].Velocity == 100 && a.NoteOns[0].PlayHint);
            CHECK(a.NoteOffs == (std::vector<uint8_t>{ 62, 64, 127 }));

            auto const& b = parsed.Channels[1];
            CHECK_EQ(b.Channel, 9);
            CHECK(b.HasProgram && b.Program == 5 && b.HasBank && b.BankMsb == 1 && b.BankLsb == 2);
            CHECK_EQ(b.Controllers.size(), 2u);
            CHECK(b.Controllers.size() == 2 && b.Controllers[0].Number == 64 && b.Controllers[0].Value == 127);
            CHECK(b.HasPitchWheel && b.PitchLsb == 0x11 && b.PitchMsb == 0x40);
        }

        // an empty journal is just its three-byte header
        auto const empty = BuildRecoveryJournal(5, {});
        CHECK_EQ(empty.size(), 3u);
        CHECK(ParseRecoveryJournal(empty.data(), empty.size(), parsed) && parsed.Channels.empty());
    }

    void TestJournalParserSkipsAndRejects()
    {
        // system journal (4 bytes), then a channel journal with M, E, T and A chapters
        std::vector<uint8_t> bytes = { 0xE0, 0x00, 0x10 };
        bytes.insert(bytes.end(), { 0x84, 0x04, 0x01, 0x02 });

        std::vector<uint8_t> chapters;
        chapters.insert(chapters.end(), { 0x80, 0x04, 0x00, 0x00 });   // M, LENGTH 4
        chapters.insert(chapters.end(), { 0x80, 0x3C, 0x05 });         // E, one log
        chapters.insert(chapters.end(), { 0x40 });                     // T
        chapters.insert(chapters.end(), { 0x80, 0x3C, 0x10 });         // A, one log

        size_t const length = 3 + chapters.size();
        bytes.insert(bytes.end(), { static_cast<uint8_t>(0x80 | (3 << 3)), static_cast<uint8_t>(length), static_cast<uint8_t>(ChapterM | ChapterE | ChapterT | ChapterA) });
        bytes.insert(bytes.end(), chapters.begin(), chapters.end());

        RecoveryJournal parsed{};
        CHECK(ParseRecoveryJournal(bytes.data(), bytes.size(), parsed));
        CHECK(parsed.HasSystemJournal);
        CHECK_EQ(parsed.Channels.size(), 1u);
        CHECK(parsed.Channels.size() == 1 && parsed.Channels[0].Channel == 3 && parsed.Channels[0].HasChannelPressure);

        auto tooLong = bytes;
        tooLong[8] = 0xFF;
        CHECK(!ParseRecoveryJournal(tooLong.data(), tooLong.size(), parsed));

        // chapter N with LOW above HIGH, other than the two empty encodings, is invalid
        std::vector<uint8_t> badN = { 0xA0, 0x00, 0x01, 0x80, 0x05, ChapterN, 0x80, 0x52 };
        CHECK(!ParseRecoveryJournal(badN.data(), badN.size(), parsed));

        // 127 note logs: LOW 15 HIGH 1 keeps it from meaning 128
        ChannelJournal many{};
        for (uint8_t n = 0; n < 127; n++) many.NoteOns.push_back(JournalNoteOn{ n, 1, false, false });
        auto const manyBytes = BuildRecoveryJournal(1, { many });
        CHECK(ParseRecoveryJournal(manyBytes.data(), manyBytes.size(), parsed));
        CHECK(parsed.Channels.size() == 1 && parsed.Channels[0].NoteOns.size() == 127);
    }

    // ------------------------------------------------------------------------------------------
    // Session engine over a simulated network
    // ------------------------------------------------------------------------------------------

    PeerAddress Address(uint8_t last, uint16_t port)
    {
        PeerAddress address{};
        address.Family = 4;
        address.Bytes[0] = 10;
        address.Bytes[3] = last;
        address.Port = port;
        return address;
    }

    struct ReceivedMidi
    {
        uint64_t Local{ 0 };
        int64_t Lead{ 0 };
        bool Recovered{ false };
        std::vector<uint8_t> Bytes;
    };

    class SimulatedNetwork;

    class SimulatedNode : public ISessionHost
    {
    public:
        SimulatedNode(SimulatedNetwork& network, PeerAddress control, uint64_t clockBase) :
            Network(network), Control(control), Data(control.WithPort(static_cast<uint16_t>(control.Port + 1))), ClockBase(clockBase)
        {
        }

        void SendControl(PeerAddress const& to, std::vector<uint8_t> const& datagram) override;
        void SendData(PeerAddress const& to, std::vector<uint8_t> const& datagram) override;

        void OnMidi(Participant const&, uint64_t localTimestamp, int64_t lead, bool recovered, std::vector<uint8_t> const& bytes) override
        {
            Received.push_back(ReceivedMidi{ localTimestamp, lead, recovered, bytes });
        }

        void OnParticipantChanged(Participant const& participant) override
        {
            Changes.push_back(participant.State);
            if (participant.State == ParticipantState::Ended) LastEndReason = participant.Reason;
        }

        void Log(std::string const&) override {}

        uint64_t Clock() const;

        std::vector<uint8_t> ReceivedStream() const
        {
            std::vector<uint8_t> out;
            for (auto const& entry : Received) if (!entry.Recovered) out.insert(out.end(), entry.Bytes.begin(), entry.Bytes.end());
            return out;
        }

        SimulatedNetwork& Network;
        PeerAddress Control;
        PeerAddress Data;
        uint64_t ClockBase{ 0 };
        bool Alive{ true };
        std::unique_ptr<Session> Engine;
        std::vector<ReceivedMidi> Received;
        std::vector<ParticipantState> Changes;
        EndReason LastEndReason{ EndReason::None };
    };

    class SimulatedNetwork
    {
    public:
        struct InFlight
        {
            uint64_t DeliverAt{ 0 };
            PeerAddress From;
            PeerAddress To;
            std::vector<uint8_t> Bytes;
        };

        uint64_t Now{ 0 };
        uint64_t OneWayDelay{ 20 };     // 2 ms, a whole number of simulation steps
        std::function<bool(PeerAddress const&, PeerAddress const&, std::vector<uint8_t> const&)> Drop;
        std::function<uint64_t(PeerAddress const&, PeerAddress const&, std::vector<uint8_t> const&)> Delay;
        std::vector<std::unique_ptr<SimulatedNode>> Nodes;
        std::deque<InFlight> Queue;
        uint64_t Dropped{ 0 };

        SimulatedNode& AddNode(uint8_t last, uint64_t clockBase, SessionConfig config)
        {
            Nodes.push_back(std::make_unique<SimulatedNode>(*this, Address(last, 5004), clockBase));
            auto& node = *Nodes.back();
            node.Engine = std::make_unique<Session>(std::move(config), node, 0x1234567ull * last + clockBase);
            return node;
        }

        void Send(PeerAddress const& from, PeerAddress const& to, std::vector<uint8_t> const& bytes)
        {
            if (Drop && Drop(from, to, bytes)) { Dropped++; return; }

            auto const deliverAt = Now + (Delay ? Delay(from, to, bytes) : OneWayDelay);

            // keep the queue in delivery order; equal times stay in send order
            auto position = Queue.end();
            while (position != Queue.begin() && std::prev(position)->DeliverAt > deliverAt) --position;
            Queue.insert(position, InFlight{ deliverAt, from, to, bytes });
        }

        void Advance(uint64_t duration, uint64_t step = 10)
        {
            auto const end = Now + duration;

            while (Now < end)
            {
                Now += step;

                while (!Queue.empty() && Queue.front().DeliverAt <= Now)
                {
                    auto item = std::move(Queue.front());
                    Queue.pop_front();

                    for (auto& node : Nodes)
                    {
                        if (!node->Alive) continue;

                        if (item.To == node->Control) node->Engine->OnDatagram(true, item.From, item.Bytes.data(), item.Bytes.size(), node->Clock());
                        else if (item.To == node->Data) node->Engine->OnDatagram(false, item.From, item.Bytes.data(), item.Bytes.size(), node->Clock());
                    }
                }

                for (auto& node : Nodes)
                {
                    if (node->Alive) node->Engine->Tick(node->Clock());
                }
            }
        }
    };

    void SimulatedNode::SendControl(PeerAddress const& to, std::vector<uint8_t> const& datagram) { Network.Send(Control, to, datagram); }
    void SimulatedNode::SendData(PeerAddress const& to, std::vector<uint8_t> const& datagram) { Network.Send(Data, to, datagram); }
    uint64_t SimulatedNode::Clock() const { return ClockBase + Network.Now; }

    SessionConfig Config(char const* name)
    {
        SessionConfig config{};
        config.LocalName = name;
        config.Ssrc = static_cast<uint32_t>(std::hash<std::string>{}(name));
        return config;
    }

    // A starts near zero; B is about to wrap its 32-bit RTP timestamp.
    constexpr uint64_t ClockA = 12345;
    constexpr uint64_t ClockB = 0xFFFF0000ull;

    void TestSessionEstablishesAndSynchronizes()
    {
        SimulatedNetwork network;
        auto& a = network.AddNode(1, ClockA, Config("Windows"));
        auto& b = network.AddNode(2, ClockB, Config("Pete\xE2\x80\x99s MacBook Pro"));

        a.Engine->Invite(b.Control, a.Clock());
        network.Advance(20000);

        auto const aView = a.Engine->Snapshot();
        auto const bView = b.Engine->Snapshot();

        CHECK_EQ(aView.size(), 1u);
        CHECK_EQ(bView.size(), 1u);

        if (aView.size() == 1 && bView.size() == 1)
        {
            CHECK(aView[0].State == ParticipantState::Connected);
            CHECK(bView[0].State == ParticipantState::Connected);
            CHECK(aView[0].RemoteName == "Pete\xE2\x80\x99s MacBook Pro");
            CHECK(bView[0].RemoteName == "Windows");

            auto const skew = static_cast<int64_t>(ClockB - ClockA);
            CHECK(aView[0].HaveClockOffset && std::llabs(aView[0].ClockOffset - skew) <= 1);
            CHECK(bView[0].HaveClockOffset && std::llabs(bView[0].ClockOffset + skew) <= 1);
            CHECK_EQ(aView[0].Stats.RoundTripTicks, 2 * network.OneWayDelay);

            // 2 s: two fast exchanges and at least one of the 1.5 s ones
            CHECK(aView[0].Stats.SyncExchanges >= 3);

            // the invited side starts exchanges of its own once the first one is done
            CHECK(aView[0].Stats.SyncExchangesStartedByPeer >= 1);
            CHECK(bView[0].Stats.SyncExchangesStartedByPeer >= 1);
        }
    }

    // Like a Mac on Wi-Fi in power save: four of every five packets toward it wait 80 ms in the
    // air, the fifth does not. Taking the latest sample would be up to 40 ms wrong.
    void TestClockFilterRejectsAsymmetricDelay()
    {
        SimulatedNetwork network;
        auto& a = network.AddNode(1, ClockA, Config("A"));
        auto& b = network.AddNode(2, ClockB, Config("B"));

        uint32_t towardB = 0;
        network.Delay = [&](PeerAddress const& from, PeerAddress const& to, std::vector<uint8_t> const&) -> uint64_t
        {
            if (from == a.Data && to == b.Data) return (towardB++ % 5 == 0) ? 20 : 820;
            return 20;
        };

        a.Engine->Invite(b.Control, a.Clock());
        network.Advance(400000);

        auto const view = a.Engine->Snapshot();
        CHECK_EQ(view.size(), 1u);

        if (view.size() == 1)
        {
            auto const skew = static_cast<int64_t>(ClockB - ClockA);
            auto const& stats = view[0].Stats;

            CHECK(stats.SyncExchanges >= 10);
            CHECK(stats.ClockOffsetSpreadTicks >= 300);     // the slow samples really were off
            CHECK_EQ(stats.BestRoundTripTicks, 40u);
            CHECK(std::llabs(view[0].ClockOffset - skew) <= 1);
        }
    }

    // macOS starts CK exchanges from its end even when it was invited. B's engine does not do
    // that here, so the test plays macOS's part by hand.
    void TestPeerStartedSyncIsUsed()
    {
        SimulatedNetwork network;
        auto& a = network.AddNode(1, ClockA, Config("A"));

        auto bConfig = Config("B");
        bConfig.ResponderStartsSync = false;
        auto& b = network.AddNode(2, ClockB, bConfig);

        a.Engine->Invite(b.Control, a.Clock());
        network.Advance(3000);

        auto const before = a.Engine->Snapshot();
        CHECK_EQ(before.size(), 1u);
        if (before.size() != 1) return;

        uint64_t stampedT1 = 0;
        network.Drop = [&](PeerAddress const& from, PeerAddress const& to, std::vector<uint8_t> const& bytes)
        {
            auto const ck = ParseSynchronization(bytes.data(), bytes.size());
            if (from == a.Data && to == b.Data && ck && ck->Count == 1) stampedT1 = ck->Timestamps[1];
            return false;
        };

        auto const ssrc = b.Engine->Config().Ssrc;
        auto const t0 = b.Clock();

        network.Send(b.Data, a.Data, BuildSynchronization(ssrc, 0, { t0, 0, 0 }));
        network.Advance(100);
        network.Drop = nullptr;
        CHECK(stampedT1 != 0);

        // a CK2 that does not echo our t1 is ignored
        network.Send(b.Data, a.Data, BuildSynchronization(ssrc, 2, { t0, stampedT1 + 5, t0 + 40 }));
        network.Advance(100);
        CHECK_EQ(a.Engine->Snapshot()[0].Stats.SyncExchangesStartedByPeer, 0u);

        // the real one: CK1 came back 2 ms after it left
        network.Send(b.Data, a.Data, BuildSynchronization(ssrc, 2, { t0, stampedT1, t0 + 40 }));
        network.Advance(100);

        auto const after = a.Engine->Snapshot()[0];
        auto const skew = static_cast<int64_t>(ClockB - ClockA);

        CHECK_EQ(after.Stats.SyncExchangesStartedByPeer, 1u);
        CHECK_EQ(after.Stats.SyncExchanges, before[0].Stats.SyncExchanges + 1);
        CHECK(std::llabs(after.ClockOffset - skew) <= 1);

        // a replay of the same CK2 counts once
        network.Send(b.Data, a.Data, BuildSynchronization(ssrc, 2, { t0, stampedT1, t0 + 40 }));
        network.Advance(100);
        CHECK_EQ(a.Engine->Snapshot()[0].Stats.SyncExchangesStartedByPeer, 1u);
    }

    void TestSessionCarriesMidiBothWays()
    {
        SimulatedNetwork network;
        auto& a = network.AddNode(1, ClockA, Config("A"));
        auto& b = network.AddNode(2, ClockB, Config("B"));

        a.Engine->Invite(b.Control, a.Clock());
        network.Advance(20000);

        std::vector<uint8_t> sent = { 0x90, 0x3C, 0x64, 0xB0, 0x07, 0x64, 0xE0, 0x00, 0x40 };
        std::vector<uint8_t> sysex(3000);
        sysex.front() = 0xF0;
        for (size_t i = 1; i + 1 < sysex.size(); i++) sysex[i] = static_cast<uint8_t>(i % 100);
        sysex.back() = 0xF7;
        sent.insert(sent.end(), sysex.begin(), sysex.end());
        sent.insert(sent.end(), { 0x80, 0x3C, 0x40 });

        a.Engine->SendMidi(sent.data(), sent.size(), a.Clock());
        network.Advance(1000);

        CHECK(b.ReceivedStream() == sent);

        // the receiver sees the sender's timestamps mapped onto its clock: one delay in the past
        if (!b.Received.empty())
        {
            CHECK_EQ(b.Received.front().Lead, -static_cast<int64_t>(network.OneWayDelay));
        }

        // B to A crosses B's 32-bit timestamp wrap during the run
        network.Advance(70000);
        uint8_t const back[] = { 0x91, 0x40, 0x7F };
        b.Engine->SendMidi(back, sizeof(back), b.Clock());
        network.Advance(1000);

        CHECK(a.ReceivedStream() == (std::vector<uint8_t>(std::begin(back), std::end(back))));
        if (!a.Received.empty())
        {
            CHECK(std::llabs(a.Received.back().Lead + static_cast<int64_t>(network.OneWayDelay)) <= 1);
        }

        // B has sent feedback for what it received
        auto const aView = a.Engine->Snapshot();
        CHECK(!aView.empty() && aView[0].Stats.FeedbackReceived > 0);
    }

    bool IsDataPacketTo(PeerAddress const& to, SimulatedNode const& node, std::vector<uint8_t> const& bytes)
    {
        AppleMidiCommand command{};
        return to == node.Data && !TryGetAppleMidiCommand(bytes.data(), bytes.size(), command);
    }

    void RunLossTest(bool senderJournals, uint64_t& recovered, uint64_t& covered, bool& noteStillOn, bool& repairOrdered)
    {
        SimulatedNetwork network;

        auto configA = Config("A");
        configA.SendJournal = senderJournals;

        auto& a = network.AddNode(1, ClockA, configA);
        auto& b = network.AddNode(2, ClockB, Config("B"));

        a.Engine->Invite(b.Control, a.Clock());
        network.Advance(20000);

        uint8_t const on[] = { 0x90, 0x3C, 0x64 };
        a.Engine->SendMidi(on, sizeof(on), a.Clock());
        network.Advance(100);

        // lose exactly the packet carrying the Note Off
        int dataPacketsSeen = 0;
        network.Drop = [&](PeerAddress const&, PeerAddress const& to, std::vector<uint8_t> const& bytes)
        {
            return IsDataPacketTo(to, b, bytes) && dataPacketsSeen++ == 0;
        };

        uint8_t const off[] = { 0x80, 0x3C, 0x40 };
        a.Engine->SendMidi(off, sizeof(off), a.Clock());
        network.Advance(100);

        uint8_t const other[] = { 0xB0, 0x01, 0x10 };
        a.Engine->SendMidi(other, sizeof(other), a.Clock());
        network.Advance(100);

        auto const view = b.Engine->Snapshot();
        recovered = view.empty() ? 0 : view[0].Stats.RecoveredNoteOffs;
        covered = view.empty() ? 0 : view[0].Stats.LossEventsCovered;
        noteStillOn = !view.empty() && view[0].ActiveNotes[0][0x3C];

        // the repair must not be stamped after the first event of the packet that revealed the loss
        repairOrdered = false;
        for (size_t i = 0; i + 1 < b.Received.size(); i++)
        {
            if (b.Received[i].Recovered && !b.Received[i + 1].Recovered) repairOrdered = b.Received[i].Local <= b.Received[i + 1].Local;
        }
    }

    // Two connections to one host, each with its own stream. A shared encoder would carry B's
    // running status into C's stream and C's into B's.
    void TestSendToOneParticipant()
    {
        SimulatedNetwork network;
        auto& host = network.AddNode(1, ClockA, Config("Host"));
        auto& b = network.AddNode(2, ClockB, Config("B"));
        auto& c = network.AddNode(3, ClockA + 777, Config("C"));

        b.Engine->Invite(host.Control, b.Clock());
        c.Engine->Invite(host.Control, c.Clock());
        network.Advance(3000);

        auto const participants = host.Engine->Snapshot();
        CHECK_EQ(participants.size(), 2u);
        if (participants.size() != 2) return;

        uint32_t toB = 0;
        uint32_t toC = 0;
        for (auto const& p : participants) (p.RemoteName == "B" ? toB : toC) = p.Id;

        uint8_t const noteOn[] = { 0x90, 0x3C, 0x64 };
        uint8_t const noteOff[] = { 0x80, 0x3C, 0x40 };
        uint8_t const runningStatusNote[] = { 0x3E, 0x64 };

        CHECK(host.Engine->SendMidiTo(toB, noteOn, sizeof(noteOn), host.Clock()));
        CHECK(host.Engine->SendMidiTo(toC, noteOff, sizeof(noteOff), host.Clock()));
        CHECK(host.Engine->SendMidiTo(toB, runningStatusNote, sizeof(runningStatusNote), host.Clock()));
        CHECK(!host.Engine->SendMidiTo(9999, noteOn, sizeof(noteOn), host.Clock()));
        network.Advance(200);

        std::vector<uint8_t> const expectedB = { 0x90, 0x3C, 0x64, 0x90, 0x3E, 0x64 };
        std::vector<uint8_t> const expectedC = { 0x80, 0x3C, 0x40 };

        CHECK(b.ReceivedStream() == expectedB);
        CHECK(c.ReceivedStream() == expectedC);
    }

    void TestLossRecoveredFromJournal()
    {
        uint64_t recovered = 0, covered = 0;
        bool stillOn = true, ordered = false;
        RunLossTest(true, recovered, covered, stillOn, ordered);

        CHECK_EQ(covered, 1u);
        CHECK_EQ(recovered, 1u);
        CHECK(!stillOn);
        CHECK(ordered);
    }

    void TestLossWithoutJournalSilences()
    {
        uint64_t recovered = 0, covered = 0;
        bool stillOn = true, ordered = false;
        RunLossTest(false, recovered, covered, stillOn, ordered);

        CHECK_EQ(covered, 0u);
        CHECK_EQ(recovered, 1u);
        CHECK(!stillOn);
        CHECK(ordered);
    }

    void TestFeedbackTrimsJournal()
    {
        for (bool highBits : { false, true })
        {
            SimulatedNetwork network;

            auto configA = Config("A");
            configA.SendJournal = true;
            auto configB = Config("B");
            configB.FeedbackSequenceInHighBits = highBits;

            auto& a = network.AddNode(1, ClockA, configA);
            auto& b = network.AddNode(2, ClockB, configB);

            a.Engine->Invite(b.Control, a.Clock());
            network.Advance(20000);

            for (uint8_t note = 40; note < 50; note++)
            {
                uint8_t const on[] = { 0x90, note, 0x50 };
                a.Engine->SendMidi(on, sizeof(on), a.Clock());
                network.Advance(20);
            }

            network.Advance(30000);

            auto const view = a.Engine->Snapshot();
            CHECK(!view.empty() && view[0].FeedbackTrims > 0);

            size_t stillJournaled = 0;
            if (!view.empty())
            {
                for (auto const& sent : view[0].SentNotes[0]) if (sent.Touched) stillJournaled++;
            }

            CHECK_EQ(stillJournaled, 0u);
        }
    }

    void TestEndSessionAndRejection()
    {
        {
            SimulatedNetwork network;
            auto& a = network.AddNode(1, ClockA, Config("A"));
            auto& b = network.AddNode(2, ClockB, Config("B"));

            a.Engine->Invite(b.Control, a.Clock());
            network.Advance(20000);

            uint8_t const on[] = { 0x90, 0x3C, 0x64 };
            a.Engine->SendMidi(on, sizeof(on), a.Clock());
            network.Advance(100);

            a.Engine->EndAll(a.Clock());
            network.Advance(100);

            CHECK(b.LastEndReason == EndReason::RemoteEndedSession);
            CHECK(b.Engine->Snapshot().empty());

            // the note left on by the departed peer was turned off
            CHECK(!b.Received.empty() && b.Received.back().Recovered && b.Received.back().Bytes[0] == 0x80);
        }

        {
            SimulatedNetwork network;
            auto configB = Config("B");
            configB.AcceptInvitations = false;

            auto& a = network.AddNode(1, ClockA, Config("A"));
            auto& b = network.AddNode(2, ClockB, configB);
            (void)b;

            a.Engine->Invite(b.Control, a.Clock());
            network.Advance(1000);

            CHECK(a.LastEndReason == EndReason::Rejected);
            CHECK(a.Engine->Snapshot().empty());
        }
    }

    void TestHeldInvitationIsAnsweredLater()
    {
        {
            SimulatedNetwork network;

            auto admission = Admission::Hold;
            std::string askedAbout;

            auto configB = Config("B");
            configB.Admit = [&](std::string const& remoteName, PeerAddress const&) { askedAbout = remoteName; return admission; };

            auto& a = network.AddNode(1, ClockA, Config("A"));
            auto& b = network.AddNode(2, ClockB, configB);

            a.Engine->Invite(b.Control, a.Clock());
            network.Advance(35000);

            // no answer at all, so A keeps asking and nobody has a connection yet
            CHECK(askedAbout == "A");
            CHECK(b.Engine->Stats().InvitationsHeld >= 3);
            CHECK(b.Engine->Stats().RejectionsSent == 0);
            CHECK(b.Engine->Snapshot().empty());
            CHECK(a.LastEndReason == EndReason::None);
            CHECK_EQ(a.Engine->ConnectedCount(), 0u);

            // decided while A is still asking: its next ask gets in
            admission = Admission::Accept;
            network.Advance(20000);

            CHECK_EQ(a.Engine->ConnectedCount(), 1u);
            CHECK_EQ(b.Engine->ConnectedCount(), 1u);
        }

        {
            SimulatedNetwork network;

            auto configB = Config("B");
            configB.Admit = [](std::string const&, PeerAddress const&) { return Admission::Hold; };

            auto& a = network.AddNode(1, ClockA, Config("A"));
            auto& b = network.AddNode(2, ClockB, configB);

            a.Engine->Invite(b.Control, a.Clock());
            network.Advance(140000);

            // never decided: A gives up on its own, as it would with a host that is not there
            CHECK(a.LastEndReason == EndReason::NoAnswer);
            CHECK(a.Engine->Snapshot().empty());
            CHECK(b.Engine->Snapshot().empty());
        }
    }

    void TestTimeouts()
    {
        {
            SimulatedNetwork network;
            auto& a = network.AddNode(1, ClockA, Config("A"));

            a.Engine->Invite(Address(99, 5004), a.Clock());
            network.Advance(140000);

            CHECK(a.LastEndReason == EndReason::NoAnswer);
        }

        {
            SimulatedNetwork network;
            auto& a = network.AddNode(1, ClockA, Config("A"));
            auto& b = network.AddNode(2, ClockB, Config("B"));

            a.Engine->Invite(b.Control, a.Clock());
            network.Advance(20000);

            // the initiator goes away without a BY
            a.Alive = false;
            network.Advance(950000, 100);

            CHECK(b.LastEndReason == EndReason::SyncTimeout);
        }

        {
            SimulatedNetwork network;
            auto& a = network.AddNode(1, ClockA, Config("A"));
            auto& b = network.AddNode(2, ClockB, Config("B"));

            a.Engine->Invite(b.Control, a.Clock());
            network.Advance(20000);

            // the responder goes away without a BY
            b.Alive = false;
            network.Advance(400000, 100);

            CHECK(a.LastEndReason == EndReason::SyncTimeout);
        }
    }

    void TestReinvitationReplacesStaleParticipant()
    {
        SimulatedNetwork network;
        auto& a = network.AddNode(1, ClockA, Config("A"));
        auto& b = network.AddNode(2, ClockB, Config("B"));

        a.Engine->Invite(b.Control, a.Clock());
        network.Advance(20000);

        // A restarts: same host, same name, new SSRC, no BY sent for the old session
        auto config = Config("A");
        config.Ssrc ^= 0x5A5A5A5A;
        a.Engine = std::make_unique<Session>(config, a, 777);

        a.Engine->Invite(b.Control, a.Clock());
        network.Advance(20000);

        auto const view = b.Engine->Snapshot();
        CHECK_EQ(view.size(), 1u);
        CHECK(!view.empty() && view[0].RemoteSsrc == config.Ssrc && view[0].State == ParticipantState::Connected);
    }

    void TestSessionSurvivesGarbage()
    {
        SimulatedNetwork network;
        auto& a = network.AddNode(1, ClockA, Config("A"));
        auto& b = network.AddNode(2, ClockB, Config("B"));

        std::mt19937 random{ 99 };

        for (int i = 0; i < 20000; i++)
        {
            std::vector<uint8_t> junk(random() % 80);
            for (auto& byte : junk) byte = static_cast<uint8_t>(random());

            // make a good share of them look like session commands or RTP
            if (junk.size() >= 4 && (i % 3) == 0) { junk[0] = 0xFF; junk[1] = 0xFF; }
            if (junk.size() >= 2 && (i % 3) == 1) { junk[0] = 0x80; junk[1] = 0x61; }

            b.Engine->OnDatagram((i & 1) != 0, Address(66, static_cast<uint16_t>(6000 + (i % 50))), junk.data(), junk.size(), b.Clock());
        }

        network.Advance(1000);

        // rejections to strangers are rate limited
        CHECK(b.Engine->Stats().RejectionsSent <= 20);
        CHECK(b.Engine->Snapshot().size() <= 16);

        // and it still works afterwards
        a.Engine->Invite(b.Control, a.Clock());
        network.Advance(20000);

        bool connected = false;
        for (auto const& participant : a.Engine->Snapshot()) if (participant.State == ParticipantState::Connected) connected = true;
        CHECK(connected);
    }
}

namespace
{
    // Some cases check thousands of values, so only a failure is worth a line in the log
    void Run(void (*test)())
    {
        WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
        test();
    }
}

void RtpMidiProtocolTests::TestInvitationRoundTrip() { Run(RtpMidiProtocolCases::TestInvitationRoundTrip); }
void RtpMidiProtocolTests::TestInvitationNameHandling() { Run(RtpMidiProtocolCases::TestInvitationNameHandling); }
void RtpMidiProtocolTests::TestSynchronizationAndFeedback() { Run(RtpMidiProtocolCases::TestSynchronizationAndFeedback); }
void RtpMidiProtocolTests::TestRtpHeader() { Run(RtpMidiProtocolCases::TestRtpHeader); }
void RtpMidiProtocolTests::TestDecodeRunningStatusAndDeltas() { Run(RtpMidiProtocolCases::TestDecodeRunningStatusAndDeltas); }
void RtpMidiProtocolTests::TestDecodeFirstDeltaAndRealTime() { Run(RtpMidiProtocolCases::TestDecodeFirstDeltaAndRealTime); }
void RtpMidiProtocolTests::TestDecodeSysEx() { Run(RtpMidiProtocolCases::TestDecodeSysEx); }
void RtpMidiProtocolTests::TestDecodeLongHeaderAndJournalOffset() { Run(RtpMidiProtocolCases::TestDecodeLongHeaderAndJournalOffset); }
void RtpMidiProtocolTests::TestDecodeRejectsMalformed() { Run(RtpMidiProtocolCases::TestDecodeRejectsMalformed); }
void RtpMidiProtocolTests::TestDecoderSurvivesRandomInput() { Run(RtpMidiProtocolCases::TestDecoderSurvivesRandomInput); }
void RtpMidiProtocolTests::TestEncoderBasics() { Run(RtpMidiProtocolCases::TestEncoderBasics); }
void RtpMidiProtocolTests::TestEncoderLargeSysEx() { Run(RtpMidiProtocolCases::TestEncoderLargeSysEx); }
void RtpMidiProtocolTests::TestEncoderStreamsOpenSysEx() { Run(RtpMidiProtocolCases::TestEncoderStreamsOpenSysEx); }
void RtpMidiProtocolTests::TestJournalRoundTrip() { Run(RtpMidiProtocolCases::TestJournalRoundTrip); }
void RtpMidiProtocolTests::TestJournalParserSkipsAndRejects() { Run(RtpMidiProtocolCases::TestJournalParserSkipsAndRejects); }
void RtpMidiProtocolTests::TestSessionEstablishesAndSynchronizes() { Run(RtpMidiProtocolCases::TestSessionEstablishesAndSynchronizes); }
void RtpMidiProtocolTests::TestClockFilterRejectsAsymmetricDelay() { Run(RtpMidiProtocolCases::TestClockFilterRejectsAsymmetricDelay); }
void RtpMidiProtocolTests::TestPeerStartedSyncIsUsed() { Run(RtpMidiProtocolCases::TestPeerStartedSyncIsUsed); }
void RtpMidiProtocolTests::TestSessionCarriesMidiBothWays() { Run(RtpMidiProtocolCases::TestSessionCarriesMidiBothWays); }
void RtpMidiProtocolTests::TestSendToOneParticipant() { Run(RtpMidiProtocolCases::TestSendToOneParticipant); }
void RtpMidiProtocolTests::TestLossRecoveredFromJournal() { Run(RtpMidiProtocolCases::TestLossRecoveredFromJournal); }
void RtpMidiProtocolTests::TestLossWithoutJournalSilences() { Run(RtpMidiProtocolCases::TestLossWithoutJournalSilences); }
void RtpMidiProtocolTests::TestFeedbackTrimsJournal() { Run(RtpMidiProtocolCases::TestFeedbackTrimsJournal); }
void RtpMidiProtocolTests::TestEndSessionAndRejection() { Run(RtpMidiProtocolCases::TestEndSessionAndRejection); }
void RtpMidiProtocolTests::TestHeldInvitationIsAnsweredLater() { Run(RtpMidiProtocolCases::TestHeldInvitationIsAnsweredLater); }
void RtpMidiProtocolTests::TestTimeouts() { Run(RtpMidiProtocolCases::TestTimeouts); }
void RtpMidiProtocolTests::TestReinvitationReplacesStaleParticipant() { Run(RtpMidiProtocolCases::TestReinvitationReplacesStaleParticipant); }
void RtpMidiProtocolTests::TestSessionSurvivesGarbage() { Run(RtpMidiProtocolCases::TestSessionSurvivesGarbage); }
