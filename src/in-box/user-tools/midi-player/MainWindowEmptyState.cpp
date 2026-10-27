// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The window with nothing queued, and putting it back that way when the last file goes. Split
// from MainWindowPlayback.cpp.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "PlayQueue.h"

namespace winrt::midiplayer::implementation
{
    namespace
    {
        // The roll's row when there is a file to show.
        constexpr double RollRowMinimumHeight = 140.0;
    }

    // Everything that describes one file goes with it, so nothing from the last file is left on
    // screen, or held in memory, once it is no longer the current one.
    void MainWindow::UnloadCurrentFile() noexcept
    {
        try
        {
            m_engine.Unload();
            m_currentSequence.reset();
            m_currentSequenceId.clear();
            m_pendingAutoPlay = false;

            m_songInfoSequence.reset();
            m_songInfoSequenceId.clear();
            SongInfoFlyout().Hide();

            StopPositionTimer();

            // Clears the track rail, empties both note views and hides them.
            RebuildTrackList();

            UpdateNowPlayingText();
            UpdateTransportState();

            // Hides the tempo, chord and lyric panels, and rewinds the clock.
            UpdatePositionDisplay();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to clear the file that was playing.")
    }

    // With nothing queued there is nothing for the title, the transport or the note views to
    // show, so only the queue, with Add files and its empty message, and the output pickers stay.
    // The output pickers stay so an instrument can be chosen before any file is added.
    void MainWindow::UpdateEmptyLayout() noexcept
    {
        try
        {
            auto const empty = m_queue.IsEmpty();

            NowPlayingPanel().Visibility(empty ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            // The empty message takes the space the note views had, and sits in the middle of it.
            RollRow().MinHeight(empty ? 0.0 : RollRowMinimumHeight);
            RollRow().Height(empty
                ? xaml::GridLengthHelper::FromPixels(0.0)
                : xaml::GridLengthHelper::FromValueAndType(1.0, xaml::GridUnitType::Star));

            auto const queueHeight = empty
                ? xaml::GridLengthHelper::FromValueAndType(1.0, xaml::GridUnitType::Star)
                : xaml::GridLengthHelper::Auto();

            QueueRow().Height(queueHeight);
            QueueBodyRow().Height(queueHeight);

            UpdateQueueVisibility();

            // The control that had focus, the remove button on the last file say, has just gone.
            // Add files is the one thing left to do.
            if (empty && !m_emptyLayoutShown && m_initialized)
            {
                OpenFilesButton().Focus(xaml::FocusState::Programmatic);
            }

            m_emptyLayoutShown = empty;
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to arrange the window for an empty queue.")
    }
}
