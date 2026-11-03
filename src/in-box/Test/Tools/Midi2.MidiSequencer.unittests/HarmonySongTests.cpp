// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "HarmonySongTests.h"

#include "Harmony.h"
#include "HarmonySong.h"

#include <string>
#include <vector>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    constexpr int64_t Beat = TicksPerQuarterNote;
    constexpr int64_t Bar = TicksPerQuarterNote * 4;

    MeterChange FourFour()
    {
        return MeterChange{};
    }

    // The example in docs/kb/midi-sequencer-key-and-chords-for-agents.md.
    std::wstring const Example = LR"(Here's your song:

```midi-sequencer-song
{
  "format": "windows-midi-sequencer-song",
  "version": 1,
  "meter": "4/4",
  "key": { "root": "A", "scale": "naturalMinor" },
  "sections": [
    { "name": "Intro", "bars": 2, "chords": [ { "chord": "Am", "beats": 4 }, { "chord": "F", "beats": 4 } ] },
    { "name": "Chorus", "bars": 2, "key": { "root": "C", "scale": "major" },
      "chords": [ { "chord": "C", "beats": 4 }, { "chord": "G/B", "beats": 2 }, { "chord": "N.C.", "beats": 2 } ] }
  ]
}
```

Enjoy!)";

    // A one-section song with what's given in place of the section, for the refusals.
    std::wstring OneSection(std::wstring const& section, std::wstring const& top = L"")
    {
        return L"{ \"format\": \"windows-midi-sequencer-song\", \"version\": 1, " + top +
            L"\"key\": { \"root\": \"C\", \"scale\": \"major\" }, \"sections\": [ " + section + L" ] }";
    }

    SongProblem ProblemOf(std::wstring const& answer)
    {
        return ReadSongAnswer(answer, FourFour()).Problem;
    }
}

void HarmonySongTests::ReadsTheGuidesExample()
{
    auto const result = ReadSongAnswer(Example, FourFour());
    VERIFY_IS_TRUE(result.Succeeded());

    auto const& plan = *result.Plan;
    VERIFY_IS_TRUE((plan.Key == KeyChange{ 0, 9, ScaleKind::NaturalMinor }));
    VERIFY_ARE_EQUAL(size_t{ 2 }, plan.Sections.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"Chorus" }, plan.Sections[1].Name);
    VERIFY_IS_TRUE(plan.Sections[1].Key.has_value());
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(11), *plan.Sections[1].Chords[1].Chord.Bass);
    VERIFY_IS_TRUE(plan.Sections[1].Chords[2].NoChord);

    auto const summary = SummarizeSong(plan);
    VERIFY_ARE_EQUAL(size_t{ 2 }, summary.Sections);
    VERIFY_ARE_EQUAL(int64_t{ 4 }, summary.Bars);
    VERIFY_ARE_EQUAL(size_t{ 4 }, summary.Chords);
    VERIFY_ARE_EQUAL(size_t{ 1 }, summary.KeyChanges);
}

void HarmonySongTests::FindsTheJsonInAnAnswer()
{
    // The song's own fence first, then a json fence, then a bare object.
    auto const fenced = ExtractSongJson(L"```json\n{\"a\":1}\n```\n```midi-sequencer-song\n{\"b\":2}\n```");
    VERIFY_ARE_EQUAL(std::wstring{ L"{\"b\":2}" }, fenced);
    VERIFY_ARE_EQUAL(std::wstring{ L"{\"a\":1}" }, ExtractSongJson(L"Sure:\r\n```json\r\n{\"a\":1}\r\n```\r\n"));
    VERIFY_ARE_EQUAL(std::wstring{ L"{ \"c\": { \"d\": 3 } }" }, ExtractSongJson(L"text { \"c\": { \"d\": 3 } } more"));
    VERIFY_IS_TRUE(ExtractSongJson(L"no json here").empty());

    // A bare object reads too, and keys it doesn't know are ignored.
    VERIFY_IS_TRUE(ReadSongAnswer(OneSection(L"{ \"name\": \"A\", \"bars\": 1, \"mood\": \"happy\", \"chords\": [ { \"chord\": \"C\", \"beats\": 4 } ] }",
        L"\"tempo\": 120, \"title\": \"Song\", "), FourFour()).Succeeded());
}

void HarmonySongTests::RefusesWithAReason()
{
    auto const good = L"{ \"name\": \"A\", \"bars\": 1, \"chords\": [ { \"chord\": \"C\", \"beats\": 4 } ] }";

    VERIFY_IS_TRUE(ProblemOf(L"  ") == SongProblem::Empty);
    VERIFY_IS_TRUE(ProblemOf(L"no song") == SongProblem::NoJson);
    VERIFY_IS_TRUE(ProblemOf(L"{ not json }") == SongProblem::BadJson);
    VERIFY_IS_TRUE(ProblemOf(L"{ \"version\": 1 }") == SongProblem::WrongFormat);
    VERIFY_IS_TRUE(ProblemOf(L"{ \"format\": \"windows-midi-sequencer-song\" }") == SongProblem::BadVersion);
    VERIFY_IS_TRUE(ProblemOf(L"{ \"format\": \"windows-midi-sequencer-song\", \"version\": 1.5 }") == SongProblem::BadVersion);

    auto const newer = ReadSongAnswer(L"{ \"format\": \"windows-midi-sequencer-song\", \"version\": 2 }", FourFour());
    VERIFY_IS_TRUE(newer.Problem == SongProblem::NewerVersion);
    VERIFY_ARE_EQUAL(int64_t{ 2 }, newer.Values[0]);

    VERIFY_IS_TRUE(ProblemOf(OneSection(good, L"\"meter\": \"four\", ")) == SongProblem::BadMeter);
    VERIFY_IS_TRUE(ProblemOf(OneSection(good, L"\"meter\": \"4/5\", ")) == SongProblem::BadMeter);

    auto const differs = ReadSongAnswer(OneSection(good, L"\"meter\": \"3/4\", "), FourFour());
    VERIFY_IS_TRUE(differs.Problem == SongProblem::MeterDiffers);
    VERIFY_IS_TRUE((differs.Values == std::vector<int64_t>{ 3, 4, 4, 4 }));

    VERIFY_IS_TRUE(ProblemOf(L"{ \"format\": \"windows-midi-sequencer-song\", \"version\": 1, \"key\": { \"root\": \"H\", \"scale\": \"major\" }, \"sections\": [] }") == SongProblem::BadKey);
    VERIFY_IS_TRUE(ProblemOf(L"{ \"format\": \"windows-midi-sequencer-song\", \"version\": 1, \"key\": { \"root\": \"C\", \"scale\": \"Major\" }, \"sections\": [] }") == SongProblem::BadKey);
    VERIFY_IS_TRUE(ProblemOf(L"{ \"format\": \"windows-midi-sequencer-song\", \"version\": 1, \"key\": { \"root\": \"Cm\", \"scale\": \"major\" }, \"sections\": [] }") == SongProblem::BadKey);
    VERIFY_IS_TRUE(ProblemOf(OneSection(L"")) == SongProblem::NoSections);

    VERIFY_IS_TRUE(ProblemOf(OneSection(L"{ \"name\": \"  \", \"bars\": 1, \"chords\": [] }")) == SongProblem::BadSectionName);
    VERIFY_IS_TRUE(ProblemOf(OneSection(L"{ \"name\": \"" + std::wstring(41, L'x') + L"\", \"bars\": 1, \"chords\": [] }")) == SongProblem::BadSectionName);

    auto const bars = ReadSongAnswer(OneSection(L"{ \"name\": \"Verse\", \"bars\": 0, \"chords\": [] }"), FourFour());
    VERIFY_IS_TRUE(bars.Problem == SongProblem::BadBars);
    VERIFY_ARE_EQUAL(std::wstring{ L"Verse" }, bars.Section);

    VERIFY_IS_TRUE(ProblemOf(OneSection(L"{ \"name\": \"Verse\", \"bars\": 1, \"key\": { \"root\": \"C\" }, \"chords\": [ { \"chord\": \"C\", \"beats\": 4 } ] }")) == SongProblem::BadKey);
    VERIFY_IS_TRUE(ProblemOf(OneSection(L"{ \"name\": \"Verse\", \"bars\": 1, \"chords\": [] }")) == SongProblem::NoChords);

    auto const chord = ReadSongAnswer(OneSection(L"{ \"name\": \"Verse\", \"bars\": 1, \"chords\": [ { \"chord\": \"C7#9\", \"beats\": 4 } ] }"), FourFour());
    VERIFY_IS_TRUE(chord.Problem == SongProblem::BadChord);
    VERIFY_ARE_EQUAL(std::wstring{ L"C7#9" }, chord.Text);

    VERIFY_IS_TRUE(ProblemOf(OneSection(L"{ \"name\": \"Verse\", \"bars\": 1, \"chords\": [ { \"chord\": \"C\", \"beats\": 0 } ] }")) == SongProblem::BadBeats);
    VERIFY_IS_TRUE(ProblemOf(OneSection(L"{ \"name\": \"Verse\", \"bars\": 1, \"chords\": [ { \"chord\": \"C\", \"beats\": 2.5 } ] }")) == SongProblem::BadBeats);
}

void HarmonySongTests::BeatsMustFillTheBars()
{
    auto const section = L"{ \"name\": \"Chorus\", \"bars\": 8, \"chords\": [ { \"chord\": \"C\", \"beats\": 32 }, { \"chord\": \"G\", \"beats\": 4 } ] }";
    auto const result = ReadSongAnswer(OneSection(section), FourFour());

    VERIFY_IS_TRUE(result.Problem == SongProblem::BeatsDontAddUp);
    VERIFY_ARE_EQUAL(std::wstring{ L"Chorus" }, result.Section);
    VERIFY_IS_TRUE((result.Values == std::vector<int64_t>{ 36, 8, 4 }));
    VERIFY_ARE_EQUAL(std::wstring{ L"4/4" }, result.Text);

    // In 6/8 a bar is six eighth notes.
    MeterChange sixEight{ 0, 6, 8 };
    VERIFY_IS_TRUE(ReadSongAnswer(OneSection(L"{ \"name\": \"A\", \"bars\": 2, \"chords\": [ { \"chord\": \"C\", \"beats\": 12 } ] }"), sixEight).Succeeded());
    VERIFY_IS_FALSE(ReadSongAnswer(OneSection(L"{ \"name\": \"A\", \"bars\": 2, \"chords\": [ { \"chord\": \"C\", \"beats\": 8 } ] }"), sixEight).Succeeded());
}

void HarmonySongTests::LimitsAreChecked()
{
    // 1,000 bars over two sections.
    auto const twoSections = L"{ \"name\": \"A\", \"bars\": 999, \"chords\": [ { \"chord\": \"C\", \"beats\": 3996 } ] }, "
        L"{ \"name\": \"B\", \"bars\": 1, \"chords\": [ { \"chord\": \"C\", \"beats\": 4 } ] }";
    auto const bars = ReadSongAnswer(OneSection(twoSections), FourFour());
    VERIFY_IS_TRUE(bars.Problem == SongProblem::TooManyBars);
    VERIFY_ARE_EQUAL(int64_t{ 1000 }, bars.Values[0]);

    // 257 sections.
    std::wstring many{};

    for (int i = 0; i < 257; ++i)
    {
        many += (i == 0 ? L"" : L", ") + std::wstring{ L"{ \"name\": \"A\", \"bars\": 1, \"chords\": [ { \"chord\": \"C\", \"beats\": 4 } ] }" };
    }

    VERIFY_IS_TRUE(ProblemOf(OneSection(many)) == SongProblem::TooManySections);

    // 4,097 chords of one beat.
    std::wstring chords{};

    for (int i = 0; i < 4100; ++i)
    {
        chords += (i == 0 ? L"" : L", ") + std::wstring{ L"{ \"chord\": \"C\", \"beats\": 1 }" };
    }

    VERIFY_IS_TRUE(ProblemOf(OneSection(L"{ \"name\": \"A\", \"bars\": 999, \"chords\": [ " + chords + L" ] }")) == SongProblem::TooManyChords);

    VERIFY_IS_TRUE(ProblemOf(std::wstring(MaximumSongAnswerLength + 1, L' ') + L"{}") == SongProblem::TooLong);
}

void HarmonySongTests::LaysOutKeysChordsAndTags()
{
    auto const plan = *ReadSongAnswer(Example, FourFour()).Plan;
    std::vector<MeterChange> const meter{ MeterChange{} };

    // From part way through bar 3: it starts at bar 3.
    auto const layout = LayOutSong(plan, meter, Bar * 2 + Beat);
    VERIFY_ARE_EQUAL(Bar * 2, layout.Start);
    VERIFY_ARE_EQUAL(Bar * 6, layout.End);

    VERIFY_ARE_EQUAL(size_t{ 2 }, layout.Keys.size());
    VERIFY_IS_TRUE((layout.Keys[0] == KeyChange{ Bar * 2, 9, ScaleKind::NaturalMinor }));
    VERIFY_IS_TRUE((layout.Keys[1] == KeyChange{ Bar * 4, 0, ScaleKind::Major }));

    // Am, F, C, G/B; "N.C." leaves the last half bar empty.
    VERIFY_ARE_EQUAL(size_t{ 4 }, layout.Chords.size());
    VERIFY_ARE_EQUAL(Bar * 3, layout.Chords[1].Tick);
    VERIFY_ARE_EQUAL(Bar, layout.Chords[1].Length);
    VERIFY_ARE_EQUAL(Bar * 5, layout.Chords[3].Tick);
    VERIFY_ARE_EQUAL(Beat * 2, layout.Chords[3].Length);
    VERIFY_ARE_EQUAL(std::wstring{ L"G/B" }, ChordName(layout.Chords[3], &layout.Keys[1]));

    VERIFY_ARE_EQUAL(size_t{ 2 }, layout.Tags.size());
    VERIFY_ARE_EQUAL(Bar * 4, layout.Tags[1].Tick);
    VERIFY_ARE_EQUAL(std::wstring{ L"Chorus" }, layout.Tags[1].Text);
    VERIFY_ARE_EQUAL(SectionColor(L"Chorus"), *layout.Tags[1].Color);

    // A section key that's the same as the one before isn't a change.
    auto same = plan;
    same.Sections[1].Key = plan.Key;
    VERIFY_ARE_EQUAL(size_t{ 1 }, LayOutSong(same, meter, 0).Keys.size());
    VERIFY_ARE_EQUAL(size_t{ 0 }, SummarizeSong(same).KeyChanges);
}

void HarmonySongTests::SectionColors()
{
    VERIFY_ARE_EQUAL(SectionColor(L"Verse"), SectionColor(L"verse 2"));
    VERIFY_ARE_EQUAL(SectionColor(L"Chorus"), SectionColor(L"CHORUS (last)"));
    VERIFY_ARE_NOT_EQUAL(SectionColor(L"Chorus"), SectionColor(L"Pre-chorus"));
    VERIFY_ARE_NOT_EQUAL(SectionColor(L"Verse"), SectionColor(L"Chorus"));
    VERIFY_ARE_EQUAL(SectionColor(L"Interlude"), SectionColor(L"Interlude"));
}

void HarmonySongTests::NullMeansLeftOut()
{
    auto const section = L"{ \"name\": \"A\", \"bars\": 1, \"key\": null, \"chords\": [ { \"chord\": \"C\", \"beats\": 4 } ] }";
    auto const result = ReadSongAnswer(OneSection(section, L"\"meter\": null, "), FourFour());
    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_IS_FALSE(result.Plan->Meter.has_value());
    VERIFY_IS_FALSE(result.Plan->Sections[0].Key.has_value());

    // Whole numbers written with a point are whole numbers.
    VERIFY_IS_TRUE(ReadSongAnswer(L"{ \"format\": \"windows-midi-sequencer-song\", \"version\": 1.0, \"key\": { \"root\": \"C\", \"scale\": \"major\" }, "
        L"\"sections\": [ { \"name\": \"A\", \"bars\": 1.0, \"chords\": [ { \"chord\": \"C\", \"beats\": 4.0 } ] } ] }", FourFour()).Succeeded());

    // A null section or chord is still an error.
    VERIFY_IS_FALSE(ReadSongAnswer(OneSection(std::wstring{ L"null, " } + section), FourFour()).Succeeded());
    VERIFY_IS_FALSE(ReadSongAnswer(OneSection(L"{ \"name\": \"A\", \"bars\": 1, \"chords\": [ null, { \"chord\": \"C\", \"beats\": 4 } ] }"), FourFour()).Succeeded());
}

void HarmonySongTests::AddingClearsTheSongsSpan()
{
    HarmonyTrack harmony{};
    ChordEvent across{};
    across.Tick = Bar;
    across.Length = Bar * 2;          // bars 2 and 3
    ChordEvent inside{};
    inside.Tick = Bar * 4;
    inside.Length = Bar;              // bar 5
    ChordEvent past{};
    past.Tick = Bar * 5;
    past.Length = Bar * 2;            // bars 6 and 7
    harmony.Chords = { across, inside, past };

    // A song over bars 3 to 6.
    ClearChordRange(harmony, Bar * 2, Bar * 6);

    VERIFY_ARE_EQUAL(size_t{ 2 }, harmony.Chords.size());
    VERIFY_ARE_EQUAL(Bar, harmony.Chords[0].Tick);
    VERIFY_ARE_EQUAL(Bar, harmony.Chords[0].Length);
    VERIFY_ARE_EQUAL(Bar * 6, harmony.Chords[1].Tick);
    VERIFY_ARE_EQUAL(Bar, harmony.Chords[1].Length);

    // A chord that covers the whole span keeps both ends.
    HarmonyTrack covering{};
    ChordEvent whole{};
    whole.Tick = 0;
    whole.Length = Bar * 8;
    covering.Chords = { whole };
    ClearChordRange(covering, Bar * 2, Bar * 4);
    VERIFY_ARE_EQUAL(size_t{ 2 }, covering.Chords.size());
    VERIFY_ARE_EQUAL(Bar * 2, covering.Chords[0].Length);
    VERIFY_ARE_EQUAL(Bar * 4, covering.Chords[1].Tick);
}

void HarmonySongTests::MeterChangesInsideTheSong()
{
    // 4/4, then 3/4 from bar 5.
    std::vector<MeterChange> const meter{ MeterChange{}, MeterChange{ Bar * 4, 3, 4 } };

    VERIFY_IS_FALSE(SongCrossesMeterChange(meter, 0, 4));
    VERIFY_IS_TRUE(SongCrossesMeterChange(meter, 0, 5));
    VERIFY_IS_TRUE(SongCrossesMeterChange(meter, Bar * 2 + Beat, 3));
    VERIFY_IS_FALSE(SongCrossesMeterChange(meter, Bar * 4, 20));
    VERIFY_IS_FALSE(SongCrossesMeterChange(std::vector<MeterChange>{ MeterChange{} }, 0, 999));
}
