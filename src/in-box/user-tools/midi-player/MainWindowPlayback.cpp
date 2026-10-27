// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The queue and the transport. Split from MainWindow.xaml.cpp, which holds the window, the
// endpoint list and the startup path.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "BackgroundWork.h"
#include "ChordText.h"
#include "GeneralMidi.h"
#include "StringResources.h"
#include "TimeText.h"

#include <limits>

namespace native = ::midiplayer;
namespace res = ::midiplayer::resources;

namespace winrt::midiplayer::implementation
{
    namespace
    {
        // The position slider counts seconds; the engine counts microseconds.
        constexpr double MicrosecondsPerSecond = 1000000.0;

        // Clamped to the file's length, because a slider at its far end can land just past it.
        uint64_t MicrosecondsFromSliderSeconds(_In_ double seconds, _In_ uint64_t durationMicroseconds) noexcept
        {
            if (!(seconds > 0.0))
            {
                return 0;
            }

            auto const microseconds = seconds * MicrosecondsPerSecond;

            return microseconds >= static_cast<double>(durationMicroseconds)
                ? durationMicroseconds
                : static_cast<uint64_t>(microseconds);
        }

        // The speed slider counts in percent of the speed the file was written at.
        constexpr double SpeedSliderWrittenSpeed = 100.0;

        // How far one press of a seek or speed shortcut moves. Seeking is in the file's own time,
        // so five seconds is five seconds of music at any speed.
        constexpr int64_t ShortcutSeekMicroseconds = 5'000'000;
        constexpr double ShortcutSpeedStepPercent = 5.0;

        // A key change from the keyboard is a semitone at a time.
        constexpr int32_t ShortcutTransposeStepSemitones = 1;

        // Speed and transpose sit this far apart side by side, and this far apart stacked.
        constexpr double SpeedPanelSideBySideSpacing = 24.0;
        constexpr double SpeedPanelStackedSpacing = 6.0;

        // The transpose slider counts semitones. It only stops on whole ones, but a value is
        // still rounded and held to the engine's limits before it is used.
        int32_t SemitonesFromSlider(_In_ double value) noexcept
        {
            if (std::isnan(value))
            {
                return 0;
            }

            auto const clamped = std::clamp(
                value,
                static_cast<double>(native::PlaybackEngine::MinimumTransposeSemitones),
                static_cast<double>(native::PlaybackEngine::MaximumTransposeSemitones));

            return static_cast<int32_t>(std::lround(clamped));
        }

        // A screen reader already reads the new value of a slider that has focus, so a shortcut
        // that changes that slider says nothing more, or the value would be heard twice.
        bool HasKeyboardFocus(_In_ controls::Control const& control) noexcept
        {
            try
            {
                return control.FocusState() != xaml::FocusState::Unfocused;
            }
            catch (...)
            {
                return false;
            }
        }

        // Groups the shortcut announcements, so a newer one replaces one not yet spoken.
        constexpr wchar_t ShortcutAnnouncementActivity[] = L"midiplayer.shortcut";

        constexpr wchar_t PlayGlyph[] = L"\uE768";
        constexpr wchar_t PauseGlyph[] = L"\uE769";

        std::wstring WidenUtf8Text(std::string const& value) noexcept
        {
            if (value.empty())
            {
                return {};
            }

            auto const required = ::MultiByteToWideChar(
                CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);

            if (required <= 0)
            {
                return {};
            }

            std::wstring wide(static_cast<size_t>(required), L'\0');

            ::MultiByteToWideChar(
                CP_UTF8, 0, value.data(), static_cast<int>(value.size()), wide.data(), required);

            return wide;
        }

        // The key signature in force at a tick, as sharps, or flats when negative. C major when
        // the file has none, which most do not.
        int32_t KeySignatureAtTick(_In_ midifile::MidiSequence const& sequence, _In_ uint32_t tick) noexcept
        {
            auto const& changes = sequence.KeySignatureMap;

            auto const after = std::upper_bound(
                changes.begin(),
                changes.end(),
                tick,
                [](uint32_t value, midifile::KeySignatureChange const& change) noexcept { return value < change.Tick; });

            return after == changes.begin() ? 0 : static_cast<int32_t>(std::prev(after)->Accidentals);
        }

        winrt::hstring TagOfSender(foundation::IInspectable const& sender) noexcept
        {
            try
            {
                if (auto const element = sender.try_as<xaml::FrameworkElement>())
                {
                    if (auto const tag = element.Tag())
                    {
                        return winrt::unbox_value_or<winrt::hstring>(tag, winrt::hstring{});
                    }
                }
            }
            catch (...)
            {
            }

            return {};
        }
    }

    // ------------------------------------------------------------------------------------
    // Adding files
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::AddFilesAsync(std::vector<std::wstring> paths, bool playWhenReady)
    {
        auto strong = get_strong();

        try
        {
            if (paths.empty())
            {
                co_return;
            }

            auto const startedEmpty = m_queue.IsEmpty();

            std::vector<std::wstring> addedIds{};

            // Every file is read for its title, length and note count, which is file work and
            // does not belong on the thread that draws.
            co_await native::RunOnBackgroundAsync([this, paths, &addedIds]()
                {
                    for (auto const& path : paths)
                    {
                        addedIds.push_back(m_queue.Add(path));
                    }
                });

            RebuildQueueList();
            UpdateTransportState();

            auto const refused = [this]() noexcept
                {
                    size_t count = 0;

                    for (auto const& entry : m_queue.Snapshot())
                    {
                        if (!entry.IsPlayable())
                        {
                            ++count;
                        }
                    }

                    return count;
                }();

            if (refused > 0)
            {
                ShowStatus(
                    res::FormatCount(L"StatusFilesNotPlayableOne", L"StatusFilesNotPlayableFormat", static_cast<uint32_t>(refused)),
                    controls::InfoBarSeverity::Warning);
            }

            if (playWhenReady)
            {
                // Opening a file means play that file, even if something else is already going.
                auto const entries = m_queue.Snapshot();

                for (auto const& id : addedIds)
                {
                    auto const found = std::find_if(
                        entries.begin(),
                        entries.end(),
                        [&id](native::QueueEntry const& entry) noexcept { return entry.Id == id; });

                    if (found != entries.end() && found->IsPlayable())
                    {
                        m_queue.SetCurrent(id);
                        break;
                    }
                }

                UpdateCurrentQueueRow();
                StartCurrentAsync(true);
            }
            else if (startedEmpty)
            {
                auto const current = m_queue.Current();

                if (current.has_value() && !current.value().IsPlayable())
                {
                    m_queue.MoveNext(false);
                    UpdateCurrentQueueRow();
                }

                // Loaded and ready, but silent until the customer presses play.
                StartCurrentAsync(false);
            }
            else
            {
                UpdateNowPlayingText();
            }
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to add files to the queue.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnOpenFilesClick(foundation::IInspectable sender, xaml::RoutedEventArgs args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        auto strong = get_strong();

        try
        {
            winrt::Windows::Storage::Pickers::FileOpenPicker picker{};

            // A picker in a desktop app has no window of its own to sit over.
            if (auto const initialize = picker.try_as<::IInitializeWithWindow>())
            {
                initialize->Initialize(WindowHandle());
            }

            picker.ViewMode(winrt::Windows::Storage::Pickers::PickerViewMode::List);
            picker.SuggestedStartLocation(winrt::Windows::Storage::Pickers::PickerLocationId::MusicLibrary);

            picker.FileTypeFilter().Append(L".mid");
            picker.FileTypeFilter().Append(L".midi");
            picker.FileTypeFilter().Append(L".smf");
            picker.FileTypeFilter().Append(L".rmi");
            picker.FileTypeFilter().Append(L".kar");

            auto const files = co_await picker.PickMultipleFilesAsync();

            if (files == nullptr || files.Size() == 0)
            {
                co_return;
            }

            std::vector<std::wstring> paths{};

            for (auto const& file : files)
            {
                paths.emplace_back(file.Path());
            }

            AddFilesAsync(paths, false);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to open files.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRootDragOver(foundation::IInspectable const&, xaml::DragEventArgs const& args)
    {
        try
        {
            if (args.DataView() != nullptr &&
                args.DataView().Contains(winrt::Windows::ApplicationModel::DataTransfer::StandardDataFormats::StorageItems()))
            {
                args.AcceptedOperation(winrt::Windows::ApplicationModel::DataTransfer::DataPackageOperation::Copy);
                args.DragUIOverride().Caption(res::GetString(L"DropCaption"));
                args.DragUIOverride().IsContentVisible(true);
            }
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to handle the drag.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnRootDrop(foundation::IInspectable sender, xaml::DragEventArgs args)
    {
        UNREFERENCED_PARAMETER(sender);

        auto strong = get_strong();

        try
        {
            if (args.DataView() == nullptr ||
                !args.DataView().Contains(winrt::Windows::ApplicationModel::DataTransfer::StandardDataFormats::StorageItems()))
            {
                co_return;
            }

            // The args are released once this handler returns at the first suspend, so anything
            // needed afterwards is taken now.
            auto const deferral = args.GetDeferral();
            auto const items = co_await args.DataView().GetStorageItemsAsync();

            std::vector<std::wstring> paths{};

            if (items != nullptr)
            {
                for (auto const& item : items)
                {
                    if (auto const file = item.try_as<winrt::Windows::Storage::StorageFile>())
                    {
                        std::wstring const path{ file.Path() };

                        if (midifile::IsStandardMidiFileExtension(path))
                        {
                            paths.push_back(path);
                        }
                    }
                }
            }

            deferral.Complete();

            if (paths.empty())
            {
                ShowStatus(res::GetString(L"StatusNothingToPlayDropped"), controls::InfoBarSeverity::Informational);
                co_return;
            }

            AddFilesAsync(paths, false);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to handle the dropped files.")
    }

    // ------------------------------------------------------------------------------------
    // The queue
    // ------------------------------------------------------------------------------------

    void MainWindow::RebuildQueueList() noexcept
    {
        try
        {
            if (m_items == nullptr)
            {
                return;
            }

            auto const entries = m_queue.Snapshot();
            auto const currentId = m_queue.CurrentId();

            m_items.Clear();

            for (auto const& entry : entries)
            {
                auto item = winrt::make_self<implementation::QueueItem>();

                item->Update(entry);
                item->IsCurrent(entry.Id == currentId);

                m_items.Append(*item);
            }

            QueueHeaderText().Text(
                res::FormatCount(L"QueueHeaderOne", L"QueueHeaderFormat", static_cast<uint32_t>(entries.size())));

            auto const empty = entries.empty();

            QueueList().Visibility(empty ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
            EmptyQueuePanel().Visibility(empty ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            ClearQueueButton().IsEnabled(!empty);

            UpdateEmptyLayout();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to rebuild the queue list.")
    }

    void MainWindow::UpdateCurrentQueueRow() noexcept
    {
        try
        {
            if (m_items == nullptr)
            {
                return;
            }

            auto const currentId = m_queue.CurrentId();

            for (uint32_t index = 0; index < m_items.Size(); ++index)
            {
                auto const item = m_items.GetAt(index);

                if (item == nullptr)
                {
                    continue;
                }

                auto* const implementation = winrt::get_self<implementation::QueueItem>(item);

                if (implementation != nullptr)
                {
                    implementation->IsCurrent(std::wstring{ implementation->Id() } == currentId);
                }
            }
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to update the current queue row.")
    }

    _Use_decl_annotations_
    void MainWindow::OnClearQueueClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            m_engine.Stop();
            m_queue.Clear();

            RebuildQueueList();
            UnloadCurrentFile();
            ClearStatus();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to clear the queue.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRemoveQueueItemClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            std::wstring const id{ TagOfSender(sender) };

            if (id.empty())
            {
                return;
            }

            auto const wasCurrent = m_queue.CurrentId() == id;
            auto const wasPlaying = m_engine.State() == native::PlaybackState::Playing;

            m_queue.Remove(id);

            RebuildQueueList();

            if (wasCurrent)
            {
                m_engine.Stop();
                m_currentSequence.reset();
                m_currentSequenceId.clear();

                StartCurrentAsync(wasPlaying);
            }

            UpdateTransportState();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to remove the file from the queue.")
    }

    _Use_decl_annotations_
    void MainWindow::OnQueueItemDoubleTapped(
        foundation::IInspectable const&,
        input::DoubleTappedRoutedEventArgs const& args)
    {
        try
        {
            auto const source = args.OriginalSource().try_as<xaml::FrameworkElement>();

            if (source == nullptr)
            {
                return;
            }

            auto const item = source.DataContext().try_as<winrt::midiplayer::QueueItem>();

            if (item == nullptr || !item.IsPlayable())
            {
                return;
            }

            if (m_queue.SetCurrent(std::wstring{ item.Id() }))
            {
                UpdateCurrentQueueRow();
                StartCurrentAsync(true);
            }
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to play the chosen file.")
    }

    void MainWindow::UpdateQueueVisibility() noexcept
    {
        try
        {
            // With nothing queued, the empty queue message is all the window has to show.
            auto const show = native::AppSettings::Current().ShowQueue() || m_queue.IsEmpty();

            QueueBody().Visibility(show ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to show or hide the queue.")
    }

    // ------------------------------------------------------------------------------------
    // Tracks and the note display
    // ------------------------------------------------------------------------------------

    void MainWindow::RebuildTrackList() noexcept
    {
        try
        {
            if (m_tracks == nullptr)
            {
                return;
            }

            m_tracks.Clear();

            // The cached pointer points into the previous sequence's text events.
            m_lastChord = nullptr;
            m_lastLyric = nullptr;

            if (m_currentSequence == nullptr)
            {
                RollPanel().Visibility(xaml::Visibility::Collapsed);
                m_noteRoll.SetSequence(nullptr);
                m_keyboardRoll.SetSequence(nullptr);

                return;
            }

            RollPanel().Visibility(xaml::Visibility::Visible);

            // The last program selected on a track before anything played is the sound its notes
            // will be heard with, so that is the one worth showing.
            std::map<uint16_t, midifile::ProgramChangeEvent> firstProgram{};

            for (auto const& change : m_currentSequence->ProgramChanges)
            {
                firstProgram.try_emplace(change.TrackIndex, change);
            }

            for (uint16_t index = 0; index < m_currentSequence->Tracks.size(); ++index)
            {
                auto const& track = m_currentSequence->Tracks[index];

                // A conductor track carries tempo and names but no notes; showing it as a
                // mutable row would be offering to silence something that makes no sound.
                if (track.NoteCount == 0)
                {
                    continue;
                }

                native::TrackRowData data{};

                data.TrackIndex = index;
                data.DisplayName = WidenUtf8Text(track.Name);
                data.NoteCount = track.NoteCount;
                data.ChannelMask = track.ChannelMask;
                data.IsPercussion =
                    (track.ChannelMask & (1u << midiapp::PercussionChannelIndex)) != 0;

                // Name precedence is: the file's own name, then the device's name for that bank
                // and program over MIDI-CI Property Exchange, then General MIDI. The middle one
                // is not wired up yet - see GeneralMidi.h.
                if (!track.InstrumentName.empty())
                {
                    data.PatchName = WidenUtf8Text(track.InstrumentName);
                }
                else
                {
                    auto const program = firstProgram.find(index);

                    if (program != firstProgram.end())
                    {
                        data.PatchName = data.IsPercussion
                            ? midiapp::GeneralMidiDrumKitName(program->second.Program)
                            : midiapp::GeneralMidiProgramName(program->second.Program);
                    }
                    else if (data.IsPercussion)
                    {
                        data.PatchName = midiapp::GeneralMidiDrumKitName(0);
                    }
                }

                auto item = winrt::make_self<implementation::TrackItem>();

                item->Update(data);

                m_tracks.Append(*item);
            }

            ApplyTrackStatesToEngine();
            RenderNoteRoll();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to rebuild the track list.")
    }

    void MainWindow::ApplyTrackStatesToEngine() noexcept
    {
        try
        {
            if (m_tracks == nullptr || m_currentSequence == nullptr)
            {
                return;
            }

            std::vector<bool> audible(m_currentSequence->Tracks.size(), true);

            for (uint32_t index = 0; index < m_tracks.Size(); ++index)
            {
                auto const item = m_tracks.GetAt(index);

                if (item == nullptr)
                {
                    continue;
                }

                auto const trackIndex = static_cast<uint16_t>(item.TrackIndex());

                if (trackIndex < audible.size())
                {
                    audible[trackIndex] = m_engine.IsTrackAudible(trackIndex);
                }
            }

            m_noteRoll.SetAudibleTracks(audible);
            m_keyboardRoll.SetAudibleTracks(audible);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to apply the track states.")
    }

    void MainWindow::RefreshTrackRowStates() noexcept
    {
        try
        {
            if (m_tracks == nullptr)
            {
                return;
            }

            auto const solo = m_engine.SoloTrack();

            for (uint32_t index = 0; index < m_tracks.Size(); ++index)
            {
                auto const item = m_tracks.GetAt(index);

                if (item == nullptr)
                {
                    continue;
                }

                auto* const implementation = winrt::get_self<implementation::TrackItem>(item);

                if (implementation == nullptr)
                {
                    continue;
                }

                auto const trackIndex = static_cast<uint16_t>(implementation->TrackIndex());

                implementation->IsMuted(m_engine.IsTrackMuted(trackIndex));
                implementation->IsSolo(solo >= 0 && trackIndex == static_cast<uint16_t>(solo));
                implementation->SetSilencedByOther(!m_engine.IsTrackAudible(trackIndex));
            }

            ApplyTrackStatesToEngine();
            RenderNoteRoll();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to refresh the track rows.")
    }

    _Use_decl_annotations_
    void MainWindow::OnTrackMuteClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const element = sender.try_as<xaml::FrameworkElement>();

            if (element == nullptr || element.Tag() == nullptr)
            {
                return;
            }

            auto const trackIndex = static_cast<uint16_t>(
                winrt::unbox_value_or<int32_t>(element.Tag(), 0));

            m_engine.SetTrackMuted(trackIndex, !m_engine.IsTrackMuted(trackIndex));

            RefreshTrackRowStates();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to mute the track.")
    }

    _Use_decl_annotations_
    void MainWindow::OnTrackSoloClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const element = sender.try_as<xaml::FrameworkElement>();

            if (element == nullptr || element.Tag() == nullptr)
            {
                return;
            }

            auto const trackIndex = static_cast<int32_t>(
                winrt::unbox_value_or<int32_t>(element.Tag(), 0));

            // Soloing the track that is already soloed turns it off, which is what every mixer
            // does and what the toggle looks like it should do.
            m_engine.SetSoloTrack(m_engine.SoloTrack() == trackIndex ? -1 : trackIndex);

            RefreshTrackRowStates();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to solo the track.")
    }

    _Use_decl_annotations_
    void MainWindow::OnNoteRollSizeChanged(foundation::IInspectable const&, xaml::SizeChangedEventArgs const&)
    {
        RenderNoteRoll();
    }

    void MainWindow::RenderNoteRoll() noexcept
    {
        try
        {
            if (m_currentSequence == nullptr)
            {
                return;
            }

            // Only the view on screen is drawn; the other one costs nothing until it is shown.
            if (m_keyboardView)
            {
                auto const host = KeyboardRollHost();

                m_keyboardRoll.Render(m_engine.Position().Microseconds, host.ActualWidth(), host.ActualHeight());
            }
            else
            {
                auto const host = NoteRollHost();

                m_noteRoll.Render(m_engine.Position().Microseconds, host.ActualWidth(), host.ActualHeight());
            }
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to draw the notes.")
    }

    void MainWindow::UpdateTrackActivity(uint32_t tick) noexcept
    {
        try
        {
            if (m_currentSequence == nullptr || m_tracks == nullptr)
            {
                return;
            }

            m_currentSequence->CollectSoundingNoteCounts(tick, m_soundingCounts);

            for (uint32_t index = 0; index < m_tracks.Size(); ++index)
            {
                auto const item = m_tracks.GetAt(index);

                if (item == nullptr)
                {
                    continue;
                }

                auto* const implementation = winrt::get_self<implementation::TrackItem>(item);

                if (implementation == nullptr)
                {
                    continue;
                }

                auto const trackIndex = static_cast<size_t>(implementation->TrackIndex());

                auto const count = (trackIndex < m_soundingCounts.size() && implementation->IsAudible())
                    ? m_soundingCounts[trackIndex]
                    : uint8_t{ 0 };

                implementation->SetSoundingNoteCount(count);
            }
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to update the track activity.")
    }

    void MainWindow::UpdateTempoDisplay(double beatsPerMinute) noexcept
    {
        try
        {
            if (m_currentSequence == nullptr)
            {
                TempoPanel().Visibility(xaml::Visibility::Collapsed);
                m_lastDisplayedTempo = -1;

                return;
            }

            TempoPanel().Visibility(xaml::Visibility::Visible);

            auto const rounded = static_cast<int32_t>(beatsPerMinute + 0.5);

            if (rounded == m_lastDisplayedTempo)
            {
                return;
            }

            m_lastDisplayedTempo = rounded;

            TempoText().Text(res::FormatString(L"LiveTempoFormat", rounded));
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to update the tempo.")
    }

    void MainWindow::UpdateChordDisplay(uint32_t tick) noexcept
    {
        try
        {
            if (m_currentSequence == nullptr || m_currentSequence->ChordSymbolIndexes.empty())
            {
                ChordPanel().Visibility(xaml::Visibility::Collapsed);
                m_lastChord = nullptr;

                return;
            }

            ChordPanel().Visibility(xaml::Visibility::Visible);

            auto const* const chord = m_currentSequence->ChordSymbolAtTick(tick);

            if (chord == m_lastChord)
            {
                return;
            }

            m_lastChord = chord;

            if (chord == nullptr)
            {
                ChordText().Text(winrt::hstring{});
                return;
            }

            auto const semitones = m_engine.TransposeSemitones();

            if (semitones == 0)
            {
                ChordText().Text(winrt::hstring{ WidenUtf8Text(chord->Text) });
                return;
            }

            // The chord the music plays now, spelled with the flats or sharps of the key it moved
            // to.
            auto const keySignature = native::TransposeKeySignature(
                KeySignatureAtTick(*m_currentSequence, chord->Tick),
                semitones);

            ChordText().Text(winrt::hstring{ WidenUtf8Text(native::TransposeChordSymbol(chord->Text, semitones, keySignature)) });
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to update the chord.")
    }

    void MainWindow::UpdateLyricDisplay(uint32_t tick) noexcept
    {
        try
        {
            if (m_currentSequence == nullptr || m_currentSequence->LyricLines.empty())
            {
                LyricText().Visibility(xaml::Visibility::Collapsed);
                m_lastLyric = nullptr;

                return;
            }

            LyricText().Visibility(xaml::Visibility::Visible);

            auto const* const line = m_currentSequence->LyricLineAtTick(tick);

            if (line == m_lastLyric)
            {
                return;
            }

            m_lastLyric = line;

            LyricText().Text(line == nullptr
                ? winrt::hstring{}
                : winrt::hstring{ WidenUtf8Text(line->Text) });
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to update the lyrics.")
    }

    _Use_decl_annotations_
    void MainWindow::OnQueueToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            native::AppSettings::Current().ShowQueue(QueueToggle().IsChecked().GetBoolean());
            UpdateQueueVisibility();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to change the queue setting.")
    }

    _Use_decl_annotations_
    void MainWindow::OnKeyboardViewToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            m_keyboardView = KeyboardViewToggle().IsChecked().GetBoolean();

            native::AppSettings::Current().KeyboardView(m_keyboardView);

            ApplyViewMode();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to change the view.")
    }

    void MainWindow::ApplyViewMode() noexcept
    {
        try
        {
            NoteRollHost().Visibility(m_keyboardView ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
            KeyboardRollHost().Visibility(m_keyboardView ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            RenderNoteRoll();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to switch the view.")
    }

    // ------------------------------------------------------------------------------------
    // Transport
    // ------------------------------------------------------------------------------------

    // One session for the life of the window. Endpoints come and go as the customer picks them,
    // but a session is what the service uses to attribute connections to this app, so making a
    // fresh one per file would have the player appear and disappear in the service's session
    // list for no reason.
    bool MainWindow::EnsureSession() noexcept
    {
        try
        {
            if (m_session != nullptr && m_session.IsOpen())
            {
                return true;
            }

            if (!midi2::MidiApi::EnsureServiceAvailable())
            {
                return false;
            }

            m_session = midi2::MidiSession::Create(res::GetString(L"AppDisplayName"));

            return m_session != nullptr;
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to create the MIDI session.")

        return false;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::StartCurrentAsync(bool autoPlay)
    {
        auto strong = get_strong();

        // A waiting "Playing" belongs to this start only, however it ends.
        auto const forgetAnnouncement = wil::scope_exit([this]() noexcept { m_announceNextPlay = false; });

        try
        {
            if (m_busy)
            {
                co_return;
            }

            auto const current = m_queue.Current();

            if (!current.has_value() || !current.value().IsPlayable())
            {
                UnloadCurrentFile();

                co_return;
            }

            auto const endpointDeviceId = SelectedEndpointDeviceId();

            if (endpointDeviceId.empty())
            {
                // Nothing to play to yet. A file opened from Explorer while the endpoint list is
                // still filling in waits for one rather than giving up.
                if (autoPlay && !m_enumerationCompleted)
                {
                    m_pendingAutoPlay = true;
                }
                else
                {
                    ShowStatus(
                        res::GetString(m_savedEndpointMissing ? L"StatusSavedEndpointMissing" : L"StatusChooseEndpoint"),
                        controls::InfoBarSeverity::Warning);
                }

                UpdateNowPlayingText();
                UpdateTransportState();

                co_return;
            }

            auto const entry = current.value();
            auto const groupIndex = native::AppSettings::Current().GroupIndex();
            auto const alreadyLoaded = m_currentSequenceId == entry.Id && m_currentSequence != nullptr;

            m_busy = true;
            UpdateTransportState();

            native::OpenResult openResult{ native::OpenResult::Success };
            bool loaded = alreadyLoaded;

            std::shared_ptr<midifile::MidiSequence const> sequence{ m_currentSequence };

            co_await native::RunOnBackgroundAsync(
                [this, endpointDeviceId, groupIndex, entry, alreadyLoaded, &openResult, &loaded, &sequence]()
                {
                    if (!EnsureSession())
                    {
                        openResult = native::OpenResult::SessionFailed;
                        return;
                    }

                    openResult = m_engine.Open(m_session, endpointDeviceId);

                    if (openResult != native::OpenResult::Success)
                    {
                        return;
                    }

                    if (!alreadyLoaded)
                    {
                        auto parsed = std::make_shared<midifile::MidiSequence>();

                        auto const readResult = midifile::ReadStandardMidiFile(entry.FilePath, *parsed);

                        if (!readResult.Succeeded())
                        {
                            loaded = false;
                            return;
                        }

                        sequence = parsed;
                    }

                    loaded = m_engine.Load(sequence, groupIndex);
                });

            m_busy = false;

            // The file was removed, or the queue cleared, while it loaded.
            if (auto const now = m_queue.Current(); !now.has_value() || now.value().Id != entry.Id)
            {
                UnloadCurrentFile();
                StartCurrentAsync(false);

                co_return;
            }

            if (openResult != native::OpenResult::Success)
            {
                auto const key =
                    (openResult == native::OpenResult::ServiceUnavailable ||
                     openResult == native::OpenResult::SessionFailed)
                    ? L"StatusServiceUnavailable"
                    : L"StatusEndpointConnectFailed";

                ShowStatus(res::GetString(key), controls::InfoBarSeverity::Error);

                UpdateTransportState();
                co_return;
            }

            if (!loaded)
            {
                ShowStatus(res::GetString(L"StatusFileNotPlayable"), controls::InfoBarSeverity::Error);

                UpdateTransportState();
                co_return;
            }

            m_currentSequence = sequence;
            m_currentSequenceId = entry.Id;

            m_noteRoll.SetSequence(m_currentSequence);
            m_keyboardRoll.SetSequence(m_currentSequence);

            UpdateCurrentQueueRow();
            RebuildTrackList();
            UpdateNowPlayingText();
            UpdatePositionDisplay();

            if (autoPlay)
            {
                m_engine.Play();
                StartPositionTimer();

                if (m_announceNextPlay)
                {
                    AnnounceShortcutResult(res::GetString(L"AnnouncePlaying"));
                }
            }

            UpdateTransportState();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to start the current file.")

        m_busy = false;
    }

    void MainWindow::HandlePlaybackCompleted() noexcept
    {
        try
        {
            auto const repeat = native::AppSettings::Current().RepeatQueue();

            if (m_queue.MoveNext(repeat))
            {
                UpdateCurrentQueueRow();
                StartCurrentAsync(true);
                return;
            }

            // Nothing left. The position rewinds so the play button starts it again from the top.
            StopPositionTimer();
            UpdatePositionDisplay();
            UpdateTransportState();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to move to the next file.")
    }

    _Use_decl_annotations_
    void MainWindow::OnPlayPauseClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            switch (m_engine.State())
            {
            case native::PlaybackState::Playing:
                m_engine.Pause();
                StopPositionTimer();
                break;

            case native::PlaybackState::Paused:
            case native::PlaybackState::Stopped:
                m_engine.Play();
                StartPositionTimer();
                break;

            default:
                // Nothing is loaded yet, so this is the first play after files were queued.
                StartCurrentAsync(true);
                return;
            }

            UpdatePositionDisplay();
            UpdateTransportState();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to start or pause playback.")
    }

    _Use_decl_annotations_
    void MainWindow::OnStopClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            m_engine.Stop();

            StopPositionTimer();
            UpdatePositionDisplay();
            UpdateTransportState();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to stop playback.")
    }

    _Use_decl_annotations_
    void MainWindow::OnPreviousClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const wasPlaying = m_engine.State() == native::PlaybackState::Playing;

            // The first press restarts the current file, the way every other player behaves.
            if (m_engine.Position().Microseconds > 3000000 || !m_queue.HasPrevious())
            {
                m_engine.SeekToMicroseconds(0);

                UpdatePositionDisplay();
                return;
            }

            if (m_queue.MovePrevious())
            {
                UpdateCurrentQueueRow();
                StartCurrentAsync(wasPlaying);
            }
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to go to the previous file.")
    }

    _Use_decl_annotations_
    void MainWindow::OnNextClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const wasPlaying = m_engine.State() == native::PlaybackState::Playing;
            auto const repeat = native::AppSettings::Current().RepeatQueue();

            if (m_queue.MoveNext(repeat))
            {
                UpdateCurrentQueueRow();
                StartCurrentAsync(wasPlaying);
            }
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to go to the next file.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRepeatToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            native::AppSettings::Current().RepeatQueue(RepeatToggle().IsChecked().GetBoolean());
            UpdateTransportState();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to change the repeat setting.")
    }

    // ------------------------------------------------------------------------------------
    // Position
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    void MainWindow::OnPositionSliderPointerPressed(
        foundation::IInspectable const&,
        input::PointerRoutedEventArgs const&)
    {
        m_scrubbing = true;
    }

    _Use_decl_annotations_
    void MainWindow::OnPositionSliderPointerReleased(
        foundation::IInspectable const&,
        input::PointerRoutedEventArgs const&)
    {
        try
        {
            if (!m_scrubbing)
            {
                return;
            }

            m_scrubbing = false;

            auto const duration = m_engine.Position().DurationMicroseconds;

            if (duration == 0)
            {
                return;
            }

            m_engine.SeekToMicroseconds(MicrosecondsFromSliderSeconds(PositionSlider().Value(), duration));

            UpdatePositionDisplay();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to seek.")
    }

    _Use_decl_annotations_
    void MainWindow::OnPositionSliderValueChanged(
        foundation::IInspectable const&,
        controls::Primitives::RangeBaseValueChangedEventArgs const& args)
    {
        try
        {
            // Dragging is handled on release, so this only has to cover the keyboard, and a
            // screen reader setting the value.
            if (m_suppressPositionHandlers || m_scrubbing)
            {
                return;
            }

            auto const duration = m_engine.Position().DurationMicroseconds;

            if (duration == 0)
            {
                return;
            }

            m_engine.SeekToMicroseconds(MicrosecondsFromSliderSeconds(args.NewValue(), duration));

            UpdatePositionDisplay();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to seek.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSliderPreviewKeyDown(foundation::IInspectable const& sender, input::KeyRoutedEventArgs const& args)
    {
        try
        {
            // The slider moves by LargeChange when the track beside the thumb is clicked, but does
            // nothing for Page Up and Page Down, which is where a keyboard user looks for it.
            auto const key = args.Key();

            if (key != winrt::Windows::System::VirtualKey::PageUp && key != winrt::Windows::System::VirtualKey::PageDown)
            {
                return;
            }

            auto const slider = sender.try_as<controls::Primitives::RangeBase>();

            if (slider == nullptr || !slider.IsEnabled())
            {
                return;
            }

            auto const step = key == winrt::Windows::System::VirtualKey::PageUp ? slider.LargeChange() : -slider.LargeChange();

            // The slider keeps the value inside its range, and its ValueChanged handler does the
            // rest, exactly as for an arrow key.
            slider.Value(slider.Value() + step);

            args.Handled(true);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to move the slider by a page.")
    }

    // ------------------------------------------------------------------------------------
    // Speed
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    void MainWindow::OnSpeedSliderValueChanged(
        foundation::IInspectable const&,
        controls::Primitives::RangeBaseValueChangedEventArgs const& args)
    {
        try
        {
            // Safe while playing: the engine starts the new speed after what it has already
            // handed to the service, so nothing is cut off.
            m_engine.SetPlaybackRate(args.NewValue() / SpeedSliderWrittenSpeed);

            // The slider takes its first value while the window is still being built, before the
            // text beside it exists.
            if (!m_initialized)
            {
                return;
            }

            UpdateSpeedDisplay();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to change the speed.")
    }

    _Use_decl_annotations_
    void MainWindow::OnResetSpeedClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            // The button turns itself off at 100 percent. Moving focus first means a keyboard or
            // screen reader user is not left on a control that has just gone away.
            SpeedSlider().Focus(xaml::FocusState::Programmatic);
            SpeedSlider().Value(SpeedSliderWrittenSpeed);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to reset the speed.")
    }

    void MainWindow::UpdateSpeedDisplay() noexcept
    {
        try
        {
            auto const percent = static_cast<int32_t>(std::lround(SpeedSlider().Value()));

            SpeedText().Text(res::FormatString(L"SpeedValueFormat", percent));
            ResetSpeedButton().IsEnabled(percent != static_cast<int32_t>(SpeedSliderWrittenSpeed));

            // The tempo shown is the tempo heard, so it follows the slider straight away.
            UpdateTempoDisplay(m_engine.Position().BeatsPerMinute * m_engine.PlaybackRate());
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to show the speed.")
    }

    // ------------------------------------------------------------------------------------
    // Transpose
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    void MainWindow::OnTransposeSliderValueChanged(
        foundation::IInspectable const&,
        controls::Primitives::RangeBaseValueChangedEventArgs const& args)
    {
        try
        {
            auto const semitones = SemitonesFromSlider(args.NewValue());

            // Safe while playing: a note already sounding keeps its pitch until it ends, and
            // drums are never moved.
            m_engine.SetTransposeSemitones(semitones);

            // Both views draw notes where they are heard.
            m_noteRoll.SetTransposeSemitones(semitones);
            m_keyboardRoll.SetTransposeSemitones(semitones);

            if (!m_initialized)
            {
                return;
            }

            UpdateTransposeDisplay();

            // Drawn now, rather than at the next frame, which does not come while paused.
            RenderNoteRoll();

            // The chord on screen is spelled again in the new key.
            m_lastChord = nullptr;
            UpdateChordDisplay(m_engine.Position().Tick);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to change the key.")
    }

    _Use_decl_annotations_
    void MainWindow::OnResetTransposeClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            // The button turns itself off in the original key. Moving focus first means a
            // keyboard or screen reader user is not left on a control that has just gone away.
            TransposeSlider().Focus(xaml::FocusState::Programmatic);
            TransposeSlider().Value(0.0);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to go back to the original key.")
    }

    void MainWindow::UpdateTransposeDisplay() noexcept
    {
        try
        {
            auto const semitones = m_engine.TransposeSemitones();

            if (semitones > 0)
            {
                TransposeText().Text(res::FormatString(L"TransposeValueUpFormat", semitones));
            }
            else if (semitones < 0)
            {
                TransposeText().Text(res::FormatString(L"TransposeValueDownFormat", -semitones));
            }
            else
            {
                TransposeText().Text(res::GetString(L"TransposeValueOriginal"));
            }

            ResetTransposeButton().IsEnabled(semitones != 0);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to show the key.")
    }

    _Use_decl_annotations_
    void MainWindow::OnNowPlayingPanelSizeChanged(foundation::IInspectable const&, xaml::SizeChangedEventArgs const& args)
    {
        try
        {
            // Each group at the width it takes on one line, which is what it takes side by side.
            foundation::Size const unlimited{ std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() };

            SpeedGroup().Measure(unlimited);
            TransposeGroup().Measure(unlimited);

            auto const sideBySide = static_cast<double>(SpeedGroup().DesiredSize().Width)
                + SpeedPanelSideBySideSpacing
                + static_cast<double>(TransposeGroup().DesiredSize().Width);

            // Stacked in a narrow window, or in a language with longer words.
            auto const fits = sideBySide <= static_cast<double>(args.NewSize().Width);

            auto const orientation = fits ? controls::Orientation::Horizontal : controls::Orientation::Vertical;

            if (SpeedPanel().Orientation() != orientation)
            {
                SpeedPanel().Orientation(orientation);
                SpeedPanel().Spacing(fits ? SpeedPanelSideBySideSpacing : SpeedPanelStackedSpacing);
            }
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to arrange the speed and key controls.")
    }

    // ------------------------------------------------------------------------------------
    // Keyboard shortcuts
    //
    // Each shortcut is defined in the XAML on the control it stands in for, so the control's
    // tooltip and its accessible accelerator key describe it, and it is off whenever that control
    // is. Handling Invoked rather than letting the control be invoked is what lets each one say
    // what it did: focus does not move, so a screen reader would otherwise say nothing.
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    void MainWindow::OnPlayPauseShortcut(input::KeyboardAccelerator const&, input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        try
        {
            args.Handled(true);

            // With nothing loaded yet, this press loads the file in the background and starts it
            // from there, so the announcement waits until it has.
            m_announceNextPlay = m_engine.State() == native::PlaybackState::Empty;

            OnPlayPauseClick(nullptr, nullptr);

            switch (m_engine.State())
            {
            case native::PlaybackState::Playing:
                AnnounceShortcutResult(res::GetString(L"AnnouncePlaying"));
                break;

            case native::PlaybackState::Paused:
                AnnounceShortcutResult(res::GetString(L"AnnouncePaused"));
                break;

            default:
                // Still loading. A problem with the file or the endpoint is announced by the
                // status bar.
                break;
            }
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to play or pause from the keyboard.")
    }

    _Use_decl_annotations_
    void MainWindow::OnStopShortcut(input::KeyboardAccelerator const&, input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        try
        {
            args.Handled(true);

            OnStopClick(nullptr, nullptr);

            AnnounceShortcutResult(res::GetString(L"AnnounceStopped"));
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to stop from the keyboard.")
    }

    _Use_decl_annotations_
    void MainWindow::OnPreviousShortcut(input::KeyboardAccelerator const&, input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        try
        {
            args.Handled(true);

            OnPreviousClick(nullptr, nullptr);

            AnnounceCurrentTitle();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to go to the previous file from the keyboard.")
    }

    _Use_decl_annotations_
    void MainWindow::OnNextShortcut(input::KeyboardAccelerator const&, input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        try
        {
            args.Handled(true);

            OnNextClick(nullptr, nullptr);

            AnnounceCurrentTitle();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to go to the next file from the keyboard.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSeekBackShortcut(input::KeyboardAccelerator const&, input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        try
        {
            args.Handled(true);

            SeekFromShortcut(-ShortcutSeekMicroseconds);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to seek back from the keyboard.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSeekForwardShortcut(input::KeyboardAccelerator const&, input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        try
        {
            args.Handled(true);

            SeekFromShortcut(ShortcutSeekMicroseconds);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to seek forward from the keyboard.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSlowerShortcut(input::KeyboardAccelerator const&, input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        try
        {
            args.Handled(true);

            SetSpeedFromShortcut(SpeedSlider().Value() - ShortcutSpeedStepPercent);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to slow down from the keyboard.")
    }

    _Use_decl_annotations_
    void MainWindow::OnFasterShortcut(input::KeyboardAccelerator const&, input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        try
        {
            args.Handled(true);

            SetSpeedFromShortcut(SpeedSlider().Value() + ShortcutSpeedStepPercent);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to speed up from the keyboard.")
    }

    _Use_decl_annotations_
    void MainWindow::OnNormalSpeedShortcut(input::KeyboardAccelerator const&, input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        try
        {
            args.Handled(true);

            SetSpeedFromShortcut(SpeedSliderWrittenSpeed);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to go back to normal speed from the keyboard.")
    }

    _Use_decl_annotations_
    void MainWindow::OnTransposeUpShortcut(input::KeyboardAccelerator const&, input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        try
        {
            args.Handled(true);

            SetTransposeFromShortcut(m_engine.TransposeSemitones() + ShortcutTransposeStepSemitones);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to transpose up from the keyboard.")
    }

    _Use_decl_annotations_
    void MainWindow::OnTransposeDownShortcut(input::KeyboardAccelerator const&, input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        try
        {
            args.Handled(true);

            SetTransposeFromShortcut(m_engine.TransposeSemitones() - ShortcutTransposeStepSemitones);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to transpose down from the keyboard.")
    }

    _Use_decl_annotations_
    void MainWindow::OnOriginalKeyShortcut(input::KeyboardAccelerator const&, input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        try
        {
            args.Handled(true);

            SetTransposeFromShortcut(0);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to go back to the original key from the keyboard.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRepeatShortcut(input::KeyboardAccelerator const&, input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        try
        {
            args.Handled(true);

            auto const repeat = !RepeatToggle().IsChecked().GetBoolean();

            RepeatToggle().IsChecked(repeat);
            OnRepeatToggled(nullptr, nullptr);

            AnnounceShortcutResult(res::GetString(repeat ? L"AnnounceRepeatOn" : L"AnnounceRepeatOff"));
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to change the repeat setting from the keyboard.")
    }

    _Use_decl_annotations_
    void MainWindow::SeekFromShortcut(int64_t offsetMicroseconds) noexcept
    {
        try
        {
            auto const position = m_engine.Position();

            if (position.DurationMicroseconds == 0)
            {
                return;
            }

            auto target = position.Microseconds;

            if (offsetMicroseconds < 0)
            {
                auto const back = static_cast<uint64_t>(-offsetMicroseconds);

                target = target > back ? target - back : uint64_t{ 0 };
            }
            else
            {
                target = (std::min)(target + static_cast<uint64_t>(offsetMicroseconds), position.DurationMicroseconds);
            }

            m_engine.SeekToMicroseconds(target);

            UpdatePositionDisplay();

            AnnouncePosition();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to seek from the keyboard.")
    }

    _Use_decl_annotations_
    void MainWindow::SetSpeedFromShortcut(double percent) noexcept
    {
        try
        {
            // The slider keeps the value inside its range and on a whole step, and changing it
            // is what changes the speed, so the shortcut and the slider cannot disagree.
            SpeedSlider().Value(percent);

            if (HasKeyboardFocus(SpeedSlider()))
            {
                return;
            }

            AnnounceShortcutResult(res::FormatString(
                L"AnnounceSpeedFormat",
                static_cast<int32_t>(std::lround(SpeedSlider().Value()))));
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to change the speed from the keyboard.")
    }

    _Use_decl_annotations_
    void MainWindow::SetTransposeFromShortcut(int32_t semitones) noexcept
    {
        try
        {
            // The slider keeps the value inside its range, and changing it is what changes the
            // key, so the shortcut and the slider cannot disagree.
            TransposeSlider().Value(static_cast<double>(semitones));

            if (HasKeyboardFocus(TransposeSlider()))
            {
                return;
            }

            auto const now = m_engine.TransposeSemitones();

            winrt::hstring message{};

            if (now == 0)
            {
                message = res::GetString(L"AnnounceTransposeOriginal");
            }
            else if (now == 1)
            {
                message = res::GetString(L"AnnounceTransposeUpOne");
            }
            else if (now == -1)
            {
                message = res::GetString(L"AnnounceTransposeDownOne");
            }
            else if (now > 0)
            {
                message = res::FormatString(L"AnnounceTransposeUpFormat", now);
            }
            else
            {
                message = res::FormatString(L"AnnounceTransposeDownFormat", -now);
            }

            AnnounceShortcutResult(message);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to change the key from the keyboard.")
    }

    _Use_decl_annotations_
    void MainWindow::AnnounceShortcutResult(winrt::hstring const& message) noexcept
    {
        try
        {
            // Any element in the window will do; the message is what is read.
            midiapp::AnnounceToScreenReader(NowPlayingTitleText(), message, ShortcutAnnouncementActivity);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to announce the result of a shortcut.")
    }

    void MainWindow::AnnounceCurrentTitle() noexcept
    {
        try
        {
            auto const current = m_queue.Current();

            if (current.has_value())
            {
                AnnounceShortcutResult(winrt::hstring{ current.value().Title });
            }
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to announce the file.")
    }

    void MainWindow::AnnouncePosition() noexcept
    {
        try
        {
            if (PositionSliderHasFocus())
            {
                return;
            }

            auto const position = m_engine.Position();

            AnnounceShortcutResult(res::FormatString(
                L"AnnouncePositionFormat",
                native::FormatDuration(position.Microseconds),
                native::FormatDuration(position.DurationMicroseconds)));
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to announce the position.")
    }

    bool MainWindow::PositionSliderHasFocus() noexcept
    {
        try
        {
            return PositionSlider().FocusState() != xaml::FocusState::Unfocused;
        }
        catch (...)
        {
            return false;
        }
    }

    // ------------------------------------------------------------------------------------
    // Go to time
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    void MainWindow::OnGoToTimeShortcut(input::KeyboardAccelerator const&, input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        try
        {
            args.Handled(true);

            ShowGoToTimeAsync();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to open Go to time.")
    }

    winrt::fire_and_forget MainWindow::ShowGoToTimeAsync()
    {
        auto strong = get_strong();

        try
        {
            if (m_goToTimeOpen)
            {
                co_return;
            }

            auto const position = m_engine.Position();

            if (position.DurationMicroseconds == 0)
            {
                co_return;
            }

            m_goToTimeOpen = true;

            auto const closed = wil::scope_exit([this]() noexcept { m_goToTimeOpen = false; });

            m_goToTimeTarget.reset();

            // Where it is now and how long the file is, so the customer knows what makes sense.
            auto const hint = res::FormatString(
                L"GoToTimeHintFormat",
                native::FormatDuration(position.Microseconds),
                native::FormatDuration(position.DurationMicroseconds));

            GoToTimeHintText().Text(hint);
            xaml::Automation::AutomationProperties::SetHelpText(GoToTimeBox(), hint);

            GoToTimeBox().Text(winrt::hstring{});
            GoToTimeProblemText().Text(winrt::hstring{});

            GoToTimeDialog().XamlRoot(Content().XamlRoot());
            GoToTimeDialog().RequestedTheme(RootGrid().ActualTheme());

            auto const result = co_await GoToTimeDialog().ShowAsync();

            if (result != controls::ContentDialogResult::Primary || !m_goToTimeTarget.has_value())
            {
                co_return;
            }

            m_engine.SeekToMicroseconds(m_goToTimeTarget.value());

            UpdatePositionDisplay();

            AnnouncePosition();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to go to a time.")
    }

    _Use_decl_annotations_
    void MainWindow::OnGoToTimeDialogOpened(controls::ContentDialog const&, controls::ContentDialogOpenedEventArgs const&)
    {
        try
        {
            // Straight into the box, so the customer can type at once.
            GoToTimeBox().Focus(xaml::FocusState::Programmatic);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to focus the time box.")
    }

    _Use_decl_annotations_
    void MainWindow::OnGoToTimePrimaryClick(controls::ContentDialog const&, controls::ContentDialogButtonClickEventArgs const& args)
    {
        try
        {
            auto const duration = m_engine.Position().DurationMicroseconds;
            auto const seconds = native::ParseTimeText(GoToTimeBox().Text());

            // Staying open with the reason read out beats closing and quietly doing nothing.
            if (!seconds.has_value())
            {
                args.Cancel(true);
                ShowGoToTimeProblem(res::GetString(L"GoToTimeNotATime"));
                return;
            }

            auto const target = seconds.value() * static_cast<uint64_t>(MicrosecondsPerSecond);

            if (target > duration)
            {
                args.Cancel(true);
                ShowGoToTimeProblem(res::FormatString(L"GoToTimePastTheEndFormat", native::FormatDuration(duration)));
                return;
            }

            m_goToTimeTarget = target;
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to read the time.")
    }

    _Use_decl_annotations_
    void MainWindow::ShowGoToTimeProblem(winrt::hstring const& message) noexcept
    {
        try
        {
            // Cleared first, so the same problem twice in a row is still read out.
            GoToTimeProblemText().Text(winrt::hstring{});
            GoToTimeProblemText().Text(message);

            GoToTimeBox().SelectAll();
            GoToTimeBox().Focus(xaml::FocusState::Programmatic);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to show the problem with the time.")
    }

    void MainWindow::StartPositionTimer() noexcept
    {
        try
        {
            if (m_positionTimer == nullptr)
            {
                m_positionTimer = xaml::DispatcherTimer{};

                // Fast enough for the notes to scroll smoothly. The text only changes when the
                // displayed second does, so the extra ticks cost almost nothing.
                m_positionTimer.Interval(std::chrono::milliseconds{ 33 });

                m_positionTimer.Tick([weak = get_weak()](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->UpdatePositionDisplay();
                        }
                    });
            }

            m_positionTimer.Start();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to start the position timer.")
    }

    void MainWindow::StopPositionTimer() noexcept
    {
        try
        {
            if (m_positionTimer != nullptr)
            {
                m_positionTimer.Stop();
            }
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to stop the position timer.")
    }

    void MainWindow::UpdatePositionDisplay() noexcept
    {
        try
        {
            auto const position = m_engine.Position();

            auto const elapsedSeconds = position.Microseconds / 1000000;

            auto const timeChanged =
                elapsedSeconds != m_lastDisplayedSecond ||
                position.DurationMicroseconds != m_lastDisplayedDuration;

            if (timeChanged)
            {
                m_lastDisplayedSecond = elapsedSeconds;
                m_lastDisplayedDuration = position.DurationMicroseconds;

                auto const elapsedText = native::FormatDuration(position.Microseconds);
                auto const durationText = native::FormatDuration(position.DurationMicroseconds);

                ElapsedText().Text(winrt::hstring{ elapsedText });
                DurationTextBlock().Text(winrt::hstring{ durationText });

                // Set before the value moves below, because the move is what a screen reader
                // hears, and it reads this text when it does.
                PositionSlider().ValueText(res::FormatString(L"AnnouncePositionFormat", elapsedText, durationText));
            }

            if (!m_scrubbing)
            {
                m_suppressPositionHandlers = true;

                auto const durationSeconds = static_cast<double>(position.DurationMicroseconds) / MicrosecondsPerSecond;

                // The length first, or a position past the old length would be clamped to it.
                if (durationSeconds > 0.0 && PositionSlider().Maximum() != durationSeconds)
                {
                    PositionSlider().Maximum(durationSeconds);
                }

                if (PositionSliderHasFocus())
                {
                    // A screen reader reads a focused slider every time its value changes. Moving
                    // it once a second, when the time it reads changes, keeps it from talking
                    // over the music thirty times a second.
                    if (timeChanged)
                    {
                        PositionSlider().Value(static_cast<double>(elapsedSeconds));
                    }
                }
                else
                {
                    // Nobody is listening to it, so it can move smoothly.
                    PositionSlider().Value(static_cast<double>(position.Microseconds) / MicrosecondsPerSecond);
                }

                m_suppressPositionHandlers = false;
            }

            PositionSlider().IsEnabled(position.DurationMicroseconds > 0);

            RenderNoteRoll();
            UpdateTrackActivity(position.Tick);
            UpdateChordDisplay(position.Tick);
            UpdateLyricDisplay(position.Tick);
            UpdateTempoDisplay(position.BeatsPerMinute * m_engine.PlaybackRate());

            if (position.State != native::PlaybackState::Playing)
            {
                UpdateTransportState();
            }
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to update the position.")
    }

    // ------------------------------------------------------------------------------------
    // What the window says
    // ------------------------------------------------------------------------------------

    void MainWindow::UpdateNowPlayingText() noexcept
    {
        UpdateSongInfoButton();

        try
        {
            auto const current = m_queue.Current();

            if (!current.has_value())
            {
                NowPlayingTitleText().Text(res::GetString(L"NowPlayingNothing"));
                NowPlayingDetailText().Text(winrt::hstring{});

                return;
            }

            auto const& entry = current.value();

            NowPlayingTitleText().Text(winrt::hstring{ entry.Title });

            if (!entry.IsPlayable())
            {
                NowPlayingDetailText().Text(res::GetString(L"QueueItemNotPlayable"));
                return;
            }

            std::wstring detail = std::wstring{ res::FormatString(
                L"NowPlayingDetailFormat",
                std::wstring{ res::FormatCount(L"TrackCountOne", L"TrackCountFormat", entry.TrackCount) },
                std::wstring{ res::FormatCount(L"NoteCountOne", L"NoteCountFormat", entry.NoteCount) }) };

            if (m_currentSequence != nullptr && !m_currentSequence->Copyright.empty())
            {
                auto const copyright = WidenUtf8Text(m_currentSequence->Copyright);

                if (!copyright.empty())
                {
                    detail += L"  \u00B7  " + copyright;
                }
            }

            NowPlayingDetailText().Text(winrt::hstring{ detail });
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to update the now playing text.")
    }

    void MainWindow::UpdateTransportState() noexcept
    {
        try
        {
            auto const state = m_engine.State();
            auto const repeat = native::AppSettings::Current().RepeatQueue();
            auto const hasFiles = !m_queue.IsEmpty();

            auto const playing = state == native::PlaybackState::Playing;

            PlayPauseIcon().Glyph(playing ? PauseGlyph : PlayGlyph);

            PlayPauseButton().IsEnabled(hasFiles && !m_busy);
            PlayPauseButton().SetValue(
                xaml::Automation::AutomationProperties::NameProperty(),
                winrt::box_value(res::GetString(playing ? L"PlayPauseButtonPause" : L"PlayPauseButtonPlay")));

            StopButton().IsEnabled(state == native::PlaybackState::Playing || state == native::PlaybackState::Paused);

            PreviousButton().IsEnabled(hasFiles && !m_busy);
            NextButton().IsEnabled(m_queue.HasNext(repeat) && !m_busy);

            OpenFilesButton().IsEnabled(!m_busy);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to update the transport.")
    }

    _Use_decl_annotations_
    void MainWindow::ShowStatus(winrt::hstring const& message, controls::InfoBarSeverity severity) noexcept
    {
        try
        {
            StatusInfoBar().Severity(severity);
            StatusInfoBar().Message(message);
            StatusInfoBar().IsOpen(true);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to show the status message.")
    }

    void MainWindow::ClearStatus() noexcept
    {
        try
        {
            StatusInfoBar().IsOpen(false);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to clear the status message.")
    }
}
