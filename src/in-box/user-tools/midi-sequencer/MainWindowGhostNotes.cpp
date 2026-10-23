// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Ghost notes (October 22, Pete): with "Other tracks" on, the piano roll shows what the other
// tracks that would be heard play while the edited clip plays on the timeline, faintly and in
// each track's color, behind the clip's own notes. Only the timeline: a clip that's only in the
// launcher has no one place in time to compare with.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "SequenceMerge.h"

namespace winrt::midisequencer::implementation
{
    void MainWindow::UpdateGhostNotes() noexcept
    {
        try
        {
            auto const on = seq::AppSettings::Current().GhostNotes();

            if (EditorGhostToggle().IsChecked().GetBoolean() != on)
            {
                EditorGhostToggle().IsChecked(on);
            }

            // Where the clip plays: the selected placement when it's this clip, or else its first.
            std::optional<seq::Placement> placed{};

            if (on && !m_editorClipId.empty())
            {
                if (auto const track = seq::FindTrack(m_doc, m_editorTrackId); track != nullptr)
                {
                    if (m_selectedTrackId == m_editorTrackId && m_selectedPlacement < track->Timeline.size() &&
                        track->Timeline[m_selectedPlacement].ClipId == m_editorClipId)
                    {
                        placed = track->Timeline[m_selectedPlacement];
                    }
                    else
                    {
                        for (auto const& placement : track->Timeline)
                        {
                            if (placement.ClipId == m_editorClipId)
                            {
                                placed = placement;
                                break;
                            }
                        }
                    }
                }
            }

            auto key = !placed.has_value() ? std::wstring{} :
                std::to_wstring(m_version) + L"|" + m_editorClipId + L"|" + m_editorTrackId + L"|" + std::to_wstring(placed->Tick) + L"|" +
                std::to_wstring(seq::PlacementLength(m_doc, *placed));

            if (key == m_ghostKey)
            {
                return;
            }

            m_ghostKey = std::move(key);

            std::vector<seq::PianoRoll::Ghost> ghosts{};

            if (placed.has_value())
            {
                auto const from = placed->Tick;
                auto const to = from + seq::PlacementLength(m_doc, *placed);

                for (auto const& other : seq::NotesOtherTracksPlay(m_doc, m_editorTrackId, from, to))
                {
                    auto const tint = seq::FromRgb(other.Color);

                    for (auto const& note : other.Notes)
                    {
                        ghosts.push_back(seq::PianoRoll::Ghost{ note.Tick, note.Length, note.Number, tint });
                    }
                }
            }

            m_roll.SetGhosts(std::move(ghosts));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to find the other tracks' notes.")
    }

    _Use_decl_annotations_
    void MainWindow::OnEditorGhostClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            seq::AppSettings::Current().GhostNotes(EditorGhostToggle().IsChecked().GetBoolean());
            m_ghostKey = L"-";
            InvalidateEditor();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show or hide the other tracks' notes.")
    }
}
