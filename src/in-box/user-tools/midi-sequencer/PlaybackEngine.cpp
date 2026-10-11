// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "PlaybackEngine.h"

#include "MessageTranslation.h"
#include "MidiClipFile.h"
#include "SequenceRender.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>

namespace midisequencer
{
    namespace
    {
        // Start-up and chase messages go out the moment Play is pressed, and the music starts a
        // moment later, so they're sure to arrive first.
        constexpr uint64_t StartLeadMicroseconds = 20000;

        // A metronome click is a sixteenth note long.
        constexpr int64_t ClickTicks = TicksPerQuarterNote / 4;

        // MIDI clock: 24 pulses a quarter note. Song position counts sixteenth notes.
        constexpr int64_t TicksPerClock = TicksPerQuarterNote / 24;
        constexpr int64_t TicksPerSongPositionBeat = TicksPerQuarterNote / 4;

        constexpr uint8_t ControllerSustain = 64;
        constexpr uint8_t ControllerAllSoundOff = 120;
        constexpr uint8_t ControllerAllNotesOff = 123;

        uint32_t SystemWord(_In_ uint8_t group, _In_ uint8_t status, _In_ uint8_t data1 = 0, _In_ uint8_t data2 = 0) noexcept
        {
            return 0x10000000u | (static_cast<uint32_t>(group & 0x0F) << 24) | (static_cast<uint32_t>(status) << 16) |
                (static_cast<uint32_t>(data1 & 0x7F) << 8) | (data2 & 0x7Fu);
        }

        bool IsChannelVoice(_In_ uint32_t word0) noexcept
        {
            auto const messageType = word0 >> 28;
            return messageType == 0x2 || messageType == 0x4;
        }

        bool SameTimeCodeOutputs(_In_ std::vector<TimeCodeOutput> const& a, _In_ std::vector<TimeCodeOutput> const& b) noexcept
        {
            return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](TimeCodeOutput const& x, TimeCodeOutput const& y)
            {
                return x.Endpoint == y.Endpoint && x.Group == y.Group && x.Rate == y.Rate && x.OffsetMicroseconds == y.OffsetMicroseconds &&
                    x.Start.Hours == y.Start.Hours && x.Start.Minutes == y.Start.Minutes && x.Start.Seconds == y.Start.Seconds &&
                    x.Start.Frames == y.Start.Frames;
            });
        }
    }

    _Use_decl_annotations_
    PlaybackEngine::PlaybackEngine(IEngineOutput& output, EngineClock clock) :
        m_output(output),
        m_clock(std::move(clock))
    {
    }

    PlaybackEngine::~PlaybackEngine()
    {
        StopThread();
    }

    _Use_decl_annotations_
    void PlaybackEngine::SetSequence(std::shared_ptr<Sequence const> sequence)
    {
        std::scoped_lock guard{ m_lock };

        if (sequence == nullptr)
        {
            return;
        }

        if (m_playing && m_clock.Now)
        {
            // Keep the music where it is: the new tempo map takes over from here. What was already
            // handed over stays handed over.
            auto const now = m_clock.Now();
            auto const tick = TickAtTimestamp(now);
            auto const unwrapped = Unwrap(tick);

            m_sequence = std::move(sequence);
            m_tempo = TempoMap{ m_sequence->Tempo };
            m_fastestTempo = FastestTempo(m_sequence->Tempo);
            ++m_editCount;
            m_segment.StartTick = tick;
            m_segment.StartTimestamp = now;
            m_segment.StartSeconds = m_tempo.SecondsAtTick(tick);
            m_segment.UnwrappedStart = unwrapped;
            m_history.clear();
            return;
        }

        m_sequence = std::move(sequence);
        m_tempo = TempoMap{ m_sequence->Tempo };
        m_fastestTempo = FastestTempo(m_sequence->Tempo);
    }

    _Use_decl_annotations_
    void PlaybackEngine::SetSettings(EngineSettings const& settings)
    {
        std::scoped_lock guard{ m_lock };

        // Time code that changes while playing is told where it is again at the next sweep.
        if (!SameTimeCodeOutputs(m_settings.TimeCodeOutputs, settings.TimeCodeOutputs))
        {
            m_timeCodeNext.clear();
        }

        m_settings = settings;
        m_settings.LookAheadMicroseconds = std::clamp<uint32_t>(m_settings.LookAheadMicroseconds, 2000, 1000000);
        m_settings.SweepMicroseconds = std::clamp<uint32_t>(m_settings.SweepMicroseconds, 1000, 50000);

        // A loop shorter than a sixteenth note would go round faster than the engine sweeps.
        if (m_settings.LoopEnd - m_settings.LoopStart < TicksPerQuarterNote / 4 || m_settings.LoopStart < 0)
        {
            m_settings.LoopEnabled = false;
        }

        // Turning the loop on while playing past its end leaves playback where it is.
        if (m_playing && m_settings.LoopEnabled && m_clock.Now)
        {
            if (TickAtTimestamp(m_clock.Now()) >= m_settings.LoopEnd)
            {
                m_settings.LoopEnabled = false;
            }
        }
    }

    _Use_decl_annotations_
    void PlaybackEngine::SetDestinationLookup(std::function<DestinationInfo(EndpointRef const&, uint8_t)> lookup)
    {
        std::scoped_lock guard{ m_lock };
        m_lookup = std::move(lookup);
    }

    _Use_decl_annotations_
    uint64_t PlaybackEngine::TimestampAtTick(int64_t tick) const noexcept
    {
        // Following, the music moves on from the last pulse at the pulses' pace.
        auto const seconds = m_following && m_followTicksPerSecond > 0
            ? static_cast<double>(tick - m_segment.StartTick) / m_followTicksPerSecond
            : m_tempo.SecondsAtTick(tick) - m_segment.StartSeconds;
        auto const offset = std::llround(seconds * static_cast<double>(m_clock.TicksPerSecond));

        if (offset < 0)
        {
            auto const back = static_cast<uint64_t>(-offset);
            return back >= m_segment.StartTimestamp ? 1 : m_segment.StartTimestamp - back;
        }

        return m_segment.StartTimestamp + static_cast<uint64_t>(offset);
    }

    _Use_decl_annotations_
    uint64_t PlaybackEngine::EarlierBy(uint64_t timestamp, int64_t microseconds) const noexcept
    {
        auto const ticks = microseconds * static_cast<int64_t>(m_clock.TicksPerSecond) / 1000000;
        return static_cast<uint64_t>(std::max<int64_t>(1, static_cast<int64_t>(timestamp) - ticks));
    }

    // How much earlier than the beat a clock output's pulses go: its device's offset, less the
    // output's own. Negative means later.
    _Use_decl_annotations_
    int64_t PlaybackEngine::ClockLeadMicroseconds(ClockOutput const& output) const
    {
        return static_cast<int64_t>(LookupDestination(output.Endpoint, output.Group).OffsetMicroseconds) - output.OffsetMicroseconds;
    }

    _Use_decl_annotations_
    int64_t PlaybackEngine::TimeCodeLeadMicroseconds(TimeCodeOutput const& output) const
    {
        return static_cast<int64_t>(LookupDestination(output.Endpoint, output.Group).OffsetMicroseconds) - output.OffsetMicroseconds;
    }

    // The furthest ahead of the beat anything is sent.
    uint64_t PlaybackEngine::LongestLeadMicroseconds() const
    {
        int64_t longest{ 0 };

        if (m_sequence != nullptr)
        {
            ForEachTrack(*m_sequence, [this, &longest](Track const& track, size_t)
            {
                if (!track.IsFolder)
                {
                    longest = std::max<int64_t>(longest, LookupDestination(track.Destination).OffsetMicroseconds);
                }

                return true;
            });
        }

        if (m_settings.Metronome.Enabled && !m_settings.Metronome.Endpoint.IsEmpty())
        {
            longest = std::max<int64_t>(longest, LookupDestination(m_settings.Metronome.Endpoint, m_settings.Metronome.Group).OffsetMicroseconds);
        }

        for (auto const& output : m_settings.ClockOutputs)
        {
            longest = std::max(longest, ClockLeadMicroseconds(output));
        }

        for (auto const& output : m_settings.TimeCodeOutputs)
        {
            longest = std::max(longest, TimeCodeLeadMicroseconds(output));
        }

        return static_cast<uint64_t>(longest);
    }

    _Use_decl_annotations_
    int64_t PlaybackEngine::TickAtTimestamp(uint64_t timestamp) const noexcept
    {
        auto const elapsed = timestamp >= m_segment.StartTimestamp
            ? static_cast<double>(timestamp - m_segment.StartTimestamp)
            : -static_cast<double>(m_segment.StartTimestamp - timestamp);

        if (m_following && m_followTicksPerSecond > 0)
        {
            return m_segment.StartTick + static_cast<int64_t>(std::floor(elapsed / static_cast<double>(m_clock.TicksPerSecond) * m_followTicksPerSecond + 1e-9));
        }

        return m_tempo.TickAtSeconds(m_segment.StartSeconds + elapsed / static_cast<double>(m_clock.TicksPerSecond));
    }

    _Use_decl_annotations_
    double PlaybackEngine::SecondsAtTimestamp(uint64_t timestamp) const noexcept
    {
        auto const elapsed = static_cast<double>(timestamp) - static_cast<double>(m_segment.StartTimestamp);
        return m_segment.StartSeconds + elapsed / static_cast<double>(m_clock.TicksPerSecond);
    }

    int64_t PlaybackEngine::SegmentEnd() const noexcept
    {
        return m_settings.LoopEnabled && !m_following && m_segment.StartTick < m_settings.LoopEnd ? m_settings.LoopEnd : INT64_MAX;
    }

    _Use_decl_annotations_
    DestinationInfo PlaybackEngine::LookupDestination(EndpointRef const& endpoint, uint8_t group) const
    {
        return m_lookup ? m_lookup(endpoint, group) : DestinationInfo{};
    }

    _Use_decl_annotations_
    DestinationInfo PlaybackEngine::LookupDestination(TrackDestination const& destination) const
    {
        auto info = LookupDestination(destination.Endpoint, destination.Group);

        switch (destination.Protocol)
        {
        case ProtocolChoice::Midi1: info.SpeaksMidi2 = false; break;
        case ProtocolChoice::Midi2: info.SpeaksMidi2 = true; break;
        default: break;
        }

        return info;
    }

    _Use_decl_annotations_
    void PlaybackEngine::RememberChannel(EndpointRef const& endpoint, bool midi2, uint32_t word0)
    {
        if (!IsChannelVoice(word0))
        {
            return;
        }

        auto const group = static_cast<uint8_t>((word0 >> 24) & 0x0F);
        auto const channel = static_cast<uint8_t>((word0 >> 16) & 0x0F);

        auto const known = std::any_of(m_channelsUsed.begin(), m_channelsUsed.end(), [&](ChannelKey const& key)
        {
            return key.Group == group && key.Channel == channel && key.Endpoint == endpoint;
        });

        if (!known)
        {
            m_channelsUsed.push_back(ChannelKey{ endpoint, group, channel, midi2 });
        }
    }

    _Use_decl_annotations_
    void PlaybackEngine::SendTo(EndpointRef const& endpoint, bool midi2, uint64_t timestamp, uint32_t const* words, uint8_t wordCount)
    {
        if (endpoint.IsEmpty() || wordCount == 0)
        {
            return;
        }

        auto const translated = midi2 ? TranslateToMidi2(words, wordCount) : TranslateToMidi1(words, wordCount);

        if (translated.Dropped)
        {
            ++m_counters.MessagesDropped;
        }

        for (uint8_t i = 0; i < translated.Count; ++i)
        {
            m_output.Send(endpoint, timestamp, translated.Messages[i].data(), translated.WordCounts[i]);
            RememberChannel(endpoint, midi2, translated.Messages[i][0]);
            ++m_counters.MessagesSent;
        }
    }

    _Use_decl_annotations_
    void PlaybackEngine::SendStartup(Track const& track, uint64_t timestamp)
    {
        auto const info = LookupDestination(track.Destination);

        for (auto message : track.Startup)
        {
            ApplyDestination(track.Destination, message.Words, message.WordCount);
            SendTo(track.Destination.Endpoint, info.SpeaksMidi2, timestamp, message.Words.data(), message.WordCount);
        }
    }

    _Use_decl_annotations_
    void PlaybackEngine::SendChase(Track const& track, int64_t fromTick, uint64_t timestamp)
    {
        if (fromTick <= 0)
        {
            return;
        }

        // Lanes aren't chased: the first render after a start sends where each one is.
        RenderOptions options{};
        options.Lanes = false;

        std::vector<RenderedMessage> earlier{};
        RenderTrack(*m_sequence, track, 0, fromTick, earlier, options);

        // The last value of each thing a channel remembers. Controllers sort first, by number, so
        // bank select goes before program change; pitch bend and pressure go last.
        std::map<uint64_t, RenderedMessage> last{};

        for (auto& message : earlier)
        {
            if (message.Kind != RenderedKind::Other || !IsChannelVoice(message.Words[0]))
            {
                continue;
            }

            ApplyDestination(track.Destination, message.Words, message.WordCount);

            auto const word0 = message.Words[0];
            auto const status = (word0 >> 20) & 0x0F;
            auto const channelKey = static_cast<uint64_t>((word0 >> 16) & 0xFF) << 32;

            uint64_t order{ 0 };

            switch (status)
            {
            case 0xB: order = 0x100 + ((word0 >> 8) & 0x7F); break;     // controllers, by number
            case 0xC: order = 0x200; break;                             // program
            case 0xE: order = 0x300; break;                             // pitch bend
            case 0xD: order = 0x301; break;                             // channel pressure
            default: continue;                                          // notes, per-note and the rest aren't state
            }

            last[channelKey | order] = message;
        }

        auto const info = LookupDestination(track.Destination);

        for (auto const& [key, message] : last)
        {
            SendTo(track.Destination.Endpoint, info.SpeaksMidi2, timestamp, message.Words.data(), message.WordCount);
        }
    }

    _Use_decl_annotations_
    void PlaybackEngine::Play(int64_t fromTick)
    {
        {
            std::scoped_lock guard{ m_lock };

            // While following, the other device starts playback.
            if (m_following || !StartLocked(fromTick, 0, false))
            {
                return;
            }
        }

        Sweep();
    }

    // Starts the music at a tick, at a time, or as soon as it safely can when the time is 0. A
    // jump moves music that's already playing: followers get a song position, not start or continue.
    _Use_decl_annotations_
    bool PlaybackEngine::StartLocked(int64_t fromTick, uint64_t startTimestamp, bool jumping)
    {
        if (m_sequence == nullptr || !m_clock.Now)
        {
            return false;
        }

        if (m_playing)
        {
            SendSilence(0);
        }

        auto const now = m_clock.Now();

        m_segment.StartTick = std::max<int64_t>(0, fromTick);

        // Late enough that the device needing its messages earliest still gets them in time.
        m_segment.StartTimestamp = startTimestamp != 0
            ? std::max(startTimestamp, now + 1)
            : now + (StartLeadMicroseconds + LongestLeadMicroseconds()) * m_clock.TicksPerSecond / 1000000;
        m_segment.StartSeconds = m_tempo.SecondsAtTick(m_segment.StartTick);
        m_segment.UnwrappedStart = m_segment.StartTick;
        m_history.clear();

        // A start inside the loop's end plays on; the loop applies only from inside it.
        m_renderedUntil.clear();
        m_metronomeUntil = m_segment.StartTick;
        m_clockUntil = m_segment.StartTick;
        m_tempoSent = std::numeric_limits<double>::quiet_NaN();
        m_sounding.clear();
        m_channelsUsed.clear();

        // A launched clip starts again from its beginning; anything waiting goes.
        for (auto& [id, state] : m_launch)
        {
            state.Pending = false;

            if (state.Mode == TrackPlayMode::Clip)
            {
                state.ClipStartUnwrapped = m_segment.UnwrappedStart;
            }
        }

        auto const startTick = m_segment.StartTick;

        ForEachTrack(*m_sequence, [this, startTick](Track const& track, size_t)
        {
            if (!track.IsFolder)
            {
                SendStartup(track, 0);

                auto const state = m_launch.find(track.Id);

                if (state == m_launch.end() || state->second.Mode == TrackPlayMode::Timeline)
                {
                    SendChase(track, startTick, 0);
                }
            }

            return true;
        });

        for (auto const& output : m_settings.ClockOutputs)
        {
            auto const position = std::min<int64_t>(startTick / TicksPerSongPositionBeat, 0x3FFF);
            auto const pointer = SystemWord(output.Group, 0xF2, static_cast<uint8_t>(position & 0x7F), static_cast<uint8_t>(position >> 7));

            if (jumping)
            {
                if (output.SendSongPosition)
                {
                    m_output.Send(output.Endpoint, EarlierBy(m_segment.StartTimestamp, ClockLeadMicroseconds(output)) - 1, &pointer, 1);
                }

                continue;
            }

            if (startTick == 0)
            {
                if (output.SendStartStop)
                {
                    auto const start = SystemWord(output.Group, 0xFA);
                    m_output.Send(output.Endpoint, EarlierBy(m_segment.StartTimestamp, ClockLeadMicroseconds(output)), &start, 1);
                }
            }
            else
            {
                if (output.SendSongPosition)
                {
                    m_output.Send(output.Endpoint, 0, &pointer, 1);
                }

                if (output.SendStartStop)
                {
                    auto const resume = SystemWord(output.Group, 0xFB);
                    m_output.Send(output.Endpoint, EarlierBy(m_segment.StartTimestamp, ClockLeadMicroseconds(output)), &resume, 1);
                }
            }
        }

        LocateTimeCode(m_segment.StartTimestamp);

        m_playing = true;
        return true;
    }

    _Use_decl_annotations_
    void PlaybackEngine::SendSilence(uint64_t timestamp)
    {
        auto const now = m_clock.Now ? m_clock.Now() : 0;

        // First, end every note that's still sounding or still to start. One whose start is still
        // waiting in the service ends just after it, so the end can't arrive first and leave it on.
        for (auto const& note : m_sounding)
        {
            if (note.OffTimestamp > now)
            {
                auto const at = note.OnTimestamp > now ? std::max(timestamp, note.OnTimestamp + 1) : timestamp;
                m_output.Send(note.Endpoint, at, note.OffWords, note.WordCount);
            }
        }

        // Then sustain off before all notes off, so a held pedal doesn't keep them ringing, then
        // all sound off and pitch bend back to the center. Only on channels this sequence used:
        // other apps may be playing the same device.
        for (auto const& key : m_channelsUsed)
        {
            auto const send = [&](uint8_t controller)
            {
                uint32_t words[2]{};

                if (key.Midi2)
                {
                    words[0] = 0x40B00000u | (static_cast<uint32_t>(key.Group) << 24) | (static_cast<uint32_t>(key.Channel) << 16) | (static_cast<uint32_t>(controller) << 8);
                    m_output.Send(key.Endpoint, timestamp, words, 2);
                }
                else
                {
                    words[0] = 0x20B00000u | (static_cast<uint32_t>(key.Group) << 24) | (static_cast<uint32_t>(key.Channel) << 16) | (static_cast<uint32_t>(controller) << 8);
                    m_output.Send(key.Endpoint, timestamp, words, 1);
                }
            };

            send(ControllerSustain);
            send(ControllerAllNotesOff);
            send(ControllerAllSoundOff);

            uint32_t bend[2]{};

            if (key.Midi2)
            {
                bend[0] = 0x40E00000u | (static_cast<uint32_t>(key.Group) << 24) | (static_cast<uint32_t>(key.Channel) << 16);
                bend[1] = 0x80000000u;
                m_output.Send(key.Endpoint, timestamp, bend, 2);
            }
            else
            {
                bend[0] = 0x20E00040u | (static_cast<uint32_t>(key.Group) << 24) | (static_cast<uint32_t>(key.Channel) << 16);
                m_output.Send(key.Endpoint, timestamp, bend, 1);
            }
        }
    }

    void PlaybackEngine::Stop()
    {
        std::scoped_lock guard{ m_lock };
        StopLocked();
    }

    void PlaybackEngine::StopLocked()
    {
        if (!m_playing || !m_clock.Now)
        {
            return;
        }

        m_playing = false;

        auto const now = m_clock.Now();
        auto const again = now + (static_cast<uint64_t>(m_settings.LookAheadMicroseconds) + 10000) * m_clock.TicksPerSecond / 1000000;

        // Now, for what's sounding, and again once everything already handed over has played.
        SendSilence(0);
        SendSilence(again);

        // Pulses already handed over still go out, so stop follows the last of them.
        auto const lastPulse = ((m_clockUntil - 1) / TicksPerClock) * TicksPerClock;

        for (auto const& output : m_settings.ClockOutputs)
        {
            if (output.SendStartStop)
            {
                uint64_t at{ 0 };

                if (output.SendClock && m_clockUntil > m_segment.StartTick && lastPulse >= m_segment.StartTick)
                {
                    auto const after = EarlierBy(TimestampAtTick(lastPulse), ClockLeadMicroseconds(output)) + 1;
                    at = after > now ? after : 0;
                }

                auto const stop = SystemWord(output.Group, 0xFC);
                m_output.Send(output.Endpoint, at, &stop, 1);
            }
        }

        // Time code receivers are told where it stopped, once what was handed over has played.
        auto const stoppedAt = std::max(m_segment.StartSeconds, SecondsAtTimestamp(now));

        for (auto const& output : m_settings.TimeCodeOutputs)
        {
            SendFullFrame(output, again, stoppedAt);
        }

        m_timeCodeNext.clear();

        // Following, it stopped where the other device's clock got to.
        m_segment.StartTick = m_following ? static_cast<int64_t>(m_followNextPulse) * TicksPerClock : std::max<int64_t>(0, TickAtTimestamp(now));
        m_segment.UnwrappedStart = m_segment.StartTick;
        m_history.clear();
        m_sounding.clear();

        for (auto& [id, state] : m_launch)
        {
            state.Pending = false;
        }
    }

    bool PlaybackEngine::IsPlaying() const
    {
        std::scoped_lock guard{ m_lock };
        return m_playing;
    }

    int64_t PlaybackEngine::PositionTick() const
    {
        std::scoped_lock guard{ m_lock };

        if (!m_playing || !m_clock.Now)
        {
            return m_segment.StartTick;
        }

        auto const tick = TickAtTimeLocked(m_clock.Now());
        return tick < 0 ? m_segment.StartTick : tick;
    }

    _Use_decl_annotations_
    int64_t PlaybackEngine::TickAtTime(uint64_t timestamp) const
    {
        std::scoped_lock guard{ m_lock };
        return TickAtTimeLocked(timestamp);
    }

    _Use_decl_annotations_
    int64_t PlaybackEngine::TickAtTimeLocked(uint64_t timestamp) const
    {
        if (!m_playing)
        {
            return -1;
        }

        if (m_following)
        {
            return std::max<int64_t>(0, TickAtTimestamp(timestamp));
        }

        auto const at = [this, timestamp](Segment const& segment)
        {
            auto const elapsed = timestamp >= segment.StartTimestamp
                ? static_cast<double>(timestamp - segment.StartTimestamp)
                : -static_cast<double>(segment.StartTimestamp - timestamp);

            auto const tick = m_tempo.TickAtSeconds(segment.StartSeconds + elapsed / static_cast<double>(m_clock.TicksPerSecond));
            return std::max(segment.StartTick, tick);
        };

        if (timestamp >= m_segment.StartTimestamp || m_history.empty())
        {
            return at(m_segment);
        }

        // Before the loop went round: the segment it was played in.
        for (auto it = m_history.rbegin(); it != m_history.rend(); ++it)
        {
            if (timestamp >= it->StartTimestamp)
            {
                return at(*it);
            }
        }

        return at(m_history.front());
    }

    _Use_decl_annotations_
    bool PlaybackEngine::IsAudible(Track const& track, bool anySolo, bool insideSoloedFolder, bool insideMutedFolder) const noexcept
    {
        if (track.Muted || insideMutedFolder)
        {
            return false;
        }

        return !anySolo || track.Soloed || insideSoloedFolder;
    }

    _Use_decl_annotations_
    void PlaybackEngine::SweepTracks(std::vector<Track> const& tracks, uint64_t now, bool anySolo, bool insideSoloedFolder, bool insideMutedFolder)
    {
        for (auto const& track : tracks)
        {
            if (track.IsFolder)
            {
                SweepTracks(track.Children, now, anySolo, insideSoloedFolder || track.Soloed, insideMutedFolder || track.Muted);
                continue;
            }

            SweepTrack(track, now, IsAudible(track, anySolo, insideSoloedFolder, insideMutedFolder));
        }
    }

    _Use_decl_annotations_
    void PlaybackEngine::SweepTrack(Track const& track, uint64_t now, bool audible)
    {
        auto const info = LookupDestination(track.Destination);

        // A destination that needs its messages early is handed them that much sooner.
        auto const horizon = PlanUntil(now + (static_cast<uint64_t>(m_settings.LookAheadMicroseconds) + info.OffsetMicroseconds) * m_clock.TicksPerSecond / 1000000);
        auto const horizonTick = std::min(TickAtTimestamp(horizon), SegmentEnd());

        auto found = m_renderedUntil.find(track.Id);
        auto const from = found == m_renderedUntil.end() ? m_segment.StartTick : found->second;

        if (horizonTick <= from)
        {
            return;
        }

        auto& state = m_launch[track.Id];

        RenderOptions options{};
        options.Midi2 = info.SpeaksMidi2;
        options.LaneStepTicks = LaneStepTicks(m_fastestTempo, info.AutomationIntervalMilliseconds);
        options.Lyrics = true;

        // Lanes send where they are after a start, the loop going round, an edit made while
        // playing, or a mute ending. Otherwise they send only where they change.
        auto& editsSeen = m_editsSeen[track.Id];
        auto startValues = found == m_renderedUntil.end() || editsSeen != m_editCount || (audible && m_inaudible.contains(track.Id));
        editsSeen = m_editCount;

        if (audible)
        {
            m_inaudible.erase(track.Id);
        }
        else
        {
            m_inaudible.insert(track.Id);
        }

        std::vector<RenderedMessage> messages{};
        auto cursor = Unwrap(from);
        auto const end = Unwrap(horizonTick);

        // A launch or a stop that falls inside this stretch splits it: what played before, up to
        // that point, and what plays after, from it. A muted track still moves on, so unmuting it
        // doesn't play what was skipped.
        for (int guard = 0; cursor < end && guard < 8; ++guard)
        {
            auto stretchEnd = end;
            auto const switching = state.Pending && state.PendingAtUnwrapped < end;

            if (switching)
            {
                stretchEnd = std::max(cursor, state.PendingAtUnwrapped);
            }

            if (stretchEnd > cursor)
            {
                messages.clear();
                options.LaneStartValues = startValues;
                RenderMode(track, state, cursor, stretchEnd, options, messages);
                startValues = false;

                if (audible)
                {
                    SendRendered(track, info, switching ? Wrap(stretchEnd) : SegmentEnd(), now, messages);
                }
            }

            if (switching)
            {
                // Notes this track started in an earlier sweep end where the launch takes over too,
                // ahead of anything the new mode starts there.
                auto const switchAt = EarlierBy(TimestampAtTick(Wrap(stretchEnd)), info.OffsetMicroseconds);

                for (auto& note : m_sounding)
                {
                    if (note.TrackId == track.Id && !note.HandedOver && note.OffTimestamp > switchAt)
                    {
                        note.OffTimestamp = std::max(switchAt, note.OnTimestamp + 1);
                        m_output.Send(note.Endpoint, note.OffTimestamp, note.OffWords, note.WordCount);
                        ++m_counters.MessagesSent;
                        note.HandedOver = true;
                    }
                }

                state.Mode = state.PendingMode;
                state.ClipId = state.PendingClipId;
                state.Slot = state.PendingSlot;
                state.ClipStartUnwrapped = state.PendingAtUnwrapped;
                state.Pending = false;
                startValues = true;

                // Back on the timeline part way through: each channel gets what it would have by now.
                if (state.Mode == TrackPlayMode::Timeline && audible)
                {
                    SendChase(track, Wrap(stretchEnd), EarlierBy(TimestampAtTick(Wrap(stretchEnd)), info.OffsetMicroseconds));
                }
            }

            cursor = stretchEnd;
        }

        m_renderedUntil[track.Id] = horizonTick;
    }

    _Use_decl_annotations_
    void PlaybackEngine::RenderMode(Track const& track, TrackLaunch const& state, int64_t fromUnwrapped, int64_t toUnwrapped, RenderOptions const& options, std::vector<RenderedMessage>& messages)
    {
        switch (state.Mode)
        {
        case TrackPlayMode::Timeline:
            RenderTrack(*m_sequence, track, Wrap(fromUnwrapped), Wrap(toUnwrapped), messages, options);
            break;

        case TrackPlayMode::Clip:
        {
            auto const clip = FindClip(*m_sequence, state.ClipId);

            if (clip == nullptr)
            {
                break;
            }

            // A launched clip loops from where it was launched, on its own clock, whatever the
            // timeline's loop does. The lyrics follow the timeline, so they stay out of it.
            Track launched{};
            launched.Id = track.Id;
            launched.Destination = track.Destination;
            launched.Timeline.push_back(Placement{ state.ClipId, state.ClipStartUnwrapped, clip->Loop ? INT64_MAX / 4 : clip->Length });

            auto launchedOptions = options;
            launchedOptions.Lyrics = false;

            RenderTrack(*m_sequence, launched, fromUnwrapped, toUnwrapped, messages, launchedOptions);

            for (auto& message : messages)
            {
                message.Tick = Wrap(message.Tick);
            }

            break;
        }

        case TrackPlayMode::Stopped:
        default:
            break;
        }
    }

    _Use_decl_annotations_
    void PlaybackEngine::SendRendered(Track const& track, DestinationInfo const& info, int64_t clampOffsAt, uint64_t now, std::vector<RenderedMessage>& messages)
    {
        auto const limit = now + LookAheadTicks();
        auto const lead = static_cast<uint64_t>(info.OffsetMicroseconds) * m_clock.TicksPerSecond / 1000000;

        // Where each note started, by group, channel and note number, so its end can't be sent
        // ahead of its start.
        std::map<uint32_t, uint64_t> starts{};

        for (auto& message : messages)
        {
            // A note still sounding where the loop goes round, or where a launch takes over, ends there.
            if (message.Kind == RenderedKind::NoteOff && message.Tick > clampOffsAt)
            {
                message.Tick = clampOffsAt;
            }

            ApplyDestination(track.Destination, message.Words, message.WordCount);

            auto const timestamp = EarlierBy(TimestampAtTick(message.Tick), info.OffsetMicroseconds);
            auto const key = message.Words[0] & 0x0F0FFF00u;

            if (message.Kind == RenderedKind::NoteOff)
            {
                auto const start = starts.find(key);
                QueueNoteOff(track.Id, track.Destination.Endpoint, info.SpeaksMidi2, start != starts.end() ? start->second : 0,
                    timestamp, lead, message.Words.data(), message.WordCount, limit);
                continue;
            }

            if (message.Kind == RenderedKind::NoteOn)
            {
                starts[key] = timestamp;
            }

            SendTo(track.Destination.Endpoint, info.SpeaksMidi2, timestamp, message.Words.data(), message.WordCount);
        }
    }

    _Use_decl_annotations_
    void PlaybackEngine::QueueNoteOff(
        std::wstring const& trackId,
        EndpointRef const& endpoint,
        bool midi2,
        uint64_t onTimestamp,
        uint64_t offTimestamp,
        uint64_t lead,
        uint32_t const* words,
        uint8_t wordCount,
        uint64_t limit)
    {
        if (endpoint.IsEmpty() || wordCount == 0)
        {
            return;
        }

        auto const translated = midi2 ? TranslateToMidi2(words, wordCount) : TranslateToMidi1(words, wordCount);

        if (translated.Count == 0)
        {
            return;
        }

        SoundingNote note{};
        note.TrackId = trackId;
        note.Endpoint = endpoint;
        note.OffWords[0] = translated.Messages[0][0];
        note.OffWords[1] = translated.Messages[0][1];
        note.WordCount = translated.WordCounts[0];
        note.OnTimestamp = onTimestamp;
        note.OffTimestamp = std::max(offTimestamp, onTimestamp + 1);
        note.Lead = lead;

        if (note.OffTimestamp <= limit)
        {
            m_output.Send(note.Endpoint, note.OffTimestamp, note.OffWords, note.WordCount);
            ++m_counters.MessagesSent;
            note.HandedOver = true;
        }

        m_sounding.push_back(std::move(note));
    }

    _Use_decl_annotations_
    void PlaybackEngine::SendDueNoteOffs(uint64_t limit)
    {
        for (auto& note : m_sounding)
        {
            if (!note.HandedOver && note.OffTimestamp <= limit)
            {
                m_output.Send(note.Endpoint, note.OffTimestamp, note.OffWords, note.WordCount);
                ++m_counters.MessagesSent;
                note.HandedOver = true;
            }
        }
    }

    uint64_t PlaybackEngine::LookAheadTicks() const noexcept
    {
        return static_cast<uint64_t>(m_settings.LookAheadMicroseconds) * m_clock.TicksPerSecond / 1000000;
    }

    _Use_decl_annotations_
    void PlaybackEngine::SweepMetronome(uint64_t now)
    {
        auto const& metronome = m_settings.Metronome;

        if (!metronome.Enabled || metronome.Endpoint.IsEmpty())
        {
            return;
        }

        auto const info = LookupDestination(metronome.Endpoint, metronome.Group);
        auto const lead = static_cast<uint64_t>(info.OffsetMicroseconds) * m_clock.TicksPerSecond / 1000000;
        auto const horizon = PlanUntil(now + (static_cast<uint64_t>(m_settings.LookAheadMicroseconds) + info.OffsetMicroseconds) * m_clock.TicksPerSecond / 1000000);
        auto const horizonTick = std::min(TickAtTimestamp(horizon), SegmentEnd());
        auto const& meter = m_sequence->Meter;

        // The first beat at or after where the metronome got to.
        auto const position = BarPositionAtTick(meter, m_metronomeUntil);
        auto tick = m_metronomeUntil - position.TicksIntoBeat;

        if (position.TicksIntoBeat > 0)
        {
            tick += TicksPerBeat(MeterAtTick(meter, tick));
        }

        while (tick < horizonTick)
        {
            auto const beat = BarPositionAtTick(meter, tick);

            Note click{};
            click.Channel = metronome.Channel;
            click.Number = metronome.Note;
            click.Velocity = beat.Beat == 1 ? metronome.FirstBeatVelocity : metronome.OtherBeatVelocity;

            uint32_t words[2]{};
            BuildNoteOn(click, metronome.Group, words);
            auto const onAt = EarlierBy(TimestampAtTick(tick), info.OffsetMicroseconds);
            SendTo(metronome.Endpoint, info.SpeaksMidi2, onAt, words, 2);

            BuildNoteOff(click, metronome.Group, words);
            QueueNoteOff(std::wstring{}, metronome.Endpoint, info.SpeaksMidi2, onAt,
                EarlierBy(TimestampAtTick(tick + ClickTicks), info.OffsetMicroseconds), lead, words, 2, now + LookAheadTicks());

            tick += TicksPerBeat(MeterAtTick(meter, tick));
        }

        m_metronomeUntil = std::max(m_metronomeUntil, horizonTick);
    }

    _Use_decl_annotations_
    void PlaybackEngine::SweepClockOutputs(uint64_t now)
    {
        if (m_settings.ClockOutputs.empty())
        {
            return;
        }

        // Far enough ahead for the output that wants its pulses earliest.
        std::vector<int64_t> leads{};
        int64_t longest{ 0 };

        for (auto const& output : m_settings.ClockOutputs)
        {
            leads.push_back(ClockLeadMicroseconds(output));
            longest = std::max(longest, leads.back());
        }

        auto const horizon = PlanUntil(now + (static_cast<uint64_t>(m_settings.LookAheadMicroseconds) + static_cast<uint64_t>(longest)) * m_clock.TicksPerSecond / 1000000);
        auto const horizonTick = std::min(TickAtTimestamp(horizon), SegmentEnd());

        SendTempoChanges(m_clockUntil, horizonTick, leads);

        auto tick = ((m_clockUntil + TicksPerClock - 1) / TicksPerClock) * TicksPerClock;

        for (; tick < horizonTick; tick += TicksPerClock)
        {
            auto const base = TimestampAtTick(tick);

            for (size_t i = 0; i < m_settings.ClockOutputs.size(); ++i)
            {
                auto const& output = m_settings.ClockOutputs[i];

                if (output.SendClock)
                {
                    auto const pulse = SystemWord(output.Group, 0xF8);
                    m_output.Send(output.Endpoint, EarlierBy(base, leads[i]), &pulse, 1);
                }
            }
        }

        m_clockUntil = std::max(m_clockUntil, horizonTick);
    }

    // The tempo where a stretch starts, then each change in it, then once a beat through a ramp.
    _Use_decl_annotations_
    void PlaybackEngine::SendTempoChanges(int64_t fromTick, int64_t toTick, std::vector<int64_t> const& leads)
    {
        std::vector<size_t> outputs{};

        for (size_t i = 0; i < m_settings.ClockOutputs.size(); ++i)
        {
            auto const& output = m_settings.ClockOutputs[i];

            if (output.SendTempo && LookupDestination(output.Endpoint, output.Group).SpeaksMidi2)
            {
                outputs.push_back(i);
            }
        }

        if (outputs.empty() || m_sequence == nullptr)
        {
            return;
        }

        auto const send = [&](int64_t tick, double bpm)
        {
            if (!std::isnan(m_tempoSent) && std::abs(bpm - m_tempoSent) < 0.005)
            {
                return;
            }

            m_tempoSent = bpm;
            auto const base = TimestampAtTick(tick);

            for (auto const i : outputs)
            {
                uint32_t words[4]{};
                BuildSetTempo(bpm, m_settings.ClockOutputs[i].Group, words);

                // Ahead of the pulse at the same moment, so the pulses are read at the new tempo.
                m_output.Send(m_settings.ClockOutputs[i].Endpoint, EarlierBy(base, leads[i]) - 1, words, 4);
            }
        };

        if (m_following)
        {
            // A followed clock's pace wavers a little: only a real change is passed on.
            auto const bpm = m_followTicksPerSecond * 60.0 / static_cast<double>(TicksPerQuarterNote);

            if (bpm > 0 && (std::isnan(m_tempoSent) || std::abs(bpm - m_tempoSent) >= 0.25))
            {
                send(fromTick, std::round(bpm * 100.0) / 100.0);
            }

            return;
        }

        if (fromTick == m_segment.StartTick)
        {
            send(fromTick, m_tempo.BeatsPerMinuteAtTick(fromTick));
        }

        if (toTick <= fromTick)
        {
            return;
        }

        auto const& points = m_sequence->Tempo;

        for (size_t i = 0; i < points.size(); ++i)
        {
            auto const& point = points[i];

            if (point.Tick >= toTick)
            {
                break;
            }

            if (point.Tick >= fromTick)
            {
                send(point.Tick, point.BeatsPerMinute);
            }

            if (point.RampToNext && i + 1 < points.size())
            {
                auto const rampEnd = std::min(points[i + 1].Tick, toTick);
                auto const first = std::max(fromTick, point.Tick + 1);
                auto beat = point.Tick + ((first - point.Tick + TicksPerQuarterNote - 1) / TicksPerQuarterNote) * TicksPerQuarterNote;

                for (; beat < rampEnd; beat += TicksPerQuarterNote)
                {
                    send(beat, m_tempo.BeatsPerMinuteAtTick(beat));
                }
            }
        }
    }

    // Tells each time code output where playback is at this moment, with a full frame message,
    // and carries on from the next whole group of eight quarter frames.
    _Use_decl_annotations_
    void PlaybackEngine::LocateTimeCode(uint64_t timestamp)
    {
        auto const& outputs = m_settings.TimeCodeOutputs;
        m_timeCodeNext.assign(outputs.size(), 0);

        auto const seconds = std::max({ 0.0, m_segment.StartSeconds, SecondsAtTimestamp(timestamp) });

        for (size_t i = 0; i < outputs.size(); ++i)
        {
            auto const quarter = midiapp::SecondsPerFrame(outputs[i].Rate) / midiapp::MidiTimeCodeQuarterFramesPerFrame;

            // A receiver puts a time code together from eight quarter frames, so they always
            // start a group of eight on the same frames, counted from the top of the sequence.
            constexpr uint64_t pieces = midiapp::MidiTimeCodePiecesPerSequence;
            auto const next = static_cast<uint64_t>(std::ceil(seconds / quarter - 1e-6));
            m_timeCodeNext[i] = (next + pieces - 1) / pieces * pieces;

            // Just ahead of the first quarter frame, so the receiver knows where it is first.
            SendFullFrame(outputs[i], EarlierBy(timestamp, TimeCodeLeadMicroseconds(outputs[i])) - 1, seconds);
        }
    }

    _Use_decl_annotations_
    void PlaybackEngine::SendFullFrame(TimeCodeOutput const& output, uint64_t timestamp, double seconds)
    {
        auto const frames = static_cast<uint64_t>(std::floor(std::max(0.0, seconds) / midiapp::SecondsPerFrame(output.Rate) + 1e-6));
        auto const position = midiapp::PositionAfterFrames(output.Start, output.Rate, frames);

        std::array<uint8_t, midiapp::MidiTimeCodeFullFramePayloadSize> payload{};
        midiapp::FillFullFramePayload(position, output.Rate, payload.data());

        // Eight bytes don't fit one system exclusive packet: a start with six, then an end with two.
        auto const group = static_cast<uint32_t>(output.Group & 0x0F) << 24;

        uint32_t const first[2]{
            0x30160000u | group | (static_cast<uint32_t>(payload[0]) << 8) | payload[1],
            (static_cast<uint32_t>(payload[2]) << 24) | (static_cast<uint32_t>(payload[3]) << 16) | (static_cast<uint32_t>(payload[4]) << 8) | payload[5] };

        uint32_t const second[2]{
            0x30320000u | group | (static_cast<uint32_t>(payload[6]) << 8) | payload[7],
            0 };

        m_output.Send(output.Endpoint, timestamp, first, 2);
        m_output.Send(output.Endpoint, timestamp, second, 2);
    }

    _Use_decl_annotations_
    void PlaybackEngine::SweepTimeCodeOutputs(uint64_t now)
    {
        auto const& outputs = m_settings.TimeCodeOutputs;

        if (outputs.empty())
        {
            m_timeCodeNext.clear();
            return;
        }

        auto const from = std::max(now, m_segment.StartTimestamp);
        auto const horizonFor = [this, now](TimeCodeOutput const& output)
        {
            auto const lead = std::max<int64_t>(TimeCodeLeadMicroseconds(output), 0);
            return SecondsAtTimestamp(PlanUntil(now + (static_cast<uint64_t>(m_settings.LookAheadMicroseconds) + static_cast<uint64_t>(lead)) * m_clock.TicksPerSecond / 1000000));
        };

        // The outputs changed, or the time on the wall jumped (a tempo change before the
        // playhead). Each receiver is told where it is now, rather than rushed or stalled to
        // catch up.
        auto relocate = m_timeCodeNext.size() != outputs.size();

        for (size_t i = 0; i < outputs.size() && !relocate; ++i)
        {
            auto const frame = midiapp::SecondsPerFrame(outputs[i].Rate);
            auto const next = static_cast<double>(m_timeCodeNext[i]) * frame / midiapp::MidiTimeCodeQuarterFramesPerFrame;

            relocate = next + frame < m_segment.StartSeconds || next > std::max(horizonFor(outputs[i]), m_segment.StartSeconds) + frame * 3;
        }

        if (relocate)
        {
            LocateTimeCode(from);
        }

        auto const end = SegmentEnd();
        auto const endSeconds = end == INT64_MAX ? std::numeric_limits<double>::infinity() : m_tempo.SecondsAtTick(end);
        auto const ticksPerSecond = static_cast<double>(m_clock.TicksPerSecond);

        for (size_t i = 0; i < outputs.size(); ++i)
        {
            auto const& output = outputs[i];
            auto const lead = TimeCodeLeadMicroseconds(output);
            auto const quarter = midiapp::SecondsPerFrame(output.Rate) / midiapp::MidiTimeCodeQuarterFramesPerFrame;
            auto const limit = std::min(horizonFor(output), endSeconds);
            auto& next = m_timeCodeNext[i];

            for (; static_cast<double>(next) * quarter < limit; ++next)
            {
                // Each group of eight spells out the frame it started on.
                auto const piece = static_cast<uint8_t>(next % midiapp::MidiTimeCodePiecesPerSequence);
                auto const position = midiapp::PositionAfterFrames(output.Start, output.Rate, (next - piece) / midiapp::MidiTimeCodeQuarterFramesPerFrame);
                auto const word = SystemWord(output.Group, 0xF1, midiapp::QuarterFrameDataByte(position, output.Rate, piece));
                auto const offset = std::max(0.0, static_cast<double>(next) * quarter - m_segment.StartSeconds) * ticksPerSecond;

                m_output.Send(output.Endpoint, EarlierBy(m_segment.StartTimestamp + static_cast<uint64_t>(std::llround(offset)), lead), &word, 1);
            }
        }
    }

    void PlaybackEngine::Sweep()
    {
        std::scoped_lock guard{ m_lock };

        if (!m_playing || m_sequence == nullptr || !m_clock.Now)
        {
            return;
        }

        ++m_counters.Sweeps;

        auto const now = m_clock.Now();

        // The followed clock has gone quiet: stop, rather than run on at a guess.
        if (m_following && m_followRunning && now > m_followLastPulse + FollowPatience())
        {
            m_followRunning = false;
            StopLocked();
            return;
        }

        WrapLoopIfDue(now);

        bool anySolo{ false };

        ForEachTrack(*m_sequence, [&anySolo](Track const& track, size_t)
        {
            anySolo = anySolo || track.Soloed;
            return !anySolo;
        });

        // Ends now due go first, so each lands ahead of a new note of the same pitch at its time.
        SendDueNoteOffs(now + LookAheadTicks());

        SweepTracks(m_sequence->Tracks, now, anySolo, false, false);
        SweepMetronome(now);
        SweepClockOutputs(now);
        SweepTimeCodeOutputs(now);

        // Notes that have ended don't need ending again.
        std::erase_if(m_sounding, [now](SoundingNote const& note) { return note.HandedOver && note.OffTimestamp <= now; });
    }

    _Use_decl_annotations_
    void PlaybackEngine::WrapLoopIfDue(uint64_t now)
    {
        for (int guard = 0; guard < 16; ++guard)
        {
            if (!m_settings.LoopEnabled || m_following || m_segment.StartTick >= m_settings.LoopEnd)
            {
                return;
            }

            // The furthest ahead anything is handed over.
            auto const offset = LongestLeadMicroseconds();

            auto const loopEndTimestamp = TimestampAtTick(m_settings.LoopEnd);
            auto const horizon = now + (static_cast<uint64_t>(m_settings.LookAheadMicroseconds) + offset) * m_clock.TicksPerSecond / 1000000;

            if (horizon < loopEndTimestamp)
            {
                return;
            }

            // Everything up to the loop's end goes out first, then playback carries on from its start.
            bool anySolo{ false };

            ForEachTrack(*m_sequence, [&anySolo](Track const& track, size_t)
            {
                anySolo = anySolo || track.Soloed;
                return !anySolo;
            });

            SendDueNoteOffs(loopEndTimestamp + LookAheadTicks());
            SweepTracks(m_sequence->Tracks, loopEndTimestamp, anySolo, false, false);
            SweepMetronome(loopEndTimestamp);
            SweepClockOutputs(loopEndTimestamp);
            SweepTimeCodeOutputs(loopEndTimestamp);

            // A note whose end is still waiting, such as one started before the loop was turned
            // on, ends at the loop's end too.
            for (auto& note : m_sounding)
            {
                if (!note.HandedOver)
                {
                    auto const end = loopEndTimestamp > note.Lead ? loopEndTimestamp - note.Lead : 1;
                    note.OffTimestamp = std::max(std::min(note.OffTimestamp, end), note.OnTimestamp + 1);
                }
            }

            SendDueNoteOffs(loopEndTimestamp);

            m_history.push_back(m_segment);

            if (m_history.size() > 8)
            {
                m_history.erase(m_history.begin());
            }

            Segment next{};
            next.StartTick = m_settings.LoopStart;
            next.StartTimestamp = loopEndTimestamp;
            next.StartSeconds = m_tempo.SecondsAtTick(m_settings.LoopStart);
            next.UnwrappedStart = m_segment.UnwrappedStart + (m_settings.LoopEnd - m_segment.StartTick);
            m_segment = next;

            m_renderedUntil.clear();
            m_metronomeUntil = m_settings.LoopStart;
            m_clockUntil = m_settings.LoopStart;

            // A device following the clock is told where the loop went back to, before its next
            // pulse. Start and stop aren't sent: those would make it start again from the top.
            for (auto const& output : m_settings.ClockOutputs)
            {
                if (output.SendSongPosition)
                {
                    auto const position = std::min<int64_t>(m_settings.LoopStart / TicksPerSongPositionBeat, 0x3FFF);
                    auto const pointer = SystemWord(output.Group, 0xF2, static_cast<uint8_t>(position & 0x7F), static_cast<uint8_t>(position >> 7));
                    m_output.Send(output.Endpoint, EarlierBy(loopEndTimestamp, ClockLeadMicroseconds(output)) - 1, &pointer, 1);
                }
            }

            // Time code goes back with the music.
            LocateTimeCode(loopEndTimestamp);

            ++m_counters.LoopPasses;
        }
    }

    int64_t PlaybackEngine::FurthestRendered() const noexcept
    {
        auto furthest = m_segment.StartTick;

        if (m_playing && m_clock.Now)
        {
            auto const ahead = m_clock.Now() + static_cast<uint64_t>(m_settings.LookAheadMicroseconds) * m_clock.TicksPerSecond / 1000000;
            furthest = std::max(furthest, TickAtTimestamp(ahead));
        }

        for (auto const& [id, until] : m_renderedUntil)
        {
            furthest = std::max(furthest, until);
        }

        return furthest;
    }

    _Use_decl_annotations_
    int64_t PlaybackEngine::QuantizePoint(LaunchQuantize quantize) const
    {
        // Nothing already handed over can change, so the earliest a launch can land is just past it.
        auto const earliest = m_playing ? FurthestRendered() : m_segment.StartTick;
        auto point = earliest;

        if (quantize < 0 && m_sequence != nullptr)
        {
            auto const bars = -quantize;
            auto const position = BarPositionAtTick(m_sequence->Meter, earliest);
            auto const onBar = position.Beat == 1 && position.TicksIntoBeat == 0;
            auto bar = position.Bar + (onBar ? 0 : 1);

            if (bars > 1)
            {
                bar = ((bar - 1 + bars - 1) / bars) * bars + 1;
            }

            point = TickAtBar(m_sequence->Meter, bar);
        }
        else if (quantize > 0)
        {
            point = ((earliest + quantize - 1) / quantize) * quantize;
        }

        // Past the loop's end, the next thing that plays is the loop's start.
        if (point >= SegmentEnd())
        {
            point = SegmentEnd();
        }

        return Unwrap(point);
    }

    _Use_decl_annotations_
    void PlaybackEngine::QueueLaunch(std::wstring const& trackId, TrackPlayMode mode, std::wstring const& clipId, LaunchQuantize quantize, int32_t slot)
    {
        auto& state = m_launch[trackId];

        if (!m_playing)
        {
            state.Mode = mode;
            state.ClipId = clipId;
            state.Slot = slot;
            state.ClipStartUnwrapped = m_segment.StartTick;
            state.Pending = false;
            return;
        }

        state.Pending = true;
        state.PendingMode = mode;
        state.PendingClipId = clipId;
        state.PendingSlot = slot;
        state.PendingAtUnwrapped = QuantizePoint(quantize);
    }

    _Use_decl_annotations_
    void PlaybackEngine::LaunchClip(std::wstring const& trackId, std::wstring const& clipId, LaunchQuantize quantize, int32_t slot)
    {
        int64_t startFrom{ -1 };

        {
            std::scoped_lock guard{ m_lock };

            if (m_sequence == nullptr || trackId.empty() || clipId.empty())
            {
                return;
            }

            if (!m_playing && !m_following)
            {
                // Launching while stopped starts playback, with this clip from its beginning.
                startFrom = m_segment.StartTick;
            }

            QueueLaunch(trackId, TrackPlayMode::Clip, clipId, quantize, slot);
        }

        if (startFrom >= 0)
        {
            Play(startFrom);
        }
    }

    _Use_decl_annotations_
    void PlaybackEngine::StopTrack(std::wstring const& trackId, LaunchQuantize quantize)
    {
        std::scoped_lock guard{ m_lock };

        if (!trackId.empty())
        {
            QueueLaunch(trackId, TrackPlayMode::Stopped, std::wstring{}, quantize, -1);
        }
    }

    _Use_decl_annotations_
    void PlaybackEngine::ReturnToTimeline(std::wstring const& trackId, LaunchQuantize quantize)
    {
        std::scoped_lock guard{ m_lock };

        if (!trackId.empty())
        {
            QueueLaunch(trackId, TrackPlayMode::Timeline, std::wstring{}, quantize, -1);
            return;
        }

        std::vector<std::wstring> ids{};

        for (auto const& [id, state] : m_launch)
        {
            if (state.Mode != TrackPlayMode::Timeline || state.Pending)
            {
                ids.push_back(id);
            }
        }

        for (auto const& id : ids)
        {
            QueueLaunch(id, TrackPlayMode::Timeline, std::wstring{}, quantize, -1);
        }
    }

    _Use_decl_annotations_
    void PlaybackEngine::QueueLaunchAt(std::wstring const& trackId, TrackPlayMode mode, std::wstring const& clipId, int64_t unwrappedTick, int32_t slot)
    {
        if (!m_playing)
        {
            QueueLaunch(trackId, mode, clipId, 0, slot);
            return;
        }

        auto& state = m_launch[trackId];
        state.Pending = true;
        state.PendingMode = mode;
        state.PendingClipId = clipId;
        state.PendingSlot = slot;
        state.PendingAtUnwrapped = std::max(unwrappedTick, Unwrap(FurthestRendered()));
    }

    _Use_decl_annotations_
    int64_t PlaybackEngine::NextLaunchPoint(LaunchQuantize quantize) const
    {
        std::scoped_lock guard{ m_lock };
        return QuantizePoint(quantize);
    }

    _Use_decl_annotations_
    void PlaybackEngine::LaunchClipAt(std::wstring const& trackId, std::wstring const& clipId, int64_t unwrappedTick, int32_t slot)
    {
        std::scoped_lock guard{ m_lock };

        if (!trackId.empty() && !clipId.empty())
        {
            QueueLaunchAt(trackId, TrackPlayMode::Clip, clipId, unwrappedTick, slot);
        }
    }

    _Use_decl_annotations_
    void PlaybackEngine::StopTrackAt(std::wstring const& trackId, int64_t unwrappedTick)
    {
        std::scoped_lock guard{ m_lock };

        if (!trackId.empty())
        {
            QueueLaunchAt(trackId, TrackPlayMode::Stopped, std::wstring{}, unwrappedTick, -1);
        }
    }

    _Use_decl_annotations_
    int64_t PlaybackEngine::UnwrappedTickAtTime(uint64_t timestamp) const
    {
        std::scoped_lock guard{ m_lock };

        if (!m_playing)
        {
            return -1;
        }

        // The stretch of playback the moment fell in. Unlike TickAtTime, a moment just before a
        // stretch began isn't held at its start: unwrapped ticks run on across a loop.
        auto segment = &m_segment;

        if (timestamp < m_segment.StartTimestamp)
        {
            for (auto it = m_history.rbegin(); it != m_history.rend(); ++it)
            {
                if (timestamp >= it->StartTimestamp)
                {
                    segment = &*it;
                    break;
                }
            }
        }

        auto const elapsed = timestamp >= segment->StartTimestamp
            ? static_cast<double>(timestamp - segment->StartTimestamp)
            : -static_cast<double>(segment->StartTimestamp - timestamp);

        auto const tick = m_tempo.TickAtSeconds(segment->StartSeconds + elapsed / static_cast<double>(m_clock.TicksPerSecond));
        return segment->UnwrappedStart + (tick - segment->StartTick);
    }

    // ---------------------------------------------------------------- following another clock

    _Use_decl_annotations_
    void PlaybackEngine::SetFollowing(bool following)
    {
        std::scoped_lock guard{ m_lock };

        if (m_following == following)
        {
            return;
        }

        // The music can't change whose clock it keeps to part way through.
        StopLocked();

        m_following = following;
        m_followPace = midiapp::ClockFollower{};
        m_followRunning = false;
        m_followNextPulse = 0;
        m_followLastPulse = 0;
        m_followLastHeard = 0;
        m_followTicksPerSecond = 0;
    }

    bool PlaybackEngine::IsFollowing() const
    {
        std::scoped_lock guard{ m_lock };
        return m_following;
    }

    _Use_decl_annotations_
    void PlaybackEngine::FollowPulse(uint64_t timestamp)
    {
        auto first{ false };

        {
            std::scoped_lock guard{ m_lock };

            if (!m_following)
            {
                return;
            }

            m_followPace.Pulse(timestamp);
            m_followLastHeard = timestamp;

            if (!m_followRunning)
            {
                return;
            }

            auto const tick = static_cast<int64_t>(m_followNextPulse++) * TicksPerClock;
            m_followLastPulse = timestamp;

            if (!m_playing)
            {
                return;
            }

            // The music is exactly here at this pulse, and moves on at the pulses' pace. The time
            // on the wall carries on, so time code doesn't jump.
            auto const seconds = SecondsAtTimestamp(timestamp);

            m_followTicksPerSecond = static_cast<double>(TicksPerClock) * static_cast<double>(m_clock.TicksPerSecond) / static_cast<double>(FollowedPulseInterval(tick));
            m_segment.StartTick = tick;
            m_segment.StartTimestamp = timestamp;
            m_segment.StartSeconds = seconds;
            m_segment.UnwrappedStart = tick;
            m_history.clear();

            first = !m_followAnchored;
            m_followAnchored = true;
        }

        // The first pulse says where the music starts: what's due there goes now, not at the next sweep.
        if (first)
        {
            Sweep();
        }
    }

    _Use_decl_annotations_
    void PlaybackEngine::FollowStart(uint64_t timestamp)
    {
        {
            std::scoped_lock guard{ m_lock };

            if (!m_following)
            {
                return;
            }

            m_followRunning = true;
            m_followNextPulse = 0;

            if (!StartFollowedLocked(0, timestamp, false))
            {
                return;
            }
        }

        Sweep();
    }

    _Use_decl_annotations_
    void PlaybackEngine::FollowContinue(uint64_t timestamp)
    {
        {
            std::scoped_lock guard{ m_lock };

            if (!m_following || (m_followRunning && m_playing))
            {
                return;
            }

            m_followRunning = true;

            if (!StartFollowedLocked(static_cast<int64_t>(m_followNextPulse) * TicksPerClock, timestamp, false))
            {
                return;
            }
        }

        Sweep();
    }

    _Use_decl_annotations_
    void PlaybackEngine::FollowStop(uint64_t)
    {
        std::scoped_lock guard{ m_lock };

        if (m_following)
        {
            m_followRunning = false;
            StopLocked();
        }
    }

    _Use_decl_annotations_
    void PlaybackEngine::FollowSongPosition(uint64_t timestamp, uint32_t sixteenths)
    {
        auto jumped = false;

        {
            std::scoped_lock guard{ m_lock };

            if (!m_following)
            {
                return;
            }

            auto const position = std::min<uint32_t>(sixteenths, 0x3FFF);
            auto const tick = static_cast<int64_t>(position) * TicksPerSongPositionBeat;
            m_followNextPulse = static_cast<uint64_t>(position) * midiapp::MidiClocksPerSongPositionStep;

            if (m_playing && m_followRunning)
            {
                // Somewhere else while playing, such as another sequencer's loop going round.
                jumped = StartFollowedLocked(tick, timestamp, true);
            }
            else if (!m_playing)
            {
                m_segment.StartTick = tick;
                m_segment.UnwrappedStart = tick;
            }
        }

        if (jumped)
        {
            Sweep();
        }
    }

    double PlaybackEngine::FollowedBeatsPerMinute() const
    {
        std::scoped_lock guard{ m_lock };

        auto const interval = m_followPace.PulseInterval();

        if (!m_following || interval == 0 || !m_clock.Now || m_clock.Now() > m_followLastHeard + FollowPatience())
        {
            return 0;
        }

        return 60.0 * static_cast<double>(m_clock.TicksPerSecond) / (static_cast<double>(interval) * midiapp::MidiClocksPerBeat);
    }

    // The first pulse after Start or Continue is where the music starts, and the one after a song
    // position is where it says. It hasn't come yet, so the music starts where it's due: a pulse
    // after the last one, or a pulse after this message when the clock wasn't already running.
    _Use_decl_annotations_
    bool PlaybackEngine::StartFollowedLocked(int64_t tick, uint64_t timestamp, bool jumping)
    {
        auto const known = m_followPace.PulseInterval() > 0;
        auto const interval = FollowedPulseInterval(tick);
        auto const next = m_followLastHeard != 0 && m_followLastHeard + interval > timestamp ? m_followLastHeard + interval : timestamp + interval;

        m_followTicksPerSecond = static_cast<double>(TicksPerClock) * static_cast<double>(m_clock.TicksPerSecond) / static_cast<double>(interval);
        m_followAnchored = false;

        // Without the pace, a guess would put the downbeat in the wrong place: nothing is planned
        // until the first pulse says where it is.
        m_followLastPulse = known ? next - interval : timestamp - std::min(timestamp, 2 * interval);

        return StartLocked(tick, next, jumping);
    }

    // The followed clock's pace, in clock ticks a pulse. Until two pulses have come, the
    // sequence's own tempo is the best guess.
    _Use_decl_annotations_
    uint64_t PlaybackEngine::FollowedPulseInterval(int64_t tick) const noexcept
    {
        if (auto const measured = m_followPace.PulseInterval(); measured > 0)
        {
            return measured;
        }

        auto const bpm = std::max(1.0, m_tempo.BeatsPerMinuteAtTick(std::max<int64_t>(0, tick)));
        return std::max<uint64_t>(1, static_cast<uint64_t>(std::llround(60.0 * static_cast<double>(m_clock.TicksPerSecond) / (bpm * midiapp::MidiClocksPerBeat))));
    }

    // How long the followed clock can go quiet before playback stops: eight pulses, and at least
    // three tenths of a second.
    uint64_t PlaybackEngine::FollowPatience() const noexcept
    {
        return std::max(m_followPace.PulseInterval() * 8, m_clock.TicksPerSecond * 3 / 10);
    }

    // Following, nothing is planned past the pulse after next: a pulse that comes late, or at a
    // new pace, can only have put that much in the wrong place.
    _Use_decl_annotations_
    uint64_t PlaybackEngine::PlanUntil(uint64_t horizon) const noexcept
    {
        if (!m_following)
        {
            return horizon;
        }

        return std::min(horizon, m_followLastPulse + 2 * FollowedPulseInterval(m_segment.StartTick));
    }

    std::vector<TrackLaunchView> PlaybackEngine::LaunchState() const
    {
        std::scoped_lock guard{ m_lock };

        std::vector<TrackLaunchView> views{};

        auto const nowUnwrapped = m_playing && m_clock.Now ? Unwrap(TickAtTimestamp(m_clock.Now())) : m_segment.UnwrappedStart;

        for (auto const& [id, state] : m_launch)
        {
            if (state.Mode == TrackPlayMode::Timeline && !state.Pending)
            {
                continue;
            }

            TrackLaunchView view{};
            view.TrackId = id;
            view.Mode = state.Mode;
            view.ClipId = state.ClipId;
            view.Slot = state.Slot;
            view.Pending = state.Pending;
            view.PendingMode = state.PendingMode;
            view.PendingClipId = state.PendingClipId;
            view.PendingSlot = state.PendingSlot;
            view.PendingTick = Wrap(state.PendingAtUnwrapped);

            if (state.Mode == TrackPlayMode::Clip && m_sequence != nullptr && m_playing)
            {
                if (auto const clip = FindClip(*m_sequence, state.ClipId); clip != nullptr && clip->Length > 0)
                {
                    auto const into = nowUnwrapped - state.ClipStartUnwrapped;

                    if (into >= 0)
                    {
                        view.Progress = clip->Loop || into < clip->Length
                            ? static_cast<double>(into % clip->Length) / static_cast<double>(clip->Length)
                            : 1.0;
                        view.Pass = static_cast<uint64_t>(into / clip->Length);
                    }
                }
            }

            views.push_back(std::move(view));
        }

        return views;
    }

    void PlaybackEngine::StartThread()
    {
        if (m_threadRunning.exchange(true))
        {
            return;
        }

        m_thread = std::thread([this]() { ThreadMain(); });
    }

    void PlaybackEngine::StopThread() noexcept
    {
        if (!m_threadRunning.exchange(false))
        {
            return;
        }

        if (m_thread.joinable())
        {
            try
            {
                m_thread.join();
            }
            catch (...)
            {
                // Only possible if this were called from the engine's own thread, which it isn't.
            }
        }
    }

    void PlaybackEngine::ThreadMain() noexcept
    {
        // A high resolution timer wakes about half a millisecond late, where Sleep can be two.
        auto const timer = ::CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);

        while (m_threadRunning.load())
        {
            try
            {
                Sweep();
            }
            catch (...)
            {
                // An exception must never leave this thread. The next sweep tries again.
            }

            uint32_t sweep{ 5000 };

            {
                std::scoped_lock guard{ m_lock };
                sweep = m_settings.SweepMicroseconds;
            }

            if (timer != nullptr)
            {
                LARGE_INTEGER due{};
                due.QuadPart = -static_cast<LONGLONG>(sweep) * 10;
                ::SetWaitableTimerEx(timer, &due, 0, nullptr, nullptr, nullptr, 0);
                ::WaitForSingleObject(timer, 100);
            }
            else
            {
                ::Sleep(std::max<DWORD>(1, sweep / 1000));
            }
        }

        if (timer != nullptr)
        {
            ::CloseHandle(timer);
        }
    }

    EngineCounters PlaybackEngine::Counters() const
    {
        std::scoped_lock guard{ m_lock };
        return m_counters;
    }
}
