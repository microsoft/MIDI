// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The launcher's state as the window shows it, and what waits for MIDI to start (October 20,
// Pete: launches made no sound, and Back to timeline seemed to do nothing).
//
// - The engine keeps what each track plays, a launched clip, nothing, or its timeline, while
//   playback is stopped too, and a change made while stopped takes effect at once. The window
//   reads it back after every change, so the launcher and the "Playing a launched clip" bands
//   always show what Play will play. Before, they were read only while playing, so after a stop
//   they showed nothing or what played last, and Back to timeline looked like it did nothing.
// - Opening a MIDI session waits for the list of endpoints, which takes several seconds on a PC
//   with many devices. A launch or Play pressed in that time used to be lost without a word; now
//   it waits, and happens as soon as MIDI is ready.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    void MainWindow::RefreshLaunchViews() noexcept
    {
        try
        {
            m_launchViews.clear();

            if (m_engine != nullptr)
            {
                for (auto& view : m_engine->LaunchState())
                {
                    m_launchViews.emplace(view.TrackId, std::move(view));
                }
            }

            UpdateTransport();
            InvalidateLaunchers();
            InvalidateArrange();

            if (m_engine != nullptr && m_engine->IsPlaying() && m_frameTimer != nullptr)
            {
                m_frameTimer.Start();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show what the launcher plays.")
    }

    _Use_decl_annotations_
    bool MainWindow::WaitForMidi(std::function<void()> action) noexcept
    {
        try
        {
            if (m_engine != nullptr)
            {
                return false;
            }

            // The last few, in order: pressing the same thing again while waiting does it once.
            if (m_waitingForMidi.size() >= 8)
            {
                m_waitingForMidi.erase(m_waitingForMidi.begin());
            }

            m_waitingForMidi.push_back(std::move(action));
            ShowMessage(res::GetString(L"MidiStarting"));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to wait for MIDI.")

        return true;
    }

    void MainWindow::RunWaitingForMidi() noexcept
    {
        try
        {
            std::vector<std::function<void()>> waiting{};
            waiting.swap(m_waitingForMidi);

            for (auto const& action : waiting)
            {
                if (m_engine == nullptr || m_closing)
                {
                    break;
                }

                action();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to do what waited for MIDI.")
    }
}
