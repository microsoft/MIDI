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
#include "GeneralMidi.h"
#include "StringResources.h"

namespace native = ::midiplayer;
namespace res = ::midiplayer::resources;

namespace winrt::midiplayer::implementation
{
    namespace
    {
        constexpr double PositionSliderSteps = 1000.0;

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
                    res::FormatString(L"StatusFilesNotPlayableFormat", static_cast<uint32_t>(refused)),
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
                res::FormatString(L"QueueHeaderFormat", static_cast<uint32_t>(entries.size())));

            auto const empty = entries.empty();

            QueueList().Visibility(empty ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
            EmptyQueuePanel().Visibility(empty ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            ClearQueueButton().IsEnabled(!empty);
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
            m_engine.Unload();

            m_currentSequence.reset();
            m_currentSequenceId.clear();

            m_queue.Clear();

            m_noteRoll.SetSequence(nullptr);

            StopPositionTimer();
            RebuildQueueList();
            RebuildTrackList();
            UpdateNowPlayingText();
            UpdateTransportState();
            UpdatePositionDisplay();
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
            auto const show = native::AppSettings::Current().ShowQueue();

            QueuePanel().Visibility(show ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
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

            if (m_currentSequence == nullptr)
            {
                RollPanel().Visibility(xaml::Visibility::Collapsed);
                m_noteRoll.SetSequence(nullptr);

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
                    (track.ChannelMask & (1u << native::PercussionChannelIndex)) != 0;

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
                            ? native::GeneralMidiDrumKitName(program->second.Program)
                            : native::GeneralMidiProgramName(program->second.Program);
                    }
                    else if (data.IsPercussion)
                    {
                        data.PatchName = native::GeneralMidiDrumKitName(0);
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

            auto const host = NoteRollHost();

            m_noteRoll.Render(m_engine.Position().Microseconds, host.ActualWidth(), host.ActualHeight());
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

            ChordText().Text(chord == nullptr
                ? winrt::hstring{}
                : winrt::hstring{ WidenUtf8Text(chord->Text) });
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to update the chord.")
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

    // ------------------------------------------------------------------------------------
    // Transport
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::StartCurrentAsync(bool autoPlay)
    {
        auto strong = get_strong();

        try
        {
            if (m_busy)
            {
                co_return;
            }

            auto const current = m_queue.Current();

            if (!current.has_value() || !current.value().IsPlayable())
            {
                m_engine.Unload();
                m_currentSequence.reset();
                m_currentSequenceId.clear();

                StopPositionTimer();
                UpdateNowPlayingText();
                UpdateTransportState();
                UpdatePositionDisplay();

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
                    openResult = m_engine.Open(endpointDeviceId);

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

            if (openResult != native::OpenResult::Success)
            {
                auto const key = openResult == native::OpenResult::ServiceUnavailable
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

            UpdateCurrentQueueRow();
            RebuildTrackList();
            UpdateNowPlayingText();
            UpdatePositionDisplay();

            if (autoPlay)
            {
                m_engine.Play();
                StartPositionTimer();
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

            auto const fraction = PositionSlider().Value() / PositionSliderSteps;

            m_engine.SeekToMicroseconds(static_cast<uint64_t>(fraction * static_cast<double>(duration)));

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
            // Dragging is handled on release, so this only has to cover the keyboard.
            if (m_suppressPositionHandlers || m_scrubbing)
            {
                return;
            }

            auto const duration = m_engine.Position().DurationMicroseconds;

            if (duration == 0)
            {
                return;
            }

            auto const fraction = args.NewValue() / PositionSliderSteps;

            m_engine.SeekToMicroseconds(static_cast<uint64_t>(fraction * static_cast<double>(duration)));

            UpdatePositionDisplay();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to seek.")
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

            if (elapsedSeconds != m_lastDisplayedSecond)
            {
                m_lastDisplayedSecond = elapsedSeconds;

                ElapsedText().Text(winrt::hstring{ native::FormatDuration(position.Microseconds) });
                DurationTextBlock().Text(winrt::hstring{ native::FormatDuration(position.DurationMicroseconds) });
            }

            if (!m_scrubbing)
            {
                m_suppressPositionHandlers = true;

                PositionSlider().Value(
                    position.DurationMicroseconds == 0
                        ? 0.0
                        : (static_cast<double>(position.Microseconds) / static_cast<double>(position.DurationMicroseconds))
                            * PositionSliderSteps);

                m_suppressPositionHandlers = false;
            }

            PositionSlider().IsEnabled(position.DurationMicroseconds > 0);

            RenderNoteRoll();
            UpdateTrackActivity(position.Tick);
            UpdateChordDisplay(position.Tick);

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
                entry.TrackCount,
                entry.NoteCount,
                static_cast<int32_t>(entry.BeatsPerMinute + 0.5)) };

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
