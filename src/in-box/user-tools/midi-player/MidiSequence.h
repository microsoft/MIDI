// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <sal.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

// The format-neutral shape of a MIDI sequence, and everything derived from a file at load time.
//
// Nothing here depends on WinRT, on the Windows MIDI Services SDK or on XAML, so the readers and
// this model can be unit tested on their own, and a future host which is not a XAML window can
// reuse them. Translation to Universal MIDI Packets happens above this layer.
//
// Standard MIDI Files and MIDI Clip Files both land here. Anything a display needs which would be
// expensive to work out while drawing - paired notes, the tempo map, bar positions - is built once
// while reading and stored, rather than derived per frame.
namespace midifile
{
    inline constexpr uint8_t ChannelNone = 0xFF;
    inline constexpr uint16_t TrackNone = 0xFFFF;

    inline constexpr uint16_t DefaultTicksPerQuarterNote = 480;
    inline constexpr uint32_t DefaultMicrosecondsPerQuarterNote = 500000;   // 120 beats per minute

    inline constexpr uint32_t MinimumMicrosecondsPerQuarterNote = 1000;     // 60000 BPM, absurd but finite
    inline constexpr uint32_t MaximumMicrosecondsPerQuarterNote = 0xFFFFFF; // the three byte field's limit

    enum class SequenceFormat : int32_t
    {
        SingleTrack = 0,
        MultiTrack = 1,

        // Independent sequences in one file. Vanishingly rare, and never simultaneous, so the
        // reader lays the tracks out one after another rather than merging them.
        MultiSequence = 2
    };

    enum class EventKind : uint8_t
    {
        NoteOff = 0,
        NoteOn = 1,
        PolyphonicPressure = 2,
        ControlChange = 3,
        ProgramChange = 4,
        ChannelPressure = 5,
        PitchBend = 6,
        SystemExclusive = 7,
        SystemCommon = 8,
        SystemRealTime = 9
    };

    enum class TextKind : uint8_t
    {
        Text = 1,
        Copyright = 2,
        TrackName = 3,
        InstrumentName = 4,
        Lyric = 5,
        Marker = 6,
        CuePoint = 7,
        ProgramName = 8,        // RP-19
        DeviceName = 9,         // RP-19

        // Values from here up are not meta event types. A Standard MIDI File has no chord event,
        // so this one is carried in a manufacturer system exclusive. MIDI 2.0 has a real
        // Set Chord Name message, so a clip file will fill this in from that instead.
        ChordSymbol = 20
    };

    struct TimeDivision
    {
        // SMPTE division measures real time rather than musical time, so tempo does not apply.
        bool IsSmpte{ false };

        uint16_t TicksPerQuarterNote{ DefaultTicksPerQuarterNote };

        uint8_t FramesPerSecond{ 0 };   // 24, 25, 29 (meaning 29.97) or 30
        uint8_t TicksPerFrame{ 0 };

        double TicksPerSecond() const noexcept;
    };

    struct TempoChange
    {
        uint32_t Tick{ 0 };
        uint32_t MicrosecondsPerQuarterNote{ DefaultMicrosecondsPerQuarterNote };

        // Time from the start of the sequence to this tick, accumulated while reading so that
        // seeking does not have to walk the whole map.
        uint64_t MicrosecondsAtTick{ 0 };
    };

    struct TimeSignatureChange
    {
        uint32_t Tick{ 0 };
        uint8_t Numerator{ 4 };
        uint8_t DenominatorPowerOfTwo{ 2 };     // 2 means a quarter note
        uint8_t ClocksPerClick{ 24 };
        uint8_t ThirtySecondNotesPerQuarter{ 8 };

        uint32_t TicksPerBar{ 0 };
        uint32_t BarNumberAtTick{ 1 };          // bars are counted from 1, the way players show them
    };

    struct KeySignatureChange
    {
        uint32_t Tick{ 0 };
        int8_t Accidentals{ 0 };                // negative for flats
        bool IsMinor{ false };
    };

    struct TextEvent
    {
        uint32_t Tick{ 0 };
        uint16_t TrackIndex{ TrackNone };
        TextKind Kind{ TextKind::Text };
        std::string Text{};                     // UTF-8
    };

    // A note with both ends already found. Building this while reading is what makes a piano roll
    // possible: a file stores the two halves separately, and pairing them at paint time would mean
    // searching forward through every event on every frame.
    struct Note
    {
        uint32_t StartTick{ 0 };
        uint32_t EndTick{ 0 };
        uint16_t TrackIndex{ TrackNone };
        uint8_t Channel{ 0 };
        uint8_t NoteNumber{ 0 };
        uint8_t Velocity{ 0 };
    };

    // Bank and program travel together, because a player has to show them together and because
    // sending one without the other selects the wrong sound.
    struct ProgramChangeEvent
    {
        uint32_t Tick{ 0 };
        uint16_t TrackIndex{ TrackNone };
        uint8_t Channel{ 0 };
        uint8_t Program{ 0 };
        uint8_t BankMsb{ 0 };
        uint8_t BankLsb{ 0 };
    };

    // One playable message. The bytes live in the sequence's single blob, so a system exclusive
    // dump of any length costs no more per event than a note on.
    struct SequenceEvent
    {
        uint32_t Tick{ 0 };
        uint32_t ByteOffset{ 0 };
        uint32_t ByteCount{ 0 };
        uint16_t TrackIndex{ TrackNone };
        EventKind Kind{ EventKind::NoteOn };
        uint8_t Channel{ ChannelNone };
    };

    struct Track
    {
        std::string Name{};                     // UTF-8
        std::string InstrumentName{};
        std::string DeviceName{};               // RP-19, the port a track was written for
        uint16_t ChannelMask{ 0 };
        uint32_t NoteCount{ 0 };
        uint32_t EventCount{ 0 };
        uint32_t LastTick{ 0 };
    };

    struct BarPosition
    {
        uint32_t Bar{ 1 };                      // counted from 1
        uint32_t Beat{ 1 };                     // counted from 1
        uint32_t TicksIntoBeat{ 0 };
    };

    class MidiSequence
    {
    public:
        SequenceFormat Format{ SequenceFormat::MultiTrack };
        TimeDivision Division{};

        std::vector<Track> Tracks{};

        // Sorted by tick. Events at the same tick keep the order they were written in, because
        // that order is often the difference between a program change landing before or after the
        // note it was meant to change.
        std::vector<SequenceEvent> Events{};
        std::vector<uint8_t> EventBytes{};

        std::vector<Note> Notes{};                          // sorted by start tick
        std::vector<TempoChange> TempoMap{};                // always has an entry at tick 0
        std::vector<TimeSignatureChange> TimeSignatureMap{}; // always has an entry at tick 0
        std::vector<KeySignatureChange> KeySignatureMap{};
        std::vector<TextEvent> TextEvents{};
        std::vector<ProgramChangeEvent> ProgramChanges{};

        // Indexes into TextEvents of the chord symbols only, in tick order. Lyrics and markers
        // share that vector, so a chord lookup would otherwise have to scan past all of them.
        std::vector<uint32_t> ChordSymbolIndexes{};

        uint32_t LastTick{ 0 };
        uint64_t DurationMicroseconds{ 0 };
        uint16_t UsedChannelMask{ 0 };

        // Lowest and highest note in the file, so a display can fill its height with the range
        // actually played rather than all 128 notes.
        uint8_t LowestNote{ 0 };
        uint8_t HighestNote{ 127 };

        // Notes are sorted by START tick, so a note which began before a visible window can still
        // be sounding inside it. Backing the search up by this much finds every one of them.
        uint32_t LongestNoteTicks{ 0 };

        std::string Title{};                    // UTF-8, best available name for the sequence
        std::string Copyright{};

        bool IsEmpty() const noexcept { return Events.empty(); }

        std::span<uint8_t const> BytesOf(_In_ SequenceEvent const& event) const noexcept;

        uint64_t MicrosecondsAtTick(uint32_t tick) const noexcept;
        uint32_t TickAtMicroseconds(uint64_t microseconds) const noexcept;

        double BeatsPerMinuteAtTick(uint32_t tick) const noexcept;
        BarPosition BarPositionAtTick(uint32_t tick) const noexcept;
        TimeSignatureChange const& TimeSignatureAtTick(uint32_t tick) const noexcept;

        // Index of the first event at or after this tick, for starting playback part way in.
        size_t FirstEventIndexAtTick(uint32_t tick) const noexcept;

        // The chord symbol in effect at this tick, or null when the file has none or the first
        // one has not been reached. Points into TextEvents and lives as long as this sequence.
        TextEvent const* ChordSymbolAtTick(uint32_t tick) const noexcept;

        // How many notes each track has sounding at this instant. Resized to the track count, so
        // a caller reusing one vector across frames does not allocate.
        void CollectSoundingNoteCounts(uint32_t tick, _Inout_ std::vector<uint8_t>& counts) const noexcept;

        // Fills in the derived maps, totals and note pairing once the raw parts are populated.
        // A reader calls this last; nothing else should need it.
        void Finalize() noexcept;

        void Clear() noexcept;
    };
}
