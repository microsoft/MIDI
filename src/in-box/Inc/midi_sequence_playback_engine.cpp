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
        constexpr uint8_t StatusControlChange = 0xB0;
        constexpr uint8_t StatusPitchBend = 0xE0;

        constexpr uint8_t ControllerBankSelectMsb = 0x00;
        constexpr uint8_t ControllerBankSelectLsb = 0x20;
        constexpr uint8_t ControllerSustainPedal = 0x40;
        constexpr uint8_t ControllerAllSoundOff = 0x78;
        constexpr uint8_t ControllerAllNotesOff = 0x7B;

        // A little runway so the first messages of a sequence are scheduled rather than late.
        constexpr uint32_t StartLeadMilliseconds = 60;

        // How long after a stop the safety sweep lands. It has to outlast anything already handed
        // to the service, which is at most one look ahead window.
        constexpr uint32_t PanicSweepMarginMilliseconds = 60;

        constexpr uint64_t MicrosecondsPerSecond = 1000000;

        uint32_t BuildMidi1Word(uint8_t group, uint8_t status, uint8_t data1, uint8_t data2) noexcept
        {
            return (Midi1ChannelVoiceMessageType << 28)
                | (static_cast<uint32_t>(group & 0x0F) << 24)
                | (static_cast<uint32_t>(status) << 16)
                | (static_cast<uint32_t>(data1 & 0x7F) << 8)
                | static_cast<uint32_t>(data2 & 0x7F);
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

            prepared.reserve(sequence->Events.size());
            words.reserve(sequence->Events.size() * 2);

            midi2::MidiGroup const group{ groupIndex };

            auto scratch = winrt::single_threaded_vector<uint8_t>();

            for (auto const& event : sequence->Events)
            {
                auto const bytes = sequence->BytesOf(event);

                if (bytes.empty())
                {
                    continue;
                }

                scratch.ReplaceAll(winrt::array_view<uint8_t const>{ bytes.data(), bytes.data() + bytes.size() });

                // Stored messages always carry an explicit status byte, so running status is off.
                auto const converted = midi2msg::MidiMessageConverter::ConvertMidi1CompleteMessageBytesToUmpWords(
                    group, scratch, false);

                if (converted == nullptr || converted.Size() == 0)
                {
                    continue;
                }

                PreparedEvent entry{};

                entry.Microseconds = sequence->MicrosecondsAtTick(event.Tick);
                entry.Tick = event.Tick;
                entry.WordOffset = static_cast<uint32_t>(words.size());
                entry.WordCount = converted.Size();
                entry.TrackIndex = event.TrackIndex;

                if (event.Kind == midifile::EventKind::NoteOn || event.Kind == midifile::EventKind::NoteOff)
                {
                    entry.NoteNumber = bytes.size() > 1 ? static_cast<uint8_t>(bytes[1] & 0x7F) : uint8_t{ 0 };
                    entry.Channel = event.Channel;

                    auto const velocity = bytes.size() > 2 ? static_cast<uint8_t>(bytes[2] & 0x7F) : uint8_t{ 0 };

                    entry.Action = (event.Kind == midifile::EventKind::NoteOn && velocity > 0)
                        ? NoteAction::Start
                        : NoteAction::End;
                }

                words.resize(words.size() + converted.Size());

                converted.GetMany(
                    0,
                    winrt::array_view<uint32_t>{ words.data() + entry.WordOffset, words.data() + words.size() });

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

                m_state = PlaybackState::Stopped;
                m_pausedMicroseconds = 0;
                m_nextEventIndex = 0;
                m_soundingNotes.fill(0);
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
            // or a note starting part way in plays with whatever sound was last selected.
            SendChaseStateUnderLock(m_nextEventIndex);

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
                SendChaseStateUnderLock(m_nextEventIndex);
            }
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to seek.")

        m_wakeUp.SetEvent();
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
                SendWordsUnderLock(
                    timestamp <= nowTimestamp ? midi2::MidiClock::TimestampConstantSendImmediately() : timestamp,
                    entry.WordOffset,
                    entry.WordCount);

                if (entry.Action == NoteAction::Start)
                {
                    m_soundingNotes[entry.NoteNumber] |= static_cast<uint16_t>(1u << (entry.Channel & 0x0F));
                }
            }

            if (entry.Action == NoteAction::End)
            {
                m_soundingNotes[entry.NoteNumber] &= static_cast<uint16_t>(~(1u << (entry.Channel & 0x0F)));
            }

            ++m_nextEventIndex;
        }
    }

    void PlaybackEngine::RebaseClockUnderLock() noexcept
    {
        m_originMicroseconds = m_pausedMicroseconds;
        m_originTimestamp = midi2::MidiClock::Now() + TicksFromMilliseconds(StartLeadMilliseconds);

        auto const found = std::lower_bound(
            m_prepared.begin(),
            m_prepared.end(),
            m_pausedMicroseconds,
            [](PreparedEvent const& entry, uint64_t value) noexcept { return entry.Microseconds < value; });

        m_nextEventIndex = static_cast<size_t>(std::distance(m_prepared.begin(), found));
    }

    uint64_t PlaybackEngine::TimestampForMicrosecondsUnderLock(uint64_t microseconds) const noexcept
    {
        if (microseconds <= m_originMicroseconds)
        {
            return m_originTimestamp;
        }

        auto const elapsed = (microseconds - m_originMicroseconds)
            * midi2::MidiClock::TimestampFrequency() / MicrosecondsPerSecond;

        return m_originTimestamp + elapsed;
    }

    uint64_t PlaybackEngine::CurrentMicrosecondsUnderLock(uint64_t nowTimestamp) const noexcept
    {
        if (nowTimestamp <= m_originTimestamp)
        {
            return m_originMicroseconds;
        }

        auto const frequency = midi2::MidiClock::TimestampFrequency();

        if (frequency == 0)
        {
            return m_originMicroseconds;
        }

        return m_originMicroseconds + ((nowTimestamp - m_originTimestamp) * MicrosecondsPerSecond / frequency);
    }

    void PlaybackEngine::SendWordsUnderLock(uint64_t timestamp, uint32_t wordOffset, uint32_t wordCount) noexcept
    {
        if (m_connection == nullptr || wordCount == 0 ||
            static_cast<size_t>(wordOffset) + wordCount > m_words.size())
        {
            return;
        }

        m_connection.SendMultipleMessagesWordArray(
            timestamp,
            wordOffset,
            wordCount,
            winrt::array_view<uint32_t const>{ m_words.data(), m_words.data() + m_words.size() });
    }

    void PlaybackEngine::SendPanicUnderLock(bool includeScheduledSweep) noexcept
    {
        if (m_connection == nullptr)
        {
            return;
        }

        // Two passes. The first silences the instrument now; the second lands after anything the
        // service has already been handed, because there is no way to recall a scheduled message.
        std::array<uint64_t, 2> timestamps{ midi2::MidiClock::TimestampConstantSendImmediately(), 0 };

        size_t passes = 1;

        if (includeScheduledSweep)
        {
            timestamps[1] = midi2::MidiClock::Now()
                + TicksFromMilliseconds(LookAheadMilliseconds + PanicSweepMarginMilliseconds);

            passes = 2;
        }

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

                    m_connection.SendSingleMessageWords(
                        timestamp,
                        BuildMidi1Word(m_groupIndex, static_cast<uint8_t>(StatusNoteOff | channel), note, 0));
                }
            }

            auto const channelMask = m_sequence == nullptr ? uint16_t{ 0xFFFF } : m_sequence->UsedChannelMask;

            for (uint8_t channel = 0; channel < 16; ++channel)
            {
                if ((channelMask & (1u << channel)) == 0)
                {
                    continue;
                }

                auto const status = static_cast<uint8_t>(StatusControlChange | channel);

                m_connection.SendSingleMessageWords(
                    timestamp, BuildMidi1Word(m_groupIndex, status, ControllerSustainPedal, 0));

                m_connection.SendSingleMessageWords(
                    timestamp, BuildMidi1Word(m_groupIndex, status, ControllerAllNotesOff, 0));

                m_connection.SendSingleMessageWords(
                    timestamp, BuildMidi1Word(m_groupIndex, status, ControllerAllSoundOff, 0));

                m_connection.SendSingleMessageWords(
                    timestamp,
                    BuildMidi1Word(m_groupIndex, static_cast<uint8_t>(StatusPitchBend | channel), 0, 0x40));
            }
        }

        m_soundingNotes.fill(0);
    }

    void PlaybackEngine::SendChaseStateUnderLock(size_t eventIndex) noexcept
    {
        if (m_connection == nullptr || m_sequence == nullptr)
        {
            return;
        }

        if (eventIndex == 0)
        {
            return;
        }

        // Walk everything before the starting point and keep only what a receiver needs to sound
        // correct: the selected sound, the controllers that were set, and the pitch bend.
        std::array<std::array<uint8_t, 128>, 16> controllers{};
        std::array<std::array<bool, 128>, 16> controllerSet{};
        std::array<uint16_t, 16> pitchBend{};
        std::array<bool, 16> pitchBendSet{};
        std::array<int16_t, 16> program{};
        std::array<uint8_t, 16> bankMsb{};
        std::array<uint8_t, 16> bankLsb{};

        program.fill(-1);

        auto const limit = eventIndex > m_sequence->Events.size() ? m_sequence->Events.size() : eventIndex;

        for (size_t index = 0; index < limit; ++index)
        {
            auto const& event = m_sequence->Events[index];

            if (event.Channel == midifile::ChannelNone || event.Channel > 15)
            {
                continue;
            }

            auto const bytes = m_sequence->BytesOf(event);

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

            default:
                break;
            }
        }

        auto const immediately = midi2::MidiClock::TimestampConstantSendImmediately();

        for (uint8_t channel = 0; channel < 16; ++channel)
        {
            auto const controlStatus = static_cast<uint8_t>(StatusControlChange | channel);

            for (uint16_t controller = 0; controller < 128; ++controller)
            {
                if (!controllerSet[channel][controller])
                {
                    continue;
                }

                m_connection.SendSingleMessageWords(
                    immediately,
                    BuildMidi1Word(
                        m_groupIndex,
                        controlStatus,
                        static_cast<uint8_t>(controller),
                        controllers[channel][controller]));
            }

            if (program[channel] >= 0)
            {
                // Bank first, then program, or the program lands in the previous bank.
                m_connection.SendSingleMessageWords(
                    immediately, BuildMidi1Word(m_groupIndex, controlStatus, ControllerBankSelectMsb, bankMsb[channel]));

                m_connection.SendSingleMessageWords(
                    immediately, BuildMidi1Word(m_groupIndex, controlStatus, ControllerBankSelectLsb, bankLsb[channel]));

                m_connection.SendSingleMessageWords(
                    immediately,
                    BuildMidi1Word(
                        m_groupIndex,
                        static_cast<uint8_t>(0xC0 | channel),
                        static_cast<uint8_t>(program[channel]),
                        0));
            }

            if (pitchBendSet[channel])
            {
                m_connection.SendSingleMessageWords(
                    immediately,
                    BuildMidi1Word(
                        m_groupIndex,
                        static_cast<uint8_t>(StatusPitchBend | channel),
                        static_cast<uint8_t>(pitchBend[channel] & 0x7F),
                        static_cast<uint8_t>((pitchBend[channel] >> 7) & 0x7F)));
            }
        }
    }
}
