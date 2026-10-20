// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// What a track records: incoming messages, already placed on the timeline, turned into a notes
// clip. Note ons are paired with their note offs into notes, MIDI 1.0 is scaled up to MIDI 2.0
// (M2-115) on the way in, and anything the track doesn't record is left out.
//
// No connections and no clock: the window hands each message over with its tick, so the tests
// drive this with made-up messages.

#include <sal.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "SequenceModel.h"

namespace midisequencer
{
    struct RecordFilter
    {
        int8_t Group{ -1 };                     // -1 is any group
        uint16_t Channels{ AllChannels };       // one bit per channel
        bool Notes{ true };
        bool Controllers{ true };
        bool PitchBend{ true };
        bool Pressure{ true };
        bool Program{ true };
        bool SystemExclusive{ false };
    };

    // Whether a message gets through a track's source settings. Used for echo too, so what you
    // hear while armed is what gets recorded.
    bool PassesRecordFilter(_In_ RecordFilter const& filter, _In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) noexcept;

    class RecordingTake
    {
    public:
        explicit RecordingTake(_In_ int64_t startTick, _In_ RecordFilter filter = {});

        // A message as it arrived, at its place on the timeline.
        void Add(_In_ int64_t tick, _In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount);

        // A message the track played itself, from a launched clip: kept whatever the track
        // records from, since it's on the channel the track plays to.
        void AddPlayed(_In_ int64_t tick, _In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount);

        // Where what the track played was cut off, by a launch, a stop, or the end of the take:
        // every note it played that sounds past here ends here. What came in on its source is
        // left as it was played.
        void CutPlayed(_In_ int64_t tick);

        bool IsEmpty() const noexcept { return m_notes.empty() && m_events.empty() && m_open.empty(); }

        int64_t StartTick() const noexcept { return m_startTick; }

        // Ends the take at endTick. Notes still held end there. The clip runs from the start of
        // the bar the take began in to the end of the bar the last thing was played in.
        Clip Finish(
            _In_ int64_t endTick,
            _In_ std::vector<MeterChange> const& meter,
            _In_ std::wstring clipId,
            _In_ std::wstring name,
            _In_ std::wstring originDetail);

        // Where the finished clip goes on the timeline. Valid after Finish.
        int64_t PlacementTick() const noexcept { return m_placementTick; }

        // What's been recorded so far, for drawing the take while it grows. Held notes end at
        // nowTick.
        std::vector<Note> NotesSoFar(_In_ int64_t nowTick) const;

        // A take recorded into a launcher slot: a looping clip that starts where the take did and
        // is exactly length long. Held notes end at nowTick (LoopSoFar) or at the clip's end
        // (FinishLoop), and anything played after the end is left out.
        Clip LoopSoFar(_In_ int64_t length, _In_ int64_t nowTick) const;
        Clip FinishLoop(_In_ int64_t length, _In_ std::wstring clipId, _In_ std::wstring name, _In_ std::wstring originDetail);

    private:
        void Keep(_In_ int64_t tick, _In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount, _In_ bool played);

        struct OpenNote
        {
            Note Value{};

            // Played by the track itself, and where a cut ends it, or -1.
            bool Played{ false };
            int64_t CutAt{ -1 };
        };

        // A note ending: at endTick, or where a cut ended it if that's sooner.
        Note Close(_In_ OpenNote const& open, _In_ int64_t endTick) const noexcept;

        int64_t m_startTick{ 0 };
        RecordFilter m_filter{};
        std::vector<Note> m_notes{};

        // Which of m_notes the track played itself.
        std::vector<size_t> m_playedNotes{};
        std::vector<OpenNote> m_open{};
        std::vector<ClipEvent> m_events{};
        int64_t m_lastTick{ 0 };
        int64_t m_placementTick{ 0 };
    };
}
