// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "NoteToolsTests.h"

#include "NoteTools.h"
#include "SequenceTestData.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;
using testdata::MakeNote;

namespace
{
    constexpr std::array<bool, 12> CMajor{ true, false, true, false, true, true, false, true, false, true, false, true };

    // The notes with the result applied, in start then note order.
    std::vector<Note> Applied(std::vector<Note> notes, NoteToolResult const& result)
    {
        for (auto const& removed : result.Removed)
        {
            auto const found = std::find(notes.begin(), notes.end(), removed);
            VERIFY_IS_TRUE(found != notes.end(), L"a removed note was one of the notes");

            if (found != notes.end())
            {
                notes.erase(found);
            }
        }

        notes.insert(notes.end(), result.Added.begin(), result.Added.end());
        std::sort(notes.begin(), notes.end(), [](Note const& a, Note const& b) { return a.Tick != b.Tick ? a.Tick < b.Tick : a.Number < b.Number; });
        return notes;
    }

    std::vector<int32_t> Numbers(std::vector<Note> const& notes)
    {
        std::vector<int32_t> numbers{};

        for (auto const& note : notes)
        {
            numbers.push_back(note.Number);
        }

        return numbers;
    }

    std::vector<Note> Triad(int64_t tick, uint8_t a, uint8_t b, uint8_t c)
    {
        return { MakeNote(tick, 480, a), MakeNote(tick, 480, b), MakeNote(tick, 480, c) };
    }
}

void NoteToolsTests::CurvesStartAtZeroAndEndAtOne()
{
    for (auto const shape : { CurveShape::Straight, CurveShape::FastFirst, CurveShape::SlowFirst, CurveShape::SCurve })
    {
        VERIFY_ARE_EQUAL(0.0, ShapeCurve(shape, 0.0, 0.7));
        VERIFY_ARE_EQUAL(1.0, ShapeCurve(shape, 1.0, 0.7));
        VERIFY_ARE_EQUAL(0.5, ShapeCurve(shape, 0.5, 0.0));
    }

    VERIFY_IS_GREATER_THAN(ShapeCurve(CurveShape::FastFirst, 0.5, 0.5), 0.5);
    VERIFY_IS_LESS_THAN(ShapeCurve(CurveShape::SlowFirst, 0.5, 0.5), 0.5);
}

void NoteToolsTests::ChordsAreFoundByStartTime()
{
    NoteToolContext context{};

    // 120 BPM: 30 ms is about 58 ticks.
    std::vector<Note> notes{ MakeNote(0, 480, 67), MakeNote(20, 480, 60), MakeNote(50, 480, 64), MakeNote(200, 480, 72) };
    auto const chords = FindChords(notes, context, 30.0);

    VERIFY_ARE_EQUAL(size_t{ 2 }, chords.size());
    VERIFY_IS_TRUE((Numbers(chords[0]) == std::vector<int32_t>{ 60, 64, 67 }));
    VERIFY_IS_TRUE((Numbers(chords[1]) == std::vector<int32_t>{ 72 }));
}

void NoteToolsTests::QuantizeWithStrengthSwingAndKeep()
{
    NoteToolContext context{};
    std::vector<Note> notes{ MakeNote(250, 100, 60) };

    QuantizeSettings settings{};
    VERIFY_ARE_EQUAL(int64_t{ 240 }, Applied(notes, Quantize(notes, context, settings))[0].Tick);

    settings.Strength = 0.5;
    VERIFY_ARE_EQUAL(int64_t{ 245 }, Applied(notes, Quantize(notes, context, settings))[0].Tick);

    // Within a tenth of the grid it stays put.
    settings.Strength = 1.0;
    settings.Keep = 0.1;
    VERIFY_IS_TRUE(Quantize(notes, context, settings).Empty());

    // 75% swing on sixteenths puts the second sixteenth of each pair at 360.
    settings.Keep = 0;
    settings.Swing = 0.75;
    std::vector<Note> swung{ MakeNote(350, 100, 60), MakeNote(470, 100, 62) };
    auto const result = Applied(swung, Quantize(swung, context, settings));
    VERIFY_ARE_EQUAL(int64_t{ 360 }, result[0].Tick);
    VERIFY_ARE_EQUAL(int64_t{ 480 }, result[1].Tick);
}

void NoteToolsTests::QuantizeEnds()
{
    NoteToolContext context{};
    std::vector<Note> notes{ MakeNote(10, 225, 60) };

    QuantizeSettings settings{};
    settings.Ends = true;
    auto const result = Applied(notes, Quantize(notes, context, settings));
    VERIFY_ARE_EQUAL(int64_t{ 0 }, result[0].Tick);
    VERIFY_ARE_EQUAL(int64_t{ 240 }, result[0].Length);

    // Without it, the length stays.
    settings.Ends = false;
    VERIFY_ARE_EQUAL(int64_t{ 225 }, Applied(notes, Quantize(notes, context, settings))[0].Length);
}

void NoteToolsTests::HumanizeIsRepeatableAndTighterOnTheBeat()
{
    NoteToolContext context{};
    std::vector<Note> notes{};

    for (int64_t i = 0; i < 64; ++i)
    {
        notes.push_back(MakeNote(i * 960, 240, 60));            // on the beat
        notes.push_back(MakeNote(i * 960 + 600, 240, 64));      // off it
    }

    HumanizeSettings settings{};
    settings.TimingMs = 20;
    settings.VelocityPercent = 0;
    settings.Seed = 7;

    auto const first = Humanize(notes, context, settings);
    auto const second = Humanize(notes, context, settings);
    VERIFY_IS_FALSE(first.Empty());
    VERIFY_IS_TRUE(first.Added == second.Added);

    settings.Seed = 8;
    VERIFY_IS_FALSE(Humanize(notes, context, settings).Added == first.Added);

    // 20 ms at 120 BPM is 38.4 ticks; on the beat it's about a third of that.
    for (size_t i = 0; i < first.Removed.size(); ++i)
    {
        auto const moved = std::llabs(first.Added[i].Tick - first.Removed[i].Tick);
        auto const onBeat = first.Removed[i].Tick % 960 == 0;
        VERIFY_IS_LESS_THAN_OR_EQUAL(moved, onBeat ? 14LL : 39LL);
        VERIFY_ARE_EQUAL(first.Removed[i].Velocity, first.Added[i].Velocity);
    }
}

void NoteToolsTests::GrooveFromAnotherClip()
{
    NoteToolContext context{};
    std::vector<Note> source{ MakeNote(0, 100, 36, 60000), MakeNote(270, 100, 36, 30000), MakeNote(480, 100, 36, 60000), MakeNote(750, 100, 36, 30000) };

    auto const groove = ExtractGroove(source, context, 240);
    VERIFY_ARE_EQUAL(size_t{ 16 }, groove.Offsets.size());
    VERIFY_ARE_EQUAL(30.0, groove.Offsets[1]);
    VERIFY_ARE_EQUAL(0.0, groove.Offsets[2]);

    std::vector<Note> notes{ MakeNote(240, 100, 60, 45000), MakeNote(3840 + 240, 100, 60, 45000), MakeNote(480, 100, 60, 45000) };
    GrooveSettings settings{};
    settings.VelocityStrength = 1.0;
    auto const result = Applied(notes, ApplyGroove(notes, context, groove, settings));

    VERIFY_ARE_EQUAL(int64_t{ 270 }, result[0].Tick);
    VERIFY_ARE_EQUAL(uint16_t{ 30000 }, result[0].Velocity);
    VERIFY_ARE_EQUAL(int64_t{ 480 }, result[1].Tick);
    VERIFY_ARE_EQUAL(uint16_t{ 60000 }, result[1].Velocity);
    VERIFY_ARE_EQUAL(int64_t{ 3840 + 270 }, result[2].Tick);
}

void NoteToolsTests::TimeFeelKeepsTheFirstStartAndLastEnd()
{
    std::vector<Note> notes{ MakeNote(0, 240, 60), MakeNote(960, 240, 62), MakeNote(1920, 240, 64) };

    TimeFeelSettings settings{};
    auto const faster = Applied(notes, TimeFeel(notes, settings));
    VERIFY_ARE_EQUAL(int64_t{ 0 }, faster[0].Tick);
    VERIFY_ARE_EQUAL(int64_t{ 2160 }, faster[2].Tick + faster[2].Length);
    VERIFY_IS_GREATER_THAN(faster[1].Tick, int64_t{ 960 });

    settings.SpeedUp = false;
    auto const slower = Applied(notes, TimeFeel(notes, settings));
    VERIFY_IS_LESS_THAN(slower[1].Tick, int64_t{ 960 });
}

void NoteToolsTests::RatchetRepeatsInsideTheNote()
{
    std::vector<Note> notes{ MakeNote(0, 960, 38, 40000) };

    RatchetSettings settings{};
    settings.EndVelocity = 0.5;
    auto const result = Ratchet(notes, settings);

    VERIFY_ARE_EQUAL(size_t{ 1 }, result.Removed.size());
    VERIFY_ARE_EQUAL(size_t{ 4 }, result.Added.size());

    for (size_t i = 0; i < 4; ++i)
    {
        VERIFY_ARE_EQUAL(static_cast<int64_t>(i) * 240, result.Added[i].Tick);
        VERIFY_ARE_EQUAL(int64_t{ 120 }, result.Added[i].Length);
    }

    VERIFY_ARE_EQUAL(uint16_t{ 40000 }, result.Added.front().Velocity);
    VERIFY_ARE_EQUAL(uint16_t{ 20000 }, result.Added.back().Velocity);
}

void NoteToolsTests::LegatoFixedAndPercentLengths()
{
    std::vector<Note> notes{ MakeNote(0, 100, 60), MakeNote(0, 100, 64), MakeNote(480, 100, 67) };

    LengthSettings settings{};
    settings.Ticks = 0;
    auto legato = Applied(notes, SetLengths(notes, settings));
    VERIFY_ARE_EQUAL(int64_t{ 480 }, legato[0].Length);
    VERIFY_ARE_EQUAL(int64_t{ 480 }, legato[1].Length);
    VERIFY_ARE_EQUAL(int64_t{ 100 }, legato[2].Length);

    settings.Ticks = 30;
    VERIFY_ARE_EQUAL(int64_t{ 450 }, Applied(notes, SetLengths(notes, settings))[0].Length);

    settings.Mode = LengthMode::Fixed;
    settings.Ticks = 300;
    for (auto const& note : Applied(notes, SetLengths(notes, settings)))
    {
        VERIFY_ARE_EQUAL(int64_t{ 300 }, note.Length);
    }

    settings.Mode = LengthMode::Percent;
    settings.Percent = 50;
    for (auto const& note : Applied(notes, SetLengths(notes, settings)))
    {
        VERIFY_ARE_EQUAL(int64_t{ 50 }, note.Length);
    }
}

void NoteToolsTests::CleanUpFixesOverlapsDuplicatesAndShortNotes()
{
    NoteToolContext context{};
    std::vector<Note> notes{ MakeNote(0, 500, 60, 30000), MakeNote(240, 100, 60, 30000), MakeNote(240, 100, 60, 50000), MakeNote(1000, 5, 62) };

    VERIFY_ARE_EQUAL(size_t{ 2 }, CountSameNoteOverlaps(notes));

    CleanUpSettings settings{};
    settings.Short = true;
    auto const result = Applied(notes, CleanUp(notes, context, settings));

    VERIFY_ARE_EQUAL(size_t{ 2 }, result.size());
    VERIFY_ARE_EQUAL(int64_t{ 240 }, result[0].Length);
    VERIFY_ARE_EQUAL(uint16_t{ 50000 }, result[1].Velocity);
    VERIFY_ARE_EQUAL(size_t{ 0 }, CountSameNoteOverlaps(result));

    // A different channel isn't the same note.
    std::vector<Note> channels{ MakeNote(0, 500, 60, 30000, 0), MakeNote(240, 100, 60, 30000, 1) };
    VERIFY_IS_TRUE(CleanUp(channels, context, CleanUpSettings{}).Empty());
}

void NoteToolsTests::TransposeInKeyStaysInTheKey()
{
    NoteToolContext context{};
    context.InKey = CMajor;

    std::vector<Note> notes{ MakeNote(0, 100, 60), MakeNote(100, 100, 64), MakeNote(200, 100, 71), MakeNote(300, 100, 61) };

    VERIFY_IS_TRUE((Numbers(Applied(notes, TransposeInKey(notes, context, 2))) == std::vector<int32_t>{ 64, 67, 74, 65 }));
    VERIFY_IS_TRUE((Numbers(Applied(notes, TransposeInKey(notes, context, 1))) == std::vector<int32_t>{ 62, 65, 72, 63 }));
    VERIFY_IS_TRUE((Numbers(Applied(notes, TransposeInKey(notes, context, -1))) == std::vector<int32_t>{ 59, 62, 69, 60 }));
    VERIFY_IS_TRUE((Numbers(Applied(notes, TransposeInKey(notes, context, 7))) == std::vector<int32_t>{ 72, 76, 83, 73 }));

    // Without a key, by semitones.
    context.InKey.reset();
    VERIFY_IS_TRUE((Numbers(Applied(notes, TransposeInKey(notes, context, 2))) == std::vector<int32_t>{ 62, 66, 73, 63 }));
}

void NoteToolsTests::FitToChordsUsesTheChordThenTheKey()
{
    NoteToolContext context{};
    context.InKey = CMajor;
    context.Chords.push_back({ 0, 1920, { true, false, false, false, true, false, false, true, false, false, false, false } });

    std::vector<Note> notes{ MakeNote(0, 480, 62), MakeNote(600, 100, 62), MakeNote(2000, 480, 65), MakeNote(2400, 480, 66) };

    auto const all = Applied(notes, FitToChords(notes, context, FitWhich::All));
    VERIFY_IS_TRUE((Numbers(all) == std::vector<int32_t>{ 60, 60, 65, 65 }));

    auto const onBeats = Applied(notes, FitToChords(notes, context, FitWhich::OnBeats));
    VERIFY_IS_TRUE((Numbers(onBeats) == std::vector<int32_t>{ 60, 62, 65, 66 }));

    auto const longNotes = Applied(notes, FitToChords(notes, context, FitWhich::Long));
    VERIFY_IS_TRUE((Numbers(longNotes) == std::vector<int32_t>{ 62, 62, 65, 66 }));
}

void NoteToolsTests::VoicingInversionsDropsAndOctaves()
{
    NoteToolContext context{};
    auto const triad = Triad(0, 60, 64, 67);

    VERIFY_IS_TRUE((Numbers(Applied(triad, Voice(triad, context, VoicingKind::InversionUp))) == std::vector<int32_t>{ 64, 67, 72 }));
    VERIFY_IS_TRUE((Numbers(Applied(triad, Voice(triad, context, VoicingKind::InversionDown))) == std::vector<int32_t>{ 55, 60, 64 }));
    VERIFY_IS_TRUE((Numbers(Applied(triad, Voice(triad, context, VoicingKind::Open))) == std::vector<int32_t>{ 60, 67, 76 }));
    VERIFY_IS_TRUE((Numbers(Applied(triad, Voice(triad, context, VoicingKind::OctaveBelow))) == std::vector<int32_t>{ 48, 60, 64, 67 }));

    auto const wide = Triad(0, 48, 64, 79);
    VERIFY_IS_TRUE((Numbers(Applied(wide, Voice(wide, context, VoicingKind::Close))) == std::vector<int32_t>{ 48, 52, 55 }));

    auto seventh = Triad(0, 60, 64, 67);
    seventh.push_back(MakeNote(0, 480, 71));
    VERIFY_IS_TRUE((Numbers(Applied(seventh, Voice(seventh, context, VoicingKind::Drop2))) == std::vector<int32_t>{ 55, 60, 64, 71 }));
    VERIFY_IS_TRUE((Numbers(Applied(seventh, Voice(seventh, context, VoicingKind::Drop3))) == std::vector<int32_t>{ 52, 60, 67, 71 }));

    // A lone note isn't a chord to invert.
    std::vector<Note> single{ MakeNote(0, 480, 60) };
    VERIFY_IS_TRUE(Voice(single, context, VoicingKind::InversionUp).Empty());
}

void NoteToolsTests::SmoothVoiceLeadingMovesLittle()
{
    NoteToolContext context{};
    std::vector<Note> notes = Triad(0, 60, 64, 67);

    for (auto const& note : Triad(960, 65, 69, 72))
    {
        notes.push_back(note);
    }

    for (auto const& note : Triad(1920, 67, 71, 74))
    {
        notes.push_back(note);
    }

    auto const result = Applied(notes, Voice(notes, context, VoicingKind::SmoothLeading));
    auto const numbers = Numbers(result);

    // C stays; F becomes C F A; G moves no more than six semitones in all from there.
    VERIFY_IS_TRUE((std::vector<int32_t>(numbers.begin(), numbers.begin() + 3) == std::vector<int32_t>{ 60, 64, 67 }));
    VERIFY_IS_TRUE((std::vector<int32_t>(numbers.begin() + 3, numbers.begin() + 6) == std::vector<int32_t>{ 60, 65, 69 }));

    auto moved{ 0 };

    for (size_t i = 0; i < 3; ++i)
    {
        moved += std::abs(numbers[6 + i] - numbers[3 + i]);
    }

    VERIFY_IS_LESS_THAN_OR_EQUAL(moved, 6);

    for (size_t i = 0; i < result.size(); ++i)
    {
        VERIFY_ARE_EQUAL(int64_t{ 480 }, result[i].Length);
    }
}

void NoteToolsTests::MirrorAndReverse()
{
    NoteToolContext context{};
    context.InKey = CMajor;

    std::vector<Note> notes{ MakeNote(0, 240, 64), MakeNote(480, 480, 62) };

    VERIFY_IS_TRUE((Numbers(Applied(notes, Mirror(notes, context, 60, false))) == std::vector<int32_t>{ 56, 58 }));
    VERIFY_IS_TRUE((Numbers(Applied(notes, Mirror(notes, context, 60, true))) == std::vector<int32_t>{ 57, 59 }));

    auto const reversed = Applied(notes, Reverse(notes));
    VERIFY_ARE_EQUAL(int64_t{ 0 }, reversed[0].Tick);
    VERIFY_ARE_EQUAL(uint8_t{ 62 }, reversed[0].Number);
    VERIFY_ARE_EQUAL(int64_t{ 720 }, reversed[1].Tick);
    VERIFY_ARE_EQUAL(uint8_t{ 64 }, reversed[1].Number);
}

void NoteToolsTests::ValuesSetRampAccentAndChance()
{
    NoteToolContext context{};
    std::vector<Note> notes{ MakeNote(0, 100, 60, 32768), MakeNote(480, 100, 60, 32768), MakeNote(960, 100, 60, 32768), MakeNote(1920, 100, 60, 32768) };

    ValueSettings settings{};
    settings.First = 25;
    for (auto const& note : Applied(notes, SetValues(notes, context, settings)))
    {
        VERIFY_ARE_EQUAL(uint16_t{ 16384 }, note.Velocity);
    }

    settings.Mode = ValueMode::Ramp;
    settings.First = 0;
    settings.Second = 100;
    auto const ramp = Applied(notes, SetValues(notes, context, settings));
    VERIFY_ARE_EQUAL(uint16_t{ 512 }, ramp[0].Velocity, L"never quieter than 1 in 7 bits");
    VERIFY_ARE_EQUAL(uint16_t{ 16384 }, ramp[1].Velocity);
    VERIFY_ARE_EQUAL(uint16_t{ 65535 }, ramp[3].Velocity);

    settings.Mode = ValueMode::Accent;
    settings.First = 10;
    auto const accent = Applied(notes, SetValues(notes, context, settings));
    VERIFY_IS_LESS_THAN(std::abs(static_cast<int32_t>(accent[0].Velocity) - 45875), 2);
    VERIFY_ARE_EQUAL(uint16_t{ 32768 }, accent[1].Velocity);
    VERIFY_IS_LESS_THAN(std::abs(static_cast<int32_t>(accent[2].Velocity) - 39322), 2);

    settings.Target = ValueTarget::Chance;
    settings.Mode = ValueMode::Set;
    settings.First = 25;
    for (auto const& note : Applied(notes, SetValues(notes, context, settings)))
    {
        VERIFY_ARE_EQUAL(uint8_t{ 25 }, note.Chance);
        VERIFY_ARE_EQUAL(uint16_t{ 32768 }, note.Velocity);
    }

    settings.Target = ValueTarget::ReleaseVelocity;
    settings.First = 40;
    for (auto const& note : Applied(notes, SetValues(notes, context, settings)))
    {
        VERIFY_ARE_EQUAL(uint16_t{ 26214 }, note.ReleaseVelocity);
    }
}

void NoteToolsTests::SelectingTopBottomEveryNthAndInvert()
{
    NoteToolContext context{};
    context.InKey = CMajor;

    std::vector<Note> all = Triad(0, 60, 64, 67);
    all.push_back(MakeNote(480, 240, 72, 20000));
    all.push_back(MakeNote(960, 240, 55));
    all.push_back(MakeNote(960, 240, 61));

    auto const pick = [&](SelectKind kind, std::vector<Note> const& selection = {})
    {
        SelectSettings settings{};
        settings.Kind = kind;
        return Numbers(SelectNotes(all, selection, context, settings));
    };

    VERIFY_IS_TRUE((pick(SelectKind::TopOfChords) == std::vector<int32_t>{ 67, 72, 61 }));
    VERIFY_IS_TRUE((pick(SelectKind::BottomOfChords) == std::vector<int32_t>{ 60, 72, 55 }));
    VERIFY_IS_TRUE((pick(SelectKind::EveryNth) == std::vector<int32_t>{ 60, 67, 55 }));
    VERIFY_IS_TRUE((pick(SelectKind::OffBeats) == std::vector<int32_t>{ 72 }));
    VERIFY_IS_TRUE((pick(SelectKind::OutsideKey) == std::vector<int32_t>{ 61 }));
    VERIFY_IS_TRUE((pick(SelectKind::VelocityRange) == std::vector<int32_t>{ 72 }));

    std::vector<Note> selection{ all[0], all[1] };
    VERIFY_IS_TRUE((pick(SelectKind::Invert, selection) == std::vector<int32_t>{ 67, 72, 55, 61 }));
    VERIFY_IS_TRUE((pick(SelectKind::SameNotes, { all[0] }) == std::vector<int32_t>{ 60 }));
}
