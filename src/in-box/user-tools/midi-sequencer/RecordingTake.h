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

        bool IsEmpty() const noexcept { return m_notes.empty() && m_events.empty() && m_open.empty(); }

        size_t NoteCount() const noexcept { return m_notes.size() + m_open.size(); }

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

    private:
        struct OpenNote
        {
            Note Value{};
        };

        int64_t m_startTick{ 0 };
        RecordFilter m_filter{};
        std::vector<Note> m_notes{};
        std::vector<OpenNote> m_open{};
        std::vector<ClipEvent> m_events{};
        int64_t m_lastTick{ 0 };
        int64_t m_placementTick{ 0 };
    };
}
