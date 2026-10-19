// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "GeneratorKindsTests.h"
#include "SequenceTestData.h"

#include "GeneratorKinds.h"
#include "SequenceGenerator.h"
#include "SequenceRender.h"
#include "SequenceSerializer.h"

#include <algorithm>
#include <numeric>
#include <set>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    constexpr int64_t Bar = TicksPerQuarterNote * 4;

    std::set<std::pair<int32_t, int32_t>> Alive(LifeBoard const& board)
    {
        std::set<std::pair<int32_t, int32_t>> alive{};

        for (int32_t row = 0; row < board.Rows; ++row)
        {
            for (int32_t column = 0; column < board.Columns; ++column)
            {
                if (board.At(column, row) != 0)
                {
                    alive.emplace(column, row);
                }
            }
        }

        return alive;
    }

    // Life in C major from C3 to C5: fifteen rows.
    GeneratorSettings FifteenRows(std::vector<LifeCell> cells, LifeSettled settled)
    {
        auto life = MakeSectionTwentyEightGenerator(GeneratorType::Life);
        life.ScaleRoot = 0;
        life.Scale = ScaleKind::Major;
        life.LowNote = 48;
        life.HighNote = 72;
        life.Scatter = false;
        life.Settled = settled;
        life.LifeCells = std::move(cells);
        return life;
    }

    bool InKeyAndRange(GeneratorSettings const& generator, std::vector<StepNote> const& notes)
    {
        auto const ladder = ScaleNotes(generator.ScaleRoot, generator.Scale, generator.LowNote, generator.HighNote);

        return std::all_of(notes.begin(), notes.end(), [&](StepNote const& note)
        {
            return std::find(ladder.begin(), ladder.end(), note.Number) != ladder.end();
        });
    }

    Sequence OneClip(GeneratorSettings generator, int64_t bars, uint32_t seed)
    {
        Sequence sequence{};
        sequence.Tempo = { TempoPoint{ 0, 120.0, false } };

        Clip clip{};
        clip.Id = L"c-g";
        clip.Kind = ClipKind::Generator;
        clip.Name = L"Generated";
        clip.Seed = seed;
        clip.Length = Bar;
        clip.Loop = true;
        clip.Generator = std::move(generator);
        sequence.Clips = { clip };

        Track track{};
        track.Id = L"t-1";
        track.Name = L"Track";
        track.Destination.Endpoint = EndpointRef{ L"Synth", L"" };
        track.Timeline = { Placement{ L"c-g", 0, bars * Bar } };
        sequence.Tracks = { track };

        NormalizeSequence(sequence);
        return sequence;
    }

    std::vector<RenderedMessage> NoteOns(Sequence const& sequence, int64_t from, int64_t to)
    {
        std::vector<RenderedMessage> messages{};
        RenderTrack(sequence, sequence.Tracks[0], from, to, messages, RenderOptions{});
        std::erase_if(messages, [](RenderedMessage const& message) { return message.Kind != RenderedKind::NoteOn; });
        return messages;
    }

    constexpr std::array<GeneratorType, 8> NewTypes{
        GeneratorType::Life, GeneratorType::LifeDrums, GeneratorType::Learn, GeneratorType::Words,
        GeneratorType::Evolve, GeneratorType::Infinity, GeneratorType::LSystem, GeneratorType::ChangeRinging };
}

void GeneratorKindsTests::ABlinkerBlinks()
{
    auto const life = FifteenRows(LifeShapeCells(LifeShape::Blinker, 16, 15), LifeSettled::Hold);

    auto const first = LifeBoardAt(life, 1, 16, 0);
    auto const second = LifeBoardAt(life, 1, 16, 1);
    auto const third = LifeBoardAt(life, 1, 16, 2);

    VERIFY_ARE_EQUAL(15, first.Rows);
    VERIFY_ARE_EQUAL(16, first.Columns);

    // Across, then up and down, then across again.
    auto const across = Alive(first);
    auto const upright = Alive(second);
    VERIFY_ARE_EQUAL(size_t{ 3 }, across.size());
    VERIFY_ARE_EQUAL(size_t{ 3 }, upright.size());
    VERIFY_IS_TRUE(std::all_of(across.begin(), across.end(), [&](auto const& cell) { return cell.second == across.begin()->second; }));
    VERIFY_IS_TRUE(std::all_of(upright.begin(), upright.end(), [&](auto const& cell) { return cell.first == upright.begin()->first; }));

    VERIFY_IS_TRUE(across == Alive(third));
    VERIFY_IS_FALSE(second.Settled);

    // Back where it was two passes ago: it has settled, and holding keeps it blinking.
    VERIFY_IS_TRUE(third.Settled);
    VERIFY_IS_TRUE(upright == Alive(LifeBoardAt(life, 1, 16, 3)));

    // Every cell of the start is born; after that only the new ones are.
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, first.At(across.begin()->first, across.begin()->second));
    auto const middle = *std::next(upright.begin());
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, second.At(middle.first, middle.second));
}

void GeneratorKindsTests::AGliderWrapsAround()
{
    // The default Life board: A minor pentatonic from A3 to A5 is eleven rows.
    auto life = MakeSectionTwentyEightGenerator(GeneratorType::Life);
    life.Scatter = false;
    life.Settled = LifeSettled::Hold;
    life.LifeCells = LifeShapeCells(LifeShape::Glider, 16, 11);

    auto const start = Alive(LifeBoardAt(life, 1, 16, 0));
    VERIFY_ARE_EQUAL(size_t{ 5 }, start.size());
    VERIFY_ARE_EQUAL(11, LifeBoardAt(life, 1, 16, 0).Rows);

    for (uint64_t pass = 1; pass < 12; ++pass)
    {
        VERIFY_ARE_EQUAL(size_t{ 5 }, Alive(LifeBoardAt(life, 1, 16, pass)).size());
    }

    // A glider moves a cell diagonally every four generations, and the board wraps, so it comes
    // home after it has crossed both ways: 4 x 16 x 11 generations.
    VERIFY_IS_FALSE(start == Alive(LifeBoardAt(life, 1, 16, 4)));
    VERIFY_IS_TRUE(start == Alive(LifeBoardAt(life, 1, 16, 704)));

    // Going back to a pass before the latest gives the same board.
    VERIFY_IS_TRUE(Alive(LifeBoardAt(life, 1, 16, 4)) == Alive(LifeBoardAt(life, 1, 16, 4 + 704)));
}

void GeneratorKindsTests::AnRPentominoSettles()
{
    auto const life = FifteenRows(LifeShapeCells(LifeShape::RPentomino, 16, 15), LifeSettled::Hold);

    // Worked out separately: on a wrapped board 16 by 15 it settles into blinking, two passes
    // after reaching it, on pass 70 (generation 69).
    uint64_t settled{ 0 };

    for (uint64_t pass = 1; pass < 200 && settled == 0; ++pass)
    {
        if (LifeBoardAt(life, 1, 16, pass).Settled)
        {
            settled = pass;
        }
    }

    VERIFY_ARE_EQUAL(uint64_t{ 69 }, settled);
}

void GeneratorKindsTests::LifeStartsAgainWhenAsked()
{
    auto const life = FifteenRows(LifeShapeCells(LifeShape::RPentomino, 16, 15), LifeSettled::StartAgain);
    auto const start = Alive(LifeBoardAt(life, 1, 16, 0));
    auto const again = LifeBoardAt(life, 1, 16, 69);

    VERIFY_IS_TRUE(again.Settled);
    VERIFY_IS_TRUE(start == Alive(again));

    auto every = MakeSectionTwentyEightGenerator(GeneratorType::Life);
    every.RestartEvery = 4;
    every.Settled = LifeSettled::Hold;

    VERIFY_IS_TRUE(Alive(LifeBoardAt(every, 7, 16, 0)) == Alive(LifeBoardAt(every, 7, 16, 4)));
    VERIFY_IS_TRUE(Alive(LifeBoardAt(every, 7, 16, 0)) == Alive(LifeBoardAt(every, 7, 16, 8)));
    VERIFY_IS_TRUE(Alive(LifeBoardAt(every, 7, 16, 1)) == Alive(LifeBoardAt(every, 7, 16, 5)));

    // Adding cells keeps a board that dies out playing.
    auto lonely = FifteenRows({ LifeCell{ 3, 3 } }, LifeSettled::AddCells);
    VERIFY_IS_TRUE(Alive(LifeBoardAt(lonely, 9, 16, 1)).size() >= 5);
}

void GeneratorKindsTests::LifePlaysItsKeyAndJoinsCells()
{
    auto const life = MakeSectionTwentyEightGenerator(GeneratorType::Life);
    size_t played{ 0 };

    for (uint64_t pass = 0; pass < 24; ++pass)
    {
        auto const notes = GeneratedPass(life, 99, 16, pass);
        played += notes.size();
        VERIFY_IS_TRUE(InKeyAndRange(life, notes));

        // One note a step: the highest live cell.
        std::set<int64_t> steps{};

        for (auto const& note : notes)
        {
            VERIFY_IS_TRUE(steps.insert(note.Step).second);
        }
    }

    VERIFY_IS_TRUE(played > 24);

    // A block stays still. Its top row's two cells play one note two steps long.
    auto block = FifteenRows({ LifeCell{ 3, 5 }, LifeCell{ 4, 5 }, LifeCell{ 3, 6 }, LifeCell{ 4, 6 } }, LifeSettled::Hold);
    auto const rows = LifeRowNotes(block);

    auto const joined = GeneratedPass(block, 1, 16, 0);
    VERIFY_ARE_EQUAL(size_t{ 1 }, joined.size());
    VERIFY_ARE_EQUAL(int64_t{ 3 }, joined[0].Step);
    VERIFY_ARE_EQUAL(int64_t{ 2 }, joined[0].Steps);
    VERIFY_ARE_EQUAL(rows[6], joined[0].Number);

    block.Join = false;
    VERIFY_ARE_EQUAL(size_t{ 2 }, GeneratedPass(block, 1, 16, 0).size());

    block.Voice = LifeVoice::All;
    VERIFY_ARE_EQUAL(size_t{ 4 }, GeneratedPass(block, 1, 16, 0).size());

    // Only births: the start is all births, and nothing is born after that.
    block.Births = LifeBirths::Only;
    VERIFY_ARE_EQUAL(size_t{ 4 }, GeneratedPass(block, 1, 16, 0).size());
    VERIFY_ARE_EQUAL(size_t{ 0 }, GeneratedPass(block, 1, 16, 3).size());
}

void GeneratorKindsTests::LifeDrumsPlayTheirDrums()
{
    auto const drums = MakeSectionTwentyEightGenerator(GeneratorType::LifeDrums);
    VERIFY_ARE_EQUAL(uint8_t{ 9 }, drums.Channel);
    VERIFY_ARE_EQUAL(size_t{ 8 }, drums.DrumRows.size());

    size_t played{ 0 };

    for (uint64_t pass = 0; pass < 16; ++pass)
    {
        for (auto const& note : GeneratedPass(drums, 21, 16, pass))
        {
            ++played;
            VERIFY_IS_TRUE(std::find(drums.DrumRows.begin(), drums.DrumRows.end(), note.Number) != drums.DrumRows.end());
            VERIFY_ARE_EQUAL(int64_t{ 1 }, note.Steps);
        }
    }

    VERIFY_IS_TRUE(played > 16);
}

void GeneratorKindsTests::LearnFollowsWhatItLearned()
{
    for (uint8_t tune = 0; tune < TuneCount; ++tune)
    {
        auto learn = MakeSectionTwentyEightGenerator(GeneratorType::Learn);
        learn.Tune = tune;

        auto const moves = LearnedMoves(learn);

        for (uint64_t pass = 0; pass < 12; ++pass)
        {
            auto const notes = GeneratedPass(learn, 5, 32, pass);
            VERIFY_IS_TRUE(notes.size() >= 4);
            VERIFY_IS_TRUE(InKeyAndRange(learn, notes));

            // Every move it makes is one the tune made.
            for (size_t i = 1; i < notes.size(); ++i)
            {
                auto const from = ScaleDegree(learn, notes[i - 1].Number);
                auto const to = ScaleDegree(learn, notes[i].Number);
                VERIFY_IS_TRUE(from >= 0 && to >= 0);
                VERIFY_IS_TRUE(moves[static_cast<size_t>(from)][static_cast<size_t>(to)] > 0);
            }
        }
    }
}

void GeneratorKindsTests::LearnFindsAClipsKey()
{
    // G major, up the scale in quarter notes and back to the D and the G, with a chord at the start.
    std::vector<Note> notes{};
    int64_t tick{ 0 };

    for (auto const number : { 67, 69, 71, 72, 74, 76, 78, 79, 74, 67 })
    {
        Note note{};
        note.Tick = tick;
        note.Length = TicksPerQuarterNote;
        note.Number = static_cast<uint8_t>(number);
        notes.push_back(note);
        tick += TicksPerQuarterNote;
    }

    Note under{};
    under.Tick = 0;
    under.Length = TicksPerQuarterNote;
    under.Number = 59;
    notes.push_back(under);

    auto const learned = LearnFromNotes(notes, TicksPerQuarterNote / 4);
    VERIFY_ARE_EQUAL(size_t{ 10 }, learned.size());

    std::vector<int32_t> positions{};

    for (auto const& note : learned)
    {
        positions.push_back(note.Position);
        VERIFY_ARE_EQUAL(uint8_t{ 4 }, note.Gap);
        VERIFY_ARE_EQUAL(uint8_t{ 4 }, note.Hold);
    }

    VERIFY_IS_TRUE((std::vector<int32_t>{ 0, 1, 2, 3, 4, 5, 6, 7, 4, 0 }) == positions);

    // Learned from the clip, it plays in the generator's own key.
    auto learn = MakeSectionTwentyEightGenerator(GeneratorType::Learn);
    learn.LearnFrom = LearnSource::Clip;
    learn.Learned = learned;
    learn.ScaleRoot = 9;
    learn.Scale = ScaleKind::NaturalMinor;
    VERIFY_IS_TRUE(InKeyAndRange(learn, GeneratedPass(learn, 3, 16, 0)));
}

void GeneratorKindsTests::KeyRulesKeepJumpsAndEnds()
{
    Clip clip{};
    clip.Kind = ClipKind::Generator;
    clip.Length = Bar * 2;
    clip.Generator = MakeChanceMelodyGenerator();
    clip.Generator->Busy = 80;
    clip.Generator->LowNote = 52;
    clip.Generator->HighNote = 88;
    clip.Generator->LargestJump = 3;
    clip.Generator->RootEnds = true;

    for (uint64_t pass = 0; pass < 8; ++pass)
    {
        clip.Seed = static_cast<uint32_t>(100 + pass);

        std::vector<Note> notes{};
        ForEachGeneratorNote(clip, 0, clip.Length, pass, 0, clip.Length, true, [&notes](Note const& note) { notes.push_back(note); });
        VERIFY_IS_TRUE(notes.size() >= 2);

        for (size_t i = 1; i < notes.size(); ++i)
        {
            VERIFY_IS_TRUE(std::abs(static_cast<int32_t>(notes[i].Number) - static_cast<int32_t>(notes[i - 1].Number)) <= 3);
        }

        VERIFY_ARE_EQUAL(4, notes.front().Number % 12);
        VERIFY_ARE_EQUAL(4, notes.back().Number % 12);
    }

    // Learn keeps the rules too.
    auto learn = MakeSectionTwentyEightGenerator(GeneratorType::Learn);
    learn.LargestJump = 2;
    learn.RootEnds = true;

    auto const notes = GeneratedPass(learn, 8, 32, 2);
    VERIFY_ARE_EQUAL(0, notes.front().Number % 12);
    VERIFY_ARE_EQUAL(0, notes.back().Number % 12);
}

void GeneratorKindsTests::KeepRepeatsAPass()
{
    auto learn = MakeSectionTwentyEightGenerator(GeneratorType::Learn);
    learn.Keep = 100;

    auto const first = GeneratedPass(learn, 17, 16, 0);

    for (uint64_t pass = 1; pass < 6; ++pass)
    {
        VERIFY_IS_TRUE(first == GeneratedPass(learn, 17, 16, pass));
    }

    learn.Keep = 0;
    auto changed = false;

    for (uint64_t pass = 1; pass < 6; ++pass)
    {
        changed = changed || GeneratedPass(learn, 17, 16, pass) != GeneratedPass(learn, 17, 16, 0);
    }

    VERIFY_IS_TRUE(changed);

    // Same each pass plays the first pass every time, whatever Keep says.
    learn.ChangesEachPass = false;
    VERIFY_IS_TRUE(GeneratedPass(learn, 17, 16, 0) == GeneratedPass(learn, 17, 16, 9));
}

void GeneratorKindsTests::SeedsFromWords()
{
    // The same on every PC, whatever the case or the spaces around it.
    VERIFY_ARE_EQUAL(0x5B9BD185u, SeedFromWords(L"Midnight drive"));
    VERIFY_ARE_EQUAL(SeedFromWords(L"Midnight drive"), SeedFromWords(L"  midnight DRIVE "));
    VERIFY_ARE_NOT_EQUAL(SeedFromWords(L"Midnight drive"), SeedFromWords(L"Midnight drives"));

    VERIFY_ARE_EQUAL(0xBEEFu, SeedFromHex(L"beef").value_or(0));
    VERIFY_ARE_EQUAL(0x12345678u, SeedFromHex(L" 12345678 ").value_or(0));
    VERIFY_IS_FALSE(SeedFromHex(L"123456789").has_value());
    VERIFY_IS_FALSE(SeedFromHex(L"Midnight").has_value());
    VERIFY_IS_FALSE(SeedFromHex(L"").has_value());
}

void GeneratorKindsTests::WordsAreSpelled()
{
    // G major pentatonic from G3 to G5: G3 A3 B3 D4 E4 G4 A4 B4 D5 E5 G5.
    auto words = MakeSectionTwentyEightGenerator(GeneratorType::Words);
    words.Text = L"Hello.";

    auto const spelled = SpellWords(words);
    VERIFY_ARE_EQUAL(size_t{ 6 }, spelled.size());

    // H is the eighth note up, and a capital, so it's accented.
    VERIFY_ARE_EQUAL(uint8_t{ 71 }, spelled[0].Number);
    VERIFY_IS_TRUE(spelled[0].Accent);
    VERIFY_ARE_EQUAL(uint8_t{ 64 }, spelled[1].Number);
    VERIFY_IS_FALSE(spelled[1].Accent);

    // L wraps round to the bottom, and the second L holds it.
    VERIFY_ARE_EQUAL(uint8_t{ 55 }, spelled[2].Number);
    VERIFY_ARE_EQUAL(int64_t{ 2 }, spelled[2].Steps);
    VERIFY_ARE_EQUAL(int64_t{ 4 }, spelled[3].Step);
    VERIFY_ARE_EQUAL(uint8_t{ 62 }, spelled[3].Number);

    // The period ends on the root nearest the last note, then rests.
    VERIFY_ARE_EQUAL(uint8_t{ 67 }, spelled[4].Number);
    VERIFY_ARE_EQUAL(int64_t{ 2 }, spelled[4].Steps);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, spelled[5].Number);
    VERIFY_ARE_EQUAL(int64_t{ 8 }, SpelledLength(spelled));

    auto const notes = GeneratedPass(words, 0, 16, 0);
    VERIFY_ARE_EQUAL(size_t{ 5 }, notes.size());
    VERIFY_ARE_EQUAL(int64_t{ 5 }, notes[4].Step);

    // Short words start again each pass.
    VERIFY_IS_TRUE(notes == GeneratedPass(words, 0, 16, 3));

    // A question ends on the fifth.
    words.Text = L"Why?";
    VERIFY_ARE_EQUAL(2, SpellWords(words)[3].Number % 12);
}

void GeneratorKindsTests::LongWordsCarryOn()
{
    auto words = MakeSectionTwentyEightGenerator(GeneratorType::Words);
    words.Text = L"abcdefghijklmnopqrstuvwxyzabcd";

    VERIFY_ARE_EQUAL(int64_t{ 30 }, SpelledLength(SpellWords(words)));

    // The second pass starts at q, the seventeenth letter.
    auto const first = GeneratedPass(words, 0, 16, 0);
    auto const second = GeneratedPass(words, 0, 16, 1);
    VERIFY_ARE_EQUAL(uint8_t{ 55 }, first.front().Number);
    VERIFY_ARE_EQUAL(uint8_t{ 67 }, second.front().Number);
    VERIFY_ARE_EQUAL(size_t{ 16 }, second.size());

    // Learn can learn from the words.
    auto learn = MakeSectionTwentyEightGenerator(GeneratorType::Learn);
    learn.LearnFrom = LearnSource::Words;
    learn.Text = words.Text;
    VERIFY_ARE_EQUAL(size_t{ 30 }, LearnedSource(learn).size());
    VERIFY_IS_TRUE(InKeyAndRange(learn, GeneratedPass(learn, 1, 16, 0)));
}

void GeneratorKindsTests::EvolveChoosesTheBest()
{
    auto const evolve = MakeSectionTwentyEightGenerator(GeneratorType::Evolve);

    for (uint64_t pass = 1; pass < 6; ++pass)
    {
        auto const family = EvolveFamilyAt(evolve, 5, 16, pass);
        auto const best = *std::max_element(family.Scores.begin(), family.Scores.end());

        VERIFY_ARE_EQUAL(best, family.Scores[family.Chosen]);
        VERIFY_IS_TRUE(family.Children[family.Chosen] == GeneratedPass(evolve, 5, 16, pass));
        VERIFY_IS_TRUE(family.Parent == GeneratedPass(evolve, 5, 16, pass - 1));
        VERIFY_IS_TRUE(InKeyAndRange(evolve, family.Children[family.Chosen]));
    }

    // Worked out again from the start, a pass is the same.
    VERIFY_IS_TRUE(GeneratedPass(evolve, 5, 16, 40) == EvolveFamilyAt(evolve, 5, 16, 40).Children[EvolveFamilyAt(evolve, 5, 16, 40).Chosen]);
}

void GeneratorKindsTests::EvolveScoresClimb()
{
    auto const evolve = MakeSectionTwentyEightGenerator(GeneratorType::Evolve);

    for (uint32_t seed = 1; seed <= 3; ++seed)
    {
        auto const scores = EvolveScores(evolve, seed, 16, 80);
        VERIFY_ARE_EQUAL(size_t{ 80 }, scores.size());

        auto const late = std::accumulate(scores.end() - 20, scores.end(), 0.0) / 20.0;
        VERIFY_IS_TRUE(late > scores.front());
    }
}

void GeneratorKindsTests::TheInfinitySeries()
{
    std::vector<int32_t> first{};

    for (uint64_t i = 0; i < 16; ++i)
    {
        first.push_back(InfinitySeries(i));
    }

    VERIFY_IS_TRUE((std::vector<int32_t>{ 0, 1, -1, 2, 1, 0, -2, 3, -1, 2, 0, 1, 2, -1, -3, 4 }) == first);

    // Every second note is the series upside down, and every fourth is the series itself.
    for (uint64_t n = 0; n < 2000; ++n)
    {
        VERIFY_ARE_EQUAL(-InfinitySeries(n), InfinitySeries(n * 2));
        VERIFY_ARE_EQUAL(InfinitySeries(n), InfinitySeries(n * 4));
    }

    // Seed 0 starts at the start of the series, one step of the scale for each of its steps.
    auto const infinity = MakeSectionTwentyEightGenerator(GeneratorType::Infinity);
    auto const ladder = ScaleNotes(infinity.ScaleRoot, infinity.Scale, infinity.LowNote, infinity.HighNote);
    auto const middle = static_cast<int64_t>(MiddleRootIndex(infinity, ladder));
    auto const notes = GeneratedPass(infinity, 0, 16, 1);

    VERIFY_ARE_EQUAL(size_t{ 16 }, notes.size());

    for (size_t i = 0; i < notes.size(); ++i)
    {
        VERIFY_ARE_EQUAL(ladder[ReflectIndex(middle + InfinitySeries(16 + i), ladder.size())], notes[i].Number);
    }

    // Two voices: the second plays every other step, an octave down.
    auto canon = infinity;
    canon.Voices = 2;
    VERIFY_ARE_EQUAL(size_t{ 24 }, GeneratedPass(canon, 0, 16, 0).size());
}

void GeneratorKindsTests::LSystemsGrowAndPlay()
{
    for (size_t preset = 0; preset < LSystemPresetCount; ++preset)
    {
        GeneratorSettings settings{};
        UseLSystemPreset(settings, preset);
        VERIFY_ARE_EQUAL(preset, MatchingLSystemPreset(settings));
    }

    auto lsystem = MakeSectionTwentyEightGenerator(GeneratorType::LSystem);
    VERIFY_ARE_EQUAL(size_t{ 0 }, MatchingLSystemPreset(lsystem));

    // The dragon curve doubles each generation: 4096 steps after twelve.
    auto const drawing = LSystemTurtle(lsystem);
    VERIFY_ARE_EQUAL(int64_t{ 4096 }, drawing.Steps);
    VERIFY_ARE_EQUAL(int32_t{ 12 }, drawing.Generations);
    VERIFY_ARE_EQUAL(size_t{ 4096 }, drawing.Lines.size());
    VERIFY_IS_TRUE(InKeyAndRange(lsystem, GeneratedPass(lsystem, 0, 16, 0)));

    lsystem.Axiom = L"FY";
    VERIFY_ARE_EQUAL(SIZE_MAX, MatchingLSystemPreset(lsystem));

    // The Cantor set: notes and rests, and each pass carries on from the last.
    auto cantor = MakeSectionTwentyEightGenerator(GeneratorType::LSystem);
    UseLSystemPreset(cantor, 5);

    std::vector<int64_t> first{};
    std::vector<int64_t> second{};

    for (auto const& note : GeneratedPass(cantor, 0, 16, 0)) { first.push_back(note.Step); }
    for (auto const& note : GeneratedPass(cantor, 0, 16, 1)) { second.push_back(note.Step); }

    VERIFY_IS_TRUE((std::vector<int64_t>{ 0, 2, 6, 8 }) == first);
    VERIFY_IS_TRUE((std::vector<int64_t>{ 2, 4, 8, 10 }) == second);
}

void GeneratorKindsTests::RingingCourses()
{
    constexpr std::array<size_t, 5> PlainBob{ 24, 40, 60, 84, 112 };
    constexpr std::array<size_t, 5> Grandsire{ 6, 30, 30, 70, 70 };

    for (uint8_t bells = MinimumBells; bells <= MaximumBells; ++bells)
    {
        auto ringing = MakeSectionTwentyEightGenerator(GeneratorType::ChangeRinging);
        ringing.Bells = bells;

        for (auto const method : { RingingMethod::PlainHunt, RingingMethod::PlainBob, RingingMethod::Grandsire })
        {
            ringing.Method = method;
            auto const course = RingingCourse(ringing);

            auto const expected = method == RingingMethod::PlainHunt ? size_t{ bells } * 2 :
                method == RingingMethod::PlainBob ? PlainBob[bells - MinimumBells] : Grandsire[bells - MinimumBells];

            VERIFY_ARE_EQUAL(expected, course.size());

            // Rounds first, then every row a different order of all the bells.
            std::set<std::vector<uint8_t>> rows{};

            for (auto const& row : course)
            {
                auto sorted = row;
                std::sort(sorted.begin(), sorted.end());

                for (uint8_t place = 0; place < bells; ++place)
                {
                    VERIFY_ARE_EQUAL(static_cast<uint8_t>(place + 1), sorted[place]);
                }

                VERIFY_IS_TRUE(rows.insert(row).second);
            }

            VERIFY_IS_TRUE(std::is_sorted(course.front().begin(), course.front().end()));
        }
    }
}

void GeneratorKindsTests::RingingPlaysRoundsFirst()
{
    // Six bells in F major: the tenor on F4 and the treble on D5.
    auto const ringing = MakeSectionTwentyEightGenerator(GeneratorType::ChangeRinging);
    VERIFY_IS_TRUE((std::vector<uint8_t>{ 74, 72, 70, 69, 67, 65 }) == BellNotes(ringing));

    auto const notes = GeneratedPass(ringing, 0, 16, 0);

    // Rounds, then the first change of Plain Bob (every pair swaps), then the handstroke gap.
    std::vector<uint8_t> numbers{};
    std::vector<int64_t> steps{};

    for (auto const& note : notes)
    {
        numbers.push_back(note.Number);
        steps.push_back(note.Step);
    }

    VERIFY_IS_TRUE((std::vector<uint8_t>{ 74, 72, 70, 69, 67, 65, 72, 74, 69, 70, 65, 67, 72, 69, 74 }) == numbers);
    VERIFY_IS_TRUE(std::find(steps.begin(), steps.end(), int64_t{ 12 }) == steps.end());
    VERIFY_ARE_EQUAL(int64_t{ 13 }, RingingRowStep(ringing, 2));
}

void GeneratorKindsTests::NewGeneratorsPlayThroughTheEngine()
{
    for (auto const type : NewTypes)
    {
        auto const sequence = OneClip(MakeSectionTwentyEightGenerator(type), 4, 4242);
        auto const whole = NoteOns(sequence, 0, Bar * 4);

        Log::Comment(String().Format(L"Type %d plays %zu notes in four bars.", static_cast<int>(type), whole.size()));
        VERIFY_IS_TRUE(whole.size() >= 4);

        // Played a little at a time, as the engine plays it, it's the same.
        std::vector<RenderedMessage> pieces{};

        for (int64_t from = 0; from < Bar * 4; from += 40)
        {
            auto const part = NoteOns(sequence, from, from + 40);
            pieces.insert(pieces.end(), part.begin(), part.end());
        }

        VERIFY_ARE_EQUAL(whole.size(), pieces.size());

        for (size_t i = 0; i < whole.size(); ++i)
        {
            VERIFY_ARE_EQUAL(whole[i].Tick, pieces[i].Tick);
            VERIFY_IS_TRUE(whole[i].Words == pieces[i].Words);
        }
    }
}

void GeneratorKindsTests::NewGeneratorsRoundTripThroughTheFile()
{
    Sequence sequence{};

    for (auto const type : NewTypes)
    {
        auto generator = MakeSectionTwentyEightGenerator(type);

        switch (type)
        {
        case GeneratorType::Life:
            generator.Rule = LifeRule::HighLife;
            generator.Scatter = false;
            generator.LifeCells = LifeShapeCells(LifeShape::Glider, 16, 11);
            generator.Voice = LifeVoice::All;
            generator.Births = LifeBirths::Only;
            generator.Join = false;
            generator.Settled = LifeSettled::StartAgain;
            generator.RestartEvery = 16;
            generator.VelocityBy = VelocityShape::Crowding;
            generator.VelocityLow = 9000;
            generator.VelocityHigh = 51000;
            break;

        case GeneratorType::LifeDrums:
            generator.DrumRows = { 36, 38, 42 };
            generator.Density = 40;
            generator.VelocityBy = VelocityShape::Beat;
            break;

        case GeneratorType::Learn:
            generator.LearnFrom = LearnSource::Clip;
            generator.Learned = TuneNotes(1);
            generator.Learned[0].Level = 5;
            generator.Learned[2].Level = 1;
            generator.LearnedFrom = L"Verse \"A\"";
            generator.Order = 3;
            generator.LargestJump = 5;
            generator.RootEnds = true;
            generator.VelocityBy = VelocityShape::Learned;
            break;

        case GeneratorType::Words:
            generator.Text = L"Say \"hi\", then go!";
            generator.VelocityBy = VelocityShape::Random;
            break;

        case GeneratorType::Evolve:
            generator.Weights = { 10, 20, 30, 40, 50 };
            generator.Mutations = 3;
            generator.Busy = 45;
            break;

        case GeneratorType::Infinity:
            generator.Interval = 2;
            generator.Voices = 3;
            generator.Busy = 70;
            break;

        case GeneratorType::LSystem:
            UseLSystemPreset(generator, 3);
            break;

        case GeneratorType::ChangeRinging:
            generator.Bells = 7;
            generator.Method = RingingMethod::Grandsire;
            generator.HandstrokeGap = false;
            break;

        default:
            break;
        }

        NormalizeGenerator(generator);

        Clip clip{};
        clip.Id = L"c-" + std::to_wstring(static_cast<int>(type));
        clip.Kind = ClipKind::Generator;
        clip.Name = L"Generated";
        clip.Seed = 77;
        clip.Generator = generator;
        sequence.Clips.push_back(clip);
    }

    sequence.Clips[0].SeedWords = L"Midnight drive";

    Sequence read{};
    auto const result = ReadSequenceJson(WriteSequenceJson(sequence), read);
    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_ARE_EQUAL(size_t{ 0 }, result.SkippedItems);
    VERIFY_ARE_EQUAL(sequence.Clips.size(), read.Clips.size());

    for (size_t i = 0; i < sequence.Clips.size(); ++i)
    {
        Log::Comment(String().Format(L"Clip %zu", i));
        VERIFY_IS_TRUE(read.Clips[i].Generator.has_value());
        VERIFY_IS_TRUE(SameGeneratorSettings(*sequence.Clips[i].Generator, *read.Clips[i].Generator));
    }

    VERIFY_ARE_EQUAL(std::wstring{ L"Midnight drive" }, read.Clips[0].SeedWords);
    VERIFY_IS_TRUE(read.Clips[1].SeedWords.empty());
}

void GeneratorKindsTests::DamagedNewGeneratorsAreClamped()
{
    std::wstring const text = LR"({
  "fileVersion": 1,
  "tracks": [],
  "clips": [
    { "id": "c-l", "kind": "generator", "name": "Life", "length": 3840, "loop": true,
      "settings": { "type": "life", "stepTicks": 240, "eachPass": "changes", "channel": 0, "noteLength": 80,
        "rule": "nonsense", "start": "cells", "density": 500, "cells": [[300, 2], [3, 99], [1, 1], "x"],
        "restartEvery": 5000, "voice": 7, "velocityBy": "nonsense" } },
    { "id": "c-d", "kind": "generator", "name": "Drums", "length": 3840, "loop": true,
      "settings": { "type": "lifeDrums", "drums": [36, 200, "kick"] } },
    { "id": "c-r", "kind": "generator", "name": "Bells", "length": 3840, "loop": true,
      "settings": { "type": "changeRinging", "bells": 99, "method": "stedman" } },
    { "id": "c-s", "kind": "generator", "name": "Plant", "length": 3840, "loop": true,
      "settings": { "type": "lSystem", "axiom": "F", "grammar": { "AB": "x", "F": "FF" }, "angle": 900 } },
    { "id": "c-m", "kind": "generator", "name": "Learn", "length": 3840, "loop": true,
      "settings": { "type": "learn", "from": "clip", "memory": 9, "learned": [[100, 50, 70], [1, 2], [1, 2, 2, 9]], "weights": [1, 2], "velocityBy": "treble" } },
    { "id": "c-x", "kind": "generator", "name": "Future", "length": 3840, "loop": true,
      "settings": { "type": "nonsense" } }
  ]
})";

    Sequence read{};
    auto const result = ReadSequenceJson(text, read);
    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_IS_TRUE(result.SkippedItems >= 6);

    auto const& life = *read.Clips[0].Generator;
    VERIFY_ARE_EQUAL(static_cast<int>(GeneratorType::Life), static_cast<int>(life.Type));
    VERIFY_ARE_EQUAL(static_cast<int>(LifeRule::Life), static_cast<int>(life.Rule));
    VERIFY_IS_FALSE(life.Scatter);
    VERIFY_ARE_EQUAL(uint8_t{ 90 }, life.Density);
    VERIFY_ARE_EQUAL(size_t{ 1 }, life.LifeCells.size());
    VERIFY_ARE_EQUAL(uint16_t{ 999 }, life.RestartEvery);

    auto const& drums = *read.Clips[1].Generator;
    VERIFY_IS_TRUE((std::vector<uint8_t>{ 36, 127 }) == drums.DrumRows);

    auto const& bells = *read.Clips[2].Generator;
    VERIFY_ARE_EQUAL(MaximumBells, bells.Bells);
    VERIFY_ARE_EQUAL(static_cast<int>(RingingMethod::PlainBob), static_cast<int>(bells.Method));

    auto const& plant = *read.Clips[3].Generator;
    VERIFY_ARE_EQUAL(size_t{ 1 }, plant.Grammar.size());
    VERIFY_IS_TRUE(L'F' == plant.Grammar[0].Symbol);
    VERIFY_ARE_EQUAL(uint8_t{ 180 }, plant.Angle);

    auto const& learn = *read.Clips[4].Generator;
    VERIFY_ARE_EQUAL(uint8_t{ 3 }, learn.Order);
    VERIFY_ARE_EQUAL(size_t{ 2 }, learn.Learned.size());
    VERIFY_ARE_EQUAL(63, static_cast<int>(learn.Learned[0].Position));
    VERIFY_ARE_EQUAL(uint8_t{ 16 }, learn.Learned[0].Gap);
    VERIFY_ARE_EQUAL(uint8_t{ 16 }, learn.Learned[0].Hold);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, learn.Learned[0].Level);
    VERIFY_ARE_EQUAL(uint8_t{ 5 }, learn.Learned[1].Level);
    VERIFY_IS_TRUE((GeneratorSettings{}.Weights) == learn.Weights);

    // A velocity shape it doesn't know, one from another type, or none: each type's own.
    VERIFY_IS_TRUE(life.VelocityBy == VelocityShape::Age);
    VERIFY_IS_TRUE(drums.VelocityBy == VelocityShape::Age);
    VERIFY_IS_TRUE(learn.VelocityBy == VelocityShape::Surprise);
    VERIFY_IS_TRUE(bells.VelocityBy == VelocityShape::Lead);

    // A type this version doesn't know plays as a Euclidean rhythm, as before.
    VERIFY_ARE_EQUAL(static_cast<int>(GeneratorType::Euclidean), static_cast<int>(read.Clips[5].Generator->Type));
}

void GeneratorKindsTests::VelocitiesStayInTheirRange()
{
    // Every shape of every type, in a range of its own. That's more passes than are kept, so
    // playing them all again works each one out again.
    auto const playAll = [](uint16_t low, uint16_t high, auto const& each)
    {
        for (auto const type : NewTypes)
        {
            for (auto const shape : VelocityShapesFor(type))
            {
                auto generator = MakeSectionTwentyEightGenerator(type);
                generator.VelocityBy = shape;
                generator.VelocityLow = low;
                generator.VelocityHigh = high;

                for (uint64_t pass = 0; pass < 6; ++pass)
                {
                    each(GeneratedPass(generator, 31, 16, pass));
                }
            }
        }
    };

    std::vector<std::vector<StepNote>> first{};
    size_t played{ 0 };

    playAll(20000, 30000, [&](std::vector<StepNote> const& notes)
    {
        for (auto const& note : notes)
        {
            VERIFY_IS_TRUE(note.Velocity >= 20000 && note.Velocity <= 30000);
        }

        played += notes.size();
        first.push_back(notes);
    });

    VERIFY_IS_TRUE(played > first.size());

    // The same again.
    size_t at{ 0 };

    playAll(20000, 30000, [&](std::vector<StepNote> const& notes)
    {
        VERIFY_IS_TRUE(notes == first[at++]);
    });

    // No room at all: every note at the one velocity.
    playAll(40000, 40000, [](std::vector<StepNote> const& notes)
    {
        for (auto const& note : notes)
        {
            VERIFY_ARE_EQUAL(uint16_t{ 40000 }, note.Velocity);
        }
    });
}

void GeneratorKindsTests::EveryGeneratorHasTheCommonShapes()
{
    for (auto const type : NewTypes)
    {
        auto const shapes = VelocityShapesFor(type);
        VERIFY_IS_TRUE(DefaultVelocityShape(type) == shapes.front());
        VERIFY_IS_TRUE(MakeSectionTwentyEightGenerator(type).VelocityBy == shapes.front());

        for (auto const common : { VelocityShape::Steady, VelocityShape::Beat, VelocityShape::Swell, VelocityShape::Random })
        {
            VERIFY_IS_TRUE(std::find(shapes.begin(), shapes.end(), common) != shapes.end());
        }
    }

    // The infinity series as busy as it goes, in one voice: a note every sixteenth.
    auto infinity = MakeSectionTwentyEightGenerator(GeneratorType::Infinity);
    infinity.Busy = 100;
    infinity.Voices = 1;
    infinity.StepTicks = TicksPerQuarterNote / 4;

    auto const velocities = [&infinity](VelocityShape shape, uint64_t pass)
    {
        infinity.VelocityBy = shape;
        std::vector<uint16_t> heard{};

        for (auto const& note : GeneratedPass(infinity, 0, 16, pass))
        {
            heard.push_back(note.Velocity);
        }

        VERIFY_ARE_EQUAL(size_t{ 16 }, heard.size());
        return heard;
    };

    // Steady: the middle of the range.
    for (auto const velocity : velocities(VelocityShape::Steady, 0))
    {
        VERIFY_ARE_EQUAL(VelocityForLoudness(0.5), velocity);
    }

    // On the beat: the bar's first step loudest, then each beat, then each half beat.
    auto const beat = velocities(VelocityShape::Beat, 0);
    VERIFY_ARE_EQUAL(DefaultVelocityHigh, beat[0]);
    VERIFY_IS_TRUE(beat[4] < beat[0] && beat[2] < beat[4] && beat[1] < beat[2]);
    VERIFY_IS_TRUE(beat[4] == beat[8] && beat[4] == beat[12] && beat[2] == beat[6] && beat[1] == beat[3]);

    // Swell: up to the middle of the pass and back down again.
    auto const swell = velocities(VelocityShape::Swell, 0);
    VERIFY_IS_TRUE(swell[0] < swell[3] && swell[3] < swell[7]);
    VERIFY_IS_TRUE(swell[15] < swell[12] && swell[12] < swell[8]);
    VERIFY_IS_TRUE(std::abs(static_cast<int32_t>(swell[7]) - static_cast<int32_t>(swell[8])) <= 1);

    // Random: different from note to note, and from pass to pass.
    auto const random = velocities(VelocityShape::Random, 0);
    VERIFY_IS_TRUE(std::set<uint16_t>(random.begin(), random.end()).size() > 8);
    VERIFY_IS_FALSE(random == velocities(VelocityShape::Random, 1));
}

void GeneratorKindsTests::NewLifeCellsPlayLoudest()
{
    // A blinker, every cell playing. On the second pass it stands upright, its middle cell older
    // than the two either side of it.
    auto life = FifteenRows(LifeShapeCells(LifeShape::Blinker, 16, 15), LifeSettled::Hold);
    life.Voice = LifeVoice::All;
    life.Join = false;
    VERIFY_IS_TRUE(life.VelocityBy == VelocityShape::Age);

    auto const board = LifeBoardAt(life, 1, 16, 1);
    auto const upright = Alive(board);
    VERIFY_ARE_EQUAL(size_t{ 3 }, upright.size());

    auto const ageOf = [&board](std::pair<int32_t, int32_t> const& cell)
    {
        return board.Ages[static_cast<size_t>(cell.second) * static_cast<size_t>(board.Columns) + static_cast<size_t>(cell.first)];
    };

    auto cell = upright.begin();
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, ageOf(*cell++));
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, ageOf(*cell++));
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, ageOf(*cell));

    // New cells loudest, older ones softer.
    auto const notes = GeneratedPass(life, 1, 16, 1);
    VERIFY_ARE_EQUAL(size_t{ 3 }, notes.size());
    VERIFY_ARE_EQUAL(VelocityForLoudness(1.0), notes[0].Velocity);
    VERIFY_ARE_EQUAL(VelocityForLoudness(0.6), notes[1].Velocity);
    VERIFY_ARE_EQUAL(VelocityForLoudness(1.0), notes[2].Velocity);

    // Crowding: the middle cell has two neighbors, and the ends one each.
    life.VelocityBy = VelocityShape::Crowding;
    auto const crowded = GeneratedPass(life, 1, 16, 1);
    VERIFY_ARE_EQUAL(VelocityForLoudness(0.25), crowded[0].Velocity);
    VERIFY_ARE_EQUAL(VelocityForLoudness(0.5), crowded[1].Velocity);
    VERIFY_ARE_EQUAL(VelocityForLoudness(0.25), crowded[2].Velocity);

    // A range of its own keeps them in step.
    life.VelocityBy = VelocityShape::Age;
    life.VelocityLow = 10000;
    life.VelocityHigh = 20000;
    auto const narrow = GeneratedPass(life, 1, 16, 1);
    VERIFY_ARE_EQUAL(uint16_t{ 20000 }, narrow[0].Velocity);
    VERIFY_ARE_EQUAL(uint16_t{ 16000 }, narrow[1].Velocity);
}

void GeneratorKindsTests::LearnPlaysLoudAndSoftAsLearned()
{
    // Up the C major scale in eighths, loud and soft by turns.
    constexpr std::array<uint8_t, 8> Scale{ 60, 62, 64, 65, 67, 69, 71, 72 };
    std::vector<Note> clip{};

    for (size_t i = 0; i < Scale.size(); ++i)
    {
        Note note{};
        note.Tick = static_cast<int64_t>(i) * TicksPerQuarterNote / 2;
        note.Length = TicksPerQuarterNote / 2;
        note.Number = Scale[i];
        note.Velocity = i % 2 == 0 ? uint16_t{ 65535 } : uint16_t{ 13000 };
        clip.push_back(note);
    }

    auto const learned = LearnFromNotes(clip, TicksPerQuarterNote / 4);
    VERIFY_ARE_EQUAL(size_t{ 8 }, learned.size());
    VERIFY_ARE_EQUAL(uint8_t{ 5 }, learned[0].Level);
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, learned[1].Level);

    auto learn = MakeSectionTwentyEightGenerator(GeneratorType::Learn);
    learn.LearnFrom = LearnSource::Clip;
    learn.Learned = learned;

    // As learned: loud and soft take turns, as they did.
    learn.VelocityBy = VelocityShape::Learned;

    for (uint64_t pass = 0; pass < 4; ++pass)
    {
        auto const notes = GeneratedPass(learn, 9, 32, pass);
        VERIFY_IS_TRUE(notes.size() >= 8);

        for (size_t i = 0; i < notes.size(); ++i)
        {
            VERIFY_IS_TRUE(notes[i].Velocity == VelocityForLoudness(1.0) || notes[i].Velocity == VelocityForLoudness(0.0));
            VERIFY_IS_TRUE(i == 0 || notes[i].Velocity != notes[i - 1].Velocity);
        }
    }

    // By surprise: a scale only ever goes one way, so after the first note nothing is a surprise.
    learn.VelocityBy = VelocityShape::Surprise;
    auto const expected = GeneratedPass(learn, 9, 32, 0);
    VERIFY_IS_TRUE(expected.size() >= 8);
    VERIFY_ARE_EQUAL(VelocityForLoudness(0.575), expected[0].Velocity);

    for (size_t i = 1; i < expected.size(); ++i)
    {
        VERIFY_ARE_EQUAL(VelocityForLoudness(0.15), expected[i].Velocity);
    }

    // A built-in tune, by surprise: its usual moves soft and its rarer ones louder, so it isn't
    // one level throughout, even where the chain's memory leaves it one way to go.
    auto ode = MakeSectionTwentyEightGenerator(GeneratorType::Learn);
    VERIFY_IS_TRUE(ode.VelocityBy == VelocityShape::Surprise);
    std::set<uint16_t> levels{};

    for (auto const& note : GeneratedPass(ode, 5, 32, 0))
    {
        levels.insert(note.Velocity);
    }

    VERIFY_IS_TRUE(levels.size() >= 3);
}

void GeneratorKindsTests::WordsStressTheirStarts()
{
    auto words = MakeSectionTwentyEightGenerator(GeneratorType::Words);
    words.Text = L"Hello world.";
    VERIFY_IS_TRUE(words.VelocityBy == VelocityShape::Speech);

    // H e ll o, a space, w o r l d, the period's note, and a rest.
    auto const spelled = SpellWords(words);
    VERIFY_ARE_EQUAL(size_t{ 12 }, spelled.size());

    // The capital is hardest. The rest rise through the sentence, each word's first letter
    // stressed, and the ending is firm.
    VERIFY_ARE_EQUAL(uint8_t{ 100 }, spelled[0].Stress);
    VERIFY_IS_TRUE(spelled[1].Stress < spelled[2].Stress && spelled[2].Stress < spelled[3].Stress);
    VERIFY_ARE_EQUAL(uint8_t{ 70 }, spelled[5].Stress);
    VERIFY_IS_TRUE(spelled[3].Stress < spelled[6].Stress && spelled[6].Stress < spelled[5].Stress);
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, spelled[9].Stress);
    VERIFY_ARE_EQUAL(uint8_t{ 80 }, spelled[10].Stress);

    // Each note plays as hard as it's spoken.
    auto const notes = GeneratedPass(words, 0, 16, 0);
    VERIFY_ARE_EQUAL(size_t{ 10 }, notes.size());
    VERIFY_ARE_EQUAL(VelocityForLoudness(1.0), notes[0].Velocity);
    VERIFY_ARE_EQUAL(VelocityForLoudness(0.7), notes[4].Velocity);
    VERIFY_ARE_EQUAL(VelocityForLoudness(0.8), notes[9].Velocity);

    // An exclamation ends hardest.
    words.Text = L"go!";
    VERIFY_ARE_EQUAL(uint8_t{ 100 }, SpellWords(words)[2].Stress);
}

void GeneratorKindsTests::BellsStressTheirRows()
{
    // Six bells of Plain Bob: rounds, the first change, the handstroke gap, and three bells of the
    // third row. The treble, bell 1, rings D5.
    auto ringing = MakeSectionTwentyEightGenerator(GeneratorType::ChangeRinging);
    VERIFY_IS_TRUE(ringing.VelocityBy == VelocityShape::Lead);
    VERIFY_ARE_EQUAL(DefaultVelocityLow, ringing.VelocityLow);
    VERIFY_ARE_EQUAL(DefaultVelocityHigh, ringing.VelocityHigh);

    // Lead: the bell leading each row.
    auto notes = GeneratedPass(ringing, 0, 16, 0);
    VERIFY_ARE_EQUAL(size_t{ 15 }, notes.size());

    for (size_t i = 0; i < notes.size(); ++i)
    {
        VERIFY_ARE_EQUAL(VelocityForLoudness(i == 0 || i == 6 || i == 12 ? 1.0 : 0.45), notes[i].Velocity);
    }

    // Strokes: the handstroke rows louder than the backstroke rows.
    ringing.VelocityBy = VelocityShape::Strokes;
    notes = GeneratedPass(ringing, 0, 16, 0);

    for (size_t i = 0; i < notes.size(); ++i)
    {
        VERIFY_ARE_EQUAL(VelocityForLoudness(i < 6 || i >= 12 ? 0.9 : 0.45), notes[i].Velocity);
    }

    // Treble: wherever the treble goes, it's louder.
    ringing.VelocityBy = VelocityShape::Treble;
    size_t trebles{ 0 };

    for (auto const& note : GeneratedPass(ringing, 0, 16, 0))
    {
        trebles += note.Number == 74 ? 1 : 0;
        VERIFY_ARE_EQUAL(VelocityForLoudness(note.Number == 74 ? 1.0 : 0.45), note.Velocity);
    }

    VERIFY_ARE_EQUAL(size_t{ 3 }, trebles);
}

void GeneratorKindsTests::LSystemsClimbAndBranch()
{
    // The dragon curve, climbing: a note above the one before is louder than the middle of the
    // range, and a note below it softer.
    auto dragon = MakeSectionTwentyEightGenerator(GeneratorType::LSystem);
    UseLSystemPreset(dragon, 0);
    VERIFY_IS_TRUE(dragon.VelocityBy == VelocityShape::Climb);

    auto const middle = VelocityForLoudness(0.5);
    auto const notes = GeneratedPass(dragon, 0, 64, 0);
    size_t ups{ 0 };
    size_t downs{ 0 };

    for (size_t i = 1; i < notes.size(); ++i)
    {
        if (notes[i].Number > notes[i - 1].Number)
        {
            ++ups;
            VERIFY_IS_TRUE(notes[i].Velocity > middle);
        }
        else if (notes[i].Number < notes[i - 1].Number)
        {
            ++downs;
            VERIFY_IS_TRUE(notes[i].Velocity < middle);
        }
    }

    VERIFY_IS_TRUE(ups > 0 && downs > 0);

    // By branch: the dragon never branches, so every note is as loud as the range goes.
    dragon.VelocityBy = VelocityShape::Depth;

    for (auto const& note : GeneratedPass(dragon, 0, 64, 0))
    {
        VERIFY_ARE_EQUAL(VelocityForLoudness(1.0), note.Velocity);
    }

    // The plant's stem is loudest, and its branches softer. Its stem is long, so this looks
    // through a few passes.
    auto plant = MakeSectionTwentyEightGenerator(GeneratorType::LSystem);
    UseLSystemPreset(plant, 3);
    plant.VelocityBy = VelocityShape::Depth;

    std::set<uint16_t> levels{};

    for (uint64_t pass = 0; pass < 32; ++pass)
    {
        for (auto const& note : GeneratedPass(plant, 0, 64, pass))
        {
            levels.insert(note.Velocity);
        }
    }

    VERIFY_IS_TRUE(levels.size() >= 3);
    VERIFY_ARE_EQUAL(VelocityForLoudness(1.0), *levels.rbegin());
}
