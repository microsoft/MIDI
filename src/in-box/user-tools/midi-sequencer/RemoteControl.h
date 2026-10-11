// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The remote control map (design section 16): which incoming message does what, and what the
// sequencer sends back so a surface can light up what's playing. Every message is one a MIDI 1.0
// controller can send. MIDI 2.0 versions of the same notes and controllers work too.
//
//   Channels 1 to 15 of group 1 are tracks 1 to 15, in track list order with folders left out (a
//   pinned track keeps its place in the list). Group 2 holds tracks 16 to 30, and so on. On a
//   track's channel:
//     note 0 to 63     launch the clip in scenes 1 to 64 (an empty slot stops the track)
//     note 64          stop the track
//     note 65          back to the timeline
//     controller 20    mute, 21 solo, 22 record arm (64 and up is on)
//   Channel 16 of group 1 is the whole sequence:
//     note 0 to 63     launch scenes 1 to 64
//     note 64          stop every track, 65 every track back to the timeline
//     note 96 play, 97 stop, 98 record, 99 loop, 100 metronome, 101 go to the start
//
// Notes act when they go on. A note off, or a note on at velocity 0, does nothing. What goes back
// is the same notes and controllers: a note on at 127 for a clip that's playing or recording and
// for a transport button that's on, and a note off when it isn't; controllers at 127 or 0.
//
// Free of pch.h and XAML, so the unit tests use it unchanged.

#include <sal.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "SequenceModel.h"

namespace midisequencer
{
    inline constexpr size_t RemoteTracksPerGroup = 15;
    inline constexpr size_t RemoteMaximumTracks = RemoteTracksPerGroup * 16;
    inline constexpr size_t RemoteMaximumScenes = 64;
    inline constexpr uint8_t RemoteSequenceChannel = 15;

    inline constexpr uint8_t RemoteNoteStop = 64;
    inline constexpr uint8_t RemoteNoteToTimeline = 65;
    inline constexpr uint8_t RemoteNotePlay = 96;
    inline constexpr uint8_t RemoteNoteStopTransport = 97;
    inline constexpr uint8_t RemoteNoteRecord = 98;
    inline constexpr uint8_t RemoteNoteLoop = 99;
    inline constexpr uint8_t RemoteNoteMetronome = 100;
    inline constexpr uint8_t RemoteNoteGoToStart = 101;

    inline constexpr uint8_t RemoteControllerMute = 20;
    inline constexpr uint8_t RemoteControllerSolo = 21;
    inline constexpr uint8_t RemoteControllerArm = 22;

    enum class RemoteAction : uint8_t
    {
        LaunchSlot,
        StopTrack,
        TrackToTimeline,
        Mute,
        Solo,
        Arm,
        LaunchScene,
        StopAll,
        AllToTimeline,
        Play,
        Stop,
        Record,
        Loop,
        Metronome,
        GoToStart,
    };

    struct RemoteCommand
    {
        RemoteAction Action{ RemoteAction::Play };

        // Counted from 0. Track is an index into RemoteTrackOrder.
        size_t Track{ 0 };
        size_t Scene{ 0 };

        // For mute, solo and arm.
        bool On{ false };

        bool operator==(RemoteCommand const&) const = default;
    };

    // What an incoming message asks for, or nothing for a message the map doesn't use.
    std::optional<RemoteCommand> ParseRemoteMessage(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) noexcept;

    // The tracks the map can reach, in track list order, folders left out.
    std::vector<std::wstring> RemoteTrackOrder(_In_ Sequence const& sequence);

    // What a surface shows. Slots holds, for each track the map reaches, whether each scene's
    // clip is playing or recording.
    struct RemoteState
    {
        std::vector<std::vector<bool>> SlotsLit{};
        std::vector<bool> Muted{};
        std::vector<bool> Soloed{};
        std::vector<bool> Armed{};
        bool Playing{ false };
        bool Recording{ false };
        bool Loop{ false };
        bool Metronome{ false };

        bool operator==(RemoteState const&) const = default;
    };

    // The MIDI 1.0 messages, one word each, that bring a surface from what it showed to what it
    // should show now. With nothing before, everything is sent, so a surface that just connected
    // catches up.
    std::vector<uint32_t> RemoteFeedback(_In_opt_ RemoteState const* before, _In_ RemoteState const& now);

    // Where a track's messages go on the map: its group and channel.
    std::pair<uint8_t, uint8_t> RemoteTrackAddress(_In_ size_t track) noexcept;
}
