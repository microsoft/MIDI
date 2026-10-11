// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Turns a track's timeline into the messages it plays, at absolute ticks. Playback and every
// export use this one path, so a file always holds what you heard.

#include <sal.h>

#include <array>
#include <cstdint>
#include <vector>

#include "SequenceModel.h"

namespace midisequencer
{
    struct ArpeggiatorInput;

    enum class RenderedKind : uint8_t
    {
        // At one tick: note offs go first, so a note repeated on the same tick restarts instead
        // of being cut off by its own end. Then other messages, then note ons.
        NoteOff = 0,
        Other = 1,
        NoteOn = 2,
    };

    struct RenderedMessage
    {
        int64_t Tick{ 0 };
        std::array<uint32_t, 4> Words{};
        uint8_t WordCount{ 0 };
        RenderedKind Kind{ RenderedKind::Other };

        // Played by an arpeggiator from the keys held, which recording keeps instead of the keys.
        bool Arpeggiated{ false };
    };

    // A file can place a one-tick looping clip for a billion ticks. One render stops here rather
    // than running the PC out of memory; real sequences are far below it.
    inline constexpr size_t MaximumRenderedMessages = 4000000;

    // How often a ramp is sent when a device has no automation interval in MIDI Settings.
    inline constexpr uint32_t DefaultLaneIntervalMilliseconds = 10;

    // About 10 ms at 120 BPM.
    inline constexpr int64_t DefaultLaneStepTicks = 20;

    struct RenderOptions
    {
        // A lane value that doesn't change at the destination's resolution isn't sent again.
        bool Midi2{ true };

        // How far apart a ramp's steps are, in ticks. Steps sit on multiples of this on the
        // timeline, so a ramp rendered in pieces comes out the same as one rendered whole.
        int64_t LaneStepTicks{ DefaultLaneStepTicks };

        // Each lane's value at fromTick as well, for a start, a jump or the loop going round.
        bool LaneStartValues{ false };

        // Whether lanes play at all. Chase leaves them out and asks for start values instead.
        bool Lanes{ true };

        // Whether the track that sings sends the lyrics as Lyric Data. Only playback and clip
        // files ask, and only for MIDI 2.0.
        bool Lyrics{ false };

        // The keys held on the track's source, which an arpeggiator that plays them steps
        // through. Only playback has them; it's left out everywhere else.
        ArpeggiatorInput* Arpeggiator{ nullptr };

        size_t MaximumMessages{ MaximumRenderedMessages };
    };

    // Every message from the track's placements with a time in [fromTick, toTick), sorted. Notes
    // come out as MIDI 2.0 note on and note off messages. Groups and channels are as stored; the
    // destination is applied by ApplyDestination.
    //
    // A note that would run past the end of its placement, or past the loop point of a looping
    // clip, ends there. The track's lanes play everywhere except inside a placement whose clip has
    // a lane for the same target, and pick up again at its end.
    void RenderTrack(
        _In_ Sequence const& sequence,
        _In_ Track const& track,
        _In_ int64_t fromTick,
        _In_ int64_t toTick,
        _Inout_ std::vector<RenderedMessage>& messages,
        _In_ RenderOptions const& options);

    inline void RenderTrack(
        _In_ Sequence const& sequence,
        _In_ Track const& track,
        _In_ int64_t fromTick,
        _In_ int64_t toTick,
        _Inout_ std::vector<RenderedMessage>& messages,
        _In_ size_t maximumMessages = MaximumRenderedMessages)
    {
        RenderOptions options{};
        options.MaximumMessages = maximumMessages;
        RenderTrack(sequence, track, fromTick, toTick, messages, options);
    }

    // The fastest tempo in a tempo map, which decides how close a ramp's steps may be.
    double FastestTempo(_In_ std::vector<TempoPoint> const& tempo) noexcept;

    // Ticks between a ramp's steps, so that even at the fastest tempo they're no closer together
    // than the device's automation interval. An interval of 0 means the device has none.
    int64_t LaneStepTicks(_In_ double fastestBeatsPerMinute, _In_ uint32_t intervalMilliseconds) noexcept;

    // The MIDI 2.0 message that sets a lane to a value, on group 0 and the lane's channel.
    void BuildLaneMessage(_In_ AutomationLane const& lane, _In_ uint32_t value, _Out_writes_(2) uint32_t* words) noexcept;

    // Where the track's last placement ends. 0 for a track with nothing on its timeline.
    int64_t TrackEndTick(_In_ Sequence const& sequence, _In_ Track const& track) noexcept;

    // Where the last placement of any track ends.
    int64_t SequenceEndTick(_In_ Sequence const& sequence) noexcept;

    // Rewrites a message for the track's destination: its group, and its channel when the
    // destination sets one. Messages without a group or channel pass through unchanged.
    void ApplyDestination(
        _In_ TrackDestination const& destination,
        _Inout_ std::array<uint32_t, 4>& words,
        _In_ uint8_t wordCount) noexcept;

    // A MIDI 2.0 note message, message type 4. Two words.
    void BuildNoteOn(_In_ Note const& note, _In_ uint8_t group, _Out_writes_(2) uint32_t* words) noexcept;
    void BuildNoteOff(_In_ Note const& note, _In_ uint8_t group, _Out_writes_(2) uint32_t* words) noexcept;
}
