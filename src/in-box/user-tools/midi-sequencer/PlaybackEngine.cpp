// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "PlaybackEngine.h"

#include "MessageTranslation.h"
#include "SequenceRender.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
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

            m_sequence = std::move(sequence);
            m_tempo = TempoMap{ m_sequence->Tempo };
            m_startTick = tick;
            m_startTimestamp = now;
            m_startSeconds = m_tempo.SecondsAtTick(tick);
            return;
        }

        m_sequence = std::move(sequence);
        m_tempo = TempoMap{ m_sequence->Tempo };
    }

    _Use_decl_annotations_
    void PlaybackEngine::SetSettings(EngineSettings const& settings)
    {
        std::scoped_lock guard{ m_lock };
        m_settings = settings;
        m_settings.LookAheadMicroseconds = std::clamp<uint32_t>(m_settings.LookAheadMicroseconds, 2000, 1000000);
        m_settings.SweepMicroseconds = std::clamp<uint32_t>(m_settings.SweepMicroseconds, 1000, 50000);
    }

    _Use_decl_annotations_
    void PlaybackEngine::SetDestinationLookup(std::function<DestinationInfo(EndpointRef const&)> lookup)
    {
        std::scoped_lock guard{ m_lock };
        m_lookup = std::move(lookup);
    }

    _Use_decl_annotations_
    uint64_t PlaybackEngine::TimestampAtTick(int64_t tick) const noexcept
    {
        auto const seconds = m_tempo.SecondsAtTick(tick) - m_startSeconds;
        auto const offset = std::llround(seconds * static_cast<double>(m_clock.TicksPerSecond));

        if (offset < 0)
        {
            auto const back = static_cast<uint64_t>(-offset);
            return back >= m_startTimestamp ? 1 : m_startTimestamp - back;
        }

        return m_startTimestamp + static_cast<uint64_t>(offset);
    }

    _Use_decl_annotations_
    int64_t PlaybackEngine::TickAtTimestamp(uint64_t timestamp) const noexcept
    {
        auto const elapsed = timestamp >= m_startTimestamp
            ? static_cast<double>(timestamp - m_startTimestamp)
            : -static_cast<double>(m_startTimestamp - timestamp);

        return m_tempo.TickAtSeconds(m_startSeconds + elapsed / static_cast<double>(m_clock.TicksPerSecond));
    }

    _Use_decl_annotations_
    DestinationInfo PlaybackEngine::LookupDestination(EndpointRef const& endpoint) const
    {
        return m_lookup ? m_lookup(endpoint) : DestinationInfo{};
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
        auto const info = LookupDestination(track.Destination.Endpoint);

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

        std::vector<RenderedMessage> earlier{};
        RenderTrack(*m_sequence, track, 0, fromTick, earlier);

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

        auto const info = LookupDestination(track.Destination.Endpoint);

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

            if (m_sequence == nullptr || !m_clock.Now)
            {
                return;
            }

            if (m_playing)
            {
                SendSilence(0);
            }

            auto const now = m_clock.Now();

            m_startTick = std::max<int64_t>(0, fromTick);
            m_startTimestamp = now + StartLeadMicroseconds * m_clock.TicksPerSecond / 1000000;
            m_startSeconds = m_tempo.SecondsAtTick(m_startTick);

            m_renderedUntil.clear();
            m_metronomeUntil = m_startTick;
            m_clockUntil = m_startTick;
            m_sounding.clear();
            m_channelsUsed.clear();

            ForEachTrack(*m_sequence, [this](Track const& track, size_t)
            {
                if (!track.IsFolder)
                {
                    SendStartup(track, 0);
                    SendChase(track, m_startTick, 0);
                }

                return true;
            });

            for (auto const& output : m_settings.ClockOutputs)
            {
                if (m_startTick == 0)
                {
                    auto const start = SystemWord(output.Group, 0xFA);
                    m_output.Send(output.Endpoint, m_startTimestamp, &start, 1);
                }
                else
                {
                    auto const position = std::min<int64_t>(m_startTick / TicksPerSongPositionBeat, 0x3FFF);
                    auto const pointer = SystemWord(output.Group, 0xF2, static_cast<uint8_t>(position & 0x7F), static_cast<uint8_t>(position >> 7));
                    auto const resume = SystemWord(output.Group, 0xFB);
                    m_output.Send(output.Endpoint, 0, &pointer, 1);
                    m_output.Send(output.Endpoint, m_startTimestamp, &resume, 1);
                }
            }

            m_playing = true;
        }

        Sweep();
    }

    _Use_decl_annotations_
    void PlaybackEngine::SendSilence(uint64_t timestamp)
    {
        auto const now = m_clock.Now ? m_clock.Now() : 0;

        // First, end every note that's still sounding or still to start.
        for (auto const& note : m_sounding)
        {
            if (note.OffTimestamp > now)
            {
                m_output.Send(note.Endpoint, timestamp, note.OffWords, 2);
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

        for (auto const& output : m_settings.ClockOutputs)
        {
            auto const stop = SystemWord(output.Group, 0xFC);
            m_output.Send(output.Endpoint, 0, &stop, 1);
        }

        m_startTick = TickAtTimestamp(now);
        m_sounding.clear();
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
            return m_startTick;
        }

        return std::max(m_startTick, TickAtTimestamp(m_clock.Now()));
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
        std::vector<RenderedMessage> messages{};

        for (auto const& track : tracks)
        {
            if (track.IsFolder)
            {
                SweepTracks(track.Children, now, anySolo, insideSoloedFolder || track.Soloed, insideMutedFolder || track.Muted);
                continue;
            }

            auto const info = LookupDestination(track.Destination.Endpoint);

            // A destination the service sends to early needs its messages that much sooner.
            auto const horizon = now + (static_cast<uint64_t>(m_settings.LookAheadMicroseconds) + info.OffsetMicroseconds) * m_clock.TicksPerSecond / 1000000;
            auto const horizonTick = TickAtTimestamp(horizon);

            auto found = m_renderedUntil.find(track.Id);
            auto const from = found == m_renderedUntil.end() ? m_startTick : found->second;

            if (horizonTick <= from)
            {
                continue;
            }

            // A muted track still moves on, so unmuting it doesn't play what was skipped.
            if (IsAudible(track, anySolo, insideSoloedFolder, insideMutedFolder))
            {
                messages.clear();
                RenderTrack(*m_sequence, track, from, horizonTick, messages);

                for (auto& message : messages)
                {
                    ApplyDestination(track.Destination, message.Words, message.WordCount);

                    auto const timestamp = TimestampAtTick(message.Tick);
                    SendTo(track.Destination.Endpoint, info.SpeaksMidi2, timestamp, message.Words.data(), message.WordCount);

                    if (message.Kind == RenderedKind::NoteOff && !track.Destination.Endpoint.IsEmpty())
                    {
                        // Remember the end, so Stop can send it early.
                        SoundingNote note{};
                        note.Endpoint = track.Destination.Endpoint;
                        note.OffTimestamp = timestamp;
                        note.Midi2 = info.SpeaksMidi2;

                        auto const translated = info.SpeaksMidi2
                            ? TranslateToMidi2(message.Words.data(), message.WordCount)
                            : TranslateToMidi1(message.Words.data(), message.WordCount);

                        if (translated.Count > 0)
                        {
                            note.OffWords[0] = translated.Messages[0][0];
                            note.OffWords[1] = translated.Messages[0][1];
                            m_sounding.push_back(note);
                        }
                    }
                }
            }

            m_renderedUntil[track.Id] = horizonTick;
        }
    }

    _Use_decl_annotations_
    void PlaybackEngine::SweepMetronome(uint64_t now)
    {
        auto const& metronome = m_settings.Metronome;

        if (!metronome.Enabled || metronome.Endpoint.IsEmpty())
        {
            return;
        }

        auto const info = LookupDestination(metronome.Endpoint);
        auto const horizon = now + (static_cast<uint64_t>(m_settings.LookAheadMicroseconds) + info.OffsetMicroseconds) * m_clock.TicksPerSecond / 1000000;
        auto const horizonTick = TickAtTimestamp(horizon);
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
            SendTo(metronome.Endpoint, info.SpeaksMidi2, TimestampAtTick(tick), words, 2);

            BuildNoteOff(click, metronome.Group, words);
            SendTo(metronome.Endpoint, info.SpeaksMidi2, TimestampAtTick(tick + ClickTicks), words, 2);

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
        int32_t earliest{ 0 };

        for (auto const& output : m_settings.ClockOutputs)
        {
            earliest = std::min(earliest, output.OffsetMicroseconds);
        }

        auto const horizon = now + (static_cast<uint64_t>(m_settings.LookAheadMicroseconds) + static_cast<uint64_t>(-earliest)) * m_clock.TicksPerSecond / 1000000;
        auto const horizonTick = TickAtTimestamp(horizon);

        auto tick = ((m_clockUntil + TicksPerClock - 1) / TicksPerClock) * TicksPerClock;

        for (; tick < horizonTick; tick += TicksPerClock)
        {
            auto const base = static_cast<int64_t>(TimestampAtTick(tick));

            for (auto const& output : m_settings.ClockOutputs)
            {
                auto const offset = static_cast<int64_t>(output.OffsetMicroseconds) * static_cast<int64_t>(m_clock.TicksPerSecond) / 1000000;
                auto const timestamp = static_cast<uint64_t>(std::max<int64_t>(1, base + offset));
                auto const pulse = SystemWord(output.Group, 0xF8);
                m_output.Send(output.Endpoint, timestamp, &pulse, 1);
            }
        }

        m_clockUntil = std::max(m_clockUntil, horizonTick);
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

        bool anySolo{ false };

        ForEachTrack(*m_sequence, [&anySolo](Track const& track, size_t)
        {
            anySolo = anySolo || track.Soloed;
            return !anySolo;
        });

        SweepTracks(m_sequence->Tracks, now, anySolo, false, false);
        SweepMetronome(now);
        SweepClockOutputs(now);

        // Notes that have ended don't need ending again.
        std::erase_if(m_sounding, [now](SoundingNote const& note) { return note.OffTimestamp <= now; });
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
