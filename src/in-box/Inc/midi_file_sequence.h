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

    // Some files never mark a line break at all, so a line is cut once it stops being readable.
    inline constexpr size_t MaximumLyricLineLength = 96;

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
        SystemRealTime = 9,

        // The bytes are already Universal MIDI Packet words in native byte order, not MIDI 1.0
        // bytes, so nothing converts them on the way out. This is how a sequence carries a
        // message MIDI 1.0 cannot express - a 32 bit controller, a 16 bit velocity, a per note
        // controller - and it is what an SMF2 clip file needs somewhere to put.
        //
        // A standard MIDI file never produces one: the reader has no way to make one, so every
        // existing file behaves exactly as it did.
        UniversalPacket = 10
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

    enum class TimingMode : uint8_t
    {
        // Ticks against the tempo map. Every standard MIDI file is this.
        Musical = 0,

        // A tick is a microsecond and tempo does not apply. This is what makes "wait 250
        // milliseconds" a step a sequence can hold, and what a system exclusive dump needs when
        // the gap between packets has to be a real gap rather than a musical one.
        //
        // A tick is 32 bits, so an absolutely timed sequence runs out at about 71 minutes. That
        // is not a limit a step list will meet.
        Absolute = 1
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

    // A file stores lyrics one syllable at a time, which is unreadable on its own. These are the
    // syllables assembled back into the lines the writer intended.
    struct LyricLine
    {
        uint32_t StartTick{ 0 };
        uint32_t EndTick{ 0 };                  // where the next line begins
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

    // The largest files in the corpus reach the two million event cap, so anything added here is
    // paid for two million times. Kind already had room for a new value, which is why carrying
    // Universal MIDI Packets needed no new field.
    static_assert(sizeof(SequenceEvent) == 16, "SequenceEvent grew; check the cost at two million events first.");

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

    // One vertical line on the note display.
    struct GridLine
    {
        uint32_t Tick{ 0 };
        bool IsBar{ false };                    // false means a beat inside a bar
    };

    class MidiSequence
    {
    public:
        SequenceFormat Format{ SequenceFormat::MultiTrack };
        TimeDivision Division{};

        // A file reader never changes this. Only a sequence built in memory does.
        TimingMode Timing{ TimingMode::Musical };

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

        // Lyric syllables assembled into readable lines, empty for a file which carries none.
        std::vector<LyricLine> LyricLines{};

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

        // A Soft Karaoke file, which announces itself with an "@K" text event and then puts its
        // words in plain text events rather than lyric events.
        bool IsKaraoke{ false };

        bool IsEmpty() const noexcept { return Events.empty(); }

        std::span<uint8_t const> BytesOf(_In_ SequenceEvent const& event) const noexcept;

        uint64_t MicrosecondsAtTick(uint32_t tick) const noexcept;
        uint32_t TickAtMicroseconds(uint64_t microseconds) const noexcept;

        double BeatsPerMinuteAtTick(uint32_t tick) const noexcept;
        BarPosition BarPositionAtTick(uint32_t tick) const noexcept;
        TimeSignatureChange const& TimeSignatureAtTick(uint32_t tick) const noexcept;

        // Bar and beat boundaries between two ticks. Kept here rather than in the renderer so the
        // arithmetic, including what a meter change partway through does, can be tested directly.
        void CollectGridLines(
            uint32_t startTick,
            uint32_t endTick,
            bool includeBeats,
            size_t maximum,
            _Inout_ std::vector<GridLine>& lines) const noexcept;

        // Index of the first event at or after this tick, for starting playback part way in.
        size_t FirstEventIndexAtTick(uint32_t tick) const noexcept;

        // The chord symbol in effect at this tick, or null when the file has none or the first
        // one has not been reached. Points into TextEvents and lives as long as this sequence.
        TextEvent const* ChordSymbolAtTick(uint32_t tick) const noexcept;

        // The lyric line being sung at this tick, or null before the first one. Points into
        // LyricLines and lives as long as this sequence.
        LyricLine const* LyricLineAtTick(uint32_t tick) const noexcept;

        // How many notes each track has sounding at this instant. Resized to the track count, so
        // a caller reusing one vector across frames does not allocate.
        void CollectSoundingNoteCounts(uint32_t tick, _Inout_ std::vector<uint8_t>& counts) const noexcept;

        // The same thing written into storage the caller already owns, which is what lets it be
        // projected without a copy. Returns how many tracks were written.
        uint32_t FillSoundingNoteCounts(
            uint32_t tick,
            _Out_writes_to_(capacity, return) uint8_t* counts,
            uint32_t capacity) const noexcept;

        // Index of the first note that could overlap a window. Notes are ordered by where they
        // start, so a note which began earlier can still be sounding, and the search has to reach
        // back by the longest note in the sequence to be sure of finding it.
        size_t FirstNoteIndexForTickRange(uint32_t startTick) const noexcept;

        uint32_t CountNotesInTickRange(uint32_t startTick, uint32_t endTick) const noexcept;

        // Every note overlapping a window, in start order, stopping at maximum. A template so a
        // caller can write straight into whatever storage it has without a list in between.
        template<typename TCallback>
        uint32_t ForEachNoteInTickRange(
            uint32_t startTick,
            uint32_t endTick,
            uint32_t maximum,
            TCallback&& callback) const noexcept
        {
            if (endTick < startTick || maximum == 0)
            {
                return 0;
            }

            uint32_t visited{ 0 };

            for (auto index = FirstNoteIndexForTickRange(startTick); index < Notes.size(); ++index)
            {
                auto const& note = Notes[index];

                if (note.StartTick > endTick)
                {
                    break;
                }

                if (note.EndTick < startTick)
                {
                    continue;
                }

                callback(note);

                if (++visited >= maximum)
                {
                    break;
                }
            }

            return visited;
        }

        // Fills in the derived maps, totals and note pairing once the raw parts are populated.
        // A reader calls this last; nothing else should need it.
        void Finalize() noexcept;

        void Clear() noexcept;

    private:
        void BuildLyricLines() noexcept;
    };
}
