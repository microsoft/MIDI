// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "BackgroundWork.h"
#include "MessageTranslation.h"
#include "SequenceRender.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        // What Capture can reach back for, per armed track.
        constexpr size_t MaximumHeardMessages = 50000;
        constexpr double HeardSeconds = 120.0;

        bool SameEndpoint(std::wstring const& left, std::wstring const& right) noexcept
        {
            try
            {
                return midiapp::EndpointIdsMatch(winrt::hstring{ left }, winrt::hstring{ right });
            }
            catch (...)
            {
                return false;
            }
        }

        seq::RecordFilter FilterFor(seq::TrackSource const& source) noexcept
        {
            seq::RecordFilter filter{};
            filter.Group = source.Group;
            filter.Channels = source.Channels;
            filter.Notes = (source.Record & seq::RecordNotes) != 0;
            filter.Controllers = (source.Record & seq::RecordControllers) != 0;
            filter.PitchBend = (source.Record & seq::RecordPitchBend) != 0;
            filter.Pressure = (source.Record & seq::RecordPressure) != 0;
            filter.Program = (source.Record & seq::RecordProgram) != 0;
            filter.SystemExclusive = source.SystemExclusive;
            return filter;
        }

        std::wstring FormatTime(double seconds)
        {
            if (seconds < 0)
            {
                seconds = 0;
            }

            auto const totalMilliseconds = static_cast<int64_t>(seconds * 1000.0 + 0.5);
            auto const minutes = totalMilliseconds / 60000;
            auto const secondsPart = (totalMilliseconds / 1000) % 60;
            auto const milliseconds = totalMilliseconds % 1000;

            return std::format(L"{}:{:02}.{:03}", minutes, secondsPart, milliseconds);
        }

        std::wstring ExecutableFolder()
        {
            std::wstring path(MAX_PATH, L'\0');
            auto const length = ::GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));

            if (length == 0 || length >= path.size())
            {
                return {};
            }

            path.resize(length);
            return std::filesystem::path{ path }.parent_path().wstring();
        }
    }

    // ---------------------------------------------------------------- MIDI

    winrt::fire_and_forget MainWindow::StartMidiAsync()
    {
        auto strong = get_strong();

        try
        {
            midi2::MidiSession session{ nullptr };
            auto available = false;

            co_await ::midisequencer::RunOnBackgroundAsync([&]()
            {
                midiapp::EndpointCatalog::Current().Start();

                available = midi2::MidiApi::EnsureServiceAvailable();

                if (available)
                {
                    session = midi2::MidiSession::Create(L"Windows MIDI Sequencer");
                }
            });

            if (m_closing)
            {
                if (session != nullptr)
                {
                    session.Close();
                }

                co_return;
            }

            if (!available || session == nullptr)
            {
                ShowMessage(res::GetString(L"MidiUnavailable"));
                UpdateStatusBar();
                co_return;
            }

            m_session = session;
            m_output = std::make_unique<seq::SessionEngineOutput>(m_session);

            auto const directory = m_directory;
            m_output->SetResolver([directory](seq::EndpointRef const& endpoint) { return directory->ResolveId(endpoint); });

            seq::EngineClock clock{};
            clock.Now = []() { return midi2::MidiClock::Now(); };
            clock.TicksPerSecond = midi2::MidiClock::TimestampFrequency();

            m_engine = std::make_unique<seq::PlaybackEngine>(*m_output, clock);
            m_engine->SetDestinationLookup([directory](seq::EndpointRef const& endpoint, uint8_t group) { return directory->Lookup(endpoint, group); });
            m_engine->SetSequence(std::make_shared<seq::Sequence const>(m_doc));
            ApplyEngineSettings();
            m_engine->StartThread();

            m_sources = std::make_unique<seq::SourceMonitor>(m_session);

            auto weak = get_weak();
            m_sources->SetHandler([weak](std::wstring const& endpointId, uint64_t timestamp, uint32_t const* words, uint8_t wordCount)
            {
                if (auto window = weak.get())
                {
                    window->OnSourceMessage(endpointId, timestamp, words, wordCount);
                }
            });

            RefreshEndpoints();
            UpdateTransport();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to start MIDI.")
    }

    void MainWindow::RefreshEndpoints() noexcept
    {
        try
        {
            // One at a time; a change that arrives during one is covered by the next.
            if (m_endpointRefreshPending.exchange(true))
            {
                return;
            }

            [](winrt::com_ptr<MainWindow> strong) -> winrt::fire_and_forget
            {
                try
                {
                    std::vector<std::wstring> detail{};

                    seq::ForEachTrack(strong->m_doc, [&](seq::Track const& track, size_t)
                    {
                        if (!track.IsFolder && !track.Destination.Endpoint.Id.empty())
                        {
                            detail.push_back(track.Destination.Endpoint.Id);
                        }

                        return true;
                    });

                    auto const directory = strong->m_directory;
                    co_await ::midisequencer::RunOnBackgroundAsync([&]() { directory->Refresh(detail); });

                    strong->m_endpointRefreshPending = false;

                    if (strong->m_closing)
                    {
                        co_return;
                    }

                    // A new sequence's first track plays to the General MIDI Synth.
                    if (strong->m_wantsDefaultDestination && !strong->m_dirty)
                    {
                        if (auto const synth = directory->Resolve(seq::EndpointRef{ L"", directory->SynthEndpointId() }); synth.has_value())
                        {
                            seq::ForEachTrack(strong->m_doc, [&](seq::Track const& track, size_t)
                            {
                                if (!track.IsFolder && track.Destination.Endpoint.IsEmpty())
                                {
                                    if (auto t = seq::FindTrack(strong->m_doc, track.Id); t != nullptr)
                                    {
                                        t->Destination.Endpoint = seq::EndpointDirectory::MakeRef(synth->Live);
                                    }
                                }

                                return true;
                            });

                            strong->m_wantsDefaultDestination = false;
                            strong->Publish();
                        }
                    }

                    strong->RebuildHeaders();
                    strong->RefreshInspector();
                    strong->UpdateEditorHeader();
                    strong->UpdateStatusBar();
                    strong->PrepareConnectionsAsync();
                }
                catch (...)
                {
                    strong->m_endpointRefreshPending = false;
                }
            }(get_strong());
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to refresh the endpoints.")
    }

    winrt::fire_and_forget MainWindow::PrepareConnectionsAsync()
    {
        auto strong = get_strong();

        try
        {
            if (m_output == nullptr || m_sources == nullptr)
            {
                co_return;
            }

            if (m_preparing.exchange(true))
            {
                m_preparePending = true;
                co_return;
            }

            do
            {
                m_preparePending = false;

                // Every destination the sequence plays to, the metronome, and the sources of
                // the armed tracks.
                std::vector<seq::EndpointRef> destinations{};
                std::vector<std::wstring> sources{};

                seq::ForEachTrack(m_doc, [&](seq::Track const& track, size_t)
                {
                    if (!track.IsFolder && !track.Destination.Endpoint.IsEmpty())
                    {
                        if (std::find(destinations.begin(), destinations.end(), track.Destination.Endpoint) == destinations.end())
                        {
                            destinations.push_back(track.Destination.Endpoint);
                        }
                    }

                    if (!track.IsFolder && m_armed.contains(track.Id) && !track.Source.Endpoint.IsEmpty())
                    {
                        auto const id = m_directory->ResolveId(track.Source.Endpoint);

                        if (!id.empty())
                        {
                            sources.push_back(id);
                        }
                    }

                    return true;
                });

                auto& settings = seq::AppSettings::Current();

                if (settings.MetronomeEnabled())
                {
                    seq::EndpointRef metronome{ settings.MetronomeEndpointName(), settings.MetronomeEndpointId() };

                    if (metronome.IsEmpty())
                    {
                        metronome.Id = m_directory->SynthEndpointId();
                    }

                    if (!metronome.IsEmpty() && std::find(destinations.begin(), destinations.end(), metronome) == destinations.end())
                    {
                        destinations.push_back(metronome);
                    }
                }

                auto* output = m_output.get();
                auto* monitor = m_sources.get();

                co_await ::midisequencer::RunOnBackgroundAsync([&]()
                {
                    output->Prepare(destinations);
                    monitor->Prepare(sources);
                });

                if (m_closing)
                {
                    break;
                }
            } while (m_preparePending.load());

            m_preparing = false;
            UpdateEchoRoutes();
        }
        catch (...)
        {
            m_preparing = false;
        }
    }

    void MainWindow::ApplyEngineSettings() noexcept
    {
        try
        {
            if (m_engine == nullptr)
            {
                return;
            }

            auto const& settings = seq::AppSettings::Current();

            seq::EngineSettings engine{};
            engine.Metronome.Enabled = settings.MetronomeEnabled() && (!settings.MetronomeOnlyWhileRecording() || m_recording);
            engine.Metronome.Endpoint = seq::EndpointRef{ settings.MetronomeEndpointName(), settings.MetronomeEndpointId() };

            if (engine.Metronome.Endpoint.IsEmpty())
            {
                engine.Metronome.Endpoint.Id = m_directory->SynthEndpointId();
            }

            engine.Metronome.Group = settings.MetronomeGroup();
            engine.Metronome.Channel = settings.MetronomeChannel();
            engine.Metronome.Note = settings.MetronomeNote();

            engine.LoopEnabled = LoopToggle().IsChecked().GetBoolean() && m_loopEnd > m_loopStart;
            engine.LoopStart = m_loopStart;
            engine.LoopEnd = m_loopEnd;

            m_engine->SetSettings(engine);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to apply the playback settings.")
    }

    // ---------------------------------------------------------------- the transport

    _Use_decl_annotations_
    void MainWindow::Play(int64_t fromTick) noexcept
    {
        try
        {
            if (m_engine == nullptr)
            {
                ShowMessage(res::GetString(L"MidiNotReady"));
                UpdateTransport();
                return;
            }

            ApplyEngineSettings();
            m_engine->Play(std::max<int64_t>(0, fromTick));
            m_position = std::max<int64_t>(0, fromTick);

            m_frameTimer.Start();
            UpdateTransport();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to play.")
    }

    void MainWindow::Stop() noexcept
    {
        try
        {
            if (m_recording)
            {
                StopRecording();
            }

            if (m_engine == nullptr)
            {
                return;
            }

            if (!m_engine->IsPlaying())
            {
                // Stop when already stopped goes back to the start.
                SetPosition(0);
                ScrollToTick(0);
                return;
            }

            m_position = m_engine->PositionTick();
            m_engine->Stop();
            m_launchViews.clear();

            UpdateTransport();
            UpdateLeds();
            InvalidateArrange();
            InvalidateLaunchers();
            InvalidateEditor();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to stop.")
    }

    void MainWindow::Silence() noexcept
    {
        try
        {
            if (m_engine != nullptr)
            {
                m_engine->Stop();
            }

            // All notes off and sustain off on every channel of every group the sequence plays
            // to, for anything left sounding by something else.
            std::vector<seq::TrackDestination> sent{};

            seq::ForEachTrack(m_doc, [&](seq::Track const& track, size_t)
            {
                if (track.IsFolder || track.Destination.Endpoint.IsEmpty())
                {
                    return true;
                }

                auto const already = std::find_if(sent.begin(), sent.end(), [&](seq::TrackDestination const& d)
                {
                    return d.Endpoint == track.Destination.Endpoint && d.Group == track.Destination.Group;
                }) != sent.end();

                if (already)
                {
                    return true;
                }

                sent.push_back(track.Destination);

                for (uint8_t channel = 0; channel < 16; ++channel)
                {
                    seq::TrackDestination destination = track.Destination;
                    destination.Channel = static_cast<int8_t>(channel);
                    destination.Protocol = seq::ProtocolChoice::Midi1;

                    for (auto const controller : { 64u, 123u, 120u })
                    {
                        uint32_t const word = 0x20000000u | (static_cast<uint32_t>(track.Destination.Group & 0x0F) << 24) | (0xB0u << 16) | (static_cast<uint32_t>(channel) << 16) | (controller << 8);
                        SendNow(destination, &word, 1);
                    }
                }

                return true;
            });

            UpdateTransport();
            ShowMessage(res::GetString(L"Silenced"));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to silence the devices.")
    }

    _Use_decl_annotations_
    void MainWindow::SetPosition(int64_t tick) noexcept
    {
        try
        {
            m_position = std::max<int64_t>(0, tick);

            if (m_engine != nullptr && m_engine->IsPlaying())
            {
                m_engine->Play(m_position);
            }

            UpdateDisplays(m_position);
            UpdatePlayhead();
            InvalidateArrange();
            InvalidateEditor();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to move the playhead.")
    }

    void MainWindow::UpdateTransport() noexcept
    {
        try
        {
            auto const playing = m_engine != nullptr && m_engine->IsPlaying();

            PlayButton().IsChecked(playing);
            RecordButton().IsChecked(m_recording.load());

            auto const anyLaunched = std::any_of(m_launchViews.begin(), m_launchViews.end(), [](auto const& entry)
            {
                return entry.second.Mode != seq::TrackPlayMode::Timeline;
            });

            BackToTimelineButton().Visibility(anyLaunched ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            {
                std::scoped_lock guard{ m_inputLock };
                auto const heard = std::any_of(m_heard.begin(), m_heard.end(), [](auto const& entry) { return !entry.second.empty(); });
                CaptureButton().IsEnabled(heard);
            }

            RecordingStatus().Visibility(m_recording ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            if (m_recording)
            {
                RecordingStatusText().Text(res::FormatString(L"RecordingStatusFormat", m_recordingPreview.size()));
            }

            auto const on = seq::AppSettings::Current().MetronomeEnabled();
            MetronomeToggle().IsChecked(on);
            MetronomeIconPath().Stroke(BrushFor(on ? m_palette.Accent : m_palette.Text2));
            MetronomeIconWeight().Fill(BrushFor(on ? m_palette.Accent : m_palette.Text2));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to update the transport.")
    }

    _Use_decl_annotations_
    void MainWindow::UpdateDisplays(int64_t tick) noexcept
    {
        try
        {
            PositionText().Text(winrt::hstring{ seq::PositionLabel(m_doc.Meter, tick) });

            seq::TempoMap const tempo{ m_doc.Tempo };
            TimeText().Text(winrt::hstring{ FormatTime(tempo.SecondsAtTick(tick)) });

            auto const bpm = tempo.BeatsPerMinuteAtTick(tick);
            TempoText().Text(winrt::hstring{ std::format(L"{:.2f}", bpm) });

            auto const& meter = seq::MeterAtTick(m_doc.Meter, tick);
            MeterText().Text(winrt::hstring{ std::format(L"{}/{}", meter.Numerator, meter.Denominator) });

            UpdateLeds();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to update the displays.")
    }

    // ---------------------------------------------------------------- recording

    void MainWindow::StartRecording() noexcept
    {
        try
        {
            if (m_engine == nullptr)
            {
                ShowMessage(res::GetString(L"MidiNotReady"));
                RecordButton().IsChecked(false);
                return;
            }

            if (m_armed.empty())
            {
                ShowMessage(res::GetString(L"RecordNothingArmed"));
                RecordButton().IsChecked(false);
                return;
            }

            auto const start = m_engine->IsPlaying() ? m_engine->PositionTick() : m_position;

            {
                std::scoped_lock guard{ m_inputLock };
                m_takes.clear();

                for (auto const& id : m_armed)
                {
                    if (auto const track = seq::FindTrack(m_doc, id); track != nullptr && !track->IsFolder)
                    {
                        m_takes.emplace(id, seq::RecordingTake{ start, FilterFor(track->Source) });
                    }
                }
            }

            m_recordStartTick = start;
            m_recording = true;
            m_recordingPreview.clear();
            m_recordingPreviewTrackId = *m_armed.begin();
            m_recordingPreviewStart = start;

            if (!m_engine->IsPlaying())
            {
                Play(start);
            }
            else
            {
                ApplyEngineSettings();
            }

            m_frameTimer.Start();
            UpdateTransport();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to start recording.")
    }

    void MainWindow::StopRecording() noexcept
    {
        try
        {
            if (!m_recording)
            {
                return;
            }

            m_recording = false;

            auto const end = m_engine != nullptr && m_engine->IsPlaying() ? m_engine->PositionTick() : m_position;

            std::map<std::wstring, seq::RecordingTake> takes{};

            {
                std::scoped_lock guard{ m_inputLock };
                takes.swap(m_takes);
            }

            m_recordingPreview.clear();
            m_recordingPreviewTrackId.clear();

            seq::ChangeList changes{};
            auto const before = m_doc.Tracks;
            size_t notes{ 0 };

            for (auto& [trackId, take] : takes)
            {
                if (take.IsEmpty())
                {
                    continue;
                }

                auto const track = seq::FindTrack(m_doc, trackId);

                if (track == nullptr)
                {
                    continue;
                }

                auto const source = m_directory->Resolve(track->Source.Endpoint);
                auto const detail = source.has_value() ? source->Live.Name : track->Source.Endpoint.Name;

                auto clip = take.Finish(std::max(end, take.StartTick() + 1), m_doc.Meter, seq::NewId(L"c"), NextClipName(), detail);
                notes += clip.Notes.size();

                auto const clipId = clip.Id;
                auto const at = take.PlacementTick();

                changes.push_back(seq::MakeClipPresenceChange(clip, true));
                m_doc.Clips.push_back(std::move(clip));

                auto target = seq::FindTrack(m_doc, trackId);
                target->Timeline.push_back(seq::Placement{ clipId, at, 0 });
                std::stable_sort(target->Timeline.begin(), target->Timeline.end(), [](seq::Placement const& a, seq::Placement const& b) { return a.Tick < b.Tick; });
            }

            ApplyEngineSettings();
            UpdateTransport();

            if (changes.empty())
            {
                ShowMessage(res::GetString(L"RecordedNothing"));
                InvalidateArrange();
                return;
            }

            changes.push_back(seq::MakeTracksChange(before, m_doc.Tracks));
            Commit(std::wstring{ res::GetString(L"UndoRecord") }, std::move(changes));
            ShowMessage(res::FormatString(L"RecordedFormat", notes));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to finish recording.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSourceMessage(std::wstring const& endpointId, uint64_t timestamp, uint32_t const* words, uint8_t wordCount) noexcept
    {
        // On the service's thread. Quick, and nothing here touches XAML.
        try
        {
            if (words == nullptr || wordCount == 0 || wordCount > 4)
            {
                return;
            }

            m_lastInputTicks = timestamp;

            auto const tick = m_engine != nullptr && m_recording ? m_engine->TickAtTime(timestamp) : -1;

            std::scoped_lock guard{ m_inputLock };

            for (auto const& route : m_echoRoutes)
            {
                if (!SameEndpoint(route.SourceId, endpointId) || !seq::PassesRecordFilter(route.Filter, words, wordCount))
                {
                    continue;
                }

                if (route.Echo)
                {
                    SendNow(route.Destination, words, wordCount);
                }

                if (tick >= 0)
                {
                    if (auto take = m_takes.find(route.TrackId); take != m_takes.end())
                    {
                        take->second.Add(tick, words, wordCount);
                    }
                }

                auto& heard = m_heard[route.TrackId];

                HeardMessage message{};
                message.Timestamp = timestamp;
                message.WordCount = wordCount;
                std::copy_n(words, wordCount, message.Words.begin());
                heard.push_back(message);

                auto const frequency = static_cast<double>(midi2::MidiClock::TimestampFrequency());

                while (heard.size() > MaximumHeardMessages ||
                    (!heard.empty() && static_cast<double>(timestamp - heard.front().Timestamp) / frequency > HeardSeconds))
                {
                    heard.pop_front();
                }
            }
        }
        catch (...)
        {
        }
    }

    void MainWindow::UpdateEchoRoutes() noexcept
    {
        try
        {
            std::vector<EchoRoute> routes{};

            seq::ForEachTrack(m_doc, [&](seq::Track const& track, size_t)
            {
                if (track.IsFolder || !m_armed.contains(track.Id) || track.Source.Endpoint.IsEmpty())
                {
                    return true;
                }

                EchoRoute route{};
                route.TrackId = track.Id;
                route.SourceId = m_directory->ResolveId(track.Source.Endpoint);
                route.Filter = FilterFor(track.Source);
                route.Destination = track.Destination;
                route.Echo = track.Source.Echo && !track.Destination.Endpoint.IsEmpty();

                if (!route.SourceId.empty())
                {
                    routes.push_back(std::move(route));
                }

                return true;
            });

            std::scoped_lock guard{ m_inputLock };
            m_echoRoutes = std::move(routes);

            for (auto it = m_heard.begin(); it != m_heard.end();)
            {
                it = m_armed.contains(it->first) ? std::next(it) : m_heard.erase(it);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to update what's echoed.")
    }

    _Use_decl_annotations_
    void MainWindow::SendNow(seq::TrackDestination const& destination, uint32_t const* words, uint8_t wordCount) noexcept
    {
        // Any thread. Sends in the protocol the destination group speaks.
        try
        {
            if (m_output == nullptr || destination.Endpoint.IsEmpty() || words == nullptr || wordCount == 0 || wordCount > 4)
            {
                return;
            }

            std::array<uint32_t, 4> message{};
            std::copy_n(words, wordCount, message.begin());
            seq::ApplyDestination(destination, message, wordCount);

            auto midi2 = true;

            switch (destination.Protocol)
            {
            case seq::ProtocolChoice::Midi1:
                midi2 = false;
                break;

            case seq::ProtocolChoice::Midi2:
                midi2 = true;
                break;

            default:
                midi2 = m_directory->Lookup(destination.Endpoint, destination.Group).SpeaksMidi2;
                break;
            }

            auto const translated = midi2
                ? seq::TranslateToMidi2(message.data(), wordCount)
                : seq::TranslateToMidi1(message.data(), wordCount);

            for (uint8_t i = 0; i < translated.Count; ++i)
            {
                m_output->Send(destination.Endpoint, 0, translated.Messages[i].data(), translated.WordCounts[i]);
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void MainWindow::AuditionNote(uint8_t note, bool on) noexcept
    {
        try
        {
            auto const track = EditorTrack();

            if (track == nullptr || track->Destination.Endpoint.IsEmpty())
            {
                return;
            }

            seq::Note value{};
            value.Number = note;
            value.Channel = static_cast<uint8_t>(std::max<int8_t>(0, track->Destination.Channel));
            value.Velocity = 0xC000;

            uint32_t words[2]{};

            if (on)
            {
                seq::BuildNoteOn(value, track->Destination.Group, words);
            }
            else
            {
                seq::BuildNoteOff(value, track->Destination.Group, words);
            }

            SendNow(track->Destination, words, 2);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to play a note.")
    }

    _Use_decl_annotations_
    void MainWindow::LaunchKeyboard(std::wstring const& endpointId, uint8_t group, int8_t channel) noexcept
    {
        try
        {
            // Installed beside this app. A developer build looks in the keyboard's own output
            // folder, which sits beside this app's: out\<tool>\<platform>\<configuration>.
            auto const folder = std::filesystem::path{ ExecutableFolder() };
            std::filesystem::path keyboard = folder / L"midikeyboard.exe";

            if (!std::filesystem::exists(keyboard) && folder.has_parent_path() && folder.parent_path().has_parent_path())
            {
                auto const configuration = folder.filename();
                auto const platform = folder.parent_path().filename();
                auto const tools = folder.parent_path().parent_path().parent_path();

                keyboard = tools / L"midikeyboard" / platform / configuration / L"midikeyboard.exe";
            }

            if (!std::filesystem::exists(keyboard))
            {
                ShowMessage(res::GetString(L"KeyboardNotFound"));
                return;
            }

            std::wstring arguments{};

            if (!endpointId.empty())
            {
                arguments = L"\"" + endpointId + L"\" --group " + std::to_wstring(static_cast<uint32_t>(group) + 1);

                if (channel >= 0)
                {
                    arguments += L" --channel " + std::to_wstring(static_cast<int32_t>(channel) + 1);
                }
            }

            auto const path = keyboard.wstring();

            SHELLEXECUTEINFOW info{};
            info.cbSize = sizeof(info);
            info.fMask = SEE_MASK_NOASYNC;
            info.lpVerb = L"open";
            info.lpFile = path.c_str();
            info.lpParameters = arguments.empty() ? nullptr : arguments.c_str();
            info.nShow = SW_SHOWNORMAL;

            if (!::ShellExecuteExW(&info))
            {
                ShowMessage(res::GetString(L"KeyboardNotFound"));
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to open MIDI Keyboard.")
    }

    seq::LaunchQuantize MainWindow::LaunchQuantize() const noexcept
    {
        auto const ticks = seq::AppSettings::Current().LaunchQuantizeTicks();

        // Whole bars follow the meter, so they're counted in bars rather than ticks.
        switch (ticks)
        {
        case 3840:
            return -1;

        case 7680:
            return -2;

        case 15360:
            return -4;

        default:
            return static_cast<seq::LaunchQuantize>(ticks);
        }
    }

    _Use_decl_annotations_
    std::wstring MainWindow::DescribeDestination(seq::Track const& track) const
    {
        if (track.Destination.Endpoint.IsEmpty())
        {
            return std::wstring{ res::GetString(L"NoDestination") };
        }

        auto const resolved = m_directory->Resolve(track.Destination.Endpoint);
        auto name = resolved.has_value() ? resolved->Live.Name : track.Destination.Endpoint.Name;

        if (name.empty())
        {
            name = std::wstring{ res::GetString(L"UnknownEndpoint") };
        }

        std::wstring text = name;

        if (track.Destination.Group > 0)
        {
            text += L" \u00B7 " + std::wstring{ res::FormatString(L"GroupShortFormat", static_cast<uint32_t>(track.Destination.Group) + 1) };
        }

        if (track.Destination.Channel >= 0)
        {
            text += L" \u00B7 " + std::wstring{ res::FormatString(L"ChannelShortFormat", static_cast<int32_t>(track.Destination.Channel) + 1) };
        }

        if (!resolved.has_value())
        {
            text += L" " + std::wstring{ res::GetString(L"NotConnectedSuffix") };
        }

        return text;
    }

    _Use_decl_annotations_
    std::wstring MainWindow::DescribeSource(seq::Track const& track) const
    {
        if (track.Source.Endpoint.IsEmpty())
        {
            return std::wstring{ res::GetString(L"NoSource") };
        }

        auto const resolved = m_directory->Resolve(track.Source.Endpoint);
        auto name = resolved.has_value() ? resolved->Live.Name : track.Source.Endpoint.Name;

        if (name.empty())
        {
            name = std::wstring{ res::GetString(L"UnknownEndpoint") };
        }

        std::wstring text = res::FormatString(L"RecordingFromFormat", name).c_str();

        if (!resolved.has_value())
        {
            text += L" " + std::wstring{ res::GetString(L"NotConnectedSuffix") };
        }

        return text;
    }

    // ---------------------------------------------------------------- the metronome

    void MainWindow::ShowMetronomeFlyout()
    {
        auto& settings = seq::AppSettings::Current();

        controls::StackPanel panel{};
        panel.Spacing(10);
        panel.Width(280);

        controls::TextBlock title{};
        title.Text(res::GetString(L"MetronomeTitle"));
        title.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
        panel.Children().Append(title);

        controls::ComboBox endpoint{};
        endpoint.Header(winrt::box_value(res::GetString(L"MetronomeEndpoint")));
        endpoint.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
        endpoint.ItemTemplate(RootGrid().Resources().Lookup(winrt::box_value(L"EndpointChoiceTemplate")).as<xaml::DataTemplate>());

        seq::EndpointRef current{ settings.MetronomeEndpointName(), settings.MetronomeEndpointId() };

        if (current.IsEmpty())
        {
            current.Id = m_directory->SynthEndpointId();
        }

        FillEndpointCombo(endpoint, false, current);
        panel.Children().Append(endpoint);

        controls::Grid row{};
        row.ColumnSpacing(8);

        controls::ColumnDefinition first{};
        first.Width(xaml::GridLength{ 1, xaml::GridUnitType::Star });
        row.ColumnDefinitions().Append(first);

        controls::ColumnDefinition second{};
        second.Width(xaml::GridLength{ 1, xaml::GridUnitType::Star });
        row.ColumnDefinitions().Append(second);

        controls::ComboBox group{};
        group.Header(winrt::box_value(res::GetString(L"MetronomeGroup")));
        group.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
        group.ItemTemplate(RootGrid().Resources().Lookup(winrt::box_value(L"NamedChoiceTemplate")).as<xaml::DataTemplate>());
        FillGroupCombo(group, false, current, settings.MetronomeGroup(), false);
        row.Children().Append(group);

        controls::ComboBox channel{};
        channel.Header(winrt::box_value(res::GetString(L"MetronomeChannel")));
        channel.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
        channel.ItemTemplate(RootGrid().Resources().Lookup(winrt::box_value(L"NamedChoiceTemplate")).as<xaml::DataTemplate>());
        FillChannelCombo(channel, settings.MetronomeChannel(), false);
        controls::Grid::SetColumn(channel, 1);
        row.Children().Append(channel);

        panel.Children().Append(row);

        controls::NumberBox note{};
        note.Header(winrt::box_value(res::GetString(L"MetronomeNote")));
        note.Minimum(0);
        note.Maximum(127);
        note.Value(settings.MetronomeNote());
        note.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Inline);
        note.Description(winrt::box_value(winrt::hstring{ seq::NoteLabel(settings.MetronomeNote()) }));
        panel.Children().Append(note);

        controls::CheckBox onlyRecording{};
        onlyRecording.Content(winrt::box_value(res::GetString(L"MetronomeOnlyRecording")));
        onlyRecording.IsChecked(settings.MetronomeOnlyWhileRecording());
        panel.Children().Append(onlyRecording);

        auto weak = get_weak();

        endpoint.SelectionChanged([weak, endpoint, group](auto&&, auto&&)
        {
            auto strong = weak.get();
            auto const choice = endpoint.SelectedItem().try_as<appshared::EndpointChoice>();

            if (!strong || choice == nullptr)
            {
                return;
            }

            seq::AppSettings::Current().MetronomeEndpoint(std::wstring{ choice.EndpointDeviceId() }, std::wstring{ choice.DisplayName() });
            strong->FillGroupCombo(group, false, seq::EndpointRef{ std::wstring{ choice.DisplayName() }, std::wstring{ choice.EndpointDeviceId() } }, 0, false);
            strong->ApplyEngineSettings();
            strong->PrepareConnectionsAsync();
        });

        group.SelectionChanged([weak, group](auto&&, auto&&)
        {
            auto strong = weak.get();
            auto const choice = group.SelectedItem().try_as<appshared::NamedChoice>();

            if (strong && choice != nullptr)
            {
                seq::AppSettings::Current().MetronomeGroup(static_cast<uint8_t>(std::clamp(choice.Value(), 0, 15)));
                strong->ApplyEngineSettings();
            }
        });

        channel.SelectionChanged([weak, channel](auto&&, auto&&)
        {
            auto strong = weak.get();
            auto const choice = channel.SelectedItem().try_as<appshared::NamedChoice>();

            if (strong && choice != nullptr)
            {
                seq::AppSettings::Current().MetronomeChannel(static_cast<uint8_t>(std::clamp(choice.Value(), 0, 15)));
                strong->ApplyEngineSettings();
            }
        });

        note.ValueChanged([weak](controls::NumberBox const& sender, auto&&)
        {
            auto strong = weak.get();
            auto const value = sender.Value();

            if (!strong || std::isnan(value))
            {
                return;
            }

            auto const number = static_cast<uint8_t>(std::clamp(static_cast<int32_t>(std::lround(value)), 0, 127));
            seq::AppSettings::Current().MetronomeNote(number);
            sender.Description(winrt::box_value(winrt::hstring{ seq::NoteLabel(number) }));
            strong->ApplyEngineSettings();
        });

        onlyRecording.Click([weak, onlyRecording](auto&&, auto&&)
        {
            if (auto strong = weak.get())
            {
                seq::AppSettings::Current().MetronomeOnlyWhileRecording(onlyRecording.IsChecked().GetBoolean());
                strong->ApplyEngineSettings();
            }
        });

        controls::Flyout flyout{};
        flyout.Content(panel);
        flyout.ShowAt(MetronomeMenuButton());
    }

    // ---------------------------------------------------------------- transport buttons

    _Use_decl_annotations_
    void MainWindow::OnGoToStartClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        SetPosition(0);
        ScrollToTick(0);
    }

    _Use_decl_annotations_
    void MainWindow::OnPlayClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (PlayButton().IsChecked().GetBoolean())
        {
            Play(m_position);
        }
        else
        {
            Stop();
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnStopClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        Stop();
    }

    _Use_decl_annotations_
    void MainWindow::OnRecordClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (RecordButton().IsChecked().GetBoolean())
        {
            StartRecording();
        }
        else
        {
            StopRecording();
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnLoopClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        ApplyEngineSettings();
        InvalidateArrange();
    }

    _Use_decl_annotations_
    void MainWindow::OnMetronomeClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        seq::AppSettings::Current().MetronomeEnabled(MetronomeToggle().IsChecked().GetBoolean());
        ApplyEngineSettings();
        PrepareConnectionsAsync();
        UpdateTransport();
    }

    _Use_decl_annotations_
    void MainWindow::OnMetronomeMenuClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            ShowMetronomeFlyout();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the metronome settings.")
    }

    _Use_decl_annotations_
    void MainWindow::OnClockClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            // Following an external clock, and sending clock, come in a later phase (design section 14).
            controls::TextBlock text{};
            text.Text(res::GetString(L"ClockNotYet"));
            text.TextWrapping(xaml::TextWrapping::Wrap);
            text.MaxWidth(280);

            controls::Flyout flyout{};
            flyout.Content(text);
            flyout.ShowAt(sender.as<xaml::FrameworkElement>());
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the clock settings.")
    }

    _Use_decl_annotations_
    void MainWindow::OnLaunchQuantizeChanged(foundation::IInspectable const&, controls::SelectionChangedEventArgs const&)
    {
        static constexpr std::array<uint32_t, 8> choices{ 0, 240, 480, 960, 1920, 3840, 7680, 15360 };

        auto const index = LaunchCombo().SelectedIndex();

        if (index >= 0 && static_cast<size_t>(index) < choices.size())
        {
            seq::AppSettings::Current().LaunchQuantizeTicks(choices[static_cast<size_t>(index)]);
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnSnapChanged(foundation::IInspectable const&, controls::SelectionChangedEventArgs const&)
    {
        static constexpr std::array<uint32_t, 7> choices{ 0, 120, 240, 480, 960, 1920, 3840 };

        auto const index = SnapCombo().SelectedIndex();

        if (index >= 0 && static_cast<size_t>(index) < choices.size())
        {
            seq::AppSettings::Current().SnapTicks(choices[static_cast<size_t>(index)]);
            m_roll.SetSnap(choices[static_cast<size_t>(index)]);

            if (!m_editorClipId.empty())
            {
                UpdateEditorHeader();
            }
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnCaptureClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            // What the armed tracks heard, made into clips, as if it had been recorded.
            std::map<std::wstring, std::deque<HeardMessage>> heard{};

            {
                std::scoped_lock guard{ m_inputLock };
                heard.swap(m_heard);
            }

            auto const frequency = static_cast<double>(midi2::MidiClock::TimestampFrequency());
            auto const playing = m_engine != nullptr && m_engine->IsPlaying();
            auto const bpm = seq::TempoMap{ m_doc.Tempo }.BeatsPerMinuteAtTick(m_position);
            auto const ticksPerSecond = bpm / 60.0 * static_cast<double>(seq::TicksPerQuarterNote);

            seq::ChangeList changes{};
            auto const before = m_doc.Tracks;
            size_t notes{ 0 };

            for (auto& [trackId, messages] : heard)
            {
                if (messages.empty())
                {
                    continue;
                }

                auto const track = seq::FindTrack(m_doc, trackId);

                if (track == nullptr)
                {
                    continue;
                }

                // While playing, each message goes where it was played. While stopped, the first
                // goes at the start of the playhead's bar and the rest keep their spacing.
                auto const first = messages.front().Timestamp;
                auto const base = seq::TickAtBar(m_doc.Meter, seq::BarPositionAtTick(m_doc.Meter, m_position).Bar);

                auto const tickOf = [&](uint64_t timestamp) -> int64_t
                {
                    if (playing)
                    {
                        auto const tick = m_engine->TickAtTime(timestamp);

                        if (tick >= 0)
                        {
                            return tick;
                        }
                    }

                    return base + static_cast<int64_t>(static_cast<double>(timestamp - first) / frequency * ticksPerSecond);
                };

                seq::RecordingTake take{ tickOf(first), FilterFor(track->Source) };

                for (auto const& message : messages)
                {
                    take.Add(tickOf(message.Timestamp), message.Words.data(), message.WordCount);
                }

                if (take.IsEmpty())
                {
                    continue;
                }

                auto const end = tickOf(messages.back().Timestamp) + seq::TicksPerQuarterNote;
                auto const source = m_directory->Resolve(track->Source.Endpoint);
                auto clip = take.Finish(end, m_doc.Meter, seq::NewId(L"c"), NextClipName(), source.has_value() ? source->Live.Name : track->Source.Endpoint.Name);
                notes += clip.Notes.size();

                auto const clipId = clip.Id;
                auto const at = take.PlacementTick();

                changes.push_back(seq::MakeClipPresenceChange(clip, true));
                m_doc.Clips.push_back(std::move(clip));

                auto target = seq::FindTrack(m_doc, trackId);
                target->Timeline.push_back(seq::Placement{ clipId, at, 0 });
                std::stable_sort(target->Timeline.begin(), target->Timeline.end(), [](seq::Placement const& a, seq::Placement const& b) { return a.Tick < b.Tick; });
            }

            UpdateTransport();

            if (changes.empty())
            {
                ShowMessage(res::GetString(L"CaptureNothing"));
                return;
            }

            changes.push_back(seq::MakeTracksChange(before, m_doc.Tracks));
            Commit(std::wstring{ res::GetString(L"UndoCapture") }, std::move(changes));
            ShowMessage(res::FormatString(L"CapturedFormat", notes));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to capture what was played.")
    }

    _Use_decl_annotations_
    void MainWindow::OnBackToTimelineClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_engine != nullptr)
        {
            m_engine->ReturnToTimeline(std::wstring{}, LaunchQuantize());
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnSilenceClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        Silence();
    }
}
