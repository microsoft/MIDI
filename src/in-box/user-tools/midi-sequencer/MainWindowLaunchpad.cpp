// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "BackgroundWork.h"
#include "HarmonyFollow.h"
#include "RemoteControl.h"
#include "SequenceArpeggiator.h"
#include "SequenceRender.h"

namespace winrt::midisequencer::implementation
{
    namespace
    {
        constexpr size_t LaunchpadMaximumHeardMessages = 50000;
        constexpr double LaunchpadHeardSeconds = 120.0;
        constexpr int64_t LaunchpadSlotEarlyTicks = seq::TicksPerQuarterNote / 4;

        bool LaunchpadSameEndpoint(_In_ std::wstring const& left, _In_ std::wstring const& right) noexcept
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

        seq::RecordFilter LaunchpadFilterFor(_In_ seq::TrackSource const& source) noexcept
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

        uint8_t ResolveRecordChannel(_In_ uint16_t allowedChannels, _In_ uint8_t preferredChannel) noexcept
        {
            uint8_t const maskedPreferred = static_cast<uint8_t>(preferredChannel & 0x0Fu);
            if ((allowedChannels & (1u << maskedPreferred)) != 0)
            {
                return maskedPreferred;
            }

            for (uint8_t ch = 0; ch < 16; ++ch)
            {
                if ((allowedChannels & (1u << ch)) != 0)
                {
                    return ch;
                }
            }

            return maskedPreferred;
        }
    }

    // ---------------------------------------------------------------- public M2/M3 binding API

    _Use_decl_annotations_
    void MainWindow::SetLaunchpadXEnabled(bool enabled) noexcept
    {
        try
        {
            if (m_launchpadEnabled == enabled)
            {
                return;
            }

            m_launchpadEnabled = enabled;

            if (enabled)
            {
                RefreshLaunchpadXConnectionAsync();
            }
            else
            {
                StopLaunchpadX(true);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to toggle Launchpad X integration.")
    }

    bool MainWindow::LaunchpadXEnabled() const noexcept
    {
        return m_launchpadEnabled;
    }

    _Use_decl_annotations_
    void MainWindow::SetLaunchpadXEndpoint(seq::EndpointRef const& endpoint) noexcept
    {
        try
        {
            if (m_launchpadEndpoint == endpoint)
            {
                return;
            }

            m_launchpadEndpoint = endpoint;

            if (m_launchpadEnabled)
            {
                RefreshLaunchpadXConnectionAsync();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to select Launchpad X endpoint.")
    }

    seq::EndpointRef const& MainWindow::LaunchpadXEndpoint() const noexcept
    {
        return m_launchpadEndpoint;
    }

    _Use_decl_annotations_
    void MainWindow::SetLaunchpadXMode(seq::launchpad::ControllerMode mode) noexcept
    {
        try
        {
            if (m_launchpadMode == mode)
            {
                return;
            }

            m_launchpadMode = mode;

            if (m_launchpadController != nullptr)
            {
                std::vector<seq::launchpad::SurfaceAction> releaseActions{};
                m_launchpadController->SetMode(mode, releaseActions);

                if (!releaseActions.empty())
                {
                    DispatchLaunchpadXActions(releaseActions, 0);
                }
            }

            FlushLaunchpadXHeldNotes();
            UpdateLaunchpadXFeedback(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to switch Launchpad X mode.")
    }

    seq::launchpad::ControllerMode MainWindow::LaunchpadXMode() const noexcept
    {
        return m_launchpadMode;
    }

    bool MainWindow::IsLaunchpadXConnected() const noexcept
    {
        return m_launchpadConnected && m_launchpadController != nullptr && m_launchpadController->IsConnected();
    }

    std::wstring MainWindow::LaunchpadXConnectedEndpointName() const
    {
        return IsLaunchpadXConnected() ? m_launchpadConnectedEndpointName : std::wstring{};
    }

    _Use_decl_annotations_
    void MainWindow::SetLaunchpadXNoteConfig(seq::launchpad::NoteLayoutConfig const& config) noexcept
    {
        try
        {
            m_launchpadNoteConfig = config;

            if (m_launchpadController != nullptr)
            {
                std::vector<seq::launchpad::SurfaceAction> releaseActions{};
                m_launchpadController->SetNoteConfig(m_launchpadNoteConfig, releaseActions);

                if (!releaseActions.empty())
                {
                    DispatchLaunchpadXActions(releaseActions, 0);
                }
            }

            FlushLaunchpadXHeldNotes();
            UpdateLaunchpadXFeedback(false);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to update Launchpad X note layout settings.")
    }

    seq::launchpad::NoteLayoutConfig const& MainWindow::LaunchpadXNoteConfig() const noexcept
    {
        return m_launchpadNoteConfig;
    }

    _Use_decl_annotations_
    void MainWindow::SetLaunchpadXFollowSongKey(bool follow) noexcept
    {
        try
        {
            if (m_launchpadFollowSongKey == follow)
            {
                return;
            }

            m_launchpadFollowSongKey = follow;
            SyncLaunchpadXKeyFromSong();
            UpdateLaunchpadXFeedback(false);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to update Launchpad X song key tracking.")
    }

    bool MainWindow::LaunchpadXFollowSongKey() const noexcept
    {
        return m_launchpadFollowSongKey;
    }

    // ---------------------------------------------------------------- lifecycle & hot-plug

    winrt::fire_and_forget MainWindow::RefreshLaunchpadXConnectionAsync()
    {
        auto strong = get_strong();

        try
        {
            if (m_closing || m_session == nullptr || m_directory == nullptr)
            {
                co_return;
            }

            if (!m_launchpadEnabled)
            {
                StopLaunchpadX(true);
                co_return;
            }

            if (m_launchpadRefreshPending)
            {
                m_launchpadRefreshAgain = true;
                co_return;
            }

            m_launchpadRefreshPending = true;

            do
            {
                m_launchpadRefreshAgain = false;

                if (auto closing = std::exchange(m_launchpadClosing, nullptr); closing != nullptr)
                {
                    try
                    {
                        co_await closing;
                    }
                    catch (...)
                    {
                    }

                    if (m_closing || !m_launchpadEnabled || m_session == nullptr)
                    {
                        break;
                    }
                }

                std::vector<seq::launchpad::LaunchpadEndpointCandidate> candidates{};
                auto const snapshotEndpoints = m_directory->Snapshot();
                candidates.reserve(snapshotEndpoints.size());

                for (auto const& summary : snapshotEndpoints)
                {
                    seq::EndpointRef const ref{ summary.Live.Name, summary.Live.EndpointDeviceId };
                    uint16_t const srcOff = GroupsTurnedOff(ref, false);
                    uint16_t const dstOff = GroupsTurnedOff(ref, true);
                    auto candidate = seq::launchpad::LaunchpadXSessionTransport::BuildCandidate(summary.Live, srcOff, dstOff);

                    if (summary.Live.DestinationGroupCount() > 0)
                    {
                        bool anyDestLeft = false;
                        for (bool const enabled : candidate.destinationGroups)
                        {
                            if (enabled)
                            {
                                anyDestLeft = true;
                                break;
                            }
                        }

                        if (!anyDestLeft)
                        {
                            continue;
                        }
                    }
                    else if ((dstOff & 1u) != 0)
                    {
                        continue;
                    }

                    candidates.push_back(std::move(candidate));
                }

                auto const selection = seq::launchpad::SelectLaunchpadEndpoint(
                    candidates,
                    m_launchpadEndpoint.Id,
                    m_launchpadEndpoint.Name);

                if (!selection.has_value())
                {
                    if (m_launchpadConnected || m_launchpadTransport != nullptr)
                    {
                        StopLaunchpadX(true);
                    }

                    continue;
                }

                if (m_launchpadConnected &&
                    m_launchpadTransport != nullptr &&
                    LaunchpadSameEndpoint(m_launchpadConnectedEndpointId, selection->endpointDeviceId) &&
                    m_launchpadConnectedGroup == selection->group)
                {
                    m_launchpadConnectedEndpointName = selection->displayName;
                    UpdateLaunchpadXFeedback(false);
                    continue;
                }

                // Tear down any previous endpoint before opening the new one.
                FlushLaunchpadXHeldNotes();
                m_launchpadConnected = false;
                m_launchpadConnectedEndpointId.clear();
                m_launchpadConnectedEndpointName.clear();
                m_launchpadConnectedGroup = 0;

                if (m_launchpadController != nullptr)
                {
                    std::vector<seq::launchpad::SurfaceAction> releaseActions{};
                    m_launchpadController->Disconnect(true, releaseActions);
                }

                UpdateLaunchpadXSourceSuppression({}, 0);
                UpdateEchoRoutes();
                PrepareConnectionsAsync();

                if (auto oldTransport = std::exchange(m_launchpadTransport, nullptr); oldTransport != nullptr)
                {
                    co_await ::midisequencer::RunOnBackgroundAsync([oldTransport]() { oldTransport->Close(); });
                }

                if (m_closing || !m_launchpadEnabled || m_session == nullptr)
                {
                    break;
                }

                auto transport = std::make_shared<seq::launchpad::LaunchpadXSessionTransport>(m_session);
                auto const weak = get_weak();
                auto const queue = DispatcherQueue();
                auto const selectedId = selection->endpointDeviceId;
                auto const selectedGroup = selection->group;
                bool opened = false;

                co_await ::midisequencer::RunOnBackgroundAsync([&]()
                {
                    opened = transport->Open(
                        selectedId,
                        selectedGroup,
                        [weak, queue](uint32_t const* words, uint8_t count, uint64_t timestamp)
                        {
                            if (queue == nullptr || words == nullptr || count == 0 || count > 4)
                            {
                                return;
                            }

                            std::array<uint32_t, 4> copy{};
                            std::copy_n(words, count, copy.begin());

                            queue.TryEnqueue([weak, copy, count, timestamp]()
                            {
                                if (auto window = weak.get())
                                {
                                    window->OnLaunchpadXUmp(copy.data(), count, timestamp);
                                }
                            });
                        },
                        [weak, queue, selectedId](bool reconnected)
                        {
                            if (queue == nullptr)
                            {
                                return;
                            }

                            queue.TryEnqueue([weak, reconnected, selectedId]()
                            {
                                if (auto window = weak.get())
                                {
                                    if (LaunchpadSameEndpoint(window->m_launchpadConnectedEndpointId, selectedId))
                                    {
                                        window->OnLaunchpadXTransportState(reconnected);
                                    }
                                }
                            });
                        });
                });

                if (m_closing || !m_launchpadEnabled)
                {
                    co_await ::midisequencer::RunOnBackgroundAsync([transport]() { transport->Close(); });
                    break;
                }

                if (!opened)
                {
                    continue;
                }

                m_launchpadTransport = transport;
                m_launchpadConnected = true;
                m_launchpadConnectedEndpointId = selection->endpointDeviceId;
                m_launchpadConnectedEndpointName = selection->displayName;
                m_launchpadConnectedGroup = selection->group;

                SyncLaunchpadXKeyFromSong();

                if (m_launchpadController != nullptr)
                {
                    std::vector<seq::launchpad::SurfaceAction> dummy{};
                    m_launchpadController->SetMode(m_launchpadMode, dummy);
                    m_launchpadController->SetNoteConfig(m_launchpadNoteConfig, dummy);

                    auto const initialSnapshot = BuildLaunchpadXSnapshot();
                    if (!m_launchpadController->Connect(
                        transport,
                        selection->group,
                        initialSnapshot,
                        selection->handshake,
                        true))
                    {
                        m_launchpadConnected = false;
                        m_launchpadConnectedEndpointId.clear();
                        m_launchpadConnectedEndpointName.clear();
                        m_launchpadConnectedGroup = 0;
                        m_launchpadTransport.reset();
                        co_await ::midisequencer::RunOnBackgroundAsync([transport]() { transport->Close(); });
                        continue;
                    }
                }

                UpdateLaunchpadXSourceSuppression(selection->endpointDeviceId, selection->group);
                UpdateEchoRoutes();
                PrepareConnectionsAsync();
            } while (m_launchpadRefreshAgain && !m_closing && m_launchpadEnabled);

            m_launchpadRefreshPending = false;
        }
        catch (...)
        {
            m_launchpadRefreshPending = false;
            LOG_CAUGHT_EXCEPTION();
        }
    }

    _Use_decl_annotations_
    void MainWindow::StopLaunchpadX(bool inBackground) noexcept
    {
        try
        {
            FlushLaunchpadXHeldNotes();

            bool const hadSuppression = !m_launchpadConnectedEndpointId.empty();
            m_launchpadConnected = false;
            m_launchpadConnectedEndpointId.clear();
            m_launchpadConnectedEndpointName.clear();
            m_launchpadConnectedGroup = 0;

            UpdateLaunchpadXSourceSuppression({}, 0);

            if (hadSuppression && !m_closing)
            {
                UpdateEchoRoutes();
                PrepareConnectionsAsync();
            }

            if (m_launchpadController != nullptr)
            {
                std::vector<seq::launchpad::SurfaceAction> releaseActions{};
                m_launchpadController->Disconnect(true, releaseActions);
            }

            if (auto transport = std::exchange(m_launchpadTransport, nullptr); transport != nullptr)
            {
                if (inBackground)
                {
                    m_launchpadClosing = ::midisequencer::RunOnBackgroundAsync([transport]() { transport->Close(); });
                }
                else
                {
                    transport->Close();
                }
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnLaunchpadXUmp(uint32_t const* words, uint8_t wordCount, uint64_t timestamp) noexcept
    {
        try
        {
            if (m_closing || !m_launchpadEnabled || m_launchpadController == nullptr || !m_launchpadConnected)
            {
                return;
            }

            auto const snapshot = BuildLaunchpadXSnapshot();
            std::vector<seq::launchpad::SurfaceAction> actions{};

            if (m_launchpadController->OnUmpMessage(words, wordCount, snapshot, actions))
            {
                DispatchLaunchpadXActions(actions, timestamp);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to process Launchpad X MIDI input.")
    }

    _Use_decl_annotations_
    void MainWindow::OnLaunchpadXTransportState(bool reconnected) noexcept
    {
        try
        {
            if (m_closing || m_launchpadTransport == nullptr || m_launchpadController == nullptr)
            {
                return;
            }

            if (!reconnected)
            {
                FlushLaunchpadXHeldNotes();

                std::vector<seq::launchpad::SurfaceAction> releaseActions{};
                m_launchpadController->Disconnect(false, releaseActions);
                m_launchpadConnected = false;

                UpdateLaunchpadXSourceSuppression({}, 0);
                UpdateEchoRoutes();
                PrepareConnectionsAsync();
                return;
            }

            if (!m_launchpadEnabled)
            {
                StopLaunchpadX(true);
                return;
            }

            m_launchpadConnected = true;
            SyncLaunchpadXKeyFromSong();

            std::vector<seq::launchpad::SurfaceAction> dummy{};
            m_launchpadController->SetMode(m_launchpadMode, dummy);
            m_launchpadController->SetNoteConfig(m_launchpadNoteConfig, dummy);

            auto const snapshot = BuildLaunchpadXSnapshot();
            if (m_launchpadController->Connect(
                m_launchpadTransport,
                m_launchpadConnectedGroup,
                snapshot,
                seq::launchpad::LaunchpadHandshakeMode::Immediate,
                true))
            {
                UpdateLaunchpadXSourceSuppression(m_launchpadConnectedEndpointId, m_launchpadConnectedGroup);
                UpdateEchoRoutes();
                PrepareConnectionsAsync();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to handle Launchpad X connection state change.")
    }

    // ---------------------------------------------------------------- action dispatch & note routing

    _Use_decl_annotations_
    void MainWindow::DispatchLaunchpadXActions(std::vector<seq::launchpad::SurfaceAction> const& actions, uint64_t timestamp) noexcept
    {
        try
        {
            bool forceRefresh = false;

            for (auto const& action : actions)
            {
                switch (action.kind)
                {
                case seq::launchpad::SurfaceActionKind::TriggerOrStopSlot:
                {
                    auto const order = seq::RemoteTrackOrder(m_doc);
                    if (action.trackIndex >= order.size() || action.sceneIndex >= m_doc.Scenes.size())
                    {
                        break;
                    }

                    auto const& trackId = order[action.trackIndex];
                    auto const* track = seq::FindTrack(m_doc, trackId);
                    if (track == nullptr || track->IsFolder)
                    {
                        break;
                    }

                    m_selectedSlotTrackId = trackId;
                    m_selectedSlot = action.sceneIndex;
                    SelectTrack(trackId);

                    auto const clipId = action.sceneIndex < track->Slots.size() ? track->Slots[action.sceneIndex] : std::wstring{};

                    if (m_slotTake.has_value() && m_slotTake->TrackId == trackId && m_slotTake->Scene == action.sceneIndex)
                    {
                        FinishSlotRecording(SlotTakeEnd::Early);
                    }
                    else if (clipId.empty() && m_armed.contains(trackId))
                    {
                        StartSlotRecording(trackId, action.sceneIndex);
                    }
                    else
                    {
                        LaunchSlot(trackId, action.sceneIndex);
                    }
                    break;
                }

                case seq::launchpad::SurfaceActionKind::LaunchScene:
                {
                    if (action.sceneIndex < m_doc.Scenes.size())
                    {
                        m_selectedSlot = action.sceneIndex;
                        ShowScene(action.sceneIndex);
                        LaunchScene(action.sceneIndex);
                    }
                    break;
                }

                case seq::launchpad::SurfaceActionKind::StopAll:
                    Stop();
                    break;

                case seq::launchpad::SurfaceActionKind::ToggleRecord:
                    if (m_recording)
                    {
                        StopRecording();
                    }
                    else if (m_slotTake.has_value())
                    {
                        FinishSlotRecording(SlotTakeEnd::Early);
                    }
                    else
                    {
                        StartRecording();
                    }
                    break;

                case seq::launchpad::SurfaceActionKind::SelectTrack:
                {
                    auto const order = seq::RemoteTrackOrder(m_doc);
                    if (action.trackIndex < order.size())
                    {
                        FlushLaunchpadXHeldNotes();
                        SelectTrack(order[action.trackIndex]);
                    }
                    break;
                }

                case seq::launchpad::SurfaceActionKind::SetViewport:
                    if (action.sceneIndex < m_doc.Scenes.size())
                    {
                        ShowScene(action.sceneIndex);
                    }
                    break;

                case seq::launchpad::SurfaceActionKind::SwitchMode:
                    m_launchpadMode = action.targetMode;
                    forceRefresh = true;
                    break;

                case seq::launchpad::SurfaceActionKind::ShiftOctave:
                    if (m_launchpadController != nullptr)
                    {
                        m_launchpadNoteConfig.octave = m_launchpadController->NoteConfig().octave;
                    }
                    break;

                case seq::launchpad::SurfaceActionKind::EmitNoteOn:
                case seq::launchpad::SurfaceActionKind::EmitNoteOff:
                case seq::launchpad::SurfaceActionKind::EmitPolyAftertouch:
                    RouteLaunchpadXNoteAction(action, timestamp);
                    break;

                default:
                    break;
                }
            }

            UpdateLaunchpadXFeedback(forceRefresh);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to dispatch Launchpad X actions.")
    }

    _Use_decl_annotations_
    void MainWindow::RouteLaunchpadXNoteAction(seq::launchpad::SurfaceAction const& action, uint64_t timestamp) noexcept
    {
        try
        {
            uint64_t const effectiveTimestamp = timestamp != 0 ? timestamp : midi2::MidiClock::Now();
            int64_t const tick = (m_engine != nullptr && m_recording) ? m_engine->TickAtTime(effectiveTimestamp) : -1;
            int64_t const slotTick = (m_engine != nullptr && m_slotRecording) ? m_engine->UnwrappedTickAtTime(effectiveTimestamp) : -1;
            double const frequency = static_cast<double>(midi2::MidiClock::TimestampFrequency());
            bool heardUpdated = false;

            auto const recordForTrack = [&](seq::Track const& track, uint32_t const* recWords, uint8_t wordCount)
            {
                if (!m_armed.contains(track.Id))
                {
                    return;
                }

                if (tick >= 0)
                {
                    if (auto take = m_takes.find(track.Id); take != m_takes.end())
                    {
                        take->second.Add(tick, recWords, wordCount);
                    }
                }

                if (slotTick >= 0 &&
                    m_slotInput.has_value() &&
                    m_slotInput->TrackId == track.Id &&
                    slotTick >= m_slotInput->Start - LaunchpadSlotEarlyTicks &&
                    slotTick < m_slotInput->Start + m_slotInput->Length)
                {
                    m_slotInput->Take.Add(slotTick, recWords, wordCount);
                }

                auto const filter = LaunchpadFilterFor(track.Source);
                if (seq::PassesRecordFilter(filter, recWords, wordCount))
                {
                    auto& heard = m_heard[track.Id];

                    HeardMessage msg{};
                    msg.Timestamp = effectiveTimestamp;
                    msg.WordCount = wordCount;
                    std::copy_n(recWords, wordCount, msg.Words.begin());
                    heard.push_back(msg);

                    while (heard.size() > LaunchpadMaximumHeardMessages ||
                        (!heard.empty() &&
                         effectiveTimestamp >= heard.front().Timestamp &&
                         static_cast<double>(effectiveTimestamp - heard.front().Timestamp) / frequency > LaunchpadHeardSeconds))
                    {
                        heard.pop_front();
                    }

                    heardUpdated = true;
                }
            };

            if (action.kind == seq::launchpad::SurfaceActionKind::EmitNoteOn)
            {
                // Release any previously active targets for this note before starting new ones.
                if (auto existing = m_launchpadHeldNotes.find(action.midiNote); existing != m_launchpadHeldNotes.end())
                {
                    for (auto const& held : existing->second)
                    {
                        if (held.Arpeggiated)
                        {
                            if (m_engine != nullptr)
                            {
                                m_engine->ReleaseKey(held.TrackId, action.midiNote);
                            }
                        }
                        else if (!held.Destination.Endpoint.IsEmpty())
                        {
                            SendNow(held.Destination, held.OffWords.data(), held.WordCount);
                        }
                    }

                    m_launchpadHeldNotes.erase(existing);
                }

                auto const targets = LaunchpadXNoteTargetTrackIds();
                std::vector<LaunchpadHeldNoteTarget> heldTargets{};
                uint16_t const vel16 = static_cast<uint16_t>(seq::ScaleUp(action.velocity, 7, 16));

                for (auto const& trackId : targets)
                {
                    auto const* track = seq::FindTrack(m_doc, trackId);
                    if (track == nullptr || track->IsFolder)
                    {
                        continue;
                    }

                    uint8_t const destChannel = static_cast<uint8_t>(std::max<int8_t>(0, track->Destination.Channel));

                    seq::Note note{};
                    note.Number = action.midiNote;
                    note.Channel = destChannel;
                    note.Velocity = vel16;
                    note.ReleaseVelocity = 0x8000;

                    uint32_t onWords[2]{};
                    uint32_t offWords[2]{};
                    seq::BuildNoteOn(note, track->Destination.Group, onWords);
                    seq::BuildNoteOff(note, track->Destination.Group, offWords);

                    bool arpeggiating = false;
                    if (m_engine != nullptr && seq::TrackHasHeldKeysArpeggiator(m_doc, *track))
                    {
                        m_engine->PressKey(track->Id, action.midiNote, vel16);
                        arpeggiating = m_engine->IsArpeggiating(track->Id);
                    }

                    if (!arpeggiating && !track->Destination.Endpoint.IsEmpty())
                    {
                        SendNow(track->Destination, onWords, 2);
                    }

                    heldTargets.push_back(LaunchpadHeldNoteTarget{
                        track->Id,
                        track->Destination,
                        { offWords[0], offWords[1] },
                        2,
                        arpeggiating,
                    });

                    if (!arpeggiating && m_armed.contains(track->Id))
                    {
                        uint8_t const recGroup = track->Source.Group >= 0
                            ? static_cast<uint8_t>(track->Source.Group & 0x0Fu)
                            : track->Destination.Group;
                        seq::Note recNote = note;
                        recNote.Channel = ResolveRecordChannel(track->Source.Channels, destChannel);

                        uint32_t recWords[2]{};
                        seq::BuildNoteOn(recNote, recGroup, recWords);

                        std::scoped_lock guard{ m_inputLock };
                        recordForTrack(*track, recWords, 2);
                    }
                }

                if (!heldTargets.empty())
                {
                    m_launchpadHeldNotes.emplace(action.midiNote, std::move(heldTargets));
                }
            }
            else if (action.kind == seq::launchpad::SurfaceActionKind::EmitNoteOff)
            {
                auto it = m_launchpadHeldNotes.find(action.midiNote);
                if (it != m_launchpadHeldNotes.end())
                {
                    for (auto const& held : it->second)
                    {
                        if (held.Arpeggiated)
                        {
                            if (m_engine != nullptr)
                            {
                                m_engine->ReleaseKey(held.TrackId, action.midiNote);
                            }
                        }
                        else if (!held.Destination.Endpoint.IsEmpty())
                        {
                            SendNow(held.Destination, held.OffWords.data(), held.WordCount);
                        }

                        if (!held.Arpeggiated && m_armed.contains(held.TrackId))
                        {
                            if (auto const* track = seq::FindTrack(m_doc, held.TrackId); track != nullptr && !track->IsFolder)
                            {
                                uint8_t const destChannel = static_cast<uint8_t>(std::max<int8_t>(0, track->Destination.Channel));
                                uint8_t const recGroup = track->Source.Group >= 0
                                    ? static_cast<uint8_t>(track->Source.Group & 0x0Fu)
                                    : track->Destination.Group;

                                seq::Note recNote{};
                                recNote.Number = action.midiNote;
                                recNote.Channel = ResolveRecordChannel(track->Source.Channels, destChannel);
                                recNote.ReleaseVelocity = 0x8000;

                                uint32_t recOffWords[2]{};
                                seq::BuildNoteOff(recNote, recGroup, recOffWords);

                                std::scoped_lock guard{ m_inputLock };
                                recordForTrack(*track, recOffWords, 2);
                            }
                        }
                    }

                    m_launchpadHeldNotes.erase(it);
                }
            }
            else if (action.kind == seq::launchpad::SurfaceActionKind::EmitPolyAftertouch)
            {
                auto const targets = LaunchpadXNoteTargetTrackIds();
                uint32_t const pressure32 = seq::ScaleUp(action.velocity, 7, 32);

                for (auto const& trackId : targets)
                {
                    auto const* track = seq::FindTrack(m_doc, trackId);
                    if (track == nullptr || track->IsFolder)
                    {
                        continue;
                    }

                    uint8_t const destChannel = static_cast<uint8_t>(std::max<int8_t>(0, track->Destination.Channel));
                    uint32_t words[2]{
                        0x40A00000u |
                            (static_cast<uint32_t>(track->Destination.Group & 0x0Fu) << 24) |
                            (static_cast<uint32_t>(destChannel & 0x0Fu) << 16) |
                            (static_cast<uint32_t>(action.midiNote & 0x7Fu) << 8),
                        pressure32,
                    };

                    if (!track->Destination.Endpoint.IsEmpty())
                    {
                        SendNow(track->Destination, words, 2);
                    }

                    if (m_armed.contains(track->Id))
                    {
                        uint8_t const recGroup = track->Source.Group >= 0
                            ? static_cast<uint8_t>(track->Source.Group & 0x0Fu)
                            : track->Destination.Group;
                        uint8_t const recChannel = ResolveRecordChannel(track->Source.Channels, destChannel);

                        uint32_t recWords[2]{
                            0x40A00000u |
                                (static_cast<uint32_t>(recGroup & 0x0Fu) << 24) |
                                (static_cast<uint32_t>(recChannel & 0x0Fu) << 16) |
                                (static_cast<uint32_t>(action.midiNote & 0x7Fu) << 8),
                            pressure32,
                        };

                        std::scoped_lock guard{ m_inputLock };
                        recordForTrack(*track, recWords, 2);
                    }
                }
            }

            if (heardUpdated)
            {
                UpdateTransport();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to route Launchpad X note event.")
    }

    void MainWindow::FlushLaunchpadXHeldNotes() noexcept
    {
        try
        {
            if (m_launchpadHeldNotes.empty())
            {
                return;
            }

            auto held = std::exchange(m_launchpadHeldNotes, {});

            for (auto const& [note, targets] : held)
            {
                for (auto const& target : targets)
                {
                    if (target.Arpeggiated)
                    {
                        if (m_engine != nullptr)
                        {
                            m_engine->ReleaseKey(target.TrackId, note);
                        }
                    }
                    else if (!target.Destination.Endpoint.IsEmpty())
                    {
                        SendNow(target.Destination, target.OffWords.data(), target.WordCount);
                    }
                }
            }
        }
        catch (...)
        {
        }
    }

    // ---------------------------------------------------------------- key, suppression & LED sync

    void MainWindow::SyncLaunchpadXKeyFromSong() noexcept
    {
        try
        {
            if (!m_launchpadFollowSongKey || m_launchpadController == nullptr)
            {
                return;
            }

            auto const key = seq::SongKeyAt(m_doc, m_position);
            if (!key.has_value())
            {
                return;
            }

            uint8_t const targetRoot = static_cast<uint8_t>(key->Root % 12u);
            if (targetRoot == m_launchpadNoteConfig.rootNote && key->Scale == m_launchpadNoteConfig.scale)
            {
                return;
            }

            m_launchpadNoteConfig.rootNote = targetRoot;
            m_launchpadNoteConfig.scale = key->Scale;

            std::vector<seq::launchpad::SurfaceAction> releaseActions{};
            m_launchpadController->SetNoteConfig(m_launchpadNoteConfig, releaseActions);

            if (!releaseActions.empty())
            {
                DispatchLaunchpadXActions(releaseActions, 0);
            }

            FlushLaunchpadXHeldNotes();
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void MainWindow::UpdateLaunchpadXSourceSuppression(std::wstring const& endpointId, uint8_t group) noexcept
    {
        try
        {
            std::scoped_lock guard{ m_inputLock };
            m_launchpadSourceId = endpointId;
            m_launchpadSourceGroup = static_cast<uint8_t>(group & 0x0Fu);
        }
        catch (...)
        {
        }
    }

    std::wstring MainWindow::EffectiveLaunchpadXTrackId() const
    {
        if (!m_selectedTrackId.empty())
        {
            if (auto const* track = seq::FindTrack(m_doc, m_selectedTrackId); track != nullptr && !track->IsFolder)
            {
                return m_selectedTrackId;
            }
        }

        auto const order = seq::RemoteTrackOrder(m_doc);
        return order.empty() ? std::wstring{} : order.front();
    }

    std::vector<std::wstring> MainWindow::LaunchpadXNoteTargetTrackIds() const
    {
        std::vector<std::wstring> targets{};

        if (!m_armed.empty())
        {
            for (auto const& id : seq::RemoteTrackOrder(m_doc))
            {
                if (m_armed.contains(id))
                {
                    targets.push_back(id);
                }
            }

            if (!targets.empty())
            {
                return targets;
            }
        }

        if (auto const effective = EffectiveLaunchpadXTrackId(); !effective.empty())
        {
            targets.push_back(effective);
        }

        return targets;
    }

    seq::launchpad::SurfaceStateSnapshot MainWindow::BuildLaunchpadXSnapshot() const
    {
        std::vector<seq::TrackLaunchView> views{};
        views.reserve(m_launchViews.size());

        for (auto const& [id, view] : m_launchViews)
        {
            views.push_back(view);
        }

        auto const selectedTrackId = EffectiveLaunchpadXTrackId();
        uint32_t const selectedScene =
            (m_selectedSlot != SIZE_MAX && m_selectedSlot < m_doc.Scenes.size())
                ? static_cast<uint32_t>(m_selectedSlot)
                : 0u;

        bool const transportPlaying = m_engine != nullptr && m_engine->IsPlaying();
        bool const transportRecording = m_recording.load() || m_slotRecording.load();
        bool const slotRecording = m_slotTake.has_value();
        std::wstring_view const slotTrackId = slotRecording ? std::wstring_view{ m_slotTake->TrackId } : std::wstring_view{};
        uint32_t const slotScene = slotRecording ? static_cast<uint32_t>(m_slotTake->Scene) : 0u;
        bool const selectedTrackArmed = !selectedTrackId.empty() && m_armed.contains(selectedTrackId);

        return seq::launchpad::BuildSurfaceSnapshot(
            m_doc,
            views,
            selectedTrackId,
            selectedScene,
            transportPlaying,
            transportRecording,
            slotRecording,
            slotTrackId,
            slotScene,
            m_launchpadNoteConfig,
            selectedTrackArmed);
    }

    _Use_decl_annotations_
    void MainWindow::UpdateLaunchpadXFeedback(bool forceFullRefresh) noexcept
    {
        try
        {
            if (!m_launchpadEnabled || m_launchpadController == nullptr || !IsLaunchpadXConnected())
            {
                return;
            }

            auto const snapshot = BuildLaunchpadXSnapshot();
            (void)m_launchpadController->SyncLeds(snapshot, forceFullRefresh);
        }
        catch (...)
        {
        }
    }
}
