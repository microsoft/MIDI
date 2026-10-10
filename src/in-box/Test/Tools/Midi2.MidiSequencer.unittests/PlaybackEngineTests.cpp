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
