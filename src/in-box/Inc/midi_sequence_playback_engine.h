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
#include <span>
#include <string>
#include <vector>
#include <condition_variable>
#include <thread>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.Midi2.h>

#include "midi_file_sequence.h"
#include "midi_sequence_playback_clock.h"
#include "midi_sequence_transposer.h"

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

    // What is sent when playback starts part way into a sequence, so the instrument is left the
    // way it would have been had the sequence played from the start. The values match
    // Windows.Devices.Midi2.Utilities.Sequencing.MidiSequenceChaseOptions.
    enum class ChaseOptions : uint32_t
    {
        None = 0x00000000,

        // Bank, program, controllers and pitch bend: the last value the sequence gave each one.
        ChannelSettings = 0x00000001,

        // Notes that started before the starting point and end after it, started again with
        // their own velocity. Off unless asked for, because starting a note part way through
        // gives it a fresh attack. That is right for an organ stop held on a control channel or
        // a pad, and wrong for a piano or a drum.
        HeldNotes = 0x00000002,

        All = ChannelSettings | HeldNotes
    };

    DEFINE_ENUM_FLAG_OPERATORS(ChaseOptions);

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

        // How fast the sequence plays, as a multiple of the speed it was written at: 1.0 is as
        // written and 0.5 is half speed. Every tempo in the file scales the same way and the
        // notes themselves are untouched, so the pitch does not change. A value outside the
        // limits is clamped, and one that is not a number is ignored. It stays set across Load.
        //
        // Safe at any time, including while playing. The new rate starts at the end of what has
        // already been handed to the service, so it is heard within one look ahead window and
        // nothing that is sounding is cut off. Position, ticks and tempo are still reported in
        // the sequence's own time.
        void SetPlaybackRate(_In_ double rate) noexcept;
        double PlaybackRate() const noexcept;

        static constexpr double MinimumPlaybackRate = PlaybackClock::MinimumRate;
        static constexpr double MaximumPlaybackRate = PlaybackClock::MaximumRate;

        // Moves every note up or down by this many semitones, except drums: notes on channel 10,
        // and on any channel the sequence has switched to a drum kit, keep their note numbers,
        // because there the number picks the drum. A value outside the limits is clamped. It
        // stays set across Load.
        //
        // Safe at any time, including while playing. The change is heard within one look ahead
        // window. A note that is already sounding keeps its pitch, and its end still reaches it.
        // A note moved outside 0 to 127 is not played.
        void SetTransposeSemitones(_In_ int32_t semitones) noexcept;
        int32_t TransposeSemitones() const noexcept;

        static constexpr int32_t MinimumTransposeSemitones = NoteTransposer::MinimumSemitones;
        static constexpr int32_t MaximumTransposeSemitones = NoteTransposer::MaximumSemitones;

        // What Play and a seek send before the music, when they start part way in. ChannelSettings
        // until an application says otherwise, which is how the engine has always behaved. Bits
        // the engine does not know are ignored. It stays set across Load.
        //
        // Safe at any time, including while playing. It applies from the next play, seek, mute or
        // solo. With HeldNotes on, muting or soloing while playing also restarts what the tracks
        // that can still be heard should be holding, because the silence a mute or solo sends
        // turns off every held note, not only those of the track that changed. Unmuting restarts
        // that track's own held notes.
        void SetChaseOptions(_In_ ChaseOptions options) noexcept;
        ChaseOptions GetChaseOptions() const noexcept;

        // Messages sent once to the engine's own connection, ahead of the music, the next time
        // it starts: from Play, or from a seek while playing. Meant for a reset such as General
        // MIDI System On, which turns off every note, so it is never sent into music already
        // going out. It lands after any silence still on its way, and the music and whatever a
        // chase restores wait settleMilliseconds more, so an instrument that takes a moment to
        // apply it hears nothing too early. Replaces messages still waiting; empty words cancel.
        // Complete Universal MIDI Packets only. It stays set across Load.
        void SendBeforeNextStart(
            _In_ std::vector<uint32_t> words,
            _In_ uint32_t settleMilliseconds) noexcept;

        bool HasMessagesBeforeNextStart() const noexcept;

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

            // Only set for MIDI 1.0 note messages, so a mute can hold back starts and a stop can
            // turn off exactly what is sounding.
            NoteAction Action{ NoteAction::None };

            // Set when any packet in the event carries a note number, so it goes through the
            // transposer.
            bool NamesNotes{ false };

            uint16_t TrackIndex{ 0 };

            // The channels playing a drum kit by this point in the sequence, one bit each. Their
            // notes are not transposed.
            uint16_t PercussionChannels{ 0 };
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
        void SendPanicToDestinationUnderLock(_In_ size_t destinationIndex, _In_ uint64_t sweepTimestamp) noexcept;

        // Restores what the sequence had set up by a point in it. onlyTrack restricts the notes
        // restarted to one track, or is -1 for every track that can be heard.
        void SendChaseStateUnderLock(
            _In_ uint64_t positionMicroseconds,
            _In_ ChaseOptions options,
            _In_ int32_t onlyTrack) noexcept;

        void SendChaseStateToDestinationUnderLock(
            _In_ size_t eventsBefore,
            _In_ size_t eventsThrough,
            _In_ size_t destinationIndex,
            _In_ uint64_t timestamp,
            _In_ ChaseOptions options,
            _In_ int32_t onlyTrack) noexcept;

        // When a chase can go out: now, or just after a silence that is still on its way.
        uint64_t ChaseTimestampUnderLock(_In_ uint64_t nowTimestamp) const noexcept;

        // Sends what SendBeforeNextStart left waiting, and makes the start wait for it.
        void SendStartMessagesUnderLock() noexcept;

        std::vector<uint32_t> m_startMessages{};
        uint32_t m_startSettleMilliseconds{ 0 };

        // Transposes note events, then rewrites the group and channel when the track's route
        // needs it, and sends the result.
        void SendEventUnderLock(_In_ uint64_t timestamp, _In_ PreparedEvent const& entry) noexcept;

        // Follows the note ons and offs among MIDI 1.0 packets on their way out.
        void TrackSoundingNotesUnderLock(_In_ std::span<uint32_t const> words) noexcept;

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

        // Where the sequence's own time falls on the MIDI clock, and at what rate.
        PlaybackClock m_clock{};
        uint64_t m_pausedMicroseconds{ 0 };

        // When the second pass of the last silence lands: from a stop, pause or seek, or from a
        // new sequence loaded while playing. It turns off every note and releases sustain and
        // pitch bend, so anything a new start sent before then would be undone by it. A start
        // waits for it instead. In the past once it has landed.
        uint64_t m_sweepTimestamp{ 0 };

        ChaseOptions m_chaseOptions{ ChaseOptions::ChannelSettings };

        size_t m_nextEventIndex{ 0 };

        std::vector<bool> m_mutedTracks{};
        int32_t m_soloTrack{ -1 };

        // Routing. m_trackRoutes is what the application asked for; m_destinations and
        // m_trackDestination are derived from it and rebuilt whenever either changes.
        std::vector<TrackRoute> m_trackRoutes{};
        std::vector<Destination> m_destinations{};
        std::vector<uint16_t> m_trackDestination{};
        bool m_hasCustomRoutes{ false };

        // Only used when a message has to be rewritten. Sized at load so a send never allocates.
        std::vector<uint32_t> m_sendScratch{};

        // What the player believes is sounding, so a stop can turn exactly those off rather than
        // relying on a device honoring all notes off. By the note that went out, after any
        // transposition.
        std::array<uint16_t, 128> m_soundingNotes{};

        NoteTransposer m_transposer{};

        std::function<void()> m_completionHandler{};

        std::thread m_worker{};
        wil::slim_event_manual_reset m_wakeUp{};
        std::atomic<bool> m_workerRunning{ false };
        std::atomic<bool> m_stopRequested{ false };
    };
}
