// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiSequence.h"

namespace midiplayer
{
    enum class OpenResult : int32_t
    {
        Success = 0,
        ServiceUnavailable = 1,
        SessionFailed = 2,
        EndpointNotFound = 3,
        ConnectionFailed = 4,
        NoEndpointChosen = 5
    };

    enum class PlaybackState : int32_t
    {
        Empty = 0,
        Stopped = 1,
        Playing = 2,
        Paused = 3
    };

    struct PlaybackPosition
    {
        uint64_t Microseconds{ 0 };
        uint64_t DurationMicroseconds{ 0 };
        uint32_t Tick{ 0 };
        uint32_t Bar{ 1 };
        uint32_t Beat{ 1 };
        double BeatsPerMinute{ 120.0 };
        PlaybackState State{ PlaybackState::Empty };
    };

    // Plays a sequence to one endpoint.
    //
    // The service does the fine timing: every message is sent with the timestamp it is due at,
    // and the service's scheduler releases it. The worker here only has to keep a short window of
    // messages ahead of the clock, which is why it wakes a few times a second rather than trying
    // to be accurate itself.
    //
    // Open, Close and Load block on the service over RPC and must never be called from the XAML
    // thread. Everything else is safe from the UI thread.
    class PlaybackEngine
    {
    public:
        PlaybackEngine() noexcept = default;
        ~PlaybackEngine() noexcept;

        PlaybackEngine(PlaybackEngine const&) = delete;
        PlaybackEngine& operator=(PlaybackEngine const&) = delete;

        // blocking
        OpenResult Open(_In_ std::wstring const& endpointDeviceId) noexcept;
        void Close() noexcept;

        bool IsOpen() const noexcept;
        std::wstring EndpointDeviceId() const noexcept;

        // Converts the sequence to Universal MIDI Packets for the chosen group and makes it
        // current. Blocking, because the conversion walks the whole file.
        bool Load(_In_ std::shared_ptr<midifile::MidiSequence const> const& sequence, uint8_t groupIndex) noexcept;

        void Unload() noexcept;

        void Play() noexcept;
        void Pause() noexcept;
        void Stop() noexcept;
        void SeekToMicroseconds(uint64_t microseconds) noexcept;

        PlaybackState State() const noexcept;
        PlaybackPosition Position() const noexcept;

        // Silences or isolates tracks while playing. Only note STARTS are suppressed: note ends,
        // program changes and controllers still go out, so nothing hangs and unmuting a track
        // does not leave it on the wrong sound.
        void SetTrackMuted(uint16_t trackIndex, bool muted) noexcept;
        void SetSoloTrack(int32_t trackIndex) noexcept;    // -1 for no solo

        bool IsTrackMuted(uint16_t trackIndex) const noexcept;
        int32_t SoloTrack() const noexcept;

        // True when this track's notes would actually reach the instrument right now.
        bool IsTrackAudible(uint16_t trackIndex) const noexcept;

        // Raised on the worker thread when the sequence runs out. The handler must marshal to the
        // UI thread itself and must not call back into Play or Load synchronously.
        void SetCompletionHandler(_In_ std::function<void()> handler) noexcept;

        // How far ahead of the clock messages are handed to the service. Also how long a stop can
        // take to silence messages already scheduled, which is why it is not larger.
        static constexpr uint32_t LookAheadMilliseconds = 250;
        static constexpr uint32_t SweepIntervalMilliseconds = 40;

    private:
        enum class NoteAction : uint8_t
        {
            None = 0,
            Start = 1,
            End = 2
        };

        struct PreparedEvent
        {
            uint64_t Microseconds{ 0 };
            uint32_t WordOffset{ 0 };
            uint32_t WordCount{ 0 };
            uint32_t Tick{ 0 };

            // Only set for note messages, so a stop can turn off exactly what is sounding.
            NoteAction Action{ NoteAction::None };
            uint8_t Channel{ 0 };
            uint8_t NoteNumber{ 0 };
            uint16_t TrackIndex{ 0 };
        };

        // caller holds m_lock
        bool IsTrackAudibleUnderLock(uint16_t trackIndex) const noexcept;

        void StartWorker() noexcept;
        void StopWorker() noexcept;
        void WorkerThread() noexcept;

        // caller holds m_lock
        void ScheduleDueEventsUnderLock(uint64_t nowTimestamp) noexcept;
        void RebaseClockUnderLock() noexcept;
        void SendPanicUnderLock(bool includeScheduledSweep) noexcept;
        void SendChaseStateUnderLock(size_t eventIndex) noexcept;
        void SendWordsUnderLock(uint64_t timestamp, uint32_t wordOffset, uint32_t wordCount) noexcept;
        uint64_t TimestampForMicrosecondsUnderLock(uint64_t microseconds) const noexcept;
        uint64_t CurrentMicrosecondsUnderLock(uint64_t nowTimestamp) const noexcept;

        mutable std::recursive_mutex m_lock{};

        winrt::Windows::Devices::Midi2::MidiSession m_session{ nullptr };
        winrt::Windows::Devices::Midi2::MidiEndpointConnection m_connection{ nullptr };
        std::wstring m_endpointDeviceId{};

        std::shared_ptr<midifile::MidiSequence const> m_sequence{};
        std::vector<PreparedEvent> m_prepared{};
        std::vector<uint32_t> m_words{};
        uint8_t m_groupIndex{ 0 };

        PlaybackState m_state{ PlaybackState::Empty };

        // The instant, on the MIDI clock, that m_originMicroseconds in the sequence corresponds to.
        uint64_t m_originTimestamp{ 0 };
        uint64_t m_originMicroseconds{ 0 };
        uint64_t m_pausedMicroseconds{ 0 };

        size_t m_nextEventIndex{ 0 };

        std::vector<bool> m_mutedTracks{};
        int32_t m_soloTrack{ -1 };

        // What the player believes is sounding, so a stop can turn exactly those off rather than
        // relying on a device honoring all notes off.
        std::array<uint16_t, 128> m_soundingNotes{};

        std::function<void()> m_completionHandler{};

        std::thread m_worker{};
        wil::slim_event_manual_reset m_wakeUp{};
        std::atomic<bool> m_workerRunning{ false };
        std::atomic<bool> m_stopRequested{ false };
    };
}
