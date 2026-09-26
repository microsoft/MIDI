// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <cstdint>
#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <condition_variable>
#include <thread>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.Midi2.h>

#include "midi_file_sequence.h"

// This compiles into the SDK and, until the player app is moved over, into the app as well. The
// app defines richer tracing of its own; without it, fall back to the platform's error logging so
// nothing escapes a worker thread either way.
#ifndef MIDI_PLAYER_CATCH_AND_LOG
#define MIDI_PLAYER_CATCH_AND_LOG(messageText)  catch (...) { LOG_IF_FAILED(E_FAIL); }
#endif

#ifndef MIDI_PLAYER_LOG_GENERAL_EXCEPTION
#define MIDI_PLAYER_LOG_GENERAL_EXCEPTION(messageText)  LOG_IF_FAILED(E_FAIL)
#endif

#ifndef MIDI_PLAYER_LOG_INFO_WITH_ENDPOINT
#define MIDI_PLAYER_LOG_INFO_WITH_ENDPOINT(messageText, endpointId)  ((void)0)
#endif

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

    // Where one track's messages go. A route that names nothing is the connection and group the
    // engine was loaded with, which is what every track gets until an application says otherwise.
    struct TrackRoute
    {
        // Null to use the engine's own connection.
        winrt::Windows::Devices::Midi2::MidiEndpointConnection Connection{ nullptr };

        int32_t GroupIndex{ -1 };        // -1 to use the group the sequence was loaded for
        int32_t ChannelOverride{ -1 };   // -1 to leave the channel the file wrote alone

        bool IsDefault() const noexcept
        {
            return Connection == nullptr && GroupIndex < 0 && ChannelOverride < 0;
        }
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
        OpenResult Open(
            _In_ winrt::Windows::Devices::Midi2::MidiSession const& session,
            _In_ std::wstring const& endpointDeviceId) noexcept;

        // Uses a connection the caller already opened. The engine will not close it.
        void AttachConnection(
            _In_ winrt::Windows::Devices::Midi2::MidiEndpointConnection const& connection) noexcept;

        bool OwnsConnection() const noexcept;
        winrt::Windows::Devices::Midi2::MidiEndpointConnection Connection() const noexcept;

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

        // Sends a track somewhere other than the engine's own connection and group. Safe to call
        // at any time, including while playing: the route is read as each message goes out, so
        // nothing has to be prepared again.
        void SetTrackRoute(uint16_t trackIndex, TrackRoute const& route) noexcept;
        void ClearTrackRoute(uint16_t trackIndex) noexcept;
        TrackRoute GetTrackRoute(uint16_t trackIndex) const noexcept;

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

        // One entry per track is one entry per event in m_prepared, and the largest files in the
        // corpus reach the two million event cap, so nothing about routing is allowed to grow
        // this. The destination is looked up from TrackIndex instead, which also means a routing
        // change does not have to prepare the sequence again.
        static_assert(sizeof(PreparedEvent) == 32, "PreparedEvent grew; check the cost at two million events first.");

        // A distinct place messages go. Entry zero is always the engine's own connection and
        // group, so a sequence with no routing set resolves every track to it and pays nothing.
        struct Destination
        {
            winrt::Windows::Devices::Midi2::MidiEndpointConnection Connection{ nullptr };
            uint8_t GroupIndex{ 0 };
            int32_t ChannelOverride{ -1 };

            // True only when the group or the channel differs from what the words already carry.
            // A different connection on its own needs no rewriting.
            bool RewritesMessages{ false };
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
        void SendPanicToDestinationUnderLock(size_t destinationIndex, bool includeScheduledSweep) noexcept;
        void SendChaseStateUnderLock(size_t eventIndex) noexcept;
        void SendChaseStateToDestinationUnderLock(size_t eventIndex, size_t destinationIndex) noexcept;
        void SendWordsUnderLock(uint64_t timestamp, uint32_t wordOffset, uint32_t wordCount, size_t destinationIndex) noexcept;
        void RebuildDestinationsUnderLock() noexcept;
        size_t DestinationForTrackUnderLock(uint16_t trackIndex) const noexcept;
        winrt::Windows::Devices::Midi2::MidiEndpointConnection ConnectionForDestinationUnderLock(size_t destinationIndex) const noexcept;
        uint64_t TimestampForMicrosecondsUnderLock(uint64_t microseconds) const noexcept;
        uint64_t CurrentMicrosecondsUnderLock(uint64_t nowTimestamp) const noexcept;

        mutable std::recursive_mutex m_lock{};

        winrt::Windows::Devices::Midi2::MidiSession m_session{ nullptr };
        bool m_ownsConnection{ false };
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

        // Routing. m_trackRoutes is what the application asked for; m_destinations and
        // m_trackDestination are derived from it and rebuilt whenever either changes.
        std::vector<TrackRoute> m_trackRoutes{};
        std::vector<Destination> m_destinations{};
        std::vector<uint16_t> m_trackDestination{};
        bool m_hasCustomRoutes{ false };

        // Only used when a destination has to rewrite a message. Sized at load so a send never
        // allocates.
        std::vector<uint32_t> m_sendScratch{};

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
