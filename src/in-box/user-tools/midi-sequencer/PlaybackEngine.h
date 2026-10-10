// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The playback engine. It hands each message to the service with the time it's due, a short time
// ahead, and the service sends it then (section 8 of the design). A message handed over can't be
// taken back, so a mute or an edit is heard one look-ahead later.
//
// No XAML and no MIDI connections of its own: it sends through IEngineOutput and tells time
// through EngineClock, so the tests drive it with a fake clock and record what it sends.
//
// Threading: the UI thread calls Play, Stop, SetSequence and the setters. The engine's own thread
// calls Sweep. One lock guards the engine's state. While it's held, the engine calls into the app
// only through IEngineOutput::Send and the destination lookup, and neither may call back into the
// engine.

#include <sal.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "SequenceModel.h"

namespace midisequencer
{
    class IEngineOutput
    {
    public:
        virtual ~IEngineOutput() = default;

        // The group and channel are already in the words. A timestamp of 0 means now.
        virtual void Send(
            _In_ EndpointRef const& endpoint,
            _In_ uint64_t timestamp,
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint8_t wordCount) noexcept = 0;
    };

    struct EngineClock
    {
        std::function<uint64_t()> Now{};
        uint64_t TicksPerSecond{ 10000000 };
    };

    // What the app knows about a destination: what it speaks, and how early the service sends to it
    // (from MIDI Settings, or worked out by the transport).
    struct DestinationInfo
    {
        bool SpeaksMidi2{ true };
        uint32_t OffsetMicroseconds{ 0 };
    };

    struct MetronomeSettings
    {
        bool Enabled{ false };
        EndpointRef Endpoint{};
        uint8_t Group{ 0 };
        uint8_t Channel{ 9 };       // counted from 0, so channel 10
        uint8_t Note{ 37 };         // General MIDI side stick
        uint16_t FirstBeatVelocity{ 0xFFFF };
        uint16_t OtherBeatVelocity{ 0xB332 };
    };

    struct ClockOutput
    {
        EndpointRef Endpoint{};
        uint8_t Group{ 0 };

        // Positive sends later, negative sends earlier, to line up a device that's slow or quick.
        int32_t OffsetMicroseconds{ 0 };
    };

    struct EngineSettings
    {
        uint32_t LookAheadMicroseconds{ 40000 };
        uint32_t SweepMicroseconds{ 5000 };
        MetronomeSettings Metronome{};
        std::vector<ClockOutput> ClockOutputs{};
    };

    struct EngineCounters
    {
        uint64_t MessagesSent{ 0 };

        // Messages a MIDI 1.0 destination can't be sent.
        uint64_t MessagesDropped{ 0 };

        uint64_t Sweeps{ 0 };
    };

    class PlaybackEngine
    {
    public:
        PlaybackEngine(_In_ IEngineOutput& output, _In_ EngineClock clock);
        ~PlaybackEngine();

        PlaybackEngine(PlaybackEngine const&) = delete;
        PlaybackEngine& operator=(PlaybackEngine const&) = delete;

        // The engine plays a snapshot. The app publishes a new one after each edit.
        void SetSequence(_In_ std::shared_ptr<Sequence const> sequence);
        void SetSettings(_In_ EngineSettings const& settings);
        void SetDestinationLookup(_In_ std::function<DestinationInfo(EndpointRef const&)> lookup);

        // Sends each track's start-up messages and, part way in, the program, controllers and
        // pitch bend each channel would have by then, then starts.
        void Play(_In_ int64_t fromTick);

        // Ends every note it started and sends sustain off and all notes off on each channel it
        // used, now and again one look-ahead later, for the notes already handed over.
        void Stop();

        bool IsPlaying() const;
        int64_t PositionTick() const;

        // Hands over everything due before now plus the look-ahead.
        void Sweep();

        // Starts and stops the engine's own thread, which calls Sweep every few milliseconds.
        void StartThread();
        void StopThread() noexcept;

        EngineCounters Counters() const;

    private:
        struct SoundingNote
        {
            EndpointRef Endpoint{};
            uint32_t OffWords[2]{};
            uint64_t OffTimestamp{ 0 };
            bool Midi2{ true };
        };

        struct ChannelKey
        {
            EndpointRef Endpoint{};
            uint8_t Group{ 0 };
            uint8_t Channel{ 0 };
            bool Midi2{ true };
        };

        uint64_t TimestampAtTick(_In_ int64_t tick) const noexcept;
        int64_t TickAtTimestamp(_In_ uint64_t timestamp) const noexcept;
        DestinationInfo LookupDestination(_In_ EndpointRef const& endpoint) const;
        bool IsAudible(_In_ Track const& track, _In_ bool anySolo, _In_ bool insideSoloedFolder, _In_ bool insideMutedFolder) const noexcept;

        void SendTo(_In_ EndpointRef const& endpoint, _In_ bool midi2, _In_ uint64_t timestamp, _In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount);
        void RememberChannel(_In_ EndpointRef const& endpoint, _In_ bool midi2, _In_ uint32_t word0);
        void SendStartup(_In_ Track const& track, _In_ uint64_t timestamp);
        void SendChase(_In_ Track const& track, _In_ int64_t fromTick, _In_ uint64_t timestamp);
        void SweepTracks(_In_ std::vector<Track> const& tracks, _In_ uint64_t now, _In_ bool anySolo, _In_ bool insideSoloedFolder, _In_ bool insideMutedFolder);
        void SweepMetronome(_In_ uint64_t now);
        void SweepClockOutputs(_In_ uint64_t now);
        void SendSilence(_In_ uint64_t timestamp);
        void ThreadMain() noexcept;

        IEngineOutput& m_output;
        EngineClock m_clock{};

        mutable std::mutex m_lock{};

        std::shared_ptr<Sequence const> m_sequence{};
        TempoMap m_tempo{};
        EngineSettings m_settings{};
        std::function<DestinationInfo(EndpointRef const&)> m_lookup{};

        bool m_playing{ false };
        int64_t m_startTick{ 0 };
        uint64_t m_startTimestamp{ 0 };
        double m_startSeconds{ 0 };

        // How far each track, the metronome and the clock outputs have been handed over.
        std::unordered_map<std::wstring, int64_t> m_renderedUntil{};
        int64_t m_metronomeUntil{ 0 };
        int64_t m_clockUntil{ 0 };

        std::vector<SoundingNote> m_sounding{};
        std::vector<ChannelKey> m_channelsUsed{};

        EngineCounters m_counters{};

        std::thread m_thread{};
        std::atomic<bool> m_threadRunning{ false };
    };
}
