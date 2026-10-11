// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "PlaybackEngineTests.h"
#include "SequenceTestData.h"

#include "MessageTranslation.h"
#include "PlaybackEngine.h"
#include "SequenceEdits.h"

#include <windows.h>

#include <algorithm>
#include <mutex>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    constexpr int64_t Bar = TicksPerQuarterNote * 4;

    // The fake clock counts microseconds, so at 120 BPM a quarter note is 500,000 and the music
    // starts 20,000 after Play.
    constexpr uint64_t PlayPressedAt = 1000000;
    constexpr uint64_t MusicStartsAt = PlayPressedAt + 20000;

    uint64_t At(int64_t tick)
    {
        return MusicStartsAt + static_cast<uint64_t>(std::llround(static_cast<double>(tick) * 500000.0 / TicksPerQuarterNote));
    }

    struct SentMessage
    {
        std::wstring Endpoint{};
        uint64_t Timestamp{ 0 };
        std::array<uint32_t, 4> Words{};
        uint8_t Count{ 0 };
    };

    class Recorder final : public IEngineOutput
    {
    public:
        void Send(EndpointRef const& endpoint, uint64_t timestamp, uint32_t const* words, uint8_t wordCount) noexcept override
        {
            try
            {
                SentMessage message{ endpoint.Name, timestamp, {}, wordCount };
                std::copy_n(words, std::min<uint8_t>(wordCount, 4), message.Words.begin());

                std::scoped_lock guard{ m_lock };
                m_sent.push_back(message);
            }
            catch (...)
            {
            }
        }

        std::vector<SentMessage> All()
        {
            std::scoped_lock guard{ m_lock };
            return m_sent;
        }

        void Clear()
        {
            std::scoped_lock guard{ m_lock };
            m_sent.clear();
        }

    private:
        std::mutex m_lock{};
        std::vector<SentMessage> m_sent{};
    };

    struct Fixture
    {
        uint64_t Now{ PlayPressedAt };
        Recorder Output{};
        PlaybackEngine Engine{ Output, EngineClock{ [this]() { return Now; }, 1000000 } };
    };

    bool IsNoteOn(SentMessage const& message) noexcept
    {
        auto const type = message.Words[0] >> 28;
        auto const status = (message.Words[0] >> 20) & 0x0F;
        return (type == 0x4 || type == 0x2) && status == 0x9;
    }

    std::vector<SentMessage> NoteOns(std::vector<SentMessage> const& messages)
    {
        std::vector<SentMessage> ons{};
        std::copy_if(messages.begin(), messages.end(), std::back_inserter(ons), IsNoteOn);
        return ons;
    }

    Track MakeTrack(std::wstring const& id, std::wstring const& endpoint, std::wstring const& clipId)
    {
        Track track{};
        track.Id = id;
        track.Name = id;
        track.Destination.Endpoint = EndpointRef{ endpoint, L"" };
        track.Destination.Channel = 0;
        track.Timeline = { Placement{ clipId, 0, 0 } };
        return track;
    }

    // One bar at 120 BPM: notes on beats 1, 2 and 3.
    Sequence KeysSequence()
    {
        Sequence sequence{};
        sequence.Tempo = { TempoPoint{ 0, 120.0, false } };

        Clip clip{};
        clip.Id = L"c-keys";
        clip.Length = Bar;
        clip.Loop = false;
        clip.Notes = {
            testdata::MakeNote(0, 480, 60),
            testdata::MakeNote(960, 480, 62),
            testdata::MakeNote(1920, 480, 64),
        };

        sequence.Tracks = { MakeTrack(L"t-keys", L"Synth", L"c-keys") };
        sequence.Clips = { clip };

        NormalizeSequence(sequence);
        return sequence;
    }

    std::shared_ptr<Sequence const> Snapshot(Sequence const& sequence)
    {
        return std::make_shared<Sequence const>(sequence);
    }

    std::vector<SentMessage> SentTo(std::vector<SentMessage> const& messages, std::wstring const& endpoint)
    {
        std::vector<SentMessage> matching{};
        std::copy_if(messages.begin(), messages.end(), std::back_inserter(matching), [&endpoint](SentMessage const& m) { return m.Endpoint == endpoint; });
        return matching;
    }

    bool IsQuarterFrame(SentMessage const& message) noexcept
    {
        return (message.Words[0] & 0xFFFF0000u) == 0x10F10000u;
    }

    bool IsFullFrameStart(SentMessage const& message) noexcept
    {
        return (message.Words[0] & 0xFFFFFFFFu) == 0x30167F7Fu;
    }

    uint8_t QuarterFrameData(SentMessage const& message) noexcept
    {
        return static_cast<uint8_t>((message.Words[0] >> 8) & 0x7F);
    }

    // 25 frames a second is 10 ms a quarter frame, which keeps the numbers round.
    TimeCodeOutput Lights(midiapp::MidiTimeCodePosition start)
    {
        return TimeCodeOutput{ EndpointRef{ L"Lights", L"" }, 0, midiapp::MidiTimeCodeFrameRate::Frames25, start, 0 };
    }
}

void PlaybackEngineTests::NotesGoOutAtTheirTimes()
{
    Fixture f{};
    f.Engine.SetSequence(Snapshot(KeysSequence()));
    f.Engine.Play(0);

    auto ons = NoteOns(f.Output.All());
    VERIFY_ARE_EQUAL(size_t{ 1 }, ons.size());
    VERIFY_ARE_EQUAL(At(0), ons[0].Timestamp);
    VERIFY_ARE_EQUAL(std::wstring{ L"Synth" }, ons[0].Endpoint);
    VERIFY_ARE_EQUAL(0x40903C00u, ons[0].Words[0]);
    VERIFY_ARE_EQUAL(0xC0000000u, ons[0].Words[1]);

    // The note's end waits until it's due, so a stop or a restart can still move it.
    auto const all = f.Output.All();
    VERIFY_IS_FALSE(std::any_of(all.begin(), all.end(), [](SentMessage const& message)
    {
        return message.Words[0] == 0x40803C00u;
    }));

    f.Now = 2100000;
    f.Engine.Sweep();

    auto const later = f.Output.All();
    VERIFY_IS_TRUE(std::any_of(later.begin(), later.end(), [](SentMessage const& message)
    {
        return message.Words[0] == 0x40803C00u && message.Timestamp == At(480);
    }));

    ons = NoteOns(f.Output.All());
    VERIFY_ARE_EQUAL(size_t{ 3 }, ons.size());
    VERIFY_ARE_EQUAL(At(960), ons[1].Timestamp);
    VERIFY_ARE_EQUAL(At(1920), ons[2].Timestamp);
    VERIFY_IS_TRUE(f.Engine.IsPlaying());
}

void PlaybackEngineTests::OnlyTheLookAheadIsHandedOver()
{
    Fixture f{};
    f.Engine.SetSequence(Snapshot(KeysSequence()));
    f.Engine.Play(0);

    // 40 ms after Play is nowhere near beat 2.
    for (auto const& message : f.Output.All())
    {
        VERIFY_IS_TRUE(!IsNoteOn(message) || message.Timestamp < PlayPressedAt + 40000);
    }

    // 10 ms before beat 2 is due, it's inside the look-ahead.
    f.Now = At(960) - 10000;
    f.Engine.Sweep();
    VERIFY_ARE_EQUAL(size_t{ 2 }, NoteOns(f.Output.All()).size());
}

void PlaybackEngineTests::AMuteIsHeardAtTheNextSweep()
{
    Fixture f{};
    auto sequence = KeysSequence();
    f.Engine.SetSequence(Snapshot(sequence));
    f.Engine.Play(0);

    sequence.Tracks[0].Muted = true;
    f.Now = 1100000;
    f.Engine.SetSequence(Snapshot(sequence));

    f.Now = 1600000;
    f.Engine.Sweep();
    VERIFY_ARE_EQUAL(size_t{ 1 }, NoteOns(f.Output.All()).size());

    // Unmuted, it carries on from here. It doesn't play what it skipped.
    sequence.Tracks[0].Muted = false;
    f.Engine.SetSequence(Snapshot(sequence));

    f.Now = 2100000;
    f.Engine.Sweep();

    auto const ons = NoteOns(f.Output.All());
    VERIFY_ARE_EQUAL(size_t{ 2 }, ons.size());
    VERIFY_ARE_EQUAL(0x40904000u, ons[1].Words[0]);
}

void PlaybackEngineTests::SoloLeavesOnlyTheSoloedTracks()
{
    auto sequence = KeysSequence();

    auto bass = MakeTrack(L"t-bass", L"Bass synth", L"c-keys");
    bass.Soloed = true;

    Track folder{};
    folder.Id = L"f-1";
    folder.IsFolder = true;
    folder.Children = { bass };

    sequence.Tracks.push_back(folder);
    NormalizeSequence(sequence);

    Fixture f{};
    f.Engine.SetSequence(Snapshot(sequence));
    f.Engine.Play(0);

    f.Now = 2100000;
    f.Engine.Sweep();

    auto const ons = NoteOns(f.Output.All());
    VERIFY_ARE_EQUAL(size_t{ 3 }, ons.size());

    for (auto const& on : ons)
    {
        VERIFY_ARE_EQUAL(std::wstring{ L"Bass synth" }, on.Endpoint);
    }
}

void PlaybackEngineTests::AMidi1DestinationGetsMidi1()
{
    Fixture f{};
    f.Engine.SetDestinationLookup([](EndpointRef const&, uint8_t) { return DestinationInfo{ false, 0 }; });
    f.Engine.SetSequence(Snapshot(KeysSequence()));
    f.Engine.Play(0);

    auto const ons = NoteOns(f.Output.All());
    VERIFY_ARE_EQUAL(size_t{ 1 }, ons.size());
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, ons[0].Count);
    VERIFY_ARE_EQUAL(0x20903C60u, ons[0].Words[0]);
}

void PlaybackEngineTests::StopEndsWhatItStarted()
{
    Fixture f{};
    f.Engine.SetSequence(Snapshot(KeysSequence()));
    f.Engine.Play(0);

    f.Now = 1100000;
    f.Output.Clear();
    f.Engine.Stop();

    VERIFY_IS_FALSE(f.Engine.IsPlaying());

    auto const sent = f.Output.All();

    // The first note was sounding, so it ends now, then the channel is quietened now and again
    // after everything already handed over has played.
    VERIFY_ARE_EQUAL(0x40803C00u, sent.front().Words[0]);
    VERIFY_ARE_EQUAL(uint64_t{ 0 }, sent.front().Timestamp);

    auto const count = [&sent](uint32_t word0, uint64_t timestamp)
    {
        return std::count_if(sent.begin(), sent.end(), [&](SentMessage const& message)
        {
            return message.Words[0] == word0 && message.Timestamp == timestamp;
        });
    };

    VERIFY_ARE_EQUAL(ptrdiff_t{ 1 }, count(0x40B04000u, 0));
    VERIFY_ARE_EQUAL(ptrdiff_t{ 1 }, count(0x40B07B00u, 0));
    VERIFY_ARE_EQUAL(ptrdiff_t{ 1 }, count(0x40B07800u, 0));
    VERIFY_ARE_EQUAL(ptrdiff_t{ 1 }, count(0x40B07B00u, 1100000 + 50000));
    VERIFY_ARE_EQUAL(ptrdiff_t{ 1 }, count(0x40E00000u, 0));

    // Sustain goes before all notes off.
    auto const sustain = std::find_if(sent.begin(), sent.end(), [](SentMessage const& m) { return m.Words[0] == 0x40B04000u; });
    auto const allNotesOff = std::find_if(sent.begin(), sent.end(), [](SentMessage const& m) { return m.Words[0] == 0x40B07B00u; });
    VERIFY_IS_TRUE(sustain < allNotesOff);
}

void PlaybackEngineTests::ARestartDoesntCutTheNewNotes()
{
    // One note two bars long.
    auto sequence = KeysSequence();
    sequence.Clips[0].Notes = { testdata::MakeNote(0, Bar * 2, 60) };
    sequence.Clips[0].Length = Bar * 2;
    NormalizeSequence(sequence);

    Fixture f{};
    f.Engine.SetSequence(Snapshot(sequence));
    f.Engine.Play(0);

    for (; f.Now < At(960); f.Now += 5000)
    {
        f.Engine.Sweep();
    }

    // Played again from the start, half way through the note.
    auto const restartedAt = f.Now;
    f.Engine.Play(0);

    for (; f.Now < restartedAt + 20000 + 4000000; f.Now += 5000)
    {
        f.Engine.Sweep();
    }

    auto const sent = f.Output.All();
    auto const ends = [&sent](uint64_t timestamp)
    {
        return std::count_if(sent.begin(), sent.end(), [timestamp](SentMessage const& m)
        {
            return m.Words[0] == 0x40803C00u && m.Timestamp == timestamp;
        });
    };

    // The first note ended at the restart. Its old end, two bars after the first start, never
    // went out: it would have cut the new note. The new note ends two bars after the new start.
    VERIFY_ARE_EQUAL(ptrdiff_t{ 1 }, ends(0));
    VERIFY_ARE_EQUAL(ptrdiff_t{ 0 }, ends(At(Bar * 2)));
    VERIFY_ARE_EQUAL(ptrdiff_t{ 1 }, ends(restartedAt + 20000 + 4000000));
}

void PlaybackEngineTests::AMidi1NoteEndsWithOneWord()
{
    Fixture f{};
    f.Engine.SetDestinationLookup([](EndpointRef const&, uint8_t) { return DestinationInfo{ false, 0 }; });
    f.Engine.SetSequence(Snapshot(KeysSequence()));
    f.Engine.Play(0);

    f.Now = 1100000;
    f.Output.Clear();
    f.Engine.Stop();

    auto const sent = f.Output.All();
    VERIFY_ARE_EQUAL(0x20803C00u, sent.front().Words[0] & 0xFFFFFF00u);
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, sent.front().Count);
}

void PlaybackEngineTests::StartingPartWayChasesTheChannelState()
{
    auto sequence = KeysSequence();
    auto& clip = sequence.Clips[0];
    clip.Length = Bar * 2;
    clip.Events = {
        testdata::MakeEvent(0, 0x20C00500),
        testdata::MakeEvent(100, 0x20B00764),
        testdata::MakeEvent(200, 0x20B00750),
        testdata::MakeEvent(Bar + 10, 0x20B00A40),
    };

    Fixture f{};
    f.Engine.SetSequence(Snapshot(sequence));
    f.Engine.Play(Bar);

    auto const sent = f.Output.All();

    std::vector<SentMessage> immediate{};
    std::copy_if(sent.begin(), sent.end(), std::back_inserter(immediate), [](SentMessage const& m) { return m.Timestamp == 0; });

    // The last volume, then the program: controllers before program, so a bank select would land first.
    VERIFY_ARE_EQUAL(size_t{ 2 }, immediate.size());
    VERIFY_ARE_EQUAL(0x40B00700u, immediate[0].Words[0]);
    VERIFY_ARE_EQUAL(ScaleUp(0x50, 7, 32), immediate[0].Words[1]);
    VERIFY_ARE_EQUAL(0x40C00000u, immediate[1].Words[0]);
    VERIFY_ARE_EQUAL(0x05000000u, immediate[1].Words[1]);

    // Nothing from before the start plays.
    VERIFY_IS_TRUE(NoteOns(sent).empty());
}

void PlaybackEngineTests::StartupMessagesGoFirst()
{
    auto sequence = KeysSequence();
    sequence.Tracks[0].Startup = { testdata::MakeEvent(0, 0x20C05100) };

    Fixture f{};
    f.Engine.SetSequence(Snapshot(sequence));
    f.Engine.Play(0);

    auto const sent = f.Output.All();
    VERIFY_ARE_EQUAL(uint64_t{ 0 }, sent.front().Timestamp);
    VERIFY_ARE_EQUAL(0x40C00000u, sent.front().Words[0]);
    VERIFY_ARE_EQUAL(0x51000000u, sent.front().Words[1]);
}

void PlaybackEngineTests::TheMetronomeClicksOnTheBeat()
{
    EngineSettings settings{};
    settings.Metronome.Enabled = true;
    settings.Metronome.Endpoint = EndpointRef{ L"General MIDI Synth", L"" };

    Fixture f{};
    f.Engine.SetSettings(settings);
    f.Engine.SetSequence(Snapshot(KeysSequence()));
    f.Engine.Play(0);

    f.Now = 3100000;
    f.Engine.Sweep();

    std::vector<SentMessage> clicks{};
    for (auto const& message : NoteOns(f.Output.All()))
    {
        if (message.Endpoint == L"General MIDI Synth")
        {
            clicks.push_back(message);
        }
    }

    VERIFY_ARE_EQUAL(size_t{ 5 }, clicks.size());

    for (size_t beat = 0; beat < clicks.size(); ++beat)
    {
        VERIFY_ARE_EQUAL(At(static_cast<int64_t>(beat) * TicksPerQuarterNote), clicks[beat].Timestamp);

        // Side stick on channel 10, louder on the first beat of each bar.
        VERIFY_ARE_EQUAL(0x40992500u, clicks[beat].Words[0]);
        VERIFY_ARE_EQUAL(beat % 4 == 0 ? 0xFFFF0000u : 0xB3320000u, clicks[beat].Words[1]);
    }
}

void PlaybackEngineTests::ClockOutSends24PulsesAQuarterNote()
{
    EngineSettings settings{};
    settings.ClockOutputs = { ClockOutput{ EndpointRef{ L"TR-8S", L"" }, 0, 0 } };

    Fixture f{};
    f.Engine.SetSettings(settings);
    f.Engine.SetSequence(Snapshot(KeysSequence()));
    f.Engine.Play(0);

    f.Now = At(TicksPerQuarterNote) - 40000;
    f.Engine.Sweep();

    std::vector<SentMessage> clock{};
    for (auto const& message : f.Output.All())
    {
        if (message.Endpoint == L"TR-8S")
        {
            clock.push_back(message);
        }
    }

    VERIFY_ARE_EQUAL(0x10FA0000u, clock.front().Words[0]);
    VERIFY_ARE_EQUAL(MusicStartsAt, clock.front().Timestamp);

    // Start, then 24 pulses in the first beat, 1/24 of 500 ms apart.
    VERIFY_ARE_EQUAL(size_t{ 25 }, clock.size());

    for (size_t pulse = 1; pulse < clock.size(); ++pulse)
    {
        VERIFY_ARE_EQUAL(0x10F80000u, clock[pulse].Words[0]);
        VERIFY_ARE_EQUAL(At(static_cast<int64_t>(pulse - 1) * TicksPerQuarterNote / 24), clock[pulse].Timestamp);
    }

    f.Output.Clear();
    f.Engine.Stop();

    auto const stopped = f.Output.All();
    VERIFY_IS_TRUE(std::any_of(stopped.begin(), stopped.end(), [](SentMessage const& m) { return m.Words[0] == 0x10FC0000u; }));
}

void PlaybackEngineTests::ClockStopFollowsTheLastPulse()
{
    EngineSettings settings{};
    settings.ClockOutputs = { ClockOutput{ EndpointRef{ L"TR-8S", L"" }, 0, 0 } };

    Fixture f{};
    f.Engine.SetSettings(settings);
    f.Engine.SetSequence(Snapshot(KeysSequence()));
    f.Engine.Play(0);

    f.Now = At(TicksPerQuarterNote);
    f.Engine.Sweep();
    f.Engine.Stop();

    auto const sent = SentTo(f.Output.All(), L"TR-8S");
    auto const stop = std::find_if(sent.begin(), sent.end(), [](SentMessage const& m) { return m.Words[0] == 0x10FC0000u; });
    VERIFY_IS_TRUE(stop != sent.end());

    uint64_t lastPulse{ 0 };

    for (auto const& message : sent)
    {
        if (message.Words[0] == 0x10F80000u)
        {
            lastPulse = std::max(lastPulse, message.Timestamp);
        }
    }

    // The look-ahead has pulses waiting past the moment Stop was pressed. A follower that got
    // stop first would count those as a running clock.
    VERIFY_IS_GREATER_THAN(lastPulse, f.Now);
    VERIFY_ARE_EQUAL(lastPulse + 1, stop->Timestamp);
}

void PlaybackEngineTests::FollowingPlaysAtTheIncomingTempo()
{
    Fixture f{};
    f.Engine.SetSequence(Snapshot(KeysSequence()));
    f.Engine.SetFollowing(true);

    // 100 BPM is a pulse every 25 ms. The other device's clock runs before it starts.
    constexpr uint64_t Pulse = 25000;
    auto t = PlayPressedAt;

    for (int i = 0; i < 8; ++i, t += Pulse)
    {
        f.Now = t;
        f.Engine.FollowPulse(t);
    }

    // Only the other device can start it.
    f.Engine.Play(0);
    VERIFY_IS_FALSE(f.Engine.IsPlaying());

    // Start comes just before a pulse, and that pulse is the top of the sequence.
    f.Now = t - 2000;
    f.Engine.FollowStart(f.Now);
    VERIFY_IS_TRUE(f.Engine.IsPlaying());

    auto const top = t;

    for (int i = 0; i < 24 * 5; ++i, t += Pulse)
    {
        f.Now = t;
        f.Engine.FollowPulse(t);
        f.Engine.Sweep();
        f.Now = t + Pulse / 2;
        f.Engine.Sweep();
    }

    // The notes on beats 1, 2 and 3 land on pulses 0, 24 and 48.
    auto const ons = NoteOns(f.Output.All());
    VERIFY_ARE_EQUAL(size_t{ 3 }, ons.size());
    VERIFY_ARE_EQUAL(top, ons[0].Timestamp);
    VERIFY_ARE_EQUAL(top + 24 * Pulse, ons[1].Timestamp);
    VERIFY_ARE_EQUAL(top + 48 * Pulse, ons[2].Timestamp);

    VERIFY_ARE_EQUAL(100.0, std::round(f.Engine.FollowedBeatsPerMinute() * 100.0) / 100.0);
}

void PlaybackEngineTests::FollowingWaitsForTheFirstPulse()
{
    // The sequence says 120 BPM, but the other device runs at 100 and sends nothing before Start.
    Fixture f{};
    f.Engine.SetSequence(Snapshot(KeysSequence()));
    f.Engine.SetFollowing(true);

    constexpr uint64_t Pulse = 25000;
    f.Now = PlayPressedAt;
    f.Engine.FollowStart(f.Now);

    // Nothing is planned before the first pulse says where the top is.
    VERIFY_ARE_EQUAL(size_t{ 0 }, NoteOns(f.Output.All()).size());

    auto t = PlayPressedAt + 3000;
    auto const top = t;

    for (int i = 0; i < 24 * 3; ++i, t += Pulse)
    {
        f.Now = t;
        f.Engine.FollowPulse(t);
        f.Engine.Sweep();
    }

    auto const ons = NoteOns(f.Output.All());
    VERIFY_ARE_EQUAL(size_t{ 3 }, ons.size());
    VERIFY_ARE_EQUAL(top, ons[0].Timestamp);
    VERIFY_ARE_EQUAL(top + 24 * Pulse, ons[1].Timestamp);
    VERIFY_ARE_EQUAL(top + 48 * Pulse, ons[2].Timestamp);
}

void PlaybackEngineTests::FollowingStopsWithTheClock()
{
    Fixture f{};
    f.Engine.SetSequence(Snapshot(KeysSequence()));
    f.Engine.SetFollowing(true);

    constexpr uint64_t Pulse = 20000;
    auto t = PlayPressedAt;

    auto const pulses = [&](int count)
    {
        for (int i = 0; i < count; ++i, t += Pulse)
        {
            f.Now = t;
            f.Engine.FollowPulse(t);
            f.Engine.Sweep();
        }
    };

    pulses(4);
    f.Engine.FollowStart(t - 1000);
    pulses(12);
    VERIFY_IS_TRUE(f.Engine.IsPlaying());

    // Stop from the other device stops it where that device's clock got to.
    f.Now = t;
    f.Engine.FollowStop(t);
    VERIFY_IS_FALSE(f.Engine.IsPlaying());
    VERIFY_ARE_EQUAL(int64_t{ 12 * (TicksPerQuarterNote / 24) }, f.Engine.PositionTick());

    // Pulses while it's stopped only keep the pace.
    pulses(4);
    VERIFY_IS_FALSE(f.Engine.IsPlaying());

    // Continue picks up from there, and the clock going quiet stops it again.
    f.Engine.FollowContinue(t - 1000);
    pulses(6);
    VERIFY_IS_TRUE(f.Engine.IsPlaying());

    f.Now = t + 400000;
    f.Engine.Sweep();
    VERIFY_IS_FALSE(f.Engine.IsPlaying());
}

void PlaybackEngineTests::FollowingStartsWhereTheSongPositionSays()
{
    Fixture f{};
    f.Engine.SetSequence(Snapshot(KeysSequence()));
    f.Engine.SetFollowing(true);

    constexpr uint64_t Pulse = 25000;
    auto t = PlayPressedAt;

    for (int i = 0; i < 4; ++i, t += Pulse)
    {
        f.Now = t;
        f.Engine.FollowPulse(t);
    }

    // Four sixteenths in is beat 2, where the second note starts.
    f.Engine.FollowSongPosition(t - 3000, 4);
    VERIFY_ARE_EQUAL(int64_t{ TicksPerQuarterNote }, f.Engine.PositionTick());

    f.Engine.FollowContinue(t - 2000);
    auto const resumed = t;

    for (int i = 0; i < 30; ++i, t += Pulse)
    {
        f.Now = t;
        f.Engine.FollowPulse(t);
        f.Engine.Sweep();
    }

    auto const ons = NoteOns(f.Output.All());
    VERIFY_ARE_EQUAL(size_t{ 2 }, ons.size());
    VERIFY_ARE_EQUAL(62u, (ons[0].Words[0] >> 8) & 0x7F);
    VERIFY_ARE_EQUAL(resumed, ons[0].Timestamp);
    VERIFY_ARE_EQUAL(resumed + 24 * Pulse, ons[1].Timestamp);
}

void PlaybackEngineTests::ClockOutputsSendOnlyWhatTheyAskFor()
{
    ClockOutput pulses{ EndpointRef{ L"Pulses", L"" }, 0, 0 };
    pulses.SendStartStop = false;
    pulses.SendSongPosition = false;

    ClockOutput transport{ EndpointRef{ L"Transport", L"" }, 0, 0 };
    transport.SendClock = false;

    EngineSettings settings{};
    settings.ClockOutputs = { pulses, transport };

    Fixture f{};
    f.Engine.SetSettings(settings);
    f.Engine.SetSequence(Snapshot(KeysSequence()));
    f.Engine.Play(Bar);

    f.Now = MusicStartsAt + 500000 - 40000;
    f.Engine.Sweep();
    f.Engine.Stop();

    auto const all = f.Output.All();

    // Pulses only: a beat of them, and nothing else.
    auto const toPulses = SentTo(all, L"Pulses");
    VERIFY_ARE_EQUAL(size_t{ 24 }, toPulses.size());
    VERIFY_IS_TRUE(std::all_of(toPulses.begin(), toPulses.end(), [](SentMessage const& m) { return m.Words[0] == 0x10F80000u; }));

    // Transport only: where it starts (bar 2 is 16 sixteenths in), continue, then stop.
    auto const toTransport = SentTo(all, L"Transport");
    VERIFY_ARE_EQUAL(size_t{ 3 }, toTransport.size());
    VERIFY_ARE_EQUAL(0x10F21000u, toTransport[0].Words[0]);
    VERIFY_ARE_EQUAL(0x10FB0000u, toTransport[1].Words[0]);
    VERIFY_ARE_EQUAL(0x10FC0000u, toTransport[2].Words[0]);
}

void PlaybackEngineTests::ClockOutputsCanSendTheTempo()
{
    // 120 BPM ramping to 140 over the first bar, then holding.
    auto sequence = KeysSequence();
    sequence.Tempo = { TempoPoint{ 0, 120.0, true }, TempoPoint{ Bar, 140.0, false } };
    NormalizeSequence(sequence);

    ClockOutput midi2{ EndpointRef{ L"Mac", L"" }, 0, 0 };
    midi2.SendTempo = true;

    ClockOutput midi1{ EndpointRef{ L"Drum machine", L"" }, 0, 0 };
    midi1.SendTempo = true;

    EngineSettings settings{};
    settings.ClockOutputs = { midi2, midi1 };

    Fixture f{};
    f.Engine.SetDestinationLookup([](EndpointRef const& endpoint, uint8_t)
    {
        DestinationInfo info{};
        info.SpeaksMidi2 = endpoint.Name != L"Drum machine";
        return info;
    });
    f.Engine.SetSettings(settings);
    f.Engine.SetSequence(Snapshot(sequence));
    f.Engine.Play(0);

    for (; f.Now < MusicStartsAt + 3000000; f.Now += 5000)
    {
        f.Engine.Sweep();
    }

    auto const tempo = [](std::vector<SentMessage> const& messages)
    {
        std::vector<SentMessage> found{};
        std::copy_if(messages.begin(), messages.end(), std::back_inserter(found), [](SentMessage const& m) { return (m.Words[0] & 0xF0FFFFFFu) == 0xD0100000u; });
        return found;
    };

    // Where it starts, each beat of the ramp, then where the ramp ends.
    auto const toMac = tempo(SentTo(f.Output.All(), L"Mac"));
    VERIFY_ARE_EQUAL(size_t{ 5 }, toMac.size());
    VERIFY_ARE_EQUAL(uint8_t{ 4 }, toMac[0].Count);
    VERIFY_ARE_EQUAL(50000000u, toMac[0].Words[1]);
    VERIFY_ARE_EQUAL(MusicStartsAt - 1, toMac[0].Timestamp);
    VERIFY_ARE_EQUAL(static_cast<uint32_t>(std::llround(6.0e9 / 125.0)), toMac[1].Words[1]);
    VERIFY_ARE_EQUAL(static_cast<uint32_t>(std::llround(6.0e9 / 140.0)), toMac[4].Words[1]);

    // A MIDI 1.0 destination still gets its pulses, but not the tempo.
    auto const toDrums = SentTo(f.Output.All(), L"Drum machine");
    VERIFY_IS_TRUE(tempo(toDrums).empty());
    VERIFY_IS_TRUE(std::any_of(toDrums.begin(), toDrums.end(), [](SentMessage const& m) { return m.Words[0] == 0x10F80000u; }));
}

void PlaybackEngineTests::TheLoopTellsClockFollowersWhereItWent()
{
    EngineSettings settings{};
    settings.ClockOutputs = { ClockOutput{ EndpointRef{ L"TR-8S", L"" }, 0, 0 } };
    settings.LoopEnabled = true;
    settings.LoopStart = Bar / 2;
    settings.LoopEnd = Bar;

    Fixture f{};
    f.Engine.SetSettings(settings);
    f.Engine.SetSequence(Snapshot(KeysSequence()));
    f.Engine.Play(0);

    for (; f.Now < At(Bar) + 100000; f.Now += 5000)
    {
        f.Engine.Sweep();
    }

    auto const sent = SentTo(f.Output.All(), L"TR-8S");

    // Back to beat 3, 8 sixteenths in, just ahead of the next pulse.
    auto const pointer = std::find_if(sent.begin(), sent.end(), [](SentMessage const& m) { return (m.Words[0] & 0xFFFF0000u) == 0x10F20000u; });
    VERIFY_IS_TRUE(pointer != sent.end());
    VERIFY_ARE_EQUAL(0x10F20800u, pointer->Words[0]);
    VERIFY_ARE_EQUAL(At(Bar) - 1, pointer->Timestamp);

    auto const next = std::find_if(pointer, sent.end(), [](SentMessage const& m) { return m.Words[0] == 0x10F80000u; });
    VERIFY_IS_TRUE(next != sent.end());
    VERIFY_ARE_EQUAL(At(Bar), next->Timestamp);

    // Nothing makes it start again from the top.
    VERIFY_IS_FALSE(std::any_of(sent.begin(), sent.end(), [](SentMessage const& m) { return m.Words[0] == 0x10FB0000u || m.Words[0] == 0x10FC0000u; }));
}

void PlaybackEngineTests::TimeCodeCountsFromTheTopOfTheSequence()
{
    EngineSettings settings{};
    settings.TimeCodeOutputs = { Lights(midiapp::MidiTimeCodePosition{ 1, 0, 0, 0 }) };

    Fixture f{};
    f.Engine.SetSettings(settings);
    f.Engine.SetSequence(Snapshot(KeysSequence()));
    f.Engine.Play(0);

    f.Now = MusicStartsAt + 500000 - 40000;
    f.Engine.Sweep();

    auto const sent = SentTo(f.Output.All(), L"Lights");

    // First a full frame, just ahead of the music: 01:00:00:00, with 25 frames a second in the
    // hours byte.
    VERIFY_ARE_EQUAL(size_t{ 52 }, sent.size());
    VERIFY_ARE_EQUAL(0x30167F7Fu, sent[0].Words[0]);
    VERIFY_ARE_EQUAL(0x01012100u, sent[0].Words[1]);
    VERIFY_ARE_EQUAL(0x30320000u, sent[1].Words[0]);
    VERIFY_ARE_EQUAL(MusicStartsAt - 1, sent[0].Timestamp);

    // Then a quarter frame every 10 ms for the first half second.
    for (size_t quarter = 0; quarter < 50; ++quarter)
    {
        VERIFY_IS_TRUE(IsQuarterFrame(sent[quarter + 2]));
        VERIFY_ARE_EQUAL(MusicStartsAt + quarter * 10000, sent[quarter + 2].Timestamp);
    }

    // The first eight spell out 01:00:00:00: frames, seconds, minutes, then hours with the rate.
    std::array<uint8_t, 8> const first{ 0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x61, 0x72 };

    for (size_t piece = 0; piece < first.size(); ++piece)
    {
        VERIFY_ARE_EQUAL(first[piece], QuarterFrameData(sent[piece + 2]));
    }

    // The next eight spell out the frame they start on, two frames later.
    VERIFY_ARE_EQUAL(uint8_t{ 0x02 }, QuarterFrameData(sent[10]));

    // Stopping tells the receiver where it stopped.
    f.Output.Clear();
    f.Engine.Stop();

    auto const stopped = SentTo(f.Output.All(), L"Lights");
    VERIFY_ARE_EQUAL(size_t{ 2 }, stopped.size());
    VERIFY_IS_TRUE(IsFullFrameStart(stopped[0]));
}

void PlaybackEngineTests::TimeCodeGoesBackWithTheLoop()
{
    EngineSettings settings{};
    settings.TimeCodeOutputs = { Lights(midiapp::MidiTimeCodePosition{}) };
    settings.LoopEnabled = true;
    settings.LoopStart = 0;
    settings.LoopEnd = Bar;

    Fixture f{};
    f.Engine.SetSettings(settings);
    f.Engine.SetSequence(Snapshot(KeysSequence()));
    f.Engine.Play(0);

    for (; f.Now < At(Bar) + 300000; f.Now += 5000)
    {
        f.Engine.Sweep();
    }

    auto const sent = SentTo(f.Output.All(), L"Lights");

    // A full frame at the start and another as the loop goes round. Nothing else relocates it.
    std::vector<SentMessage> fullFrames{};
    std::copy_if(sent.begin(), sent.end(), std::back_inserter(fullFrames), IsFullFrameStart);
    VERIFY_ARE_EQUAL(size_t{ 2 }, fullFrames.size());
    VERIFY_ARE_EQUAL(At(Bar) - 1, fullFrames[1].Timestamp);
    VERIFY_ARE_EQUAL(0x01012000u, fullFrames[1].Words[1]);

    // Two seconds of quarter frames before the loop's end, none at it or after it in that pass.
    auto const beforeEnd = std::count_if(sent.begin(), sent.end(), [](SentMessage const& m) { return IsQuarterFrame(m) && m.Timestamp < At(Bar); });
    VERIFY_ARE_EQUAL(ptrdiff_t{ 200 }, beforeEnd);

    // Round again, it counts from frame 0 at the loop's start.
    auto const again = std::find_if(sent.begin(), sent.end(), [](SentMessage const& m) { return IsQuarterFrame(m) && m.Timestamp >= At(Bar); });
    VERIFY_IS_TRUE(again != sent.end());
    VERIFY_ARE_EQUAL(At(Bar), again->Timestamp);
    VERIFY_ARE_EQUAL(uint8_t{ 0x00 }, QuarterFrameData(*again));
}

void PlaybackEngineTests::TimeCodeStartsPartWayAtThatTime()
{
    EngineSettings settings{};
    settings.TimeCodeOutputs = { Lights(midiapp::MidiTimeCodePosition{ 1, 0, 0, 0 }) };

    Fixture f{};
    f.Engine.SetSettings(settings);
    f.Engine.SetSequence(Snapshot(KeysSequence()));

    // Bar 2 is two seconds in at 120 BPM.
    f.Engine.Play(Bar);

    f.Now = MusicStartsAt;
    f.Engine.Sweep();

    auto const sent = SentTo(f.Output.All(), L"Lights");
    VERIFY_IS_TRUE(sent.size() >= 6);

    // 01:00:02:00.
    VERIFY_ARE_EQUAL(0x01012100u, sent[0].Words[1]);
    VERIFY_ARE_EQUAL(0x30320200u, sent[1].Words[0]);

    // The quarter frames start with the music, spelling out the same time: frame 0, then the
    // seconds' low four bits in the third.
    VERIFY_ARE_EQUAL(MusicStartsAt, sent[2].Timestamp);
    VERIFY_ARE_EQUAL(uint8_t{ 0x00 }, QuarterFrameData(sent[2]));
    VERIFY_ARE_EQUAL(uint8_t{ 0x22 }, QuarterFrameData(sent[4]));
}

void PlaybackEngineTests::ADestinationOffsetSendsThatMuchEarly()
{
    auto sequence = KeysSequence();
    sequence.Tracks.push_back(MakeTrack(L"t-far", L"Far synth", L"c-keys"));
    NormalizeSequence(sequence);

    Fixture f{};
    f.Engine.SetDestinationLookup([](EndpointRef const& endpoint, uint8_t)
    {
        return DestinationInfo{ true, endpoint.Name == L"Far synth" ? 600000u : 0u };
    });
    f.Engine.SetSequence(Snapshot(sequence));
    f.Engine.Play(0);

    // The in-box service doesn't send early, so the engine does. The music starts late enough
    // for the device that needs its messages 600 ms early, and that device gets each one 600 ms
    // before the other device gets the same note.
    constexpr uint64_t Start = MusicStartsAt + 600000;

    f.Now = PlayPressedAt + 600000;
    f.Engine.Sweep();

    auto const ons = NoteOns(f.Output.All());

    auto const times = [&ons](std::wstring const& endpoint)
    {
        std::vector<uint64_t> found{};

        for (auto const& on : ons)
        {
            if (on.Endpoint == endpoint)
            {
                found.push_back(on.Timestamp);
            }
        }

        return found;
    };

    auto const prompt = times(L"Synth");
    auto const early = times(L"Far synth");

    VERIFY_ARE_EQUAL(size_t{ 1 }, prompt.size());
    VERIFY_ARE_EQUAL(Start, prompt[0]);

    VERIFY_ARE_EQUAL(size_t{ 2 }, early.size());
    VERIFY_ARE_EQUAL(Start - 600000, early[0]);
    VERIFY_ARE_EQUAL(Start + 500000 - 600000, early[1]);
}

void PlaybackEngineTests::TheThreadPlaysOnItsOwn()
{
    LARGE_INTEGER frequency{};
    ::QueryPerformanceFrequency(&frequency);

    Recorder output{};
    PlaybackEngine engine{ output, EngineClock{ []()
    {
        LARGE_INTEGER counter{};
        ::QueryPerformanceCounter(&counter);
        return static_cast<uint64_t>(counter.QuadPart);
    }, static_cast<uint64_t>(frequency.QuadPart) } };

    engine.SetSequence(Snapshot(KeysSequence()));
    engine.StartThread();
    engine.Play(0);

    ::Sleep(700);

    engine.Stop();
    engine.StopThread();

    auto const counters = engine.Counters();
    Log::Comment(String().Format(L"%llu sweeps, %llu messages", counters.Sweeps, counters.MessagesSent));

    // About 140 sweeps in 0.7 s at 5 ms each, and the first two beats.
    VERIFY_IS_TRUE(counters.Sweeps > 40);
    VERIFY_ARE_EQUAL(size_t{ 2 }, NoteOns(output.All()).size());
}

namespace
{
    uint8_t NoteNumber(SentMessage const& message) noexcept
    {
        return static_cast<uint8_t>((message.Words[0] >> 8) & 0x7F);
    }

    // Sweeps every 5 ms of fake time up to (not including) the end.
    void RunUntil(Fixture& f, uint64_t end)
    {
        for (; f.Now < end; f.Now += 5000)
        {
            f.Engine.Sweep();
        }
    }
}

void PlaybackEngineTests::TheLoopGoesRoundAtItsEnd()
{
    Fixture f{};
    f.Engine.SetSequence(Snapshot(KeysSequence()));

    EngineSettings settings{};
    settings.LoopEnabled = true;
    settings.LoopStart = 0;
    settings.LoopEnd = Bar;
    f.Engine.SetSettings(settings);

    f.Engine.Play(0);
    RunUntil(f, At(Bar * 2 + Bar / 2));

    // Three passes of the bar's three notes, each pass starting where the last one ended.
    auto const ons = NoteOns(f.Output.All());
    VERIFY_ARE_EQUAL(size_t{ 9 }, ons.size());

    for (size_t i = 0; i < ons.size(); ++i)
    {
        auto const pass = static_cast<int64_t>(i / 3);
        auto const beat = static_cast<int64_t>(i % 3);
        VERIFY_ARE_EQUAL(At(pass * Bar + beat * TicksPerQuarterNote), ons[i].Timestamp);
    }

    VERIFY_IS_TRUE(f.Engine.Counters().LoopPasses >= 2);

    // A moment in the second pass maps back onto the loop, which is what recording needs.
    VERIFY_ARE_EQUAL(int64_t{ 960 }, f.Engine.TickAtTime(At(Bar + 960)));
    VERIFY_ARE_EQUAL(int64_t{ 1920 }, f.Engine.TickAtTime(At(Bar * 2 + 1920)));
}

void PlaybackEngineTests::ANoteHeldOverTheLoopEndEndsThere()
{
    auto sequence = KeysSequence();
    sequence.Clips[0].Notes = { testdata::MakeNote(0, Bar * 2, 60) };
    sequence.Clips[0].Length = Bar * 2;

    Fixture f{};
    f.Engine.SetSequence(Snapshot(sequence));

    EngineSettings settings{};
    settings.LoopEnabled = true;
    settings.LoopStart = 0;
    settings.LoopEnd = Bar;
    f.Engine.SetSettings(settings);

    f.Engine.Play(0);
    RunUntil(f, At(Bar + Bar / 2));

    auto const sent = f.Output.All();
    auto const firstOff = std::find_if(sent.begin(), sent.end(), [](SentMessage const& m)
    {
        return (m.Words[0] & 0xFFF0FF00u) == 0x40803C00u && m.Timestamp != 0;
    });

    VERIFY_IS_TRUE(firstOff != sent.end());
    VERIFY_ARE_EQUAL(At(Bar), firstOff->Timestamp);
}

void PlaybackEngineTests::ALaunchedClipStartsAtTheNextBar()
{
    auto sequence = KeysSequence();

    Clip loop{};
    loop.Id = L"c-loop";
    loop.Length = Bar / 2;
    loop.Loop = true;
    loop.Notes = { testdata::MakeNote(0, 240, 72) };
    sequence.Clips.push_back(loop);
    NormalizeSequence(sequence);

    Fixture f{};
    f.Engine.SetSequence(Snapshot(sequence));
    f.Engine.Play(0);
    RunUntil(f, At(480));

    f.Engine.LaunchClip(L"t-keys", L"c-loop", -1);

    auto const pending = f.Engine.LaunchState();
    VERIFY_ARE_EQUAL(size_t{ 1 }, pending.size());
    VERIFY_IS_TRUE(pending[0].Pending);
    VERIFY_ARE_EQUAL(Bar, pending[0].PendingTick);

    RunUntil(f, At(Bar * 2));

    // The timeline plays its bar, then the launched clip loops from bar 2.
    auto const ons = NoteOns(f.Output.All());
    VERIFY_ARE_EQUAL(size_t{ 6 }, ons.size());
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, NoteNumber(ons[0]));
    VERIFY_ARE_EQUAL(uint8_t{ 64 }, NoteNumber(ons[2]));

    for (size_t i = 3; i < ons.size(); ++i)
    {
        VERIFY_ARE_EQUAL(uint8_t{ 72 }, NoteNumber(ons[i]));
        VERIFY_ARE_EQUAL(At(Bar + static_cast<int64_t>(i - 3) * (Bar / 2)), ons[i].Timestamp);
    }

    auto const playing = f.Engine.LaunchState();
    VERIFY_ARE_EQUAL(size_t{ 1 }, playing.size());
    VERIFY_IS_TRUE(playing[0].Mode == TrackPlayMode::Clip);
    VERIFY_IS_FALSE(playing[0].Pending);
}

void PlaybackEngineTests::ALaunchLightsOnlyItsSlot()
{
    auto sequence = KeysSequence();

    Clip loop{};
    loop.Id = L"c-loop";
    loop.Length = Bar / 2;
    loop.Loop = true;
    loop.Notes = { testdata::MakeNote(0, 240, 72) };
    sequence.Clips.push_back(loop);
    NormalizeSequence(sequence);

    Fixture f{};
    f.Engine.SetSequence(Snapshot(sequence));
    f.Engine.Play(0);
    RunUntil(f, At(480));

    // The same clip sits in scenes 1 and 3, and scene 3 launched it.
    f.Engine.LaunchClip(L"t-keys", L"c-loop", -1, 2);

    auto const pending = f.Engine.LaunchState();
    VERIFY_ARE_EQUAL(size_t{ 1 }, pending.size());
    VERIFY_IS_TRUE(pending[0].Queues(L"c-loop", 2));
    VERIFY_IS_FALSE(pending[0].Queues(L"c-loop", 0));
    VERIFY_IS_FALSE(pending[0].Plays(L"c-loop", 2));

    RunUntil(f, At(Bar + 480));

    auto const playing = f.Engine.LaunchState();
    VERIFY_ARE_EQUAL(size_t{ 1 }, playing.size());
    VERIFY_ARE_EQUAL(int32_t{ 2 }, playing[0].Slot);
    VERIFY_IS_TRUE(playing[0].Plays(L"c-loop", 2));
    VERIFY_IS_FALSE(playing[0].Plays(L"c-loop", 0));
    VERIFY_IS_FALSE(playing[0].Plays(L"c-keys", 2));

    // A launch that named no slot lights every slot that holds the clip.
    TrackLaunchView anySlot{};
    anySlot.Mode = TrackPlayMode::Clip;
    anySlot.ClipId = L"c-loop";
    VERIFY_IS_TRUE(anySlot.Plays(L"c-loop", 0));
    VERIFY_IS_TRUE(anySlot.Plays(L"c-loop", 5));
}

void PlaybackEngineTests::StoppingATrackSilencesItFromTheNextBar()
{
    auto sequence = KeysSequence();
    sequence.Clips[0].Loop = true;
    sequence.Tracks[0].Timeline[0].Length = Bar * 4;

    Fixture f{};
    f.Engine.SetSequence(Snapshot(sequence));
    f.Engine.Play(0);
    RunUntil(f, At(480));

    f.Engine.StopTrack(L"t-keys", -1);
    RunUntil(f, At(Bar * 3));

    // Only the first bar's three notes.
    VERIFY_ARE_EQUAL(size_t{ 3 }, NoteOns(f.Output.All()).size());
}

void PlaybackEngineTests::AClipCanStartAtAnExactPlace()
{
    auto sequence = KeysSequence();
    sequence.Clips[0].Loop = true;
    sequence.Tracks[0].Timeline[0].Length = Bar * 4;

    Clip loop{};
    loop.Id = L"c-loop";
    loop.Length = Bar / 2;
    loop.Loop = true;
    loop.Notes = { testdata::MakeNote(0, 240, 72) };
    sequence.Clips.push_back(loop);
    NormalizeSequence(sequence);

    Fixture f{};
    f.Engine.SetSequence(Snapshot(sequence));
    f.Engine.Play(0);
    RunUntil(f, At(480));

    // What recording into a slot does: the track goes quiet where the take starts, and the take
    // loops from where it ends.
    auto const start = f.Engine.NextLaunchPoint(-1);
    VERIFY_ARE_EQUAL(Bar, start);

    f.Engine.StopTrackAt(L"t-keys", start);
    RunUntil(f, At(Bar + 480));

    VERIFY_ARE_EQUAL(int64_t{ Bar + 480 }, f.Engine.UnwrappedTickAtTime(At(Bar + 480)));

    f.Engine.LaunchClipAt(L"t-keys", L"c-loop", Bar * 2);
    RunUntil(f, At(Bar * 3));

    // The first bar's three notes, a quiet bar, then the loop from bar 3.
    auto const ons = NoteOns(f.Output.All());
    VERIFY_ARE_EQUAL(size_t{ 6 }, ons.size());
    VERIFY_ARE_EQUAL(uint8_t{ 64 }, NoteNumber(ons[2]));
    VERIFY_ARE_EQUAL(uint8_t{ 72 }, NoteNumber(ons[3]));
    VERIFY_ARE_EQUAL(At(Bar * 2), ons[3].Timestamp);
    VERIFY_ARE_EQUAL(At(Bar * 2 + Bar / 2), ons[4].Timestamp);
}

void PlaybackEngineTests::BackToTimelinePicksUpTheTimeline()
{
    auto sequence = KeysSequence();
    sequence.Clips[0].Loop = true;
    sequence.Tracks[0].Timeline[0].Length = Bar * 4;

    Fixture f{};
    f.Engine.SetSequence(Snapshot(sequence));
    f.Engine.Play(0);
    RunUntil(f, At(480));

    f.Engine.StopTrack(L"t-keys", -1);
    RunUntil(f, At(Bar + 480));

    f.Engine.ReturnToTimeline(std::wstring{}, -1);
    RunUntil(f, At(Bar * 3 - 480));

    // Bar 1 plays, bar 2 is silent, bar 3 plays again from the timeline.
    auto const ons = NoteOns(f.Output.All());
    VERIFY_ARE_EQUAL(size_t{ 6 }, ons.size());
    VERIFY_ARE_EQUAL(At(Bar * 2), ons[3].Timestamp);
    VERIFY_IS_TRUE(f.Engine.LaunchState().empty());
}

void PlaybackEngineTests::AGroupCanSpeakADifferentProtocol()
{
    auto sequence = KeysSequence();
    sequence.Tracks[0].Destination.Group = 2;

    Fixture f{};

    // Group 3 of this device is a MIDI 1.0 function block; the rest speak MIDI 2.0.
    f.Engine.SetDestinationLookup([](EndpointRef const&, uint8_t group) { return DestinationInfo{ group != 2, 0 }; });
    f.Engine.SetSequence(Snapshot(sequence));
    f.Engine.Play(0);

    auto const ons = NoteOns(f.Output.All());
    VERIFY_ARE_EQUAL(size_t{ 1 }, ons.size());
    VERIFY_ARE_EQUAL(0x22903C60u, ons[0].Words[0]);

    // A track that says MIDI 2.0 gets it, whatever the device says.
    sequence.Tracks[0].Destination.Protocol = ProtocolChoice::Midi2;

    Fixture g{};
    g.Engine.SetDestinationLookup([](EndpointRef const&, uint8_t group) { return DestinationInfo{ group != 2, 0 }; });
    g.Engine.SetSequence(Snapshot(sequence));
    g.Engine.Play(0);

    auto const forced = NoteOns(g.Output.All());
    VERIFY_ARE_EQUAL(size_t{ 1 }, forced.size());
    VERIFY_ARE_EQUAL(0x42903C00u, forced[0].Words[0]);
}

void MessageTranslationTests::Midi2NotesAndControllersBecomeMidi1()
{
    uint32_t const note[2]{ 0x43953C00, 0xC0000000 };
    auto result = TranslateToMidi1(note, 2);
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, result.Count);
    VERIFY_ARE_EQUAL(0x23953C60u, result.Messages[0][0]);

    // A MIDI 2.0 note at velocity 0 is still a note, so MIDI 1.0 gets the quietest one, not a note off.
    uint32_t const silent[2]{ 0x40903C00, 0x00000000 };
    result = TranslateToMidi1(silent, 2);
    VERIFY_ARE_EQUAL(0x20903C01u, result.Messages[0][0]);

    uint32_t const controller[2]{ 0x40B04A00, 0xFFFFFFFF };
    result = TranslateToMidi1(controller, 2);
    VERIFY_ARE_EQUAL(0x20B04A7Fu, result.Messages[0][0]);

    uint32_t const bend[2]{ 0x40E00000, 0x80000000 };
    result = TranslateToMidi1(bend, 2);
    VERIFY_ARE_EQUAL(0x20E00040u, result.Messages[0][0]);

    // Not a MIDI 2.0 channel voice message: through unchanged.
    uint32_t const exclusive[2]{ 0x3016437E, 0x7F060100 };
    result = TranslateToMidi1(exclusive, 2);
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, result.WordCounts[0]);
    VERIFY_ARE_EQUAL(0x3016437Eu, result.Messages[0][0]);
}

void MessageTranslationTests::ProgramWithBankAndRegisteredControllersExpand()
{
    uint32_t const program[2]{ 0x40C00001, 0x05000203 };
    auto result = TranslateToMidi1(program, 2);
    VERIFY_ARE_EQUAL(uint8_t{ 3 }, result.Count);
    VERIFY_ARE_EQUAL(0x20B00002u, result.Messages[0][0]);
    VERIFY_ARE_EQUAL(0x20B02003u, result.Messages[1][0]);
    VERIFY_ARE_EQUAL(0x20C00500u, result.Messages[2][0]);

    // NRPN 7:54, the Moog One's filter cutoff, at the top of its range.
    uint32_t const nrpn[2]{ 0x40310736, 0xFFFFFFFF };
    result = TranslateToMidi1(nrpn, 2);
    VERIFY_ARE_EQUAL(uint8_t{ 4 }, result.Count);
    VERIFY_ARE_EQUAL(0x20B16307u, result.Messages[0][0]);
    VERIFY_ARE_EQUAL(0x20B16236u, result.Messages[1][0]);
    VERIFY_ARE_EQUAL(0x20B1067Fu, result.Messages[2][0]);
    VERIFY_ARE_EQUAL(0x20B1267Fu, result.Messages[3][0]);
}

void MessageTranslationTests::PerNoteMessagesAreDroppedForMidi1()
{
    uint32_t const perNoteBend[2]{ 0x40603C00, 0x90000000 };
    auto const result = TranslateToMidi1(perNoteBend, 2);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, result.Count);
    VERIFY_IS_TRUE(result.Dropped);
}

void MessageTranslationTests::Midi1BecomesMidi2()
{
    uint32_t const noteOff[1]{ 0x20903C00 };
    auto result = TranslateToMidi2(noteOff, 1);
    VERIFY_ARE_EQUAL(0x40803C00u, result.Messages[0][0]);
    VERIFY_ARE_EQUAL(0x80000000u, result.Messages[0][1]);

    uint32_t const note[1]{ 0x20903C40 };
    result = TranslateToMidi2(note, 1);
    VERIFY_ARE_EQUAL(0x40903C00u, result.Messages[0][0]);
    VERIFY_ARE_EQUAL(0x80000000u, result.Messages[0][1]);

    uint32_t const volume[1]{ 0x20B0077F };
    result = TranslateToMidi2(volume, 1);
    VERIFY_ARE_EQUAL(0x40B00700u, result.Messages[0][0]);
    VERIFY_ARE_EQUAL(0xFFFFFFFFu, result.Messages[0][1]);

    uint32_t const bend[1]{ 0x20E00040 };
    result = TranslateToMidi2(bend, 1);
    VERIFY_ARE_EQUAL(0x40E00000u, result.Messages[0][0]);
    VERIFY_ARE_EQUAL(0x80000000u, result.Messages[0][1]);

    uint32_t const program[1]{ 0x20C05100 };
    result = TranslateToMidi2(program, 1);
    VERIFY_ARE_EQUAL(0x40C00000u, result.Messages[0][0]);
    VERIFY_ARE_EQUAL(0x51000000u, result.Messages[0][1]);
}

namespace
{
    bool IsCutoff(SentMessage const& message) noexcept
    {
        return (message.Words[0] & 0xFFF0FF00u) == 0x40B04A00u;
    }

    Sequence WithCutoffLane(std::vector<LanePoint> points)
    {
        auto sequence = KeysSequence();
        auto lane = MakeLane(LaneTarget::Controller, 74, 0);
        lane.Points = std::move(points);
        sequence.Tracks[0].Lanes = { lane };
        return sequence;
    }
}

void PlaybackEngineTests::LanesStartWhereTheyAreAtThePlayhead()
{
    Fixture f{};
    f.Engine.SetSequence(Snapshot(WithCutoffLane({ LanePoint{ 0, 0, false }, LanePoint{ Bar, 0xFFFFFFFFu, false } })));
    f.Engine.Play(Bar / 2);

    // Half way up the ramp, sent as the music starts.
    auto const sent = f.Output.All();
    auto const first = std::find_if(sent.begin(), sent.end(), IsCutoff);

    VERIFY_IS_TRUE(first != sent.end());
    VERIFY_ARE_EQUAL(MusicStartsAt, first->Timestamp);
    VERIFY_ARE_EQUAL(0x80000000u, first->Words[1]);
}

void PlaybackEngineTests::TheLoopSendsLaneValuesAgain()
{
    // Low for half a bar, then a jump up. Without a fresh value at the loop's start the synth
    // would stay high every time round.
    Fixture f{};
    f.Engine.SetSequence(Snapshot(WithCutoffLane({ LanePoint{ 0, 0x10000000u, true }, LanePoint{ Bar / 2, 0xF0000000u, false } })));

    EngineSettings settings{};
    settings.LoopEnabled = true;
    settings.LoopStart = 0;
    settings.LoopEnd = Bar;
    f.Engine.SetSettings(settings);

    f.Engine.Play(0);
    RunUntil(f, At(Bar + Bar / 4));

    auto const sent = f.Output.All();
    auto const lowAt = [&sent](uint64_t timestamp)
    {
        return std::any_of(sent.begin(), sent.end(), [timestamp](SentMessage const& message)
        {
            return IsCutoff(message) && message.Words[1] == 0x10000000u && message.Timestamp == timestamp;
        });
    };

    VERIFY_IS_TRUE(lowAt(At(0)));
    VERIFY_IS_TRUE(lowAt(At(Bar)));

    VERIFY_IS_TRUE(std::any_of(sent.begin(), sent.end(), [](SentMessage const& message)
    {
        return IsCutoff(message) && message.Words[1] == 0xF0000000u && message.Timestamp == At(Bar / 2);
    }));
}

void PlaybackEngineTests::AMidi1DestinationGetsALaneAsMidi1()
{
    auto sequence = KeysSequence();
    auto lane = MakeLane(LaneTarget::Assignable, 7 * 128 + 54, 0);
    lane.Points = { LanePoint{ 0, 0x80000000u, false } };
    sequence.Tracks[0].Lanes = { lane };

    Fixture f{};
    f.Engine.SetDestinationLookup([](EndpointRef const&, uint8_t) { return DestinationInfo{ false, 0 }; });
    f.Engine.SetSequence(Snapshot(sequence));
    f.Engine.Play(0);

    // NRPN 7:54 at 14 bits, as four controllers, in order.
    auto const sent = f.Output.All();
    std::vector<uint32_t> words{};

    for (auto const& message : sent)
    {
        if (message.Timestamp == MusicStartsAt && (message.Words[0] & 0xFFF0FF00u) != 0x20903C00u)
        {
            words.push_back(message.Words[0]);
        }
    }

    VERIFY_ARE_EQUAL(size_t{ 4 }, words.size());
    VERIFY_ARE_EQUAL(0x20B06307u, words[0]);
    VERIFY_ARE_EQUAL(0x20B06236u, words[1]);
    VERIFY_ARE_EQUAL(0x20B00640u, words[2]);
    VERIFY_ARE_EQUAL(0x20B02600u, words[3]);
}

void PlaybackEngineTests::AnEditWhilePlayingSendsTheNewValue()
{
    auto sequence = WithCutoffLane({ LanePoint{ 0, 0x20000000u, false } });

    Fixture f{};
    f.Engine.SetSequence(Snapshot(sequence));
    f.Engine.Play(0);
    RunUntil(f, At(Bar / 4));

    // A flat lane moved up has no point or step coming, so the edit itself has to send it.
    sequence.Tracks[0].Lanes[0].Points[0].Value = 0xE0000000u;
    f.Engine.SetSequence(Snapshot(sequence));
    f.Output.Clear();
    RunUntil(f, At(Bar / 2));

    auto const sent = f.Output.All();
    VERIFY_IS_TRUE(std::any_of(sent.begin(), sent.end(), [](SentMessage const& message)
    {
        return IsCutoff(message) && message.Words[1] == 0xE0000000u;
    }));
}
