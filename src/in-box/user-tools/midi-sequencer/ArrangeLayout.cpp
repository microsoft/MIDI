// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "ArrangeLayout.h"

#include <algorithm>

namespace midisequencer
{
    namespace
    {
        ArrangeRow const* RowAt(_In_ std::vector<ArrangeRow> const& rows, _In_ double y) noexcept
        {
            auto found = std::upper_bound(rows.begin(), rows.end(), y,
                [](double value, ArrangeRow const& row) { return value < row.Top; });

            if (found == rows.begin())
            {
                return nullptr;
            }

            --found;
            return y < found->Top + found->Height ? &*found : nullptr;
        }

        struct Walker
        {
            ArrangeLayout& Layout;

            void Add(_Inout_ std::vector<ArrangeRow>& rows, _Inout_ double& height, _In_ ArrangeRow row)
            {
                row.Top = height;
                height += row.Height;
                rows.push_back(std::move(row));
            }

            void AddLanes(_Inout_ std::vector<ArrangeRow>& rows, _Inout_ double& height, _In_ Track const& track, _In_ ArrangeRow const& trackRow)
            {
                if (track.IsFolder || !track.LanesOpen)
                {
                    return;
                }

                for (auto const& lane : track.Lanes)
                {
                    ArrangeRow row{};
                    row.Kind = ArrangeRowKind::Lane;
                    row.TrackId = track.Id;
                    row.LaneId = lane.Id;
                    row.Depth = trackRow.Depth;
                    row.Height = LaneRowHeight;
                    row.Pinned = trackRow.Pinned;
                    row.InsideMutedFolder = trackRow.InsideMutedFolder;
                    row.InsideSoloedFolder = trackRow.InsideSoloedFolder;
                    Add(rows, height, std::move(row));
                }
            }

            void Visit(
                _In_ std::vector<Track> const& tracks,
                _In_ size_t depth,
                _In_ bool visible,
                _In_ std::wstring const& folderName,
                _In_ bool muted,
                _In_ bool soloed)
            {
                for (auto const& track : tracks)
                {
                    ArrangeRow row{};
                    row.Kind = track.IsFolder ? ArrangeRowKind::Folder : ArrangeRowKind::Track;
                    row.TrackId = track.Id;
                    row.Depth = depth;
                    row.Height = RowHeightOf(track);
                    row.InsideMutedFolder = muted;
                    row.InsideSoloedFolder = soloed;

                    if (track.Pinned)
                    {
                        row.Pinned = true;
                        row.Depth = 0;
                        row.FolderName = folderName;
                        auto const copy = row;
                        Add(Layout.Pinned, Layout.PinnedHeight, std::move(row));
                        AddLanes(Layout.Pinned, Layout.PinnedHeight, track, copy);
                    }
                    else if (visible)
                    {
                        auto const copy = row;
                        Add(Layout.Scrolling, Layout.ScrollingHeight, std::move(row));
                        AddLanes(Layout.Scrolling, Layout.ScrollingHeight, track, copy);
                    }

                    if (track.IsFolder)
                    {
                        Visit(track.Children, depth + 1, visible && track.Open,
                            track.Name, muted || track.Muted, soloed || track.Soloed);
                    }
                }
            }
        };
    }

    _Use_decl_annotations_
    double RowHeightOf(Track const& track) noexcept
    {
        if (track.Height == 0)
        {
            return track.IsFolder ? FolderRowHeight : TrackRowHeight;
        }

        return std::clamp(static_cast<double>(track.Height), static_cast<double>(MinimumTrackRowHeight), static_cast<double>(MaximumTrackRowHeight));
    }

    _Use_decl_annotations_
    double TempoRowHeightOf(Sequence const& sequence) noexcept
    {
        if (sequence.TempoRowHeight == 0)
        {
            return TempoRowHeight;
        }

        return std::clamp(static_cast<double>(sequence.TempoRowHeight), TempoRowHeight, static_cast<double>(MaximumTrackRowHeight));
    }

    _Use_decl_annotations_
    ArrangeRow const* ArrangeLayout::PinnedRowAt(double y) const noexcept
    {
        return RowAt(Pinned, y);
    }

    _Use_decl_annotations_
    ArrangeRow const* ArrangeLayout::ScrollingRowAt(double y) const noexcept
    {
        return RowAt(Scrolling, y);
    }

    _Use_decl_annotations_
    ArrangeRow const* ArrangeLayout::FindRow(std::wstring_view trackId) const noexcept
    {
        for (auto const* rows : { &Pinned, &Scrolling })
        {
            for (auto const& row : *rows)
            {
                if (row.TrackId == trackId && row.Kind != ArrangeRowKind::Lane)
                {
                    return &row;
                }
            }
        }

        return nullptr;
    }

    _Use_decl_annotations_
    ArrangeLayout BuildArrangeLayout(Sequence const& sequence)
    {
        ArrangeLayout layout{};

        ArrangeRow tempo{};
        tempo.Kind = ArrangeRowKind::Tempo;
        tempo.Height = TempoRowHeightOf(sequence);
        tempo.Pinned = true;
        tempo.Top = 0;
        layout.PinnedHeight = tempo.Height;
        layout.Pinned.push_back(tempo);

        if (sequence.Lyrics)
        {
            ArrangeRow lyrics{};
            lyrics.Kind = ArrangeRowKind::Lyrics;
            lyrics.Height = LyricsRowHeight;
            lyrics.Pinned = true;
            lyrics.Top = layout.PinnedHeight;
            layout.PinnedHeight += LyricsRowHeight;
            layout.Pinned.push_back(lyrics);
        }

        Walker walker{ layout };
        walker.Visit(sequence.Tracks, 0, true, std::wstring{}, false, false);

        ArrangeRow add{};
        add.Kind = ArrangeRowKind::AddTrack;
        add.Height = AddTrackRowHeight;
        walker.Add(layout.Scrolling, layout.ScrollingHeight, std::move(add));

        return layout;
    }
}
