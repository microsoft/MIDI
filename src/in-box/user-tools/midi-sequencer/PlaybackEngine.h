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
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "SequenceModel.h"
#include "SequenceArpeggiator.h"
#include "SequenceRender.h"

#include "ClockFollower.h"
#include "MidiTimeCode.h"

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

    // What the app knows about a destination: what it speaks, and how early to send to it (from
    // MIDI Settings, or worked out by the transport). The engine sends that much early itself,
    // because the in-box service doesn't.
    struct DestinationInfo
    {
        bool SpeaksMidi2{ true };
        uint32_t OffsetMicroseconds{ 0 };

        // How often a ramp may be sent to the device, from MIDI Settings. 0 when it has none.
        uint32_t AutomationIntervalMilliseconds{ 0 };
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

        // What it gets. A device that only wants some of it can go without the rest.
        bool SendClock{ true };
        bool SendStartStop{ true };
        bool SendSongPosition{ true };

        // A MIDI 2.0 Set Tempo message at each change, and on each beat of a ramp, so the device
        // knows the tempo before the pulses show it. Only sent to a destination that speaks MIDI 2.0.
        bool SendTempo{ false };
    };

    // MIDI Time Code, for video and lighting gear. It counts the time on the wall from the top of
    // the sequence, starting at Start, so tempo changes don't change it.
    struct TimeCodeOutput
    {
        EndpointRef Endpoint{};
        uint8_t Group{ 0 };
        midiapp::MidiTimeCodeFrameRate Rate{ midiapp::MidiTimeCodeFrameRate::Frames30 };
        midiapp::MidiTimeCodePosition Start{};

        // Positive sends later, negative sends earlier.
        int32_t OffsetMicroseconds{ 0 };
    };

    struct EngineSettings
    {
        uint32_t LookAheadMicroseconds{ 40000 };
        uint32_t SweepMicroseconds{ 5000 };
        MetronomeSettings Metronome{};
        std::vector<ClockOutput> ClockOutputs{};
        std::vector<TimeCodeOutput> TimeCodeOutputs{};

        // The timeline loop. Playback that reaches LoopEnd carries on from LoopStart.
        bool LoopEnabled{ false };
        int64_t LoopStart{ 0 };
        int64_t LoopEnd{ 0 };
    };

    struct EngineCounters
    {
        uint64_t MessagesSent{ 0 };

        // Messages a MIDI 1.0 destination can't be sent.
        uint64_t MessagesDropped{ 0 };

        uint64_t Sweeps{ 0 };

        // Times the timeline loop went back to its start.
        uint64_t LoopPasses{ 0 };
    };

    // What a track plays: its timeline, a clip launched from the launcher, or nothing.
    enum class TrackPlayMode : uint8_t
    {
        Timeline = 0,
        Clip = 1,
        Stopped = 2,
    };

    // Launch quantization: 0 is immediately, a positive value is a grid in ticks, and a negative
    // value is that many bars (so -1 is the next bar line, whatever the meter).
    using LaunchQuantize = int64_t;

    // One track's launcher state, for drawing. Ticks are on the timeline.
    struct TrackLaunchView
    {
        std::wstring TrackId{};
        TrackPlayMode Mode{ TrackPlayMode::Timeline };
        std::wstring ClipId{};

        // The launcher slot the clip was launched from, or -1. A clip can sit in several slots.
        int32_t Slot{ -1 };

        // How far through its clip a launched clip is, 0 to 1.
        double Progress{ 0 };

        // How many times it has gone round since it was launched.
        uint64_t Pass{ 0 };

        bool Pending{ false };
        TrackPlayMode PendingMode{ TrackPlayMode::Timeline };
        std::wstring PendingClipId{};
        int32_t PendingSlot{ -1 };
        int64_t PendingTick{ 0 };

        // Whether the slot holding this clip is the one playing, or the one waiting to. A launch
        // that named no slot counts for every slot that holds the clip.
        bool Plays(_In_ std::wstring const& clipId, _In_ size_t slot) const noexcept
        {
            return Mode == TrackPlayMode::Clip && !clipId.empty() && ClipId == clipId && (Slot < 0 || static_cast<size_t>(Slot) == slot);
        }

        bool Queues(_In_ std::wstring const& clipId, _In_ size_t slot) const noexcept
        {
            return Pending && PendingMode == TrackPlayMode::Clip && !clipId.empty() && PendingClipId == clipId && (PendingSlot < 0 || static_cast<size_t>(PendingSlot) == slot);
        }
    };

    // A note an arpeggiator played from the keys held, as it was handed over: what recording and
    // Capture keep, instead of the keys. The words are as rendered, before the destination.
    struct ArpeggiatedNote
    {
        std::wstring TrackId{};
        uint64_t Timestamp{ 0 };
        std::array<uint32_t, 4> Words{};
        uint8_t WordCount{ 0 };
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

        // What a destination speaks on a group, and how early to send to it. A track's own
        // protocol choice overrides what this says.
        void SetDestinationLookup(_In_ std::function<DestinationInfo(EndpointRef const&, uint8_t group)> lookup);

        // Sends each track's start-up messages and, part way in, the program, controllers and
        // pitch bend each channel would have by then, then starts.
        void Play(_In_ int64_t fromTick);

        // Ends every note it started and sends sustain off and all notes off on each channel it
        // used, now and again one look-ahead later, for the notes already handed over.
        void Stop();

        bool IsPlaying() const;
        int64_t PositionTick() const;

        // Where on the timeline a moment was, for recording: a message's timestamp, worked out
        // with the tempo map and any loop the playback went round. -1 when not playing.
        int64_t TickAtTime(_In_ uint64_t timestamp) const;

        // The launcher. A launch or a stop waits for the next quantize point that hasn't been
        // handed over yet. Launching while stopped starts playback with the clip.
        void LaunchClip(_In_ std::wstring const& trackId, _In_ std::wstring const& clipId, _In_ LaunchQuantize quantize, _In_ int32_t slot = -1);
        void StopTrack(_In_ std::wstring const& trackId, _In_ LaunchQuantize quantize);

        // Back to the timeline at the next quantize point. An empty id means every track.
        void ReturnToTimeline(_In_ std::wstring const& trackId, _In_ LaunchQuantize quantize);

        std::vector<TrackLaunchView> LaunchState() const;

        // The same at an exact place, for recording into a slot. Places here are unwrapped ticks,
        // which keep counting when the timeline loop goes round. A place already handed over moves
        // to the first one that isn't.
        int64_t NextLaunchPoint(_In_ LaunchQuantize quantize) const;
        void LaunchClipAt(_In_ std::wstring const& trackId, _In_ std::wstring const& clipId, _In_ int64_t unwrappedTick, _In_ int32_t slot = -1);
        void StopTrackAt(_In_ std::wstring const& trackId, _In_ int64_t unwrappedTick);

        // Where on the unwrapped timeline a moment was. -1 when not playing.
        int64_t UnwrappedTickAtTime(_In_ uint64_t timestamp) const;

        // Following another device's clock. While it's on, the app hands over what that device
        // sends, with the time each message arrived: its pulses set the tempo, and its Start,
        // Continue, Stop and song position run the transport. Play does nothing then, the loop is
        // off, and playback stops when the pulses stop coming.
        void SetFollowing(_In_ bool following);
        bool IsFollowing() const;
        void FollowPulse(_In_ uint64_t timestamp);
        void FollowStart(_In_ uint64_t timestamp);
        void FollowContinue(_In_ uint64_t timestamp);
        void FollowStop(_In_ uint64_t timestamp);
        void FollowSongPosition(_In_ uint64_t timestamp, _In_ uint32_t sixteenths);

        // The tempo of the clock being followed, or 0 while it isn't sending.
        double FollowedBeatsPerMinute() const;

        // Keys played on the source of a track, for an arpeggiator on it that plays what's held.
        // Any thread. Stopping playback ends what was latched.
        void PressKey(_In_ std::wstring const& trackId, _In_ uint8_t number, _In_ uint16_t velocity);
        void ReleaseKey(_In_ std::wstring const& trackId, _In_ uint8_t number);

        // For a track that stopped listening, so a key that went up unheard doesn't stay down.
        void ForgetKeys(_In_ std::wstring const& trackId);

        // Whether an arpeggiator that plays held keys was playing on the track at the last sweep,
        // so what's played on its source goes to the arpeggiator, not straight out. Any thread.
        bool IsArpeggiating(_In_ std::wstring const& trackId) const;

        // What arpeggiators played since the last call, oldest first. Any thread.
        std::vector<ArpeggiatedNote> TakeArpeggiated();

        // Hands over everything due before now plus the look-ahead.
        void Sweep();

        // Starts and stops the engine's own thread, which calls Sweep every few milliseconds.
        void StartThread();
        void StopThread() noexcept;

        EngineCounters Counters() const;

    private:
        // A note that's been started. Its end goes to the service only once it's due within the
        // look-ahead: a message handed over can't be taken back, and an end sent early would cut
        // the next note of the same pitch after a restart or a launch.
        struct SoundingNote
        {
            std::wstring TrackId{};
            EndpointRef Endpoint{};
            uint32_t OffWords[2]{};
            uint8_t WordCount{ 2 };
            uint64_t OnTimestamp{ 0 };
            uint64_t OffTimestamp{ 0 };

            // The device's offset, in clock ticks, already taken off both timestamps.
            uint64_t Lead{ 0 };

            bool HandedOver{ false };
        };

        struct ChannelKey
        {
            EndpointRef Endpoint{};
            uint8_t Group{ 0 };
            uint8_t Channel{ 0 };
            bool Midi2{ true };
        };

        // A stretch of playback with one mapping from ticks to time. Playback starts one, and each
        // time the loop goes round another begins at the loop start. Unwrapped ticks keep counting
        // across loops, which is what launched clips use so they don't jump when the loop does.
        struct Segment
        {
            int64_t StartTick{ 0 };
            uint64_t StartTimestamp{ 0 };
            double StartSeconds{ 0 };
            int64_t UnwrappedStart{ 0 };
        };

        struct TrackLaunch
        {
            TrackPlayMode Mode{ TrackPlayMode::Timeline };
            std::wstring ClipId{};
            int32_t Slot{ -1 };
            int64_t ClipStartUnwrapped{ 0 };

            bool Pending{ false };
            TrackPlayMode PendingMode{ TrackPlayMode::Timeline };
            std::wstring PendingClipId{};
            int32_t PendingSlot{ -1 };
            int64_t PendingAtUnwrapped{ 0 };
        };

        uint64_t TimestampAtTick(_In_ int64_t tick) const noexcept;
        uint64_t EarlierBy(_In_ uint64_t timestamp, _In_ int64_t microseconds) const noexcept;
        int64_t ClockLeadMicroseconds(_In_ ClockOutput const& output) const;
        int64_t TimeCodeLeadMicroseconds(_In_ TimeCodeOutput const& output) const;
        uint64_t LongestLeadMicroseconds() const;
        int64_t TickAtTimestamp(_In_ uint64_t timestamp) const noexcept;
        double SecondsAtTimestamp(_In_ uint64_t timestamp) const noexcept;
        int64_t TickAtTimeLocked(_In_ uint64_t timestamp) const;
        int64_t Unwrap(_In_ int64_t tick) const noexcept { return m_segment.UnwrappedStart + (tick - m_segment.StartTick); }
        int64_t Wrap(_In_ int64_t unwrapped) const noexcept { return m_segment.StartTick + (unwrapped - m_segment.UnwrappedStart); }
        int64_t SegmentEnd() const noexcept;
        int64_t QuantizePoint(_In_ LaunchQuantize quantize) const;
        int64_t FurthestRendered() const noexcept;
        void QueueLaunch(_In_ std::wstring const& trackId, _In_ TrackPlayMode mode, _In_ std::wstring const& clipId, _In_ LaunchQuantize quantize, _In_ int32_t slot);
        void QueueLaunchAt(_In_ std::wstring const& trackId, _In_ TrackPlayMode mode, _In_ std::wstring const& clipId, _In_ int64_t unwrappedTick, _In_ int32_t slot);

        DestinationInfo LookupDestination(_In_ TrackDestination const& destination) const;
        DestinationInfo LookupDestination(_In_ EndpointRef const& endpoint, _In_ uint8_t group) const;
        bool IsAudible(_In_ Track const& track, _In_ bool anySolo, _In_ bool insideSoloedFolder, _In_ bool insideMutedFolder) const noexcept;

        void SendTo(_In_ EndpointRef const& endpoint, _In_ bool midi2, _In_ uint64_t timestamp, _In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount);
        void RememberChannel(_In_ EndpointRef const& endpoint, _In_ bool midi2, _In_ uint32_t word0);
        void SendStartup(_In_ Track const& track, _In_ uint64_t timestamp);
        void SendChase(_In_ Track const& track, _In_ int64_t fromTick, _In_ uint64_t timestamp);
        void SweepTracks(_In_ std::vector<Track> const& tracks, _In_ uint64_t now, _In_ bool anySolo, _In_ bool insideSoloedFolder, _In_ bool insideMutedFolder);
        void SweepTrack(_In_ Track const& track, _In_ uint64_t now, _In_ bool audible);
        void RenderMode(_In_ Track const& track, _In_ TrackLaunch const& state, _In_ int64_t fromUnwrapped, _In_ int64_t toUnwrapped, _In_ RenderOptions const& options, _Inout_ std::vector<RenderedMessage>& messages);
        void SendRendered(_In_ Track const& track, _In_ DestinationInfo const& info, _In_ int64_t clampOffsAt, _In_ uint64_t now, _Inout_ std::vector<RenderedMessage>& messages);
        void QueueNoteOff(
            _In_ std::wstring const& trackId,
            _In_ EndpointRef const& endpoint,
            _In_ bool midi2,
            _In_ uint64_t onTimestamp,
            _In_ uint64_t offTimestamp,
            _In_ uint64_t lead,
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint8_t wordCount,
            _In_ uint64_t limit);
        void SendDueNoteOffs(_In_ uint64_t limit);
        uint64_t LookAheadTicks() const noexcept;
        void SweepMetronome(_In_ uint64_t now);
        void SweepClockOutputs(_In_ uint64_t now);
        void SendTempoChanges(_In_ int64_t fromTick, _In_ int64_t toTick, _In_ std::vector<int64_t> const& leads);
        void LocateTimeCode(_In_ uint64_t timestamp);
        void SendFullFrame(_In_ TimeCodeOutput const& output, _In_ uint64_t timestamp, _In_ double seconds);
        void SweepTimeCodeOutputs(_In_ uint64_t now);
        void WrapLoopIfDue(_In_ uint64_t now);
        void SendSilence(_In_ uint64_t timestamp);
        bool StartLocked(_In_ int64_t fromTick, _In_ uint64_t startTimestamp, _In_ bool jumping);
        void StopLocked();
        bool StartFollowedLocked(_In_ int64_t tick, _In_ uint64_t timestamp, _In_ bool jumping);
        uint64_t FollowedPulseInterval(_In_ int64_t tick) const noexcept;
        uint64_t FollowPatience() const noexcept;
        uint64_t PlanUntil(_In_ uint64_t horizon) const noexcept;
        void ThreadMain() noexcept;

        IEngineOutput& m_output;
        EngineClock m_clock{};

        mutable std::mutex m_lock{};

        std::shared_ptr<Sequence const> m_sequence{};
        TempoMap m_tempo{};
        double m_fastestTempo{ 120.0 };
        EngineSettings m_settings{};
        std::function<DestinationInfo(EndpointRef const&, uint8_t)> m_lookup{};

        bool m_playing{ false };
        Segment m_segment{};

        // The segments before this one, newest last, so a message recorded just before the loop
        // went round still lands where it was played.
        std::vector<Segment> m_history{};

        // How far each track, the metronome and the clock outputs have been handed over.
        std::unordered_map<std::wstring, int64_t> m_renderedUntil{};
        int64_t m_metronomeUntil{ 0 };
        int64_t m_clockUntil{ 0 };

        // The tempo the clock outputs were last told, or not a number when they haven't been.
        double m_tempoSent{ std::numeric_limits<double>::quiet_NaN() };

        // The next quarter frame each time code output sends, counted from the top of the
        // sequence, so the same place always gets the same time code. Empty means each output
        // has to be told where it is first.
        std::vector<uint64_t> m_timeCodeNext{};

        std::unordered_map<std::wstring, TrackLaunch> m_launch{};

        // Lanes send where they are again after an edit made while playing, and when a muted
        // track is heard again: each counts edits, and a track remembers the last one it saw.
        uint64_t m_editCount{ 0 };
        std::unordered_map<std::wstring, uint64_t> m_editsSeen{};
        std::unordered_set<std::wstring> m_inaudible{};

        std::vector<SoundingNote> m_sounding{};
        std::vector<ChannelKey> m_channelsUsed{};

        // Keys held on each track's source, and the tracks whose arpeggiator played them in the
        // last sweep.
        std::unordered_map<std::wstring, ArpeggiatorInput> m_keys{};
        std::unordered_set<std::wstring> m_arpeggiating{};
        std::vector<ArpeggiatedNote> m_arpeggiated{};

        // Following another device's clock: the pace of its pulses, whether it's running, the
        // number of its next pulse counted from the top (24 a quarter note), and when its last
        // pulse came. The music is anchored at that pulse and moves on at the pace.
        bool m_following{ false };
        midiapp::ClockFollower m_followPace{};
        bool m_followRunning{ false };
        uint64_t m_followNextPulse{ 0 };
        uint64_t m_followLastPulse{ 0 };
        uint64_t m_followLastHeard{ 0 };
        double m_followTicksPerSecond{ 0 };

        // Whether a pulse has placed the music since Start, Continue or a song position.
        bool m_followAnchored{ false };

        EngineCounters m_counters{};

        std::thread m_thread{};
        std::atomic<bool> m_threadRunning{ false };
    };
}
