// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Recording what a track plays (October 19, Pete): an armed track's take holds what it played
// from launched clips, a generator's notes as it made them included, merged with what came in
// on its Record from, so recording a launcher performance onto the timeline needs no Record from.
// The track's own timeline isn't kept again; it's already in the sequence.

#include "pch.h"
#include "MainWindow.xaml.h"

namespace winrt::midisequencer::implementation
{
    // The armed tracks, for the engine to keep what they play while recording; none to stop.
    void MainWindow::SetRecordedTracks(bool recording) noexcept
    {
        try
        {
            if (m_engine == nullptr)
            {
                return;
            }

            std::unordered_set<std::wstring> tracks{};

            if (recording)
            {
                std::scoped_lock guard{ m_inputLock };

                for (auto const& [trackId, take] : m_takes)
                {
                    tracks.insert(trackId);
                }
            }

            m_engine->SetRecordedTracks(std::move(tracks));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to set the tracks being recorded.")
    }

    // Each frame, and once more as recording stops: what the recorded tracks played goes into
    // their takes, up to the end of the take.
    _Use_decl_annotations_
    void MainWindow::RecordPlayedNotes(int64_t until) noexcept
    {
        try
        {
            if (m_engine == nullptr)
            {
                return;
            }

            auto const played = m_engine->TakePlayed();

            if (played.empty())
            {
                return;
            }

            // Places on the timeline, worked out before the input lock: the engine has its own.
            std::vector<int64_t> ticks(played.size(), -1);

            for (size_t i = 0; i < played.size(); ++i)
            {
                ticks[i] = m_engine->TickAtTime(played[i].Timestamp);
            }

            std::scoped_lock guard{ m_inputLock };

            for (size_t i = 0; i < played.size(); ++i)
            {
                if (ticks[i] < 0 || ticks[i] > until)
                {
                    continue;
                }

                if (auto take = m_takes.find(played[i].TrackId); take != m_takes.end())
                {
                    // No words: where a launch or a stop cut the track's clip off.
                    if (played[i].WordCount == 0)
                    {
                        take->second.CutPlayed(ticks[i]);
                    }
                    else
                    {
                        take->second.AddPlayed(ticks[i], played[i].Words.data(), played[i].WordCount);
                    }
                }
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to record what a track played.")
    }
}
