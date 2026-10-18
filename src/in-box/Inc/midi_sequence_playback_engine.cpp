// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "midi_sequence_playback_engine.h"

#include <winrt/Windows.Devices.Midi2.Utilities.Messages.h>

// Declared here rather than relying on a host project's headers, so this compiles the same way
// inside the SDK and inside a tool.
namespace midi2 = ::winrt::Windows::Devices::Midi2;
namespace midi2msg = ::winrt::Windows::Devices::Midi2::Utilities::Messages;

namespace midiplayer
{
    namespace
    {
        constexpr uint32_t Midi1ChannelVoiceMessageType = 0x2;

        constexpr uint8_t StatusNoteOff = 0x80;
        constexpr uint8_t StatusNoteOn = 0x90;
        constexpr uint8_t StatusControlChange = 0xB0;
        constexpr uint8_t StatusPitchBend = 0xE0;

        constexpr uint8_t ControllerBankSelectMsb = 0x00;
        constexpr uint8_t ControllerBankSelectLsb = 0x20;
        constexpr uint8_t ControllerSustainPedal = 0x40;
        constexpr uint8_t ControllerAllSoundOff = 0x78;
        constexpr uint8_t ControllerAllNotesOff = 0x7B;

        // All sound off, all notes off and the four mode messages after it each end every note
        // on the channel. Reset all controllers and local control, between them, do not.
        constexpr bool EndsEveryNote(uint8_t controller) noexcept
        {
            return controller == ControllerAllSoundOff || controller >= ControllerAllNotesOff;
        }

        bool HasOption(ChaseOptions options, ChaseOptions option) noexcept
        {
            return (options & option) == option;
        }

        // A little runway so the first messages of a sequence are scheduled rather than late.
        constexpr uint32_t StartLeadMilliseconds = 60;

        // How long after a stop the safety sweep lands. It has to outlast anything already handed
        // to the service, which is at most one look ahead window.
        constexpr uint32_t PanicSweepMarginMilliseconds = 60;

        uint32_t BuildMidi1Word(uint8_t group, uint8_t status, uint8_t data1, uint8_t data2) noexcept
        {
            return (Midi1ChannelVoiceMessageType << 28)
                | (static_cast<uint32_t>(group & 0x0F) << 24)
                | (static_cast<uint32_t>(status) << 16)
                | (static_cast<uint32_t>(data1 & 0x7F) << 8)
                | static_cast<uint32_t>(data2 & 0x7F);
        }

        // Word count for each Universal MIDI Packet message type, from the specification. Worked
        // out here rather than called across the ABI, because this runs per packet on the way out.
        constexpr std::array<uint8_t, 16> PacketWordCounts{ 1, 1, 1, 2, 2, 4, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4 };

        constexpr uint8_t MessageTypeOf(uint32_t word) noexcept
        {
            return static_cast<uint8_t>((word >> 28) & 0x0F);
        }

        // Utility and UMP Stream messages have no group field; everything else carries it in the
        // same four bits.
        constexpr bool MessageTypeCarriesGroup(uint8_t messageType) noexcept
        {
            return messageType != 0x0 && messageType != 0xF;
        }

        constexpr bool MessageTypeCarriesChannel(uint8_t messageType) noexcept
        {
            return messageType == 0x2 || messageType == 0x4;
        }

        uint64_t TicksFromMilliseconds(uint32_t milliseconds) noexcept
        {
            return static_cast<uint64_t>(milliseconds) * midi2::MidiClock::TimestampFrequency() / 1000;
        }
    }

    PlaybackEngine::~PlaybackEngine() noexcept
    {
        StopWorker();
        Close();
    }

    _Use_decl_annotations_
    OpenResult PlaybackEngine::Open(
        midi2::MidiSession const& session,
        std::wstring const& endpointDeviceId) noexcept
    {
        try
        {
            if (endpointDeviceId.empty())
            {
                return OpenResult::NoEndpointChosen;
            }

            if (session == nullptr)
            {
                return OpenResult::SessionFailed;
            }

            std::lock_guard<std::recursive_mutex> const guard{ m_lock };

            if (m_connection != nullptr && m_endpointDeviceId == endpointDeviceId)
            {
                return OpenResult::Success;
            }

            Close();

            if (!midi2::MidiApi::EnsureServiceAvailable())
            {
                return OpenResult::ServiceUnavailable;
            }

            // The session belongs to the caller. Only the connection is ours to close.
            m_session = session;
            m_ownsConnection = true;

            m_connection = m_session.CreateEndpointConnection(winrt::hstring{ endpointDeviceId });

            if (m_connection == nullptr)
            {
                return OpenResult::EndpointNotFound;
            }

            if (!m_connection.Open())
            {
                m_session.DisconnectEndpointConnection(m_connection.ConnectionId());
                m_connection = nullptr;

                return OpenResult::ConnectionFailed;
            }

            m_endpointDeviceId = endpointDeviceId;

            MIDI_PLAYER_LOG_INFO_WITH_ENDPOINT(L"Player connected.", endpointDeviceId.c_str());

            StartWorker();

            return OpenResult::Success;
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to connect to the endpoint.")

        return OpenResult::ConnectionFailed;
    }

    _Use_decl_annotations_
    void PlaybackEngine::AttachConnection(midi2::MidiEndpointConnection const& connection) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> const guard{ m_lock };

            Close();

            if (connection == nullptr)
            {
                return;
            }

            m_connection = connection;
            m_session = nullptr;
            m_ownsConnection = false;

            m_endpointDeviceId = connection.ConnectedEndpointDeviceId().c_str();

            StartWorker();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to attach the connection.")
    }

    bool PlaybackEngine::OwnsConnection() const noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };

        return m_ownsConnection;
    }

    midi2::MidiEndpointConnection PlaybackEngine::Connection() const noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };

        return m_connection;
    }

    void PlaybackEngine::Close() noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> const guard{ m_lock };

            if (m_state == PlaybackState::Playing)
            {
                SendPanicUnderLock(false);
            }

            m_state = m_sequence == nullptr ? PlaybackState::Empty : PlaybackState::Stopped;

            // A borrowed connection is left exactly as it was found.
            if (m_ownsConnection && m_connection != nullptr && m_session != nullptr)
            {
                m_session.DisconnectEndpointConnection(m_connection.ConnectionId());
            }

            m_connection = nullptr;
            m_session = nullptr;
            m_ownsConnection = false;

            m_endpointDeviceId.clear();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to close the connection.")
    }

    bool PlaybackEngine::IsOpen() const noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };
        return m_connection != nullptr;
    }

    std::wstring PlaybackEngine::EndpointDeviceId() const noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };
        return m_endpointDeviceId;
    }

    _Use_decl_annotations_
    bool PlaybackEngine::Load(
        std::shared_ptr<midifile::MidiSequence const> const& sequence,
        uint8_t groupIndex) noexcept
    {
        try
        {
            if (sequence == nullptr || sequence->IsEmpty())
            {
                Unload();
                return false;
            }

            // Conversion happens once, here, rather than in the playback sweep: a file is
            // thousands of messages and the sweep runs while music is playing.
            std::vector<PreparedEvent> prepared{};
            std::vector<uint32_t> words{};
            uint32_t widestEvent = 1;

            prepared.reserve(sequence->Events.size());
            words.reserve(sequence->Events.size() * 2);

            midi2::MidiGroup const group{ groupIndex };

            auto scratch = winrt::single_threaded_vector<uint8_t>();

            // Followed in playing order, so each event knows which channels are drum kits when
            // it plays, wherever playback starts.
            midifile::PercussionTracker percussion{};

            for (auto const& event : sequence->Events)
            {
                auto const bytes = sequence->BytesOf(event);

                percussion.Observe(event, bytes);

                if (bytes.empty())
                {
                    continue;
                }

                PreparedEvent entry{};

                entry.Microseconds = sequence->MicrosecondsAtTick(event.Tick);
                entry.Tick = event.Tick;
                entry.WordOffset = static_cast<uint32_t>(words.size());
                entry.TrackIndex = event.TrackIndex;
                entry.PercussionChannels = percussion.PercussionChannels();

                if (event.Kind == midifile::EventKind::UniversalPacket)
                {
                    // Already Universal MIDI Packet words. Nothing converts them; they are copied
                    // through and only the group is stamped, because the group belongs to where
                    // the sequence is being played rather than to what was written.
                    if (bytes.size() < sizeof(uint32_t) || (bytes.size() % sizeof(uint32_t)) != 0)
                    {
                        continue;
                    }

                    auto const wordCount = static_cast<uint32_t>(bytes.size() / sizeof(uint32_t));

                    words.resize(words.size() + wordCount);

                    // memcpy because the blob packs events end to end and a word can land on any
                    // byte offset.
                    std::memcpy(words.data() + entry.WordOffset, bytes.data(), bytes.size());

                    for (uint32_t index = 0; index < wordCount; )
                    {
                        auto& word = words[static_cast<size_t>(entry.WordOffset) + index];
                        auto const messageType = MessageTypeOf(word);

                        if (MessageTypeCarriesGroup(messageType))
                        {
                            word = (word & 0xF0FFFFFFu) | (static_cast<uint32_t>(groupIndex & 0x0F) << 24);
                        }

                        auto const packetWords = PacketWordCounts[messageType];

                        if (packetWords == 0 || index + packetWords > wordCount)
                        {
                            break;
                        }

                        index += packetWords;
                    }

                    entry.WordCount = wordCount;
                }
                else
                {
                    scratch.ReplaceAll(winrt::array_view<uint8_t const>{ bytes.data(), bytes.data() + bytes.size() });

                    // Stored messages always carry an explicit status byte, so running status is off.
                    auto const converted = midi2msg::MidiMessageConverter::ConvertMidi1CompleteMessageBytesToUmpWords(
                        group, scratch, false);

                    if (converted == nullptr || converted.Size() == 0)
                    {
                        continue;
                    }

                    entry.WordCount = converted.Size();

                    words.resize(words.size() + converted.Size());

                    converted.GetMany(
                        0,
                        winrt::array_view<uint32_t>{ words.data() + entry.WordOffset, words.data() + words.size() });
                }

                if (event.Kind == midifile::EventKind::NoteOn || event.Kind == midifile::EventKind::NoteOff)
                {
                    auto const velocity = bytes.size() > 2 ? static_cast<uint8_t>(bytes[2] & 0x7F) : uint8_t{ 0 };

                    entry.Action = (event.Kind == midifile::EventKind::NoteOn && velocity > 0)
                        ? NoteAction::Start
                        : NoteAction::End;
                }

                entry.NamesNotes = NoteTransposer::NamesAnyNote(
                    std::span<uint32_t const>{ words.data() + entry.WordOffset, entry.WordCount });

                if (entry.WordCount > widestEvent)
                {
                    widestEvent = entry.WordCount;
                }

                prepared.push_back(entry);
            }

            if (prepared.empty())
            {
                Unload();
                return false;
            }

            {
                std::lock_guard<std::recursive_mutex> const guard{ m_lock };

                if (m_state == PlaybackState::Playing)
                {
                    SendPanicUnderLock(true);
                }

                m_sequence = sequence;
                m_prepared = std::move(prepared);
                m_words = std::move(words);
                m_groupIndex = groupIndex;

                m_mutedTracks.assign(sequence->Tracks.size(), false);
                m_soloTrack = -1;

                // Routes survive a new sequence where the track still exists, because an
                // application sets them up once for a rig rather than once per file.
                m_trackRoutes.resize(sequence->Tracks.size());
                RebuildDestinationsUnderLock();

                // Sized once so a rewritten send never allocates on the sweep. Twice the widest
                // event, because transposing can end a note before starting one.
                m_sendScratch.clear();
                m_sendScratch.reserve(static_cast<size_t>(widestEvent) * 2);

                m_state = PlaybackState::Stopped;
                m_pausedMicroseconds = 0;
                m_nextEventIndex = 0;
                m_soundingNotes.fill(0);
                m_transposer.Clear();
            }

            return true;
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to prepare the sequence.")

        return false;
    }

    void PlaybackEngine::Unload() noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> const guard{ m_lock };

            if (m_state == PlaybackState::Playing)
            {
                SendPanicUnderLock(true);
            }

            m_sequence.reset();
            m_prepared.clear();
            m_words.clear();

            m_state = PlaybackState::Empty;
            m_pausedMicroseconds = 0;
            m_nextEventIndex = 0;
            m_soundingNotes.fill(0);
            m_transposer.Clear();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to unload the sequence.")
    }

    void PlaybackEngine::Play() noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> const guard{ m_lock };

            if (m_sequence == nullptr || m_prepared.empty() || m_connection == nullptr)
            {
                return;
            }

            if (m_state == PlaybackState::Playing)
            {
                return;
            }

            if (m_pausedMicroseconds >= m_sequence->DurationMicroseconds)
            {
                m_pausedMicroseconds = 0;
            }

            RebaseClockUnderLock();

            // Bank, program, controllers and pitch bend as they stood at this point in the file,
            // or a note starting part way in plays with whatever sound was last selected. And,
            // when the application asked for them, the notes that should already be sounding.
            SendChaseStateUnderLock(m_pausedMicroseconds, m_chaseOptions, -1);

            m_state = PlaybackState::Playing;
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to start playback.")

        m_wakeUp.SetEvent();
    }

    void PlaybackEngine::Pause() noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> const guard{ m_lock };

            if (m_state != PlaybackState::Playing)
            {
                return;
            }

            m_pausedMicroseconds = CurrentMicrosecondsUnderLock(midi2::MidiClock::Now());
            m_state = PlaybackState::Paused;

            SendPanicUnderLock(true);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to pause playback.")
    }

    void PlaybackEngine::Stop() noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> const guard{ m_lock };

            if (m_sequence == nullptr)
            {
                return;
            }

            auto const wasPlaying = m_state == PlaybackState::Playing;

            m_state = PlaybackState::Stopped;
            m_pausedMicroseconds = 0;
            m_nextEventIndex = 0;

            if (wasPlaying)
            {
                SendPanicUnderLock(true);
            }
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to stop playback.")
    }

    void PlaybackEngine::SeekToMicroseconds(uint64_t microseconds) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> const guard{ m_lock };

            if (m_sequence == nullptr || m_prepared.empty())
            {
                return;
            }

            auto const duration = m_sequence->DurationMicroseconds;

            m_pausedMicroseconds = microseconds > duration ? duration : microseconds;

            auto const wasPlaying = m_state == PlaybackState::Playing;

            if (wasPlaying)
            {
                SendPanicUnderLock(true);
            }

            RebaseClockUnderLock();

            if (wasPlaying)
            {
                SendChaseStateUnderLock(m_pausedMicroseconds, m_chaseOptions, -1);
            }
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to seek.")

        m_wakeUp.SetEvent();
    }

    _Use_decl_annotations_
    void PlaybackEngine::SetPlaybackRate(double rate) noexcept
    {
        auto playing = false;

        try
        {
            std::lock_guard<std::recursive_mutex> const guard{ m_lock };

            if (!m_clock.RequestRate(rate))
            {
                return;
            }

            playing = m_state == PlaybackState::Playing;
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to change the playback rate.")

        // While playing, the worker moves the clock over on its next pass. Waking it makes that
        // this pass rather than one interval from now. Otherwise the next Play picks it up.
        if (playing)
        {
            m_wakeUp.SetEvent();
        }
    }

    double PlaybackEngine::PlaybackRate() const noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };
        return m_clock.RequestedRate();
    }

    _Use_decl_annotations_
    void PlaybackEngine::SetTransposeSemitones(int32_t semitones) noexcept
    {
        // Nothing to wake: what is already with the service keeps its pitch, and the worker's
        // next pass, at most one sweep interval away, hands over everything after it transposed.
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };
        m_transposer.SetSemitones(semitones);
    }

    int32_t PlaybackEngine::TransposeSemitones() const noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };
        return m_transposer.Semitones();
    }

    _Use_decl_annotations_
    void PlaybackEngine::SetChaseOptions(ChaseOptions options) noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };

        m_chaseOptions = options & ChaseOptions::All;
    }

    ChaseOptions PlaybackEngine::GetChaseOptions() const noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };
        return m_chaseOptions;
    }

    PlaybackState PlaybackEngine::State() const noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };
        return m_state;
    }

    PlaybackPosition PlaybackEngine::Position() const noexcept
    {
        PlaybackPosition position{};

        try
        {
            std::lock_guard<std::recursive_mutex> const guard{ m_lock };

            position.State = m_state;

            if (m_sequence == nullptr)
            {
                return position;
            }

            position.DurationMicroseconds = m_sequence->DurationMicroseconds;

            position.Microseconds = m_state == PlaybackState::Playing
                ? CurrentMicrosecondsUnderLock(midi2::MidiClock::Now())
                : m_pausedMicroseconds;

            if (position.Microseconds > position.DurationMicroseconds)
            {
                position.Microseconds = position.DurationMicroseconds;
            }

            position.Tick = m_sequence->TickAtMicroseconds(position.Microseconds);

            auto const bar = m_sequence->BarPositionAtTick(position.Tick);

            position.Bar = bar.Bar;
            position.Beat = bar.Beat;
            position.BeatsPerMinute = m_sequence->BeatsPerMinuteAtTick(position.Tick);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to read the position.")

        return position;
    }

    _Use_decl_annotations_
    void PlaybackEngine::SetCompletionHandler(std::function<void()> handler) noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };
        m_completionHandler = std::move(handler);
    }

    void PlaybackEngine::SetTrackMuted(uint16_t trackIndex, bool muted) noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };

        if (trackIndex >= m_mutedTracks.size())
        {
            return;
        }

        if (m_mutedTracks[trackIndex] == muted)
        {
            return;
        }

        m_mutedTracks[trackIndex] = muted;

        // Whatever this track already started has to be turned off, or muting leaves a chord
        // hanging until the file happens to end it.
        if (muted && m_state == PlaybackState::Playing)
        {
            SendPanicUnderLock(false);
        }

        if (m_state == PlaybackState::Playing && HasOption(m_chaseOptions, ChaseOptions::HeldNotes))
        {
            auto const position = CurrentMicrosecondsUnderLock(midi2::MidiClock::Now());

            if (muted)
            {
                // That silence turned off every held note on every channel, not only this
                // track's, and reset sustain and pitch bend. Put back what the others still hold.
                SendChaseStateUnderLock(position, m_chaseOptions, -1);
            }
            else
            {
                // Nothing was silenced, so only this track's own held notes need to come back.
                SendChaseStateUnderLock(position, ChaseOptions::HeldNotes, static_cast<int32_t>(trackIndex));
            }
        }
    }

    void PlaybackEngine::SetSoloTrack(int32_t trackIndex) noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };

        auto const wanted = (trackIndex >= 0 && static_cast<size_t>(trackIndex) < m_mutedTracks.size())
            ? trackIndex
            : -1;

        if (m_soloTrack == wanted)
        {
            return;
        }

        m_soloTrack = wanted;

        if (m_state == PlaybackState::Playing)
        {
            SendPanicUnderLock(false);

            // That silence turned off every held note. Put back what the tracks that can still
            // be heard are holding.
            if (HasOption(m_chaseOptions, ChaseOptions::HeldNotes))
            {
                SendChaseStateUnderLock(CurrentMicrosecondsUnderLock(midi2::MidiClock::Now()), m_chaseOptions, -1);
            }
        }
    }

    bool PlaybackEngine::IsTrackMuted(uint16_t trackIndex) const noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };

        return trackIndex < m_mutedTracks.size() && m_mutedTracks[trackIndex];
    }

    int32_t PlaybackEngine::SoloTrack() const noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };
        return m_soloTrack;
    }

    bool PlaybackEngine::IsTrackAudible(uint16_t trackIndex) const noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };
        return IsTrackAudibleUnderLock(trackIndex);
    }

    bool PlaybackEngine::IsTrackAudibleUnderLock(uint16_t trackIndex) const noexcept
    {
        if (m_soloTrack >= 0)
        {
            return trackIndex == static_cast<uint16_t>(m_soloTrack);
        }

        return trackIndex >= m_mutedTracks.size() || !m_mutedTracks[trackIndex];
    }

    // ------------------------------------------------------------------------------------------
    // worker
    // ------------------------------------------------------------------------------------------

    void PlaybackEngine::StartWorker() noexcept
    {
        if (m_workerRunning.load())
        {
            return;
        }

        m_stopRequested.store(false);
        m_workerRunning.store(true);

        try
        {
            m_worker = std::thread([this]() noexcept { WorkerThread(); });
        }
        catch (...)
        {
            m_workerRunning.store(false);
        }
    }

    void PlaybackEngine::StopWorker() noexcept
    {
        m_stopRequested.store(true);
        m_wakeUp.SetEvent();

        if (m_worker.joinable())
        {
            try
            {
                m_worker.join();
            }
            catch (...)
            {
            }
        }

        m_workerRunning.store(false);
    }

    void PlaybackEngine::WorkerThread() noexcept
    try
    {
        while (!m_stopRequested.load())
        {
            m_wakeUp.wait(SweepIntervalMilliseconds);
            m_wakeUp.ResetEvent();

            if (m_stopRequested.load())
            {
                break;
            }

            std::function<void()> completion{};

            {
                std::lock_guard<std::recursive_mutex> const guard{ m_lock };

                if (m_state != PlaybackState::Playing || m_connection == nullptr)
                {
                    continue;
                }

                auto const now = midi2::MidiClock::Now();

                // Before anything new is handed over, so a new rate starts where the last pass
                // left off.
                m_clock.ApplyRequestedRate(now);

                ScheduleDueEventsUnderLock(now);

                if (m_nextEventIndex >= m_prepared.size() &&
                    CurrentMicrosecondsUnderLock(now) >= m_sequence->DurationMicroseconds)
                {
                    // A file is not obliged to end its own notes, and a truncated one often does
                    // not, so reaching the end has to silence the instrument like a stop does.
                    // Immediate only: everything is already in the past, and a swept panic would
                    // land on top of the next item in the queue.
                    SendPanicUnderLock(false);

                    m_state = PlaybackState::Stopped;
                    m_pausedMicroseconds = 0;
                    m_nextEventIndex = 0;

                    completion = m_completionHandler;
                }
            }

            if (completion)
            {
                completion();
            }
        }
    }
    catch (...)
    {
        // A thread body must never let anything escape: it would take the whole app down.
        MIDI_PLAYER_LOG_GENERAL_EXCEPTION(L"The playback worker stopped unexpectedly.");
    }

    void PlaybackEngine::ScheduleDueEventsUnderLock(uint64_t nowTimestamp) noexcept
    {
        auto const horizon = nowTimestamp + TicksFromMilliseconds(LookAheadMilliseconds);

        while (m_nextEventIndex < m_prepared.size())
        {
            auto const& entry = m_prepared[m_nextEventIndex];

            auto const timestamp = TimestampForMicrosecondsUnderLock(entry.Microseconds);

            if (timestamp > horizon)
            {
                break;
            }

            // A message whose moment has already gone still has to be sent, or a note is left on.
            // A muted track loses only its note STARTS: ends, program changes and controllers all
            // still go, so nothing hangs and the sound stays correct when it is unmuted.
            if (entry.Action != NoteAction::Start || IsTrackAudibleUnderLock(entry.TrackIndex))
            {
                SendEventUnderLock(
                    timestamp <= nowTimestamp ? midi2::MidiClock::TimestampConstantSendImmediately() : timestamp,
                    entry);
            }

            ++m_nextEventIndex;
        }

        // Everything due by the horizon is now with the service, whether or not anything was due.
        m_clock.MarkScheduledThrough(horizon);
    }

    void PlaybackEngine::RebaseClockUnderLock() noexcept
    {
        auto const now = midi2::MidiClock::Now();

        // The second pass of a silence still on its way would land on top of the music, cut off
        // its first notes and undo the chase. So the music waits for it, then starts after the
        // same short lead as always.
        auto const earliest = m_sweepTimestamp > now ? m_sweepTimestamp : now;

        m_clock.Start(
            midi2::MidiClock::TimestampFrequency(),
            earliest + TicksFromMilliseconds(StartLeadMilliseconds),
            m_pausedMicroseconds);

        auto const found = std::lower_bound(
            m_prepared.begin(),
            m_prepared.end(),
            m_pausedMicroseconds,
            [](PreparedEvent const& entry, uint64_t value) noexcept { return entry.Microseconds < value; });

        m_nextEventIndex = static_cast<size_t>(std::distance(m_prepared.begin(), found));
    }

    uint64_t PlaybackEngine::TimestampForMicrosecondsUnderLock(uint64_t microseconds) const noexcept
    {
        return m_clock.TimestampForMicroseconds(microseconds);
    }

    uint64_t PlaybackEngine::CurrentMicrosecondsUnderLock(uint64_t nowTimestamp) const noexcept
    {
        return m_clock.MicrosecondsAtTimestamp(nowTimestamp);
    }

    _Use_decl_annotations_
    void PlaybackEngine::SendEventUnderLock(uint64_t timestamp, PreparedEvent const& entry) noexcept
    {
        auto const destinationIndex = DestinationForTrackUnderLock(entry.TrackIndex);

        if (entry.WordCount == 0 || static_cast<size_t>(entry.WordOffset) + entry.WordCount > m_words.size() ||
            destinationIndex >= m_destinations.size())
        {
            return;
        }

        auto const& destination = m_destinations[destinationIndex];

        if (!entry.NamesNotes && !destination.RewritesMessages)
        {
            auto const connection = ConnectionForDestinationUnderLock(destinationIndex);

            if (connection == nullptr)
            {
                return;
            }

            connection.SendMultipleMessagesWordArray(
                timestamp,
                entry.WordOffset,
                entry.WordCount,
                winrt::array_view<uint32_t const>{ m_words.data(), m_words.data() + m_words.size() });

            return;
        }

        std::span<uint32_t const> const original{ m_words.data() + entry.WordOffset, entry.WordCount };

        // The scratch buffer was sized at load to twice the widest event; none of this allocates.
        if (entry.NamesNotes)
        {
            // Every note goes through the transposer, moved or not, so an end always finds the
            // pitch its start went out at.
            m_sendScratch.resize(original.size() * 2);

            auto const written = m_transposer.Rewrite(original, m_sendScratch, entry.PercussionChannels);

            m_sendScratch.resize(written);

            // Before the route changes the channel: what is sounding is kept by the sequence's
            // own channel, and the silence works out each route's channel from it.
            if (entry.Action != NoteAction::None)
            {
                TrackSoundingNotesUnderLock(m_sendScratch);
            }

            if (m_sendScratch.empty())
            {
                return;
            }
        }
        else
        {
            m_sendScratch.assign(original.begin(), original.end());
        }

        // The group is baked into the words at load time, so a track sent to a different group or
        // channel gets a rewritten copy.
        if (destination.RewritesMessages)
        {
            size_t index = 0;

            while (index < m_sendScratch.size())
            {
                auto const messageType = MessageTypeOf(m_sendScratch[index]);
                auto const packetWords = PacketWordCounts[messageType];

                if (index + packetWords > m_sendScratch.size())
                {
                    break;
                }

                if (MessageTypeCarriesGroup(messageType))
                {
                    m_sendScratch[index] = (m_sendScratch[index] & 0xF0FFFFFFu)
                        | (static_cast<uint32_t>(destination.GroupIndex & 0x0F) << 24);
                }

                if (destination.ChannelOverride >= 0 && MessageTypeCarriesChannel(messageType))
                {
                    m_sendScratch[index] = (m_sendScratch[index] & 0xFFF0FFFFu)
                        | (static_cast<uint32_t>(destination.ChannelOverride & 0x0F) << 16);
                }

                index += packetWords;
            }
        }

        auto const connection = ConnectionForDestinationUnderLock(destinationIndex);

        if (connection == nullptr)
        {
            return;
        }

        connection.SendMultipleMessagesWordArray(
            timestamp,
            0,
            static_cast<uint32_t>(m_sendScratch.size()),
            winrt::array_view<uint32_t const>{ m_sendScratch.data(), m_sendScratch.data() + m_sendScratch.size() });
    }

    _Use_decl_annotations_
    void PlaybackEngine::TrackSoundingNotesUnderLock(std::span<uint32_t const> words) noexcept
    {
        for (size_t index = 0; index < words.size(); index += PacketWordCounts[MessageTypeOf(words[index])])
        {
            auto const word = words[index];

            if (MessageTypeOf(word) != Midi1ChannelVoiceMessageType)
            {
                continue;
            }

            auto const status = static_cast<uint8_t>((word >> 16) & 0xF0);
            auto const channelBit = static_cast<uint16_t>(1u << ((word >> 16) & 0x0F));
            auto const note = static_cast<size_t>((word >> 8) & 0x7F);

            if (status == StatusNoteOn && (word & 0x7F) != 0)
            {
                m_soundingNotes[note] |= channelBit;
            }
            else if (status == StatusNoteOn || status == StatusNoteOff)
            {
                m_soundingNotes[note] &= static_cast<uint16_t>(~channelBit);
            }
        }
    }

    winrt::Windows::Devices::Midi2::MidiEndpointConnection PlaybackEngine::ConnectionForDestinationUnderLock(
        size_t destinationIndex) const noexcept
    {
        if (destinationIndex >= m_destinations.size())
        {
            return m_connection;
        }

        auto const& destination = m_destinations[destinationIndex];

        return destination.Connection != nullptr ? destination.Connection : m_connection;
    }

    size_t PlaybackEngine::DestinationForTrackUnderLock(uint16_t trackIndex) const noexcept
    {
        if (!m_hasCustomRoutes || trackIndex >= m_trackDestination.size())
        {
            return 0;
        }

        return m_trackDestination[trackIndex];
    }

    void PlaybackEngine::RebuildDestinationsUnderLock() noexcept
    {
        try
        {
            m_destinations.clear();
            m_destinations.push_back(Destination{ nullptr, m_groupIndex, -1, false });

            m_trackDestination.assign(m_trackRoutes.size(), uint16_t{ 0 });

            m_hasCustomRoutes = false;

            for (size_t track = 0; track < m_trackRoutes.size(); ++track)
            {
                auto const& route = m_trackRoutes[track];

                if (route.IsDefault())
                {
                    continue;
                }

                m_hasCustomRoutes = true;

                Destination wanted{};

                wanted.Connection = route.Connection;
                wanted.GroupIndex = route.GroupIndex >= 0
                    ? static_cast<uint8_t>(route.GroupIndex & 0x0F)
                    : m_groupIndex;
                wanted.ChannelOverride = route.ChannelOverride >= 0 ? (route.ChannelOverride & 0x0F) : -1;
                wanted.RewritesMessages = wanted.GroupIndex != m_groupIndex || wanted.ChannelOverride >= 0;

                size_t found = m_destinations.size();

                for (size_t candidate = 0; candidate < m_destinations.size(); ++candidate)
                {
                    auto const& existing = m_destinations[candidate];

                    if (existing.Connection == wanted.Connection &&
                        existing.GroupIndex == wanted.GroupIndex &&
                        existing.ChannelOverride == wanted.ChannelOverride)
                    {
                        found = candidate;
                        break;
                    }
                }

                if (found == m_destinations.size())
                {
                    m_destinations.push_back(wanted);
                }

                m_trackDestination[track] = static_cast<uint16_t>(found);
            }
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to work out where the tracks should be sent.")
    }

    _Use_decl_annotations_
    void PlaybackEngine::SetTrackRoute(uint16_t trackIndex, TrackRoute const& route) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> const guard{ m_lock };

            if (trackIndex >= m_trackRoutes.size())
            {
                m_trackRoutes.resize(static_cast<size_t>(trackIndex) + 1);
            }

            m_trackRoutes[trackIndex] = route;

            RebuildDestinationsUnderLock();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to set the track route.")
    }

    void PlaybackEngine::ClearTrackRoute(uint16_t trackIndex) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> const guard{ m_lock };

            if (trackIndex >= m_trackRoutes.size())
            {
                return;
            }

            m_trackRoutes[trackIndex] = TrackRoute{};

            RebuildDestinationsUnderLock();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to clear the track route.")
    }

    TrackRoute PlaybackEngine::GetTrackRoute(uint16_t trackIndex) const noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };

        if (trackIndex >= m_trackRoutes.size())
        {
            return TrackRoute{};
        }

        return m_trackRoutes[trackIndex];
    }

    void PlaybackEngine::SendPanicUnderLock(bool includeScheduledSweep) noexcept
    {
        // A panic can be asked for before anything has been loaded, and entry zero is what makes
        // it reach the engine's own connection.
        if (m_destinations.empty())
        {
            RebuildDestinationsUnderLock();
        }

        // The second pass lands after anything the service has already been handed, because there
        // is no way to recall a scheduled message. One time for every destination, so the next
        // start has a single moment to wait for.
        uint64_t sweepTimestamp{ 0 };

        if (includeScheduledSweep)
        {
            sweepTimestamp = midi2::MidiClock::Now()
                + TicksFromMilliseconds(LookAheadMilliseconds + PanicSweepMarginMilliseconds);

            if (sweepTimestamp > m_sweepTimestamp)
            {
                m_sweepTimestamp = sweepTimestamp;
            }
        }

        // Every destination, not just the default one. A note left sounding on an endpoint the
        // panic skipped is exactly the failure a panic exists to prevent, and sending a note off
        // to a device that was not playing it costs nothing.
        for (size_t destination = 0; destination < m_destinations.size(); ++destination)
        {
            SendPanicToDestinationUnderLock(destination, sweepTimestamp);
        }

        m_soundingNotes.fill(0);

        // Every note is off, so none has a pitch left to remember.
        m_transposer.Clear();
    }

    _Use_decl_annotations_
    void PlaybackEngine::SendPanicToDestinationUnderLock(size_t destinationIndex, uint64_t sweepTimestamp) noexcept
    {
        auto const connection = ConnectionForDestinationUnderLock(destinationIndex);

        if (connection == nullptr || destinationIndex >= m_destinations.size())
        {
            return;
        }

        auto const& destination = m_destinations[destinationIndex];
        auto const groupIndex = destination.GroupIndex;

        auto const effectiveChannel = [&destination](uint8_t channel) noexcept
            {
                return destination.ChannelOverride >= 0
                    ? static_cast<uint8_t>(destination.ChannelOverride & 0x0F)
                    : channel;
            };

        // Two passes when there is a sweep. The first silences the instrument now; the second
        // lands after anything the service has already been handed.
        std::array<uint64_t, 2> const timestamps{ midi2::MidiClock::TimestampConstantSendImmediately(), sweepTimestamp };

        size_t const passes = sweepTimestamp != 0 ? 2 : 1;

        for (size_t pass = 0; pass < passes; ++pass)
        {
            auto const timestamp = timestamps[pass];

            for (uint8_t note = 0; note < 128; ++note)
            {
                auto mask = m_soundingNotes[note];

                for (uint8_t channel = 0; mask != 0 && channel < 16; ++channel)
                {
                    if ((mask & (1u << channel)) == 0)
                    {
                        continue;
                    }

                    mask &= static_cast<uint16_t>(~(1u << channel));

                    connection.SendSingleMessageWords(
                        timestamp,
                        BuildMidi1Word(
                            groupIndex,
                            static_cast<uint8_t>(StatusNoteOff | effectiveChannel(channel)),
                            note,
                            0));
                }
            }

            auto const channelMask = m_sequence == nullptr ? uint16_t{ 0xFFFF } : m_sequence->UsedChannelMask;

            for (uint8_t channel = 0; channel < 16; ++channel)
            {
                if ((channelMask & (1u << channel)) == 0)
                {
                    continue;
                }

                auto const outgoing = effectiveChannel(channel);
                auto const status = static_cast<uint8_t>(StatusControlChange | outgoing);

                connection.SendSingleMessageWords(
                    timestamp, BuildMidi1Word(groupIndex, status, ControllerSustainPedal, 0));

                connection.SendSingleMessageWords(
                    timestamp, BuildMidi1Word(groupIndex, status, ControllerAllNotesOff, 0));

                connection.SendSingleMessageWords(
                    timestamp, BuildMidi1Word(groupIndex, status, ControllerAllSoundOff, 0));

                connection.SendSingleMessageWords(
                    timestamp,
                    BuildMidi1Word(groupIndex, static_cast<uint8_t>(StatusPitchBend | outgoing), 0, 0x40));
            }
        }
    }

    _Use_decl_annotations_
    uint64_t PlaybackEngine::ChaseTimestampUnderLock(uint64_t nowTimestamp) const noexcept
    {
        // Just after the second pass of a silence still on its way, which would otherwise turn
        // restored notes straight back off and release the restored sustain and pitch bend. The
        // music waits for the same moment; see RebaseClockUnderLock.
        if (m_sweepTimestamp > nowTimestamp)
        {
            return m_sweepTimestamp + 1;
        }

        return midi2::MidiClock::TimestampConstantSendImmediately();
    }

    _Use_decl_annotations_
    void PlaybackEngine::SendChaseStateUnderLock(uint64_t positionMicroseconds, ChaseOptions options, int32_t onlyTrack) noexcept
    {
        auto const wanted = options & ChaseOptions::All;

        if (m_sequence == nullptr || positionMicroseconds == 0 || wanted == ChaseOptions::None)
        {
            return;
        }

        auto const* const sequence = m_sequence.get();
        auto const& events = sequence->Events;

        // Found by time, the same way the playback sweep finds where to start, rather than by a
        // count of prepared events. The two counts only agree when preparing the sequence dropped
        // nothing, and a note off missed here would leave a note sounding.
        auto const firstAtPosition = std::partition_point(
            events.begin(),
            events.end(),
            [sequence, positionMicroseconds](midifile::SequenceEvent const& event) noexcept
            {
                return sequence->MicrosecondsAtTick(event.Tick) < positionMicroseconds;
            });

        // What is exactly at the position plays as soon as the music starts.
        auto const firstAfterPosition = std::partition_point(
            firstAtPosition,
            events.end(),
            [sequence, positionMicroseconds](midifile::SequenceEvent const& event) noexcept
            {
                return sequence->MicrosecondsAtTick(event.Tick) <= positionMicroseconds;
            });

        auto const eventsBefore = static_cast<size_t>(std::distance(events.begin(), firstAtPosition));
        auto const eventsThrough = static_cast<size_t>(std::distance(events.begin(), firstAfterPosition));

        if (eventsBefore == 0)
        {
            return;
        }

        auto const timestamp = ChaseTimestampUnderLock(midi2::MidiClock::Now());

        // Once per destination. Each pass keeps only the tracks that go there, so a rack of
        // instruments is each left in the state its own tracks put it in rather than in whatever
        // state the last track to be walked happened to leave.
        for (size_t destination = 0; destination < m_destinations.size(); ++destination)
        {
            if (onlyTrack >= 0 && DestinationForTrackUnderLock(static_cast<uint16_t>(onlyTrack)) != destination)
            {
                continue;
            }

            SendChaseStateToDestinationUnderLock(eventsBefore, eventsThrough, destination, timestamp, wanted, onlyTrack);
        }
    }

    _Use_decl_annotations_
    void PlaybackEngine::SendChaseStateToDestinationUnderLock(
        size_t eventsBefore,
        size_t eventsThrough,
        size_t destinationIndex,
        uint64_t timestamp,
        ChaseOptions options,
        int32_t onlyTrack) noexcept
    {
        auto const connection = ConnectionForDestinationUnderLock(destinationIndex);

        if (connection == nullptr || m_sequence == nullptr || destinationIndex >= m_destinations.size())
        {
            return;
        }

        auto const& events = m_sequence->Events;

        auto const limit = eventsBefore > events.size() ? events.size() : eventsBefore;
        auto const through = eventsThrough > events.size() ? events.size() : eventsThrough;

        if (limit == 0)
        {
            return;
        }

        auto const chaseSettings = HasOption(options, ChaseOptions::ChannelSettings);
        auto const chaseNotes = HasOption(options, ChaseOptions::HeldNotes);

        auto const& destination = m_destinations[destinationIndex];
        auto const groupIndex = destination.GroupIndex;

        auto const effectiveChannel = [&destination](uint8_t channel) noexcept
            {
                return destination.ChannelOverride >= 0
                    ? static_cast<uint8_t>(destination.ChannelOverride & 0x0F)
                    : channel;
            };

        // Walk everything before the starting point and keep only what a receiver needs to sound
        // correct: the selected sound, the controllers that were set, and the pitch bend.
        std::array<std::array<uint8_t, 128>, 16> controllers{};
        std::array<std::array<bool, 128>, 16> controllerSet{};
        std::array<uint16_t, 16> pitchBend{};
        std::array<bool, 16> pitchBendSet{};
        std::array<int16_t, 16> program{};
        std::array<uint8_t, 16> bankMsb{};
        std::array<uint8_t, 16> bankLsb{};

        // The velocity of each note the receiver was left holding, or zero for one it was not.
        std::array<std::array<uint8_t, 128>, 16> heldVelocity{};

        // Which channels are drum kits at the starting point, so the notes restarted there are
        // transposed the same way the rest of the sequence is. Every track counts, the same as
        // when the sequence was prepared.
        midifile::PercussionTracker percussion{};

        program.fill(-1);

        for (size_t index = 0; index < limit; ++index)
        {
            auto const& event = events[index];
            auto const bytes = m_sequence->BytesOf(event);

            if (chaseNotes)
            {
                percussion.Observe(event, bytes);
            }

            if (event.Channel == midifile::ChannelNone || event.Channel > 15)
            {
                continue;
            }

            if (DestinationForTrackUnderLock(event.TrackIndex) != destinationIndex)
            {
                continue;
            }

            if (bytes.size() < 2)
            {
                continue;
            }

            auto const channel = event.Channel;

            switch (event.Kind)
            {
            case midifile::EventKind::ControlChange:
                if (bytes.size() >= 3)
                {
                    auto const controller = static_cast<uint8_t>(bytes[1] & 0x7F);

                    // Skip the channel mode messages; replaying them would silence the instrument.
                    if (controller < ControllerAllSoundOff)
                    {
                        controllers[channel][controller] = static_cast<uint8_t>(bytes[2] & 0x7F);
                        controllerSet[channel][controller] = true;
                    }
                    else if (EndsEveryNote(controller))
                    {
                        // But they did end whatever the channel was holding at the time.
                        heldVelocity[channel].fill(0);
                    }

                    if (controller == ControllerBankSelectMsb) { bankMsb[channel] = static_cast<uint8_t>(bytes[2] & 0x7F); }
                    else if (controller == ControllerBankSelectLsb) { bankLsb[channel] = static_cast<uint8_t>(bytes[2] & 0x7F); }
                }
                break;

            case midifile::EventKind::ProgramChange:
                program[channel] = static_cast<int16_t>(bytes[1] & 0x7F);
                break;

            case midifile::EventKind::PitchBend:
                if (bytes.size() >= 3)
                {
                    pitchBend[channel] = static_cast<uint16_t>(
                        (static_cast<uint16_t>(bytes[2] & 0x7F) << 7) | static_cast<uint16_t>(bytes[1] & 0x7F));

                    pitchBendSet[channel] = true;
                }
                break;

            case midifile::EventKind::NoteOn:
            case midifile::EventKind::NoteOff:
                if (chaseNotes && bytes.size() >= 3)
                {
                    auto const note = static_cast<uint8_t>(bytes[1] & 0x7F);
                    auto const velocity = static_cast<uint8_t>(bytes[2] & 0x7F);

                    if (event.Kind == midifile::EventKind::NoteOff || velocity == 0)
                    {
                        // Note ends go out from every track, muted or not, so any of them ends it.
                        heldVelocity[channel][note] = 0;
                    }
                    else if (IsTrackAudibleUnderLock(event.TrackIndex))
                    {
                        // A muted track's start never reached the receiver, so it changes nothing.
                        // When only one track's notes are wanted, a later start from another
                        // track is still sounding, and is left to that track.
                        heldVelocity[channel][note] =
                            (onlyTrack < 0 || event.TrackIndex == static_cast<uint16_t>(onlyTrack)) ? velocity : uint8_t{ 0 };
                    }
                }
                break;

            default:
                break;
            }
        }

        // Events exactly at the starting point play as soon as the music starts. They only matter
        // here for the notes: one that ends there is already over, and one that starts again
        // there is about to sound anyway, so restarting either would only add a blip.
        for (size_t index = limit; chaseNotes && index < through; ++index)
        {
            auto const& event = events[index];

            if (event.Channel == midifile::ChannelNone || event.Channel > 15 ||
                DestinationForTrackUnderLock(event.TrackIndex) != destinationIndex)
            {
                continue;
            }

            auto const bytes = m_sequence->BytesOf(event);

            if (bytes.size() < 3)
            {
                continue;
            }

            auto const channel = event.Channel;

            if (event.Kind == midifile::EventKind::ControlChange)
            {
                if (EndsEveryNote(static_cast<uint8_t>(bytes[1] & 0x7F)))
                {
                    heldVelocity[channel].fill(0);
                }
            }
            else if (event.Kind == midifile::EventKind::NoteOff ||
                (event.Kind == midifile::EventKind::NoteOn &&
                    ((bytes[2] & 0x7F) == 0 || IsTrackAudibleUnderLock(event.TrackIndex))))
            {
                heldVelocity[channel][bytes[1] & 0x7F] = 0;
            }
        }

        for (uint8_t channel = 0; chaseSettings && channel < 16; ++channel)
        {
            auto const outgoing = effectiveChannel(channel);
            auto const controlStatus = static_cast<uint8_t>(StatusControlChange | outgoing);

            for (uint16_t controller = 0; controller < 128; ++controller)
            {
                if (!controllerSet[channel][controller])
                {
                    continue;
                }

                connection.SendSingleMessageWords(
                    timestamp,
                    BuildMidi1Word(
                        groupIndex,
                        controlStatus,
                        static_cast<uint8_t>(controller),
                        controllers[channel][controller]));
            }

            if (program[channel] >= 0)
            {
                // Bank first, then program, or the program lands in the previous bank.
                connection.SendSingleMessageWords(
                    timestamp, BuildMidi1Word(groupIndex, controlStatus, ControllerBankSelectMsb, bankMsb[channel]));

                connection.SendSingleMessageWords(
                    timestamp, BuildMidi1Word(groupIndex, controlStatus, ControllerBankSelectLsb, bankLsb[channel]));

                connection.SendSingleMessageWords(
                    timestamp,
                    BuildMidi1Word(
                        groupIndex,
                        static_cast<uint8_t>(0xC0 | outgoing),
                        static_cast<uint8_t>(program[channel]),
                        0));
            }

            if (pitchBendSet[channel])
            {
                connection.SendSingleMessageWords(
                    timestamp,
                    BuildMidi1Word(
                        groupIndex,
                        static_cast<uint8_t>(StatusPitchBend | outgoing),
                        static_cast<uint8_t>(pitchBend[channel] & 0x7F),
                        static_cast<uint8_t>((pitchBend[channel] >> 7) & 0x7F)));
            }
        }

        if (!chaseNotes)
        {
            return;
        }

        // Last, once every channel has its sound, so each note starts on the right one. A route
        // that sends several channels to one would otherwise start the same note twice.
        std::array<uint16_t, 128> started{};

        for (uint8_t channel = 0; channel < 16; ++channel)
        {
            auto const outgoing = effectiveChannel(channel);
            auto const outgoingBit = static_cast<uint16_t>(1u << outgoing);
            auto const channelBit = static_cast<uint16_t>(1u << channel);
            auto const noteOnStatus = static_cast<uint8_t>(StatusNoteOn | outgoing);
            auto const isPercussion = percussion.IsPercussion(channel);

            for (uint8_t note = 0; note < 128; ++note)
            {
                auto const velocity = heldVelocity[channel][note];

                if (velocity == 0)
                {
                    continue;
                }

                // Transposed like every other note, and remembered, so its end reaches it.
                auto const start = m_transposer.StartNote(channel, note, isPercussion);

                if (start.EndFirst >= 0)
                {
                    auto const previous = static_cast<uint8_t>(start.EndFirst);

                    connection.SendSingleMessageWords(
                        timestamp,
                        BuildMidi1Word(groupIndex, static_cast<uint8_t>(StatusNoteOff | outgoing), previous, 0));

                    m_soundingNotes[previous] &= static_cast<uint16_t>(~channelBit);
                }

                if (start.Note < 0)
                {
                    continue;
                }

                auto const pitch = static_cast<uint8_t>(start.Note);

                if ((started[pitch] & outgoingBit) != 0)
                {
                    continue;
                }

                started[pitch] |= outgoingBit;

                connection.SendSingleMessageWords(
                    timestamp, BuildMidi1Word(groupIndex, noteOnStatus, pitch, velocity));

                // Tracked like any other note, so a pause, stop, mute or the end turns it off.
                m_soundingNotes[pitch] |= channelBit;
            }
        }
    }
}
