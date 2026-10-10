// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The edits the window makes to a sequence, each as reversible changes for the undo history.
// Free of pch.h and XAML like the rest of the model, so the unit tests use the same code.

#include <sal.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "SequenceModel.h"
#include "SequenceUndo.h"

namespace midisequencer
{
    using ChangeList = std::vector<std::unique_ptr<SequenceChange>>;

    // A clip added to or removed from the sequence's clip list, kept whole so undo can put it back.
    std::unique_ptr<SequenceChange> MakeClipPresenceChange(_In_ Clip clip, _In_ bool added);

    // The track tree before and after an edit. The tree holds no notes (clips do), so a copy of
    // it is small even for a big sequence.
    std::unique_ptr<SequenceChange> MakeTracksChange(_In_ std::vector<Track> before, _In_ std::vector<Track> after);

    // One clip's settings (name, color, length, loop, seed), leaving its notes and events alone.
    std::unique_ptr<SequenceChange> MakeClipSettingsChange(_In_ Clip const& before, _In_ Clip const& after);

    // A clip's events (controllers, per-note changes and the rest) before and after.
    std::unique_ptr<SequenceChange> MakeClipEventsChange(_In_ std::wstring clipId, _In_ std::vector<ClipEvent> before, _In_ std::vector<ClipEvent> after);

    std::unique_ptr<SequenceChange> MakeTempoChange(_In_ std::vector<TempoPoint> before, _In_ std::vector<TempoPoint> after);
    std::unique_ptr<SequenceChange> MakeMeterChange(_In_ std::vector<MeterChange> before, _In_ std::vector<MeterChange> after);
    std::unique_ptr<SequenceChange> MakeSequenceTagsChange(_In_ std::vector<Tag> before, _In_ std::vector<Tag> after);
    std::unique_ptr<SequenceChange> MakeScenesChange(_In_ std::vector<Scene> before, _In_ std::vector<Scene> after);
    std::unique_ptr<SequenceChange> MakeNameChange(_In_ std::wstring before, _In_ std::wstring after);

    // Where a track sits: the list that holds it (the top level or a folder's children) and its
    // index in that list. Nothing when the id isn't in the sequence.
    struct TrackLocation
    {
        std::vector<Track>* Container{ nullptr };
        size_t Index{ 0 };
        Track* Parent{ nullptr };   // the folder, or null at the top level
    };

    TrackLocation LocateTrack(_In_ Sequence& sequence, _In_ std::wstring_view id) noexcept;

    // The folder a track sits in, or null at the top level.
    Track const* ParentFolder(_In_ Sequence const& sequence, _In_ std::wstring_view id) noexcept;

    // True when candidate is the folder itself or anywhere inside it.
    bool IsInside(_In_ Sequence const& sequence, _In_ std::wstring_view candidateId, _In_ std::wstring_view folderId) noexcept;

    // A new track or folder, with a fresh id and the next color in the swatch order.
    Track MakeTrack(_In_ Sequence const& sequence, _In_ std::wstring name, _In_ bool folder);

    // Inserts after the given track, in the same list, or at the end of the top level when the id
    // is empty or unknown. Returns where it went.
    void InsertTrackAfter(_Inout_ Sequence& sequence, _In_ std::wstring_view afterId, _In_ Track track);

    // Moves a track into a folder (at its end), or to the top level when folderId is empty.
    // Refuses to move a folder into itself.
    bool MoveTrackToFolder(_Inout_ Sequence& sequence, _In_ std::wstring_view trackId, _In_ std::wstring_view folderId);

    // Up or down within its list. False at either end.
    bool MoveTrackBy(_Inout_ Sequence& sequence, _In_ std::wstring_view trackId, _In_ int32_t delta);

    std::optional<Track> RemoveTrack(_Inout_ Sequence& sequence, _In_ std::wstring_view trackId);

    // Clips no placement or slot uses any more, taken out of the sequence and returned so the
    // caller can record them for undo.
    std::vector<Clip> RemoveUnusedClips(_Inout_ Sequence& sequence);

    // An empty notes clip, with a fresh id and seed.
    Clip MakeNotesClip(_In_ std::wstring name, _In_ int64_t length);

    // The ten colors offered for tracks (0xRRGGBB), in the order the comps show them.
    std::array<uint32_t, 10> const& TrackColorSwatches() noexcept;

    // The channel a new track should play on: the first one no track already uses on that
    // destination, so two tracks don't land on top of each other.
    int8_t NextFreeChannel(_In_ Sequence const& sequence, _In_ EndpointRef const& endpoint, _In_ uint8_t group) noexcept;

    // "Track 3", "Folder 2": the first number not already used by a name of that form.
    std::wstring NextName(_In_ Sequence const& sequence, _In_ std::wstring_view stem);

    // Snaps a tick to a grid. A grid of 0 leaves it alone.
    int64_t SnapTick(_In_ int64_t tick, _In_ int64_t grid) noexcept;

    // The notes of a clip that start inside [fromTick, toTick) and lie inside [lowNote, highNote].
    std::vector<size_t> NotesInBox(
        _In_ Clip const& clip,
        _In_ int64_t fromTick,
        _In_ int64_t toTick,
        _In_ uint8_t lowNote,
        _In_ uint8_t highNote);
}
