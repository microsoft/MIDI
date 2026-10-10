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
                    row.Height = track.IsFolder ? FolderRowHeight : TrackRowHeight;
                    row.InsideMutedFolder = muted;
                    row.InsideSoloedFolder = soloed;

                    if (track.Pinned)
                    {
                        row.Pinned = true;
                        row.Depth = 0;
                        row.FolderName = folderName;
                        Add(Layout.Pinned, Layout.PinnedHeight, std::move(row));
                    }
                    else if (visible)
                    {
                        Add(Layout.Scrolling, Layout.ScrollingHeight, std::move(row));
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
                if (row.TrackId == trackId)
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
        tempo.Height = TempoRowHeight;
        tempo.Pinned = true;
        tempo.Top = 0;
        layout.PinnedHeight = TempoRowHeight;
        layout.Pinned.push_back(tempo);

        Walker walker{ layout };
        walker.Visit(sequence.Tracks, 0, true, std::wstring{}, false, false);

        ArrangeRow add{};
        add.Kind = ArrangeRowKind::AddTrack;
        add.Height = AddTrackRowHeight;
        walker.Add(layout.Scrolling, layout.ScrollingHeight, std::move(add));

        return layout;
    }
}
