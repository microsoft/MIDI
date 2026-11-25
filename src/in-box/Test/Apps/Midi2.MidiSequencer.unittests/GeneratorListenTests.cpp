// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "GeneratorListenTests.h"
#include "SequenceTestData.h"

#include "GeneratorKinds.h"
#include "GeneratorListen.h"
#include "SequenceGenerator.h"
#include "SequenceRender.h"
#include "SequenceSerializer.h"

#include <algorithm>
#include <set>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    constexpr int64_t Bar = TicksPerQuarterNote * 4;
    constexpr int64_t Eighth = TicksPerQuarterNote / 2;

    // Up the C major scale from C4 to C5 in eighths: one bar.
    Clip ScaleClip(std::wstring id)
    {
        Clip clip{};
        clip.Id = std::move(id);
        clip.Kind = ClipKind::Notes;
        clip.Name = L"Scale";
        clip.Length = Bar;
        clip.Loop = true;

        constexpr std::array<uint8_t, 8> Numbers{ 60, 62, 64, 65, 67, 69, 71, 72 };

        for (size_t i = 0; i < Numbers.size(); ++i)
        {
            Note note{};
            note.Tick = static_cast<int64_t>(i) * Eighth;
            note.Length = Eighth;
            note.Number = Numbers[i];
            note.Velocity = 40000;
            clip.Notes.push_back(note);
        }

        return clip;
    }

    // A Learn clip that keeps listening, starting from a tune it learned once.
    Clip Listener(std::wstring id, ListenKind kind, std::wstring source, uint8_t tune, uint32_t seed)
    {
        auto generator = MakeSectionTwentyEightGenerator(GeneratorType::Learn);
        generator.LearnFrom = LearnSource::Clip;
        generator.Learned = TuneNotes(tune);
        generator.ListenTo = kind;
        generator.ListenId = std::move(source);
        generator.KeepListening = true;
        generator.Keep = 0;
        NormalizeGenerator(generator);

        Clip clip{};
        clip.Id = std::move(id);
        clip.Kind = ClipKind::Generator;
        clip.Name = L"Listener";
        clip.Length = Bar;
        clip.Loop = true;
        clip.Seed = seed;
        clip.Generator = generator;
        return clip;
    }

    Track TrackOf(std::wstring id, std::vector<Placement> timeline)
    {
        Track track{};
        track.Id = std::move(id);
        track.Name = L"Track";
        track.Destination.Endpoint = EndpointRef{ L"Synth", L"" };
        track.Timeline = std::move(timeline);
        return track;
    }

    // What one pass of a clip plays, hearing what it listens to in the sequence.
    std::vector<Note> PassNotes(Sequence const& sequence, Clip const& clip, int64_t passStart, uint64_t pass)
    {
        ListenContext const listen{ &sequence, nullptr };
        std::vector<Note> notes{};
        ForEachGeneratorNote(clip, passStart, passStart + clip.Length, pass, passStart, passStart + clip.Length, true,
            [&notes](Note const& note) { notes.push_back(note); }, &listen);
        return notes;
    }

    // Every move from one note to the next is one the learned notes make.
    bool FollowsMoves(GeneratorSettings const& generator, std::vector<Note> const& notes)
    {
        auto const moves = LearnedMoves(generator);

        for (size_t i = 1; i < notes.size(); ++i)
        {
            auto const from = ScaleDegree(generator, notes[i - 1].Number);
            auto const to = ScaleDegree(generator, notes[i].Number);

            if (from < 0 || to < 0 || moves[static_cast<size_t>(from)][static_cast<size_t>(to)] == 0)
            {
                return false;
            }
        }

        return true;
    }

    std::vector<uint8_t> Numbers(std::vector<Note> const& notes)
    {
        std::vector<uint8_t> numbers{};

        for (auto const& note : notes)
        {
            numbers.push_back(note.Number);
        }

        return numbers;
    }
}

void GeneratorListenTests::AClipIsHeardSideBySide()
{
    Sequence sequence{};
    sequence.Clips = { ScaleClip(L"c-scale") };
    ListenContext const listen{ &sequence, nullptr };

    // As if it started at tick 0: its notes, as long and as loud as they play.
    auto const first = NotesHeard(listen, ListenKind::Clip, L"c-scale", 0, Bar);
    VERIFY_IS_TRUE((std::vector<uint8_t>{ 60, 62, 64, 65, 67, 69, 71, 72 }) == Numbers(first));
    VERIFY_ARE_EQUAL(Eighth, first[0].Length);
    VERIFY_ARE_EQUAL(uint16_t{ 40000 }, first[0].Velocity);

    // It loops, so three bars on it plays them again.
    auto const later = NotesHeard(listen, ListenKind::Clip, L"c-scale", Bar * 3, Bar * 4);
    VERIFY_ARE_EQUAL(size_t{ 8 }, later.size());
    VERIFY_ARE_EQUAL(Bar * 3, later[0].Tick);

    // Half a bar hears half of it, and a clip that's gone hears nothing.
    VERIFY_ARE_EQUAL(size_t{ 4 }, NotesHeard(listen, ListenKind::Clip, L"c-scale", 0, Bar / 2).size());
    VERIFY_ARE_EQUAL(size_t{ 0 }, NotesHeard(listen, ListenKind::Clip, L"c-gone", 0, Bar).size());
}

void GeneratorListenTests::LearnPlaysWhatItHeard()
{
    Sequence sequence{};
    sequence.Clips = { ScaleClip(L"c-scale"), Listener(L"c-learn", ListenKind::Clip, L"c-scale", 1, 5) };
    auto const& learn = sequence.Clips[1];
    ListenContext const listen{ &sequence, nullptr };

    // The first pass has heard nothing yet, so it plays from what it learned last: Frère Jacques.
    VERIFY_IS_TRUE(HeardBefore(listen, learn, 0, 0).Learned.empty());
    auto const opening = PassNotes(sequence, learn, 0, 0);
    VERIFY_IS_TRUE(opening.size() >= 4);
    VERIFY_IS_TRUE(FollowsMoves(*learn.Generator, opening));

    // After that, the scale it heard: one step up at a time, the way a scale goes.
    auto const hearing = HeardBefore(listen, learn, Bar, 1);
    auto const& heard = hearing.Learned;
    VERIFY_ARE_EQUAL(size_t{ 8 }, heard.size());
    VERIFY_IS_TRUE(heard == LearnFromNotes(NotesHeard(listen, ListenKind::Clip, L"c-scale", 0, Bar), learn.Generator->StepTicks));

    // And the notes themselves, on its own steps from the start of the bar it heard.
    VERIFY_ARE_EQUAL(size_t{ 8 }, hearing.Notes.size());
    VERIFY_ARE_EQUAL(int64_t{ 0 }, hearing.Notes[0].Step);
    VERIFY_ARE_EQUAL(Eighth / learn.Generator->StepTicks, hearing.Notes[1].Step);
    VERIFY_ARE_EQUAL(uint8_t{ 72 }, hearing.Notes[7].Number);

    auto withHeard = *learn.Generator;
    withHeard.Learned = heard;

    for (uint64_t pass = 1; pass < 6; ++pass)
    {
        auto const notes = PassNotes(sequence, learn, static_cast<int64_t>(pass) * Bar, pass);
        VERIFY_IS_TRUE(notes.size() >= 4);
        VERIFY_IS_TRUE(FollowsMoves(withHeard, notes));
    }

    // Frère Jacques never goes from A to B, and the scale always does: listening changed it.
    VERIFY_IS_FALSE(FollowsMoves(*learn.Generator, PassNotes(sequence, learn, Bar, 1)));

    // Learned once, it plays what it learned on every pass.
    auto once = learn;
    once.Generator->KeepListening = false;
    VERIFY_IS_TRUE(FollowsMoves(*once.Generator, PassNotes(sequence, once, Bar * 3, 3)));
}

void GeneratorListenTests::ATrackIsHeardWhereItPlays()
{
    // The scale plays on Lead for the first two bars, then stops. The listener plays on a track of
    // its own for four bars, listening to Lead.
    Sequence sequence{};
    sequence.Clips = { ScaleClip(L"c-scale"), Listener(L"c-learn", ListenKind::Track, L"t-lead", 1, 9) };
    sequence.Tracks = {
        TrackOf(L"t-lead", { Placement{ L"c-scale", 0, Bar * 2 } }),
        TrackOf(L"t-learn", { Placement{ L"c-learn", 0, Bar * 4 } }) };
    NormalizeSequence(sequence);

    auto const& learn = *FindClip(sequence, L"c-learn");
    ListenContext const listen{ &sequence, nullptr };

    // Each pass hears the bar before it on Lead's timeline.
    VERIFY_ARE_EQUAL(size_t{ 8 }, NotesHeard(listen, ListenKind::Track, L"t-lead", Bar, Bar * 2).size());
    VERIFY_ARE_EQUAL(size_t{ 8 }, HeardBefore(listen, learn, Bar, 1).Learned.size());
    VERIFY_ARE_EQUAL(size_t{ 8 }, HeardBefore(listen, learn, Bar * 2, 2).Learned.size());

    // By the fourth bar, Lead has been quiet for a bar, so it plays from what it learned last.
    VERIFY_IS_TRUE(HeardBefore(listen, learn, Bar * 3, 3).Learned.empty());
    VERIFY_IS_TRUE(FollowsMoves(*learn.Generator, PassNotes(sequence, learn, Bar * 3, 3)));

    // Where it plays matters: a first pass placed in the second bar hears the first, and one
    // placed in the third hears nothing.
    VERIFY_ARE_EQUAL(size_t{ 8 }, HeardBefore(listen, learn, Bar * 2, 1).Learned.size());
    VERIFY_IS_TRUE(HeardBefore(listen, learn, Bar * 3, 1).Learned.empty());

    // The render path that playback and exports use hears the same.
    std::vector<RenderedMessage> messages{};
    RenderTrack(sequence, sequence.Tracks[1], 0, Bar * 4, messages);

    std::vector<Note> secondBar{};

    for (auto const& message : messages)
    {
        if (message.Kind == RenderedKind::NoteOn && message.Tick >= Bar && message.Tick < Bar * 2)
        {
            Note note{};
            note.Number = static_cast<uint8_t>((message.Words[0] >> 8) & 0x7F);
            secondBar.push_back(note);
        }
    }

    auto withHeard = *learn.Generator;
    withHeard.Learned = HeardBefore(listen, learn, Bar, 1).Learned;
    VERIFY_IS_TRUE(secondBar.size() >= 4);
    VERIFY_IS_TRUE(FollowsMoves(withHeard, secondBar));
}

void GeneratorListenTests::LoopsPlayTheSameColdOrInOrder()
{
    // A listens to B while B listens to A, each a pass behind. Copies of the same pair under other
    // ids find nothing the first worked out, so one plays in order and the others are asked cold.
    auto const pair = [](std::wstring a, std::wstring b)
    {
        Sequence sequence{};
        sequence.Clips = { Listener(a, ListenKind::Clip, b, 0, 11), Listener(b, ListenKind::Clip, a, 2, 12) };
        return sequence;
    };

    auto const inOrder = pair(L"c-a1", L"c-b1");
    auto const cold = pair(L"c-a2", L"c-b2");
    constexpr uint64_t Far = 150;

    std::vector<Note> last{};
    std::set<std::vector<uint8_t>> heard{};

    for (uint64_t pass = 0; pass <= Far; ++pass)
    {
        last = PassNotes(inOrder, inOrder.Clips[0], static_cast<int64_t>(pass) * Bar, pass);
        heard.insert(Numbers(last));
    }

    // It changes as it goes, and a pass asked for cold plays what it played in order.
    VERIFY_IS_TRUE(heard.size() >= 3);
    VERIFY_IS_TRUE(last == PassNotes(cold, cold.Clips[0], static_cast<int64_t>(Far) * Bar, Far));

    // Much further ahead, cold, without running out of room.
    auto const deep = pair(L"c-a3", L"c-b3");
    VERIFY_IS_TRUE(PassNotes(deep, deep.Clips[0], Bar * 1500, 1500).size() >= 2);
}

void GeneratorListenTests::LearnsOnceFromAGeneratorOrATrack()
{
    // A Life clip, which changes each pass, and the scale twice on a track, with a bar between.
    Clip life{};
    life.Id = L"c-life";
    life.Kind = ClipKind::Generator;
    life.Name = L"Life";
    life.Length = Bar;
    life.Loop = true;
    life.Seed = 3;
    life.Generator = MakeSectionTwentyEightGenerator(GeneratorType::Life);

    Sequence sequence{};
    sequence.Clips = { ScaleClip(L"c-scale"), life };
    sequence.Tracks = { TrackOf(L"t-lead", { Placement{ L"c-scale", 0, Bar }, Placement{ L"c-scale", Bar * 2, Bar } }) };
    NormalizeSequence(sequence);

    constexpr int64_t Sixteenth = TicksPerQuarterNote / 4;

    // A clip that's the same each pass is learned from one pass; one that changes, from four.
    VERIFY_ARE_EQUAL(size_t{ 8 }, LearnOnce(sequence, Sixteenth, ListenKind::Clip, L"c-scale").size());
    VERIFY_IS_TRUE(LearnOnce(sequence, Sixteenth, ListenKind::Clip, L"c-life").size() >= 8);

    // A track, from its timeline: both scales.
    VERIFY_ARE_EQUAL(size_t{ 16 }, LearnOnce(sequence, Sixteenth, ListenKind::Track, L"t-lead").size());

    // Nothing to learn from.
    VERIFY_IS_TRUE(LearnOnce(sequence, Sixteenth, ListenKind::Track, L"t-gone").empty());
    VERIFY_ARE_EQUAL(std::wstring{ L"Scale" }, ListenSourceName(sequence, ListenKind::Clip, L"c-scale"));
    VERIFY_ARE_EQUAL(std::wstring{ L"Track" }, ListenSourceName(sequence, ListenKind::Track, L"t-lead"));
}

void GeneratorListenTests::ListeningRoundTripsThroughTheFile()
{
    auto once = Listener(L"c-once", ListenKind::Clip, L"c-scale", 0, 4);
    once.Generator->KeepListening = false;

    Sequence sequence{};
    sequence.Clips = { ScaleClip(L"c-scale"), Listener(L"c-learn", ListenKind::Track, L"t-lead", 1, 9), once };
    sequence.Tracks = { TrackOf(L"t-lead", { Placement{ L"c-scale", 0, Bar } }) };
    NormalizeSequence(sequence);

    Sequence read{};
    auto const result = ReadSequenceJson(WriteSequenceJson(sequence), read);
    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_ARE_EQUAL(size_t{ 0 }, result.SkippedItems);

    for (size_t i = 1; i < sequence.Clips.size(); ++i)
    {
        VERIFY_IS_TRUE(SameGeneratorSettings(*sequence.Clips[i].Generator, *read.Clips[i].Generator));
    }

    VERIFY_IS_TRUE(read.Clips[1].Generator->ListenTo == ListenKind::Track);
    VERIFY_ARE_EQUAL(std::wstring{ L"t-lead" }, read.Clips[1].Generator->ListenId);
    VERIFY_IS_TRUE(read.Clips[1].Generator->KeepListening);
    VERIFY_IS_TRUE(read.Clips[2].Generator->ListenTo == ListenKind::Clip);
    VERIFY_IS_FALSE(read.Clips[2].Generator->KeepListening);

    // A kind it doesn't know listens to nothing.
    std::wstring const damaged = LR"({
  "fileVersion": 1,
  "tracks": [],
  "clips": [
    { "id": "c-x", "kind": "generator", "name": "Learn", "length": 3840, "loop": true,
      "settings": { "type": "learn", "from": "clip", "listen": "nonsense", "listenId": "c-y", "keepListening": true } }
  ]
})";

    Sequence broken{};
    VERIFY_IS_TRUE(ReadSequenceJson(damaged, broken).Succeeded());
    VERIFY_IS_TRUE(broken.Clips[0].Generator->ListenTo == ListenKind::None);
    VERIFY_IS_FALSE(broken.Clips[0].Generator->KeepListening);
}

namespace
{
    // A Life or Evolve clip listening to a clip, a pass behind, or learning once.
    Clip Carrier(std::wstring id, GeneratorType type, std::wstring source, uint32_t seed, bool keepListening)
    {
        auto generator = MakeSectionTwentyEightGenerator(type);
        generator.ChangesEachPass = true;
        generator.ListenTo = ListenKind::Clip;
        generator.ListenId = std::move(source);
        generator.KeepListening = keepListening;
        NormalizeGenerator(generator);

        Clip clip{};
        clip.Id = std::move(id);
        clip.Kind = ClipKind::Generator;
        clip.Name = L"Carrier";
        clip.Length = Bar;
        clip.Loop = true;
        clip.Seed = seed;
        clip.Generator = generator;
        return clip;
    }

    std::vector<std::pair<int64_t, uint8_t>> StepsAndNumbers(std::vector<StepNote> const& notes)
    {
        std::vector<std::pair<int64_t, uint8_t>> pairs{};

        for (auto const& note : notes)
        {
            pairs.emplace_back(note.Step, note.Number);
        }

        return pairs;
    }

    std::vector<std::pair<int64_t, uint8_t>> TicksAndNumbers(std::vector<Note> const& notes)
    {
        std::vector<std::pair<int64_t, uint8_t>> pairs{};

        for (auto const& note : notes)
        {
            pairs.emplace_back(note.Tick, note.Number);
        }

        std::sort(pairs.begin(), pairs.end());
        return pairs;
    }
}

void GeneratorListenTests::RingingRingsWhatItHeard()
{
    Sequence sequence{};
    sequence.Clips = { ScaleClip(L"c-scale") };

    auto ringing = MakeSectionTwentyEightGenerator(GeneratorType::ChangeRinging);
    NormalizeGenerator(ringing);
    auto const own = BellNotes(ringing);

    ringing.ListenTo = ListenKind::Clip;
    ringing.ListenId = L"c-scale";
    ringing.Learned = LearnOnce(sequence, ringing.StepTicks, ListenKind::Clip, L"c-scale");
    NormalizeGenerator(ringing);
    VERIFY_ARE_EQUAL(size_t{ 8 }, ringing.Learned.size());

    // A bell for each, every one a note it heard, in its key and range, the treble highest.
    auto const bells = BellNotes(ringing);
    auto const heard = LearnedPitches(ringing, ringing.Learned);
    VERIFY_ARE_EQUAL(static_cast<size_t>(ringing.Bells), bells.size());
    VERIFY_IS_TRUE(std::is_sorted(bells.rbegin(), bells.rend()));

    for (auto const bell : bells)
    {
        VERIFY_IS_TRUE(std::find(heard.begin(), heard.end(), bell) != heard.end());
    }

    // Without a source, its own bells again.
    ringing.ListenTo = ListenKind::None;
    VERIFY_IS_TRUE(BellNotes(ringing) == own);
}

void GeneratorListenTests::LifeDropsWhatItHears()
{
    // A drum lands on its own row, on the step it played; a step past the end wraps.
    auto drums = MakeSectionTwentyEightGenerator(GeneratorType::LifeDrums);
    NormalizeGenerator(drums);

    auto const rows = LifeRowNotes(drums);
    auto const rowOf = [&rows](uint8_t note) { return static_cast<uint16_t>(std::distance(rows.begin(), std::find(rows.begin(), rows.end(), note))); };

    std::vector<StepNote> const kit{ StepNote{ 0, 1, 36, 40000 }, StepNote{ 4, 1, 38, 40000 }, StepNote{ 2, 1, 42, 40000 }, StepNote{ 17, 1, 42, 40000 } };
    std::vector<LifeCell> expected{ LifeCell{ 0, rowOf(36) }, LifeCell{ 4, rowOf(38) }, LifeCell{ 2, rowOf(42) }, LifeCell{ 1, rowOf(42) } };
    std::sort(expected.begin(), expected.end());
    VERIFY_IS_TRUE(HeardCells(drums, kit, 16) == expected);

    // On a melodic board, the row of the note it is, three octaves down or up moved into the range.
    auto life = MakeSectionTwentyEightGenerator(GeneratorType::Life);
    NormalizeGenerator(life);
    auto const notes = LifeRowNotes(life);
    VERIFY_IS_TRUE(notes.size() >= 3);

    std::vector<StepNote> const melody{
        StepNote{ 3, 1, notes[2], 40000 },
        StepNote{ 5, 1, static_cast<uint8_t>(notes[2] - 36), 40000 },
        StepNote{ 7, 1, static_cast<uint8_t>(std::min(127, notes.back() + 36)), 40000 } };

    auto const cells = HeardCells(life, melody, 16);
    VERIFY_ARE_EQUAL(size_t{ 3 }, cells.size());
    VERIFY_ARE_EQUAL(uint16_t{ 2 }, cells[0].Row);
    VERIFY_ARE_EQUAL(uint16_t{ 2 }, cells[1].Row);
    VERIFY_IS_TRUE(notes[cells[2].Row] % 12 == notes.back() % 12 || notes[cells[2].Row] >= notes.back() - 2);
}

void GeneratorListenTests::LifeKeepsListening()
{
    // An empty board listening to the scale: everything on it was heard.
    auto const make = [](std::wstring id)
    {
        auto clip = Carrier(std::move(id), GeneratorType::Life, L"c-scale", 2, true);
        clip.Generator->Scatter = false;
        clip.Generator->LifeCells.clear();
        clip.Generator->Settled = LifeSettled::Hold;
        return clip;
    };

    Sequence sequence{};
    sequence.Clips = { ScaleClip(L"c-scale"), make(L"c-life1"), make(L"c-life2") };
    auto const& life = sequence.Clips[1];
    ListenContext const listen{ &sequence, nullptr };
    auto const hearing = MakePassHearing(listen, life, 0, 0);
    VERIFY_IS_TRUE(static_cast<bool>(hearing.Before));

    // The first pass has heard nothing, and the board started empty.
    auto const steps = GeneratorStepCount(life);
    auto const start = LifeBoardAt(*life.Generator, life.Seed, steps, 0, &hearing);
    VERIFY_IS_TRUE(std::all_of(start.Cells.begin(), start.Cells.end(), [](uint8_t cell) { return cell == 0; }));
    VERIFY_IS_TRUE(PassNotes(sequence, life, 0, 0).empty());

    // The next has the scale on it, new this pass, on the steps it played.
    auto const second = LifeBoardAt(*life.Generator, life.Seed, steps, 1, &hearing);
    auto const dropped = HeardCells(*life.Generator, HeardBefore(listen, life, Bar, 1).Notes, second.Columns);
    VERIFY_ARE_EQUAL(size_t{ 8 }, dropped.size());

    for (auto const& cell : dropped)
    {
        VERIFY_ARE_EQUAL(uint8_t{ 2 }, second.At(cell.Column, cell.Row));
    }

    // The scale keeps arriving, so every pass after the first plays.
    std::vector<Note> last{};

    for (uint64_t pass = 1; pass <= 70; ++pass)
    {
        last = PassNotes(sequence, life, Bar * static_cast<int64_t>(pass), pass);
        VERIFY_IS_FALSE(last.empty());
    }

    // A copy asked for cold plays what the first played in order.
    VERIFY_IS_TRUE(TicksAndNumbers(last) == TicksAndNumbers(PassNotes(sequence, sequence.Clips[2], Bar * 70, 70)));

    // Learned once, it doesn't hear anything after its start.
    auto once = life;
    once.Generator->KeepListening = false;
    VERIFY_IS_TRUE(PassNotes(sequence, once, Bar * 5, 5).empty());
}

void GeneratorListenTests::LifeLoopsPlayTheSameColdOrInOrder()
{
    // Two Life boards, each dropping what the other played a pass before onto its own.
    auto const pair = [](std::wstring a, std::wstring b)
    {
        Sequence sequence{};
        sequence.Clips = { Carrier(a, GeneratorType::Life, b, 21, true), Carrier(b, GeneratorType::Life, a, 22, true) };
        return sequence;
    };

    auto const inOrder = pair(L"c-la1", L"c-lb1");
    auto const cold = pair(L"c-la2", L"c-lb2");
    constexpr uint64_t Far = 100;

    std::vector<Note> last{};

    for (uint64_t pass = 0; pass <= Far; ++pass)
    {
        last = PassNotes(inOrder, inOrder.Clips[0], Bar * static_cast<int64_t>(pass), pass);
    }

    VERIFY_IS_TRUE(TicksAndNumbers(last) == TicksAndNumbers(PassNotes(cold, cold.Clips[0], Bar * static_cast<int64_t>(Far), Far)));

    // Much further ahead, cold, without locking up or running out of room.
    auto const deep = pair(L"c-la3", L"c-lb3");
    (void)PassNotes(deep, deep.Clips[0], Bar * 1500, 1500);
}

void GeneratorListenTests::EvolveSoundsLikeWhatItHears()
{
    Sequence sequence{};
    sequence.Clips = { ScaleClip(L"c-scale") };

    auto evolve = Carrier(L"c-evolve", GeneratorType::Evolve, L"c-scale", 7, false).Generator.value();
    evolve.Learned = LearnOnce(sequence, evolve.StepTicks, ListenKind::Clip, L"c-scale");

    // It starts from what it learned: the scale's steps, in its own key and range.
    auto const start = EvolveFamilyAt(evolve, 7, 16, 0);
    VERIFY_ARE_EQUAL(size_t{ 8 }, start.Target.size());
    VERIFY_IS_TRUE(StepsAndNumbers(start.Parent) == StepsAndNumbers(start.Target));
    VERIFY_ARE_EQUAL(1.0, start.Rules[5]);

    // Aiming only for it keeps it closer than not aiming for it at all.
    auto aimed = evolve;
    aimed.Weights = { 0, 0, 0, 0, 0 };
    aimed.SoundsLike = 100;

    auto loose = evolve;
    loose.SoundsLike = 0;

    double closer{ 0 };
    double further{ 0 };

    for (uint64_t pass = 20; pass <= 40; ++pass)
    {
        closer += EvolveFamilyAt(aimed, 7, 16, pass).Rules[5];
        further += EvolveFamilyAt(loose, 7, 16, pass).Rules[5];
    }

    VERIFY_IS_TRUE(closer > further);

    // Without a source there's nothing to sound like.
    auto alone = evolve;
    alone.ListenTo = ListenKind::None;
    VERIFY_IS_TRUE(EvolveFamilyAt(alone, 7, 16, 3).Target.empty());
}

void GeneratorListenTests::EvolveAimsAtWhatItHeardEachPass()
{
    // The scale plays on Lead for the first bar only, and Evolve keeps listening to Lead.
    Sequence sequence{};
    auto listener = Carrier(L"c-evolve", GeneratorType::Evolve, L"t-lead", 3, true);
    listener.Generator->ListenTo = ListenKind::Track;
    sequence.Clips = { ScaleClip(L"c-scale"), listener };
    sequence.Tracks = {
        TrackOf(L"t-lead", { Placement{ L"c-scale", 0, Bar } }),
        TrackOf(L"t-evolve", { Placement{ L"c-evolve", 0, Bar * 4 } }) };
    NormalizeSequence(sequence);

    auto const& evolve = *FindClip(sequence, L"c-evolve");
    ListenContext const listen{ &sequence, nullptr };
    auto const hearing = MakePassHearing(listen, evolve, 0, 0);

    // The second pass aims at the scale it heard; the fourth heard nothing, and learned nothing.
    auto const second = EvolveFamilyAt(*evolve.Generator, evolve.Seed, GeneratorStepCount(evolve), 1, &hearing);
    VERIFY_ARE_EQUAL(size_t{ 8 }, second.Target.size());
    VERIFY_IS_TRUE(EvolveFamilyAt(*evolve.Generator, evolve.Seed, GeneratorStepCount(evolve), 3, &hearing).Target.empty());

    // What it plays is the copy it chose, as playback hears it.
    auto const notes = PassNotes(sequence, evolve, Bar, 1);
    std::vector<uint8_t> chosen{};

    for (auto const& note : second.Children[second.Chosen])
    {
        chosen.push_back(note.Number);
    }

    VERIFY_IS_TRUE(chosen == Numbers(notes));
}

void GeneratorListenTests::ATrackListenerKeepsOneRunEveryPass()
{
    // Life keeps listening to Lead from a placement that starts at bar 2.
    Sequence sequence{};
    auto listener = Carrier(L"c-life", GeneratorType::Life, L"t-lead", 5, true);
    listener.Generator->ListenTo = ListenKind::Track;
    sequence.Clips = { ScaleClip(L"c-scale"), listener };
    sequence.Tracks = {
        TrackOf(L"t-lead", { Placement{ L"c-scale", 0, Bar } }),
        TrackOf(L"t-life", { Placement{ L"c-life", Bar, Bar * 4 } }) };
    NormalizeSequence(sequence);

    auto const& life = *FindClip(sequence, L"c-life");
    ListenContext const listen{ &sequence, nullptr };

    // Every pass hears from the same first pass, so one run carries on through them all, past the
    // passes worked out one by one too, rather than starting again each pass.
    auto const runAt = [&](uint64_t pass)
    {
        return MakePassHearing(listen, life, Bar + static_cast<int64_t>(pass) * life.Length, pass).RunKey;
    };

    auto const first = runAt(1);
    VERIFY_ARE_EQUAL(first, runAt(2));
    VERIFY_ARE_EQUAL(first, runAt(MaximumSteppedPasses));
    VERIFY_ARE_EQUAL(first, runAt(MaximumSteppedPasses + 1));
    VERIFY_ARE_EQUAL(first, runAt(MaximumSteppedPasses + 2));
}

void GeneratorListenTests::ListenersKeepWhatTheyLearnedInTheFile()
{
    Sequence sequence{};
    sequence.Clips = { ScaleClip(L"c-scale") };

    auto evolve = Carrier(L"c-evolve", GeneratorType::Evolve, L"c-scale", 7, false);
    evolve.Generator->Learned = LearnOnce(sequence, evolve.Generator->StepTicks, ListenKind::Clip, L"c-scale");
    evolve.Generator->LearnedFrom = L"Scale";
    evolve.Generator->SoundsLike = 35;

    auto ringing = Carrier(L"c-ringing", GeneratorType::ChangeRinging, L"c-scale", 1, false);
    ringing.Generator->Learned = LearnOnce(sequence, ringing.Generator->StepTicks, ListenKind::Clip, L"c-scale");
    ringing.Generator->LearnedFrom = L"Scale";

    auto life = Carrier(L"c-life", GeneratorType::Life, L"c-scale", 1, true);
    life.Generator->LearnedFrom = L"Scale";

    sequence.Clips.push_back(evolve);
    sequence.Clips.push_back(ringing);
    sequence.Clips.push_back(life);
    NormalizeSequence(sequence);

    Sequence read{};
    auto const result = ReadSequenceJson(WriteSequenceJson(sequence), read);
    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_ARE_EQUAL(size_t{ 0 }, result.SkippedItems);

    for (size_t i = 1; i < sequence.Clips.size(); ++i)
    {
        VERIFY_IS_TRUE(SameGeneratorSettings(*sequence.Clips[i].Generator, *read.Clips[i].Generator));
    }

    VERIFY_ARE_EQUAL(uint8_t{ 35 }, read.Clips[1].Generator->SoundsLike);
    VERIFY_ARE_EQUAL(size_t{ 8 }, read.Clips[1].Generator->Learned.size());
    VERIFY_ARE_EQUAL(size_t{ 8 }, read.Clips[2].Generator->Learned.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"Scale" }, read.Clips[3].Generator->LearnedFrom);
}