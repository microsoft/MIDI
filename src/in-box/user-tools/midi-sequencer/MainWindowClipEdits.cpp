// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Merging and splitting clips and tracks from the timeline (October 22, Pete): Ctrl+click picks
// more clips on the selected track, and "Merge selected clips" makes them one; the razor (or
// "Split here") cuts a clip in two; "Merge into" folds one track into another. The work itself
// is SequenceMerge.h, which the unit tests cover.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "SequenceMerge.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    std::vector<size_t> MainWindow::MorePlacements() const
    {
        std::vector<size_t> result{};

        // Only while the clip they were picked with is still the selected one.
        if (m_morePlacements.empty() || m_moreTrackId != m_selectedTrackId || m_moreAnchor != m_selectedPlacement)
        {
            return result;
        }

        auto const track = seq::FindTrack(m_doc, m_moreTrackId);

        if (track == nullptr)
        {
            return result;
        }

        for (auto const index : m_morePlacements)
        {
            if (index < track->Timeline.size() && index != m_selectedPlacement)
            {
                result.push_back(index);
            }
        }

        return result;
    }

    _Use_decl_annotations_
    void MainWindow::TogglePlacementInSelection(std::wstring const& trackId, size_t placement) noexcept
    {
        try
        {
            auto const track = seq::FindTrack(m_doc, trackId);

            if (track == nullptr || placement >= track->Timeline.size() || trackId != m_selectedTrackId)
            {
                return;
            }

            auto more = MorePlacements();

            if (placement == m_selectedPlacement)
            {
                // The one they were picked with: the next picked one takes its place.
                if (more.empty())
                {
                    return;
                }

                m_selectedPlacement = more.front();
                more.erase(more.begin());
            }
            else if (auto it = std::find(more.begin(), more.end(), placement); it != more.end())
            {
                more.erase(it);
            }
            else
            {
                more.push_back(placement);
            }

            m_morePlacements = std::move(more);
            m_moreAnchor = m_selectedPlacement;
            m_moreTrackId = trackId;
            InvalidateArrange();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to pick a clip.")
    }

    void MainWindow::MergeSelectedPlacements() noexcept
    {
        try
        {
            auto indexes = MorePlacements();

            if (indexes.empty() || m_selectedPlacement == SIZE_MAX)
            {
                return;
            }

            indexes.push_back(m_selectedPlacement);

            auto const trackId = m_selectedTrackId;
            auto const before = m_doc.Tracks;
            auto const merged = seq::MergePlacements(m_doc, trackId, indexes, seq::NewId(L"c"), NextClipName(L"MergedClipName"));

            if (!merged.has_value())
            {
                return;
            }

            seq::ChangeList changes{};
            changes.push_back(seq::MakeClipPresenceChange(merged->Made, true));
            changes.push_back(seq::MakeTracksChange(before, m_doc.Tracks));

            for (auto& clip : seq::RemoveUnusedClips(m_doc))
            {
                changes.push_back(seq::MakeClipPresenceChange(std::move(clip), false));
            }

            m_morePlacements.clear();

            if (!m_editorClipId.empty() && seq::FindClip(m_doc, m_editorClipId) == nullptr)
            {
                CloseEditor();
            }

            Commit(std::wstring{ res::GetString(L"UndoMergeClips") }, std::move(changes));

            m_selectedPlacement = merged->Placement;
            InvalidateArrange();
            OpenClipInEditor(merged->Made.Id, trackId);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to merge clips.")
    }

    _Use_decl_annotations_
    void MainWindow::SplitPlacementAt(std::wstring const& trackId, size_t placement, int64_t tick) noexcept
    {
        try
        {
            auto const track = seq::FindTrack(m_doc, trackId);

            if (track == nullptr || placement >= track->Timeline.size())
            {
                return;
            }

            auto const clip = seq::FindClip(m_doc, track->Timeline[placement].ClipId);
            auto const rightName = std::wstring{ res::FormatString(L"SplitClipNameFormat", clip != nullptr ? clip->Name : std::wstring{}) };

            auto const before = m_doc.Tracks;
            auto added = seq::SplitPlacement(m_doc, trackId, placement, tick, []() { return seq::NewId(L"c"); }, rightName);

            if (added.empty())
            {
                return;
            }

            auto const rightId = added.back().Id;

            seq::ChangeList changes{};

            for (auto const& made : added)
            {
                changes.push_back(seq::MakeClipPresenceChange(made, true));
            }

            changes.push_back(seq::MakeTracksChange(before, m_doc.Tracks));

            for (auto& unused : seq::RemoveUnusedClips(m_doc))
            {
                changes.push_back(seq::MakeClipPresenceChange(std::move(unused), false));
            }

            m_morePlacements.clear();

            if (!m_editorClipId.empty() && seq::FindClip(m_doc, m_editorClipId) == nullptr)
            {
                CloseEditor();
            }

            Commit(std::wstring{ res::GetString(L"UndoSplitClip") }, std::move(changes));

            // The right part is selected, so another cut or a move carries on from there.
            if (auto const after = seq::FindTrack(m_doc, trackId); after != nullptr)
            {
                SelectTrack(trackId);

                for (size_t i = 0; i < after->Timeline.size(); ++i)
                {
                    if (after->Timeline[i].ClipId == rightId)
                    {
                        m_selectedPlacement = i;
                    }
                }
            }

            InvalidateArrange();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to split a clip.")
    }

    _Use_decl_annotations_
    void MainWindow::SetRazor(bool on) noexcept
    {
        try
        {
            m_razor = on;
            m_razorTrackId.clear();
            m_razorTick = -1;

            if (RazorToggle().IsChecked().GetBoolean() != on)
            {
                RazorToggle().IsChecked(on);
            }

            InvalidateArrange();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change the razor.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRazorClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        SetRazor(RazorToggle().IsChecked().GetBoolean());
    }

    _Use_decl_annotations_
    void MainWindow::UpdateRazor(bool pinned, foundation::Point point) noexcept
    {
        try
        {
            std::wstring trackId{};
            int64_t tick{ -1 };

            auto const hit = HitTestLane(pinned, point);

            if (hit.Row != nullptr && hit.Row->Kind == seq::ArrangeRowKind::Track && hit.Placement != SIZE_MAX)
            {
                if (auto const track = seq::FindTrack(m_doc, hit.Row->TrackId); track != nullptr && hit.Placement < track->Timeline.size())
                {
                    auto const& placed = track->Timeline[hit.Placement];
                    auto const end = placed.Tick + seq::PlacementLength(m_doc, placed);
                    auto const at = seq::SnapTick(hit.Tick, std::max<int64_t>(1, SnapGrid()));

                    // Only a cut strictly inside the clip; at either end there's nothing to cut.
                    if (at > placed.Tick && at < end)
                    {
                        trackId = track->Id;
                        tick = at;
                    }
                }
            }

            if (trackId != m_razorTrackId || tick != m_razorTick)
            {
                m_razorTrackId = std::move(trackId);
                m_razorTick = tick;
                InvalidateArrange();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to move the razor.")
    }

    _Use_decl_annotations_
    bool MainWindow::RazorPressed(bool pinned, foundation::Point point) noexcept
    {
        try
        {
            UpdateRazor(pinned, point);

            if (m_razorTrackId.empty() || m_razorTick < 0)
            {
                return false;
            }

            auto const hit = HitTestLane(pinned, point);

            if (hit.Row == nullptr || hit.Placement == SIZE_MAX)
            {
                return false;
            }

            auto const trackId = m_razorTrackId;
            auto const tick = m_razorTick;

            SplitPlacementAt(trackId, hit.Placement, tick);
            UpdateRazor(pinned, point);
            return true;
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to cut a clip.")

        return false;
    }

    _Use_decl_annotations_
    void MainWindow::MergeTrackInto(std::wstring const& sourceId, std::wstring const& destinationId) noexcept
    {
        try
        {
            if (sourceId == destinationId || seq::FindTrack(m_doc, sourceId) == nullptr || seq::FindTrack(m_doc, destinationId) == nullptr)
            {
                return;
            }

            auto const before = m_doc.Tracks;
            auto const result = seq::MergeTrackInto(m_doc, sourceId, destinationId,
                []() { return seq::NewId(L"c"); },
                [](std::wstring const& destinationName, std::wstring const& sourceName)
                {
                    return std::wstring{ res::FormatString(L"MergedTrackClipNameFormat", destinationName, sourceName) };
                });

            if (!result.Merged)
            {
                return;
            }

            seq::ChangeList changes{};

            for (auto const& made : result.Added)
            {
                changes.push_back(seq::MakeClipPresenceChange(made, true));
            }

            changes.push_back(seq::MakeTracksChange(before, m_doc.Tracks));

            for (auto& unused : seq::RemoveUnusedClips(m_doc))
            {
                changes.push_back(seq::MakeClipPresenceChange(std::move(unused), false));
            }

            m_armed.erase(sourceId);
            m_morePlacements.clear();
            m_selectedPlacement = SIZE_MAX;

            if (m_inspectorTrackId == sourceId)
            {
                HideInspector();
            }

            if (!m_editorClipId.empty() && (m_editorTrackId == sourceId || seq::FindClip(m_doc, m_editorClipId) == nullptr))
            {
                CloseEditor();
            }

            Commit(std::wstring{ res::GetString(L"UndoMergeTracks") }, std::move(changes));
            SelectTrack(destinationId);

            if (result.LanesSkipped > 0)
            {
                ShowMessage(res::FormatString(L"MergeTrackLanesSkippedFormat", static_cast<uint32_t>(result.LanesSkipped)));
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to merge tracks.")
    }

    _Use_decl_annotations_
    void MainWindow::AddMergeTrackMenu(controls::MenuFlyout const& menu, std::wstring const& trackId)
    {
        controls::MenuFlyoutSubItem sub{};
        sub.Text(res::GetString(L"MenuMergeInto"));

        controls::FontIcon icon{};
        icon.Glyph(L"\uEA3C");
        sub.Icon(icon);

        auto weak = get_weak();

        for (auto const& track : m_doc.Tracks)
        {
            if (track.IsFolder || track.Id == trackId)
            {
                continue;
            }

            controls::MenuFlyoutItem item{};
            item.Text(winrt::hstring{ track.Name });

            auto const destinationId = track.Id;
            item.Click([weak, trackId, destinationId](auto&&, auto&&)
            {
                if (auto s = weak.get()) { s->MergeTrackInto(trackId, destinationId); }
            });

            sub.Items().Append(item);
        }

        sub.IsEnabled(sub.Items().Size() > 0);
        menu.Items().Append(sub);
    }
}
