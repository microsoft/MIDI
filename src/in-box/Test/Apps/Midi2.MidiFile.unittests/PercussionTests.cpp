// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "PercussionTests.h"
#include "SmfTestFileBuilder.h"

#include "midi_file_smf_reader.h"

#include <windows.h>

#include <cstring>
#include <vector>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace midifile;
using namespace smftest;

namespace
{
    constexpr uint16_t TicksPerQuarterNote = 480;

    // Channels counted from 0, the way the file stores them.
    constexpr uint8_t Piano = 0;
    constexpr uint8_t Drums = PercussionChannelIndex;

    MidiSequence ReadTrack(_In_ TrackBuilder& track)
    {
        track.EndOfTrack(0);

        auto const bytes = BuildFile(0, TicksPerQuarterNote, { track });

        MidiSequence sequence{};
        auto const result = ParseStandardMidiFile(std::span<uint8_t const>{ bytes.data(), bytes.size() }, sequence, ReadLimits{});

        VERIFY_IS_TRUE(result.Succeeded());

        return sequence;
    }

    // A short note, so each one in a test is its own.
    void AddNote(_In_ TrackBuilder& track, _In_ uint8_t channel, _In_ uint8_t note)
    {
        track.NoteOn(0, channel, note, 100).NoteOff(120, channel, note, 0);
    }

    // The notes in the order they start, each as percussion or not.
    std::vector<bool> PercussionFlags(_In_ MidiSequence const& sequence)
    {
        std::vector<bool> flags{};

        for (auto const& note : sequence.Notes)
        {
            flags.push_back(note.IsPercussion);
        }

        return flags;
    }

    void VerifyFlags(_In_ MidiSequence const& sequence, _In_ std::vector<bool> const& expected)
    {
        auto const actual = PercussionFlags(sequence);

        VERIFY_ARE_EQUAL(expected.size(), actual.size());

        for (size_t index = 0; index < expected.size() && index < actual.size(); ++index)
        {
            Log::Comment(String().Format(L"note %d: expected %s", static_cast<int>(index), expected[index] ? L"drum" : L"melodic"));
            VERIFY_ARE_EQUAL(expected[index], actual[index]);
        }
    }

    // A Roland GS data set, with its checksum, for one byte at one address.
    std::vector<uint8_t> GsDataSet(_In_ uint8_t high, _In_ uint8_t middle, _In_ uint8_t low, _In_ uint8_t value)
    {
        auto const sum = static_cast<uint32_t>(high) + middle + low + value;
        auto const checksum = static_cast<uint8_t>((128 - (sum % 128)) % 128);

        return { 0x41, 0x10, 0x42, 0x12, high, middle, low, value, checksum, 0xF7 };
    }

    std::vector<uint8_t> XgParameterChange(_In_ uint8_t high, _In_ uint8_t middle, _In_ uint8_t low, _In_ uint8_t value)
    {
        return { 0x43, 0x10, 0x4C, high, middle, low, value, 0xF7 };
    }

    // Feeds the tracker one packet, the way a clip file's events carry them.
    void ObservePacket(_In_ PercussionTracker& tracker, _In_ std::vector<uint32_t> const& words)
    {
        std::vector<uint8_t> bytes(words.size() * sizeof(uint32_t));
        std::memcpy(bytes.data(), words.data(), bytes.size());

        SequenceEvent event{};
        event.Kind = EventKind::UniversalPacket;

        tracker.Observe(event, std::span<uint8_t const>{ bytes.data(), bytes.size() });
    }
}

void PercussionTests::TreatsChannelTenAsDrums()
{
    TrackBuilder track{};

    AddNote(track, Piano, 60);
    AddNote(track, Drums, 36);
    AddNote(track, 3, 64);

    VerifyFlags(ReadTrack(track), { false, true, false });
}

void PercussionTests::FollowsDrumKitBankSelects()
{
    TrackBuilder track{};

    // XG drum kit bank, then back to a normal voice.
    track.ControlChange(0, 2, 0x00, 127);
    AddNote(track, 2, 36);
    track.ControlChange(0, 2, 0x00, 0);
    AddNote(track, 2, 60);

    // General MIDI 2 rhythm bank.
    track.ControlChange(0, 4, 0x00, 120);
    AddNote(track, 4, 38);

    // Any other bank is a melodic one.
    track.ControlChange(0, 5, 0x00, 121);
    AddNote(track, 5, 62);

    VerifyFlags(ReadTrack(track), { true, false, true, false });
}

void PercussionTests::FollowsGsRhythmParts()
{
    TrackBuilder track{};

    // Block A is part 11, which listens on channel 11, stored as 10.
    track.SystemExclusive(0, GsDataSet(0x40, 0x1A, 0x15, 0x01));
    AddNote(track, 10, 36);

    // Block 2 is part 2, on channel 2, stored as 1. Map 2 is drums as well.
    track.SystemExclusive(0, GsDataSet(0x40, 0x12, 0x15, 0x02));
    AddNote(track, 1, 38);

    // Off again.
    track.SystemExclusive(0, GsDataSet(0x40, 0x1A, 0x15, 0x00));
    AddNote(track, 10, 60);

    VerifyFlags(ReadTrack(track), { true, true, false });
}

void PercussionTests::FollowsXgPartMode()
{
    TrackBuilder track{};

    track.SystemExclusive(0, XgParameterChange(0x08, 0x04, 0x07, 0x01));
    AddNote(track, 4, 36);

    track.SystemExclusive(0, XgParameterChange(0x08, 0x04, 0x07, 0x00));
    AddNote(track, 4, 60);

    VerifyFlags(ReadTrack(track), { true, false });
}

void PercussionTests::GoesBackToChannelTenOnAReset()
{
    TrackBuilder track{};

    // GM System On
    track.ControlChange(0, 2, 0x00, 127);
    AddNote(track, 2, 36);
    track.SystemExclusive(0, { 0x7E, 0x7F, 0x09, 0x01, 0xF7 });
    AddNote(track, 2, 60);

    // GS reset
    track.ControlChange(0, 3, 0x00, 120);
    AddNote(track, 3, 36);
    track.SystemExclusive(0, GsDataSet(0x40, 0x00, 0x7F, 0x00));
    AddNote(track, 3, 60);

    // XG System On
    track.ControlChange(0, 5, 0x00, 127);
    AddNote(track, 5, 36);
    track.SystemExclusive(0, XgParameterChange(0x00, 0x00, 0x7E, 0x00));
    AddNote(track, 5, 60);

    VerifyFlags(ReadTrack(track), { true, false, true, false, true, false });
}

void PercussionTests::NeverTreatsChannelTenAsMelodic()
{
    TrackBuilder track{};

    // Every way of turning drums off, aimed at channel 10.
    track.ControlChange(0, Drums, 0x00, 0);
    AddNote(track, Drums, 36);

    track.SystemExclusive(0, GsDataSet(0x40, 0x10, 0x15, 0x00));
    AddNote(track, Drums, 38);

    track.SystemExclusive(0, XgParameterChange(0x08, Drums, 0x07, 0x00));
    AddNote(track, Drums, 42);

    VerifyFlags(ReadTrack(track), { true, true, true });
}

void PercussionTests::FollowsMidi2ProgramChangeBanks()
{
    PercussionTracker tracker{};

    VERIFY_IS_TRUE(tracker.IsPercussion(Drums));
    VERIFY_IS_FALSE(tracker.IsPercussion(6));

    // MIDI 2.0 program change on channel 7 (stored 6), bank valid, bank MSB 127.
    ObservePacket(tracker, { 0x40C60001u, 0x00007F00u });
    VERIFY_IS_TRUE(tracker.IsPercussion(6));

    // The same without the bank valid flag changes nothing.
    ObservePacket(tracker, { 0x40C60000u, 0x00000000u });
    VERIFY_IS_TRUE(tracker.IsPercussion(6));

    // Bank MSB 0, valid: melodic again.
    ObservePacket(tracker, { 0x40C60001u, 0x00000000u });
    VERIFY_IS_FALSE(tracker.IsPercussion(6));

    // MIDI 1.0 bank select carried in a packet, channel 8 (stored 7), value 120.
    ObservePacket(tracker, { 0x20B70078u });
    VERIFY_IS_TRUE(tracker.IsPercussion(7));

    // MIDI 2.0 control change 0 with a 32 bit value whose top seven bits are 127.
    ObservePacket(tracker, { 0x40B30000u, 0xFE000000u });
    VERIFY_IS_TRUE(tracker.IsPercussion(3));

    // Several packets stored as one event, the way an application can add them. Each is read.
    ObservePacket(tracker, { 0x20B10000u | 127u, 0x40C40001u, 0x00007800u, 0x20B30000u });
    VERIFY_IS_TRUE(tracker.IsPercussion(1));
    VERIFY_IS_TRUE(tracker.IsPercussion(4));
    VERIFY_IS_FALSE(tracker.IsPercussion(3));

    VERIFY_ARE_EQUAL(
        static_cast<int>((1u << 1) | (1u << 4) | (1u << 7) | (1u << PercussionChannelIndex)),
        static_cast<int>(tracker.PercussionChannels()));
}

void PercussionTests::IgnoresShortAndForeignMessages()
{
    PercussionTracker tracker{};

    SequenceEvent event{};
    event.Kind = EventKind::SystemExclusive;

    // Cut short partway through a GS message, a different Roland model, and a stray byte.
    std::vector<std::vector<uint8_t>> messages{
        { 0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x1A },
        { 0xF0, 0x41, 0x10, 0x16, 0x12, 0x40, 0x1A, 0x15, 0x01, 0x10, 0xF7 },
        { 0xF0 },
        { 0x00, 0x41, 0x10, 0x42, 0x12, 0x40, 0x1A, 0x15, 0x01, 0x10, 0xF7 } };

    for (auto const& message : messages)
    {
        tracker.Observe(event, std::span<uint8_t const>{ message.data(), message.size() });
    }

    for (uint8_t channel = 0; channel < 16; ++channel)
    {
        VERIFY_ARE_EQUAL(channel == Drums, tracker.IsPercussion(channel));
    }

    // A packet too short to read, and a channel number out of range.
    event.Kind = EventKind::UniversalPacket;
    uint8_t const shortPacket[]{ 0x01, 0x02 };
    tracker.Observe(event, std::span<uint8_t const>{ shortPacket, sizeof(shortPacket) });

    VERIFY_IS_FALSE(tracker.IsPercussion(16));
    VERIFY_IS_FALSE(tracker.IsPercussion(255));
}
