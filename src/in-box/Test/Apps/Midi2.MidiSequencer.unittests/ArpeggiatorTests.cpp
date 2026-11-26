// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "ArpeggiatorTests.h"

#include "PlaybackEngine.h"
#include "SequenceArpeggiator.h"
#include "SequenceGenerator.h"
#include "SequenceRender.h"
#include "SequenceSerializer.h"

#include <windows.h>

#include <algorithm>
#include <mutex>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    constexpr int64_t Beat = TicksPerQuarterNote;
    constexpr int64_t Bar = Beat * 4;
    constexpr int64_t Sixteenth = Beat / 4;

    std::vector<uint8_t> Numbers(std::vector<HeldNote> const& notes)
    {
        std::vector<uint8_t> numbers{};

        for (auto const& note : notes)
        {
            numbers.push_back(note.Number);
        }

        return numbers;
    }

    bool SameNumbers(std::vector<HeldNote> const& notes, std::initializer_list<uint8_t> expected)
    {
        return Numbers(notes) == std::vector<uint8_t>{ expected };
    }

    Clip ArpeggiatorClip(ArpeggiatorSource source, std::wstring trackId, int64_t stepTicks)
    {
        Clip clip{};
        clip.Id = L"c-arp";
        clip.Name = L"Arp";
        clip.Kind = ClipKind::Generator;
        clip.Length = Bar;
        clip.Loop = true;
        clip.Seed = 7;
        clip.Generator = MakeArpeggiatorGenerator();
        clip.Generator->ArpSource = source;
        clip.Generator->ArpTrackId = std::move(trackId);
        clip.Generator->ArpOctaves = 1;
        clip.Generator->StepTicks = stepTicks;
        clip.Generator->NoteLength = 50;
        return clip;
    }

    Track PlacedTrack(std::wstring id, std::wstring const& clipId)
    {
        Track track{};
        track.Id = std::move(id);
        track.Name = track.Id;
        track.Destination.Endpoint = EndpointRef{ L"Synth", L"" };
        track.Timeline.push_back(Placement{ clipId, 0, Bar });
        return track;
    }

    // Half a bar of C major, then half a bar of F major.
    Clip ChordsClip()
    {
        Clip clip{};
        clip.Id = L"c-chords";
        clip.Name = L"Chords";
        clip.Length = Bar;
        clip.Loop = false;

        auto const add = [&clip](int64_t tick, uint8_t number)
        {
            Note note{};
            note.Tick = tick;
            note.Length = Bar / 2;
            note.Number = number;
            note.Velocity = static_cast<uint16_t>(0x8000 + number);
            clip.Notes.push_back(note);
        };

        for (auto const number : { 60, 64, 67 })
        {
            add(0, static_cast<uint8_t>(number));
        }

        for (auto const number : { 65, 69, 72 })
        {
            add(Bar / 2, static_cast<uint8_t>(number));
        }

        SortClip(clip);
        return clip;
    }

    Sequence ArpeggiatorSequenceWith(ArpeggiatorSource source, int64_t stepTicks)
    {
        Sequence sequence{};
        sequence.Tempo = { TempoPoint{ 0, 120.0, false } };
        sequence.Clips.push_back(ArpeggiatorClip(source, L"t-chords", stepTicks));
        sequence.Clips.push_back(ChordsClip());
        sequence.Tracks.push_back(PlacedTrack(L"t-arp", L"c-arp"));
        sequence.Tracks.push_back(PlacedTrack(L"t-chords", L"c-chords"));
        return sequence;
    }

    struct PlayedNote
    {
        int64_t Tick{ 0 };
        uint8_t Number{ 0 };
        uint16_t Velocity{ 0 };
    };

    std::vector<PlayedNote> PlayedNotes(std::vector<RenderedMessage> const& messages)
    {
        std::vector<PlayedNote> notes{};

        for (auto const& message : messages)
        {
            if (message.Kind == RenderedKind::NoteOn)
            {
                notes.push_back(PlayedNote{ message.Tick, static_cast<uint8_t>((message.Words[0] >> 8) & 0x7F), static_cast<uint16_t>(message.Words[1] >> 16) });
            }
        }

        return notes;
    }

    std::vector<PlayedNote> Render(Sequence const& sequence, int64_t from, int64_t to, ArpeggiatorInput* keys)
    {
        RenderOptions options{};
        options.Arpeggiator = keys;

        std::vector<RenderedMessage> messages{};
        RenderTrack(sequence, sequence.Tracks[0], from, to, messages, options);
        return PlayedNotes(messages);
    }

    void VerifyNumbers(std::vector<PlayedNote> const& notes, std::initializer_list<uint8_t> expected)
    {
        VERIFY_ARE_EQUAL(expected.size(), notes.size());

        size_t i{ 0 };

        for (auto const number : expected)
        {
            VERIFY_ARE_EQUAL(number, notes[i++].Number);
        }
    }

    // The engine, with a clock that counts microseconds and an output that keeps what it's given.
    class Recorder final : public IEngineOutput
    {
    public:
        void Send(EndpointRef const&, uint64_t timestamp, uint32_t const* words, uint8_t wordCount) noexcept override
        {
            try
            {
                std::scoped_lock guard{ m_lock };

                if (wordCount >= 2 && (words[0] >> 28) == 0x4 && ((words[0] >> 20) & 0x0F) == 0x9)
                {
                    m_ons.emplace_back(timestamp, static_cast<uint8_t>((words[0] >> 8) & 0x7F));
                }
            }
            catch (...)
            {
            }
        }

        std::vector<std::pair<uint64_t, uint8_t>> NoteOns()
        {
            std::scoped_lock guard{ m_lock };
            return m_ons;
        }

    private:
        std::mutex m_lock{};
        std::vector<std::pair<uint64_t, uint8_t>> m_ons{};
    };
}

void ArpeggiatorTests::OrdersStepThroughTheChord()
{
    // Held E, then C, then G.
    std::vector<HeldNote> const chord{ { 64, 100 }, { 60, 200 }, { 67, 300 } };

    VERIFY_IS_TRUE(SameNumbers(ArpeggiatorSequence(chord, ArpeggiatorOrder::Up, 1), { 60, 64, 67 }));
    VERIFY_IS_TRUE(SameNumbers(ArpeggiatorSequence(chord, ArpeggiatorOrder::Down, 1), { 67, 64, 60 }));
    VERIFY_IS_TRUE(SameNumbers(ArpeggiatorSequence(chord, ArpeggiatorOrder::AsHeld, 2), { 64, 60, 67, 76, 72, 79 }));
    VERIFY_IS_TRUE(SameNumbers(ArpeggiatorSequence(chord, ArpeggiatorOrder::Random, 1), { 60, 64, 67 }));

    // Up and down doesn't play the top or the bottom twice in a row.
    VERIFY_IS_TRUE(SameNumbers(ArpeggiatorSequence(chord, ArpeggiatorOrder::UpAndDown, 2), { 60, 64, 67, 72, 76, 79, 76, 72, 67, 64 }));
    VERIFY_IS_TRUE(SameNumbers(ArpeggiatorSequence({ { 60, 1 }, { 64, 1 } }, ArpeggiatorOrder::UpAndDown, 1), { 60, 64 }));

    // Velocities stay with their notes.
    auto const up = ArpeggiatorSequence(chord, ArpeggiatorOrder::Up, 2);
    VERIFY_ARE_EQUAL(uint16_t{ 200 }, up[0].Velocity);
    VERIFY_ARE_EQUAL(uint16_t{ 200 }, up[3].Velocity);

    // Nothing above 127, a note held twice counts once at its loudest, and octaves stay in range.
    VERIFY_IS_TRUE(SameNumbers(ArpeggiatorSequence({ { 120, 1 } }, ArpeggiatorOrder::Up, 2), { 120 }));

    auto const twice = ArpeggiatorSequence({ { 60, 10 }, { 60, 90 } }, ArpeggiatorOrder::Up, 1);
    VERIFY_ARE_EQUAL(size_t{ 1 }, twice.size());
    VERIFY_ARE_EQUAL(uint16_t{ 90 }, twice[0].Velocity);

    VERIFY_ARE_EQUAL(size_t{ 3 * MaximumArpeggiatorOctaves }, ArpeggiatorSequence(chord, ArpeggiatorOrder::Up, 200).size());
    VERIFY_IS_TRUE(ArpeggiatorSequence({}, ArpeggiatorOrder::Up, 2).empty());
}

void ArpeggiatorTests::HeldKeysPlayFromTheFirstStep()
{
    auto const sequence = ArpeggiatorSequenceWith(ArpeggiatorSource::Held, Sixteenth);

    // Nothing to play from: an export of a held arpeggiator has no notes.
    VERIFY_IS_TRUE(Render(sequence, 0, Bar, nullptr).empty());

    ArpeggiatorInput keys{};
    keys.Press(67, 0xA000);
    keys.Press(60, 0xC000);

    auto notes = Render(sequence, 0, Bar / 4, &keys);
    VerifyNumbers(notes, { 60, 67, 60, 67 });
    VERIFY_ARE_EQUAL(int64_t{ 0 }, notes[0].Tick);
    VERIFY_ARE_EQUAL(Sixteenth * 3, notes[3].Tick);
    VERIFY_ARE_EQUAL(uint16_t{ 0xC000 }, notes[0].Velocity);
    VERIFY_ARE_EQUAL(uint16_t{ 0xA000 }, notes[1].Velocity);
    VERIFY_IS_TRUE(keys.Used);
    VERIFY_ARE_EQUAL(uint64_t{ 4 }, keys.StepsPlayed);

    // The next window carries on where that one left off.
    notes = Render(sequence, Bar / 4, Bar / 2, &keys);
    VerifyNumbers(notes, { 60, 67, 60, 67 });

    // Let go, and it stops; nothing held still counts as playing from the keys.
    keys.Release(60);
    keys.Release(67);
    keys.Used = false;
    VERIFY_IS_TRUE(Render(sequence, Bar / 2, Bar, &keys).empty());
    VERIFY_IS_TRUE(keys.Used);

    // A new chord starts at the start of its order.
    keys.Press(72, 0x9000);
    keys.Press(64, 0x9000);
    VERIFY_ARE_EQUAL(uint64_t{ 0 }, keys.StepsPlayed);
    VerifyNumbers(Render(sequence, Bar / 2, Bar / 2 + Sixteenth * 3, &keys), { 64, 72, 64 });
}

void ArpeggiatorTests::LatchKeepsTheLastKeys()
{
    auto sequence = ArpeggiatorSequenceWith(ArpeggiatorSource::Held, Sixteenth);
    sequence.Clips[0].Generator->ArpLatch = true;
    sequence.Clips[0].Generator->ArpOrder = ArpeggiatorOrder::Down;

    ArpeggiatorInput keys{};
    keys.Press(60, 0xC000);
    keys.Press(64, 0xC000);
    keys.Release(60);
    keys.Release(64);

    VerifyNumbers(Render(sequence, 0, Sixteenth * 3, &keys), { 64, 60, 64 });

    // Keys added while others are down join the chord; the first key after letting go starts a
    // new one.
    keys.Press(67, 0xC000);
    keys.Press(69, 0xC000);
    keys.Release(67);
    VerifyNumbers(Render(sequence, Sixteenth * 3, Sixteenth * 5, &keys), { 69, 67 });

    keys.Release(69);
    keys.Press(48, 0xC000);
    VerifyNumbers(Render(sequence, Sixteenth * 5, Sixteenth * 7, &keys), { 48, 48 });

    // Stopping playback ends what was latched.
    keys.Release(48);
    keys.Restart();
    VERIFY_IS_TRUE(Render(sequence, Sixteenth * 7, Bar, &keys).empty());
}

void ArpeggiatorTests::AnotherTracksChordsPlay()
{
    auto const sequence = ArpeggiatorSequenceWith(ArpeggiatorSource::Track, Beat / 2);

    // Each chord starts at the start of the order, with the velocities its notes have.
    auto const notes = Render(sequence, 0, Bar, nullptr);
    VerifyNumbers(notes, { 60, 64, 67, 60, 65, 69, 72, 65 });
    VERIFY_ARE_EQUAL(Bar / 2, notes[4].Tick);
    VERIFY_ARE_EQUAL(uint16_t{ 0x8000 + 64 }, notes[1].Velocity);

    // The same notes, rendered in pieces, as any other clip.
    std::vector<PlayedNote> pieces{};

    for (int64_t from = 0; from < Bar; from += 300)
    {
        auto const part = Render(sequence, from, std::min(Bar, from + 300), nullptr);
        pieces.insert(pieces.end(), part.begin(), part.end());
    }

    VERIFY_ARE_EQUAL(notes.size(), pieces.size());

    for (size_t i = 0; i < notes.size(); ++i)
    {
        VERIFY_ARE_EQUAL(notes[i].Tick, pieces[i].Tick);
        VERIFY_ARE_EQUAL(notes[i].Number, pieces[i].Number);
    }

    // What's sounding at a tick, as the chord began.
    auto const chord = NotesSoundingAt(sequence, sequence.Tracks[1], Bar / 2 + 10);
    VERIFY_IS_TRUE(SameNumbers(chord.Notes, { 65, 69, 72 }));
    VERIFY_ARE_EQUAL(Bar / 2, chord.StartTick);
    VERIFY_IS_TRUE(NotesSoundingAt(sequence, sequence.Tracks[1], Bar + 10).Notes.empty());

    // An arpeggiator never follows another one, or a track that's gone.
    auto following = sequence;
    following.Clips[0].Generator->ArpTrackId = L"t-arp";
    VERIFY_IS_TRUE(Render(following, 0, Bar, nullptr).empty());

    following.Clips[0].Generator->ArpTrackId = L"t-gone";
    VERIFY_IS_TRUE(Render(following, 0, Bar, nullptr).empty());
}

void ArpeggiatorTests::MakeNotesKeepsWhatItPlayed()
{
    auto sequence = ArpeggiatorSequenceWith(ArpeggiatorSource::Track, Beat / 2);
    sequence.Tracks[0].Timeline[0].Tick = Bar / 4;
    sequence.Tracks[0].Timeline[0].Length = Bar / 2;

    auto const& arpeggiator = sequence.Clips[0];
    auto const made = MakeNotesFromArpeggiator(sequence, arpeggiator, sequence.Tracks[0].Timeline[0]);

    VERIFY_ARE_EQUAL(static_cast<int>(ClipKind::Notes), static_cast<int>(made.Kind));
    VERIFY_ARE_EQUAL(Bar / 2, made.Length);
    VERIFY_IS_FALSE(made.Loop);
    VERIFY_ARE_EQUAL(static_cast<int>(ClipOrigin::Generated), static_cast<int>(made.Origin));

    // From a beat in: the second half of the C chord's order, then the F chord from its start.
    VERIFY_ARE_EQUAL(size_t{ 4 }, made.Notes.size());
    VERIFY_ARE_EQUAL(int64_t{ 0 }, made.Notes[0].Tick);
    VERIFY_ARE_EQUAL(uint8_t{ 67 }, made.Notes[0].Number);
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, made.Notes[1].Number);
    VERIFY_ARE_EQUAL(Bar / 4, made.Notes[2].Tick);
    VERIFY_ARE_EQUAL(uint8_t{ 65 }, made.Notes[2].Number);

    // One that plays held keys has nothing to keep.
    auto held = arpeggiator;
    held.Generator->ArpSource = ArpeggiatorSource::Held;
    VERIFY_IS_TRUE(MakeNotesFromArpeggiator(sequence, held, sequence.Tracks[0].Timeline[0]).Notes.empty());
}

void ArpeggiatorTests::ArpeggiatorSettingsSurviveTheFile()
{
    auto original = ArpeggiatorSequenceWith(ArpeggiatorSource::Track, Beat / 2);
    auto& generator = *original.Clips[0].Generator;
    generator.ArpOrder = ArpeggiatorOrder::UpAndDown;
    generator.ArpOctaves = 3;
    generator.ArpLatch = true;
    NormalizeSequence(original);

    auto const text = WriteSequenceJson(original);

    Sequence read{};
    VERIFY_IS_TRUE(ReadSequenceJson(text, read).Succeeded());

    auto const clip = FindClip(read, L"c-arp");
    VERIFY_IS_NOT_NULL(clip);
    VERIFY_IS_TRUE(clip->Generator.has_value());
    VERIFY_ARE_EQUAL(static_cast<int>(GeneratorType::Arpeggiator), static_cast<int>(clip->Generator->Type));
    VERIFY_ARE_EQUAL(static_cast<int>(ArpeggiatorSource::Track), static_cast<int>(clip->Generator->ArpSource));
    VERIFY_ARE_EQUAL(std::wstring{ L"t-chords" }, clip->Generator->ArpTrackId);
    VERIFY_ARE_EQUAL(static_cast<int>(ArpeggiatorOrder::UpAndDown), static_cast<int>(clip->Generator->ArpOrder));
    VERIFY_ARE_EQUAL(uint8_t{ 3 }, clip->Generator->ArpOctaves);
    VERIFY_IS_TRUE(clip->Generator->ArpLatch);
    VERIFY_IS_TRUE(clip->Generator->Unknown == nullptr || !clip->Generator->Unknown.HasKey(L"order"));
    VERIFY_ARE_EQUAL(text, WriteSequenceJson(read));

    // Out of range or unknown, it falls back to what a new arpeggiator has.
    auto const damaged = text.find(L"\"upAndDown\"");
    VERIFY_IS_TRUE(damaged != std::wstring::npos);

    auto changed = text;
    changed.replace(damaged, 11, L"\"sideways\"");

    Sequence fallback{};
    VERIFY_IS_TRUE(ReadSequenceJson(changed, fallback).Succeeded());
    VERIFY_ARE_EQUAL(static_cast<int>(ArpeggiatorOrder::Up), static_cast<int>(FindClip(fallback, L"c-arp")->Generator->ArpOrder));
}

void ArpeggiatorTests::TheEngineArpeggiatesKeysAsTheyArePlayed()
{
    // At 120 BPM the fake clock's quarter note is 500,000 microseconds, and the music starts
    // 20,000 after Play.
    uint64_t now{ 1000000 };
    Recorder output{};
    PlaybackEngine engine{ output, EngineClock{ [&now]() { return now; }, 1000000 } };

    auto sequence = ArpeggiatorSequenceWith(ArpeggiatorSource::Held, Sixteenth);
    sequence.Tracks.pop_back();
    engine.SetSequence(std::make_shared<Sequence const>(sequence));

    // Keys held before Play are there when it starts.
    engine.PressKey(L"t-arp", 67, 0xC000);
    engine.PressKey(L"t-arp", 60, 0xC000);
    engine.Play(0);

    VERIFY_IS_TRUE(engine.IsArpeggiating(L"t-arp"));
    VERIFY_IS_FALSE(engine.IsArpeggiating(L"t-other"));

    now = 1020000 + 500000;
    engine.Sweep();

    auto const ons = output.NoteOns();
    VERIFY_ARE_EQUAL(size_t{ 5 }, ons.size());
    VERIFY_ARE_EQUAL(uint64_t{ 1020000 }, ons[0].first);
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, ons[0].second);
    VERIFY_ARE_EQUAL(uint8_t{ 67 }, ons[1].second);
    VERIFY_ARE_EQUAL(uint64_t{ 1020000 + 125000 }, ons[1].first);
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, ons[4].second);

    // What it played is kept for recording, once, with the times it was handed over.
    auto const played = engine.TakeArpeggiated();
    auto const playedOns = std::count_if(played.begin(), played.end(), [](ArpeggiatedNote const& note)
        {
            return note.TrackId == L"t-arp" && ((note.Words[0] >> 20) & 0xF) == 0x9;
        });

    VERIFY_ARE_EQUAL(ptrdiff_t{ 5 }, playedOns);
    VERIFY_ARE_EQUAL(uint64_t{ 1020000 }, played.front().Timestamp);
    VERIFY_IS_TRUE(engine.TakeArpeggiated().empty());

    // Let go of both, and the steps after that play nothing.
    engine.ReleaseKey(L"t-arp", 60);
    engine.ReleaseKey(L"t-arp", 67);
    now += 500000;
    engine.Sweep();
    VERIFY_ARE_EQUAL(size_t{ 5 }, output.NoteOns().size());

    engine.Stop();
    VERIFY_IS_FALSE(engine.IsArpeggiating(L"t-arp"));

    engine.ForgetKeys(L"t-arp");
}
