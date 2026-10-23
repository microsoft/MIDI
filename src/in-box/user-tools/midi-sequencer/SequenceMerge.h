// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Merging and splitting clips and tracks (October 22, Pete):
//
// - Merging clips on a track: the placements become one notes clip, from the first one's start
//   to the last one's end, with every note where it played, so the spacing between them stays.
// - Splitting a placement at a tick: the left part keeps playing up to the cut, the right part
//   becomes a notes clip of what played from there.
// - Merging one track into another: the destination keeps its settings (where it plays, its
//   color, its sound). Placements that don't overlap anything move across as they are, still
//   linked to their clips; where the two tracks play at the same time, what both played becomes
//   one notes clip, and the same note sounding twice at once becomes one longer note.
//
// A plain notes clip is copied note for note, chance and all. A pattern or generator is what it
// played there, worked out the same way playback works it out, so its notes are kept as they
// were. Free of pch.h and XAML like the rest of the model, so the unit tests use the same code.

#include <sal.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "SequenceModel.h"

namespace midisequencer
{
    // How long a placement plays: its own length, or its clip's.
    int64_t PlacementLength(_In_ Sequence const& sequence, _In_ Placement const& placement) noexcept;

    // What some placements play between two ticks, with ticks counted from fromTick. Notes that
    // run past toTick end there. A clip's lanes come along, their points where they played.
    struct PlacedContent
    {
        std::vector<Note> Notes{};
        std::vector<ClipEvent> Events{};
        std::vector<AutomationLane> Lanes{};
    };

    PlacedContent ContentOfPlacements(
        _In_ Sequence const& sequence,
        _In_ Track const& track,
        _In_ std::vector<Placement> const& placements,
        _In_ int64_t fromTick,
        _In_ int64_t toTick);

    // The same note (channel and number) sounding twice at once becomes one note, from the first
    // one's start to the later end, at the first one's velocity. Sorted as a clip keeps them.
    std::vector<Note> MergeOverlappingNotes(_In_ std::vector<Note> notes);

    // The placements at these indexes on a track, as one notes clip in their place. Needs two or
    // more. Returns the clip, already in the sequence, and where its placement is.
    struct MergedClip
    {
        Clip Made{};
        size_t Placement{ SIZE_MAX };
    };

    std::optional<MergedClip> MergePlacements(
        _Inout_ Sequence& sequence,
        _In_ std::wstring_view trackId,
        _In_ std::vector<size_t> indexes,
        _In_ std::wstring clipId,
        _In_ std::wstring name);

    // Splits a placement at a tick strictly inside it. A pattern or generator keeps playing on
    // the left, cut short there; a notes clip's left part becomes a notes clip of its own, so
    // nothing past the cut hides in it. The right part is always a new notes clip. Returns the
    // clips added to the sequence, empty when it couldn't split. Clips nothing uses any more are
    // the caller's to take out (RemoveUnusedClips).
    std::vector<Clip> SplitPlacement(
        _Inout_ Sequence& sequence,
        _In_ std::wstring_view trackId,
        _In_ size_t index,
        _In_ int64_t tick,
        _In_ std::function<std::wstring()> const& newClipId,
        _In_ std::wstring const& rightName);

    // Merges one track into another and takes the first one out. See the top of this file.
    // Launcher slots: an empty one takes the other track's clip; where both have a different clip,
    // the two become one looping notes clip as long as the longer. Lanes the destination already
    // has (the same target, number and channel) aren't added; LanesSkipped counts them.
    struct TrackMerge
    {
        bool Merged{ false };
        std::vector<Clip> Added{};
        size_t LanesSkipped{ 0 };
    };

    TrackMerge MergeTrackInto(
        _Inout_ Sequence& sequence,
        _In_ std::wstring_view sourceId,
        _In_ std::wstring_view destinationId,
        _In_ std::function<std::wstring()> const& newClipId,
        _In_ std::function<std::wstring(std::wstring const& destinationName, std::wstring const& sourceName)> const& mergedName);

    // What every other unmuted track (not in a muted folder either) plays on the timeline between
    // two ticks, with ticks counted from fromTick. Solo doesn't count. For the piano roll's ghost
    // notes.
    struct OtherTrackNotes
    {
        std::wstring TrackId{};
        uint32_t Color{ 0 };
        std::vector<Note> Notes{};
    };

    std::vector<OtherTrackNotes> NotesOtherTracksPlay(
        _In_ Sequence const& sequence,
        _In_ std::wstring_view exceptTrackId,
        _In_ int64_t fromTick,
        _In_ int64_t toTick);
}
