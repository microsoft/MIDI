// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The rows of the main window, worked out from the sequence: which rows are pinned to the top,
// which scroll, how tall each is and where it starts. Free of XAML so the tests use it too.

#include <sal.h>

#include <cstdint>
#include <string>
#include <vector>

#include "SequenceModel.h"

namespace midisequencer
{
    enum class ArrangeRowKind : uint8_t
    {
        Tempo = 0,
        Track = 1,
        Folder = 2,
        AddTrack = 3,

        // One automation lane, under its track.
        Lane = 4,

        // The lyrics track, pinned under Tempo and meter when the sequence has one.
        Lyrics = 5,
    };

    // Row heights from the comps (seq.css).
    inline constexpr double TempoRowHeight = 44.0;
    inline constexpr double LyricsRowHeight = 36.0;
    inline constexpr double TrackRowHeight = 46.0;
    inline constexpr double FolderRowHeight = 30.0;
    inline constexpr double AddTrackRowHeight = 40.0;
    inline constexpr double LaneRowHeight = 58.0;

    struct ArrangeRow
    {
        ArrangeRowKind Kind{ ArrangeRowKind::Track };
        std::wstring TrackId{};

        // Automation rows only: the lane, among its track's lanes.
        std::wstring LaneId{};

        size_t Depth{ 0 };
        double Top{ 0 };
        double Height{ 0 };
        bool Pinned{ false };

        // The folder a pinned track belongs to, shown in front of its name.
        std::wstring FolderName{};

        // What its folders say: M and S on a folder apply to everything inside it.
        bool InsideMutedFolder{ false };
        bool InsideSoloedFolder{ false };
    };

    struct ArrangeLayout
    {
        std::vector<ArrangeRow> Pinned{};
        std::vector<ArrangeRow> Scrolling{};
        double PinnedHeight{ 0 };
        double ScrollingHeight{ 0 };

        // The row a point is in, or null. y is from the top of that area.
        ArrangeRow const* PinnedRowAt(_In_ double y) const noexcept;
        ArrangeRow const* ScrollingRowAt(_In_ double y) const noexcept;

        // A track's or folder's own row, not one of its automation rows.
        ArrangeRow const* FindRow(_In_ std::wstring_view trackId) const noexcept;
    };

    // The Tempo and meter track is always the first pinned row. A pinned track is drawn only in
    // the pinned area, even when its folder is closed. A track's open lanes follow it, wherever
    // it's drawn.
    ArrangeLayout BuildArrangeLayout(_In_ Sequence const& sequence);
}
