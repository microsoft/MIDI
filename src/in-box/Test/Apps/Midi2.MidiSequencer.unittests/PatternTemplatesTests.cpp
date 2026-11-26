// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "PatternTemplatesTests.h"

#include "PatternTemplates.h"
#include "SequencePattern.h"
#include "SequenceRender.h"

#include <algorithm>
#include <iterator>
#include <set>
#include <string>
#include <vector>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    constexpr int64_t Bar = TicksPerQuarterNote * 4;

    uint16_t Velocity(uint8_t outOf127) noexcept
    {
        return static_cast<uint16_t>(ScaleUp(outOf127, 7, 16));
    }

    PatternTemplate const& Template(std::wstring_view id)
    {
        auto const found = FindPatternTemplate(id);
        VERIFY_IS_NOT_NULL(found, std::wstring{ id }.c_str());
        return *found;
    }

    PatternLane const* LaneFor(PatternSettings const& pattern, uint8_t note)
    {
        auto const found = std::find_if(pattern.Lanes.begin(), pattern.Lanes.end(), [note](PatternLane const& lane) { return lane.Note == note; });
        return found != pattern.Lanes.end() ? &*found : nullptr;
    }

    std::vector<uint16_t> Indexes(PatternLane const& lane)
    {
        std::vector<uint16_t> indexes{};

        for (auto const& step : lane.Steps)
        {
            indexes.push_back(step.Index);
        }

        return indexes;
    }

    // A melodic word the template text means as a note: semitones, then "!" or "~".
    bool IsNoteWord(std::wstring_view word)
    {
        while (!word.empty() && (word.back() == L'!' || word.back() == L'~'))
        {
            word.remove_suffix(1);
        }

        if (!word.empty() && (word.front() == L'-' || word.front() == L'+'))
        {
            word.remove_prefix(1);
        }

        return !word.empty() && word.size() <= 3 && std::all_of(word.begin(), word.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; });
    }

    std::vector<std::wstring_view> Words(std::wstring_view text)
    {
        std::vector<std::wstring_view> words{};
        size_t start{ 0 };

        while (start < text.size())
        {
            auto const end = std::min(text.find(L' ', start), text.size());

            if (end > start)
            {
                words.push_back(text.substr(start, end - start));
            }

            start = end + 1;
        }

        return words;
    }
}

void PatternTemplatesTests::EveryTemplateIsWellFormed()
{
    std::set<std::wstring_view> ids{};
    size_t drums{ 0 };
    size_t melodic{ 0 };

    for (auto const& source : PatternTemplates())
    {
        auto const name = std::wstring{ source.Id };
        auto const label = name.c_str();

        VERIFY_IS_FALSE(source.Id.empty());
        VERIFY_IS_TRUE(ids.insert(source.Id).second, label);
        VERIFY_IS_TRUE(source.BeatsPerMinute >= 40 && source.BeatsPerMinute <= 240, label);
        VERIFY_IS_TRUE(source.Swing >= MinimumSwing && source.Swing <= MaximumSwing, label);
        VERIFY_IS_TRUE(std::find(std::begin(PatternStepSizes), std::end(PatternStepSizes), source.StepTicks) != std::end(PatternStepSizes), label);

        if (source.Mode == PatternMode::Drums)
        {
            ++drums;

            // Drum templates come before melodic ones, in drum groups.
            VERIFY_ARE_EQUAL(size_t{ 0 }, melodic, label);
            VERIFY_IS_TRUE(source.Group <= PatternTemplateGroup::World, label);
            VERIFY_IS_TRUE(source.Melody.empty(), label);
            VERIFY_IS_FALSE(source.Lanes.empty(), label);

            std::set<uint8_t> notes{};

            for (auto const& lane : source.Lanes)
            {
                // Every lane is one bar, says something, and has its own General MIDI drum.
                VERIFY_ARE_EQUAL(static_cast<int64_t>(lane.Steps.size()) * source.StepTicks, Bar, label);
                VERIFY_IS_TRUE(lane.Steps.find_first_not_of(L".xXog?2345678") == std::wstring_view::npos, label);
                VERIFY_IS_TRUE(lane.Steps.find_first_not_of(L'.') != std::wstring_view::npos, label);
                VERIFY_IS_TRUE(lane.Note >= 35 && lane.Note <= 81, label);
                VERIFY_IS_TRUE(notes.insert(lane.Note).second, label);
            }
        }
        else
        {
            ++melodic;

            VERIFY_IS_TRUE(source.Mode == PatternMode::Melodic, label);
            VERIFY_IS_TRUE(source.Group >= PatternTemplateGroup::Basslines, label);
            VERIFY_IS_TRUE(source.Lanes.empty(), label);
            VERIFY_ARE_EQUAL(0, source.RootOctaveNote % 12, label);

            // Every word is a rest, a hold or a note. None is dropped by mistake.
            auto const words = Words(source.Melody);
            size_t notes{ 0 };

            VERIFY_ARE_EQUAL(static_cast<int64_t>(words.size()) * source.StepTicks, Bar, label);

            for (auto const word : words)
            {
                auto const known = word == L"." || word == L"-" || IsNoteWord(word);
                VERIFY_IS_TRUE(known, (name + L": " + std::wstring{ word }).c_str());
                notes += IsNoteWord(word) ? 1 : 0;
            }

            for (uint8_t key = 0; key < 12; ++key)
            {
                auto const pattern = MakePatternFromTemplate(source, key, 0);

                VERIFY_ARE_EQUAL(size_t{ 1 }, pattern.Lanes.size(), label);
                VERIFY_ARE_EQUAL(notes, pattern.Lanes[0].Steps.size(), label);

                // Low enough for a bass and high enough to hear, in every key.
                for (auto const& step : pattern.Lanes[0].Steps)
                {
                    VERIFY_IS_TRUE(step.Note >= 18 && step.Note <= 84, label);
                }
            }
        }

        // What it makes is a whole bar, already in order, and has steps to play.
        auto const pattern = MakePatternFromTemplate(source, 0, 9);
        auto normalized = pattern;
        NormalizePattern(normalized);

        VERIFY_IS_TRUE(PatternHasSteps(pattern), label);
        VERIFY_ARE_EQUAL(static_cast<int64_t>(pattern.StepCount) * pattern.StepTicks, Bar, label);
        VERIFY_ARE_EQUAL(pattern.Lanes.size(), normalized.Lanes.size(), label);

        for (size_t i = 0; i < pattern.Lanes.size(); ++i)
        {
            VERIFY_IS_TRUE(pattern.Lanes[i].Steps == normalized.Lanes[i].Steps, label);
        }
    }

    VERIFY_IS_TRUE(drums >= 16);
    VERIFY_IS_TRUE(melodic >= 12);
    VERIFY_IS_NULL(FindPatternTemplate(L"NoSuchTemplate"));
}

void PatternTemplatesTests::ADrumTemplateMakesItsLanes()
{
    auto const pattern = MakePatternFromTemplate(Template(L"Funk"), 0, 3);

    VERIFY_IS_TRUE(pattern.Mode == PatternMode::Drums);
    VERIFY_ARE_EQUAL(uint16_t{ 16 }, pattern.StepCount);
    VERIFY_ARE_EQUAL(TicksPerQuarterNote / 4, pattern.StepTicks);
    VERIFY_ARE_EQUAL(uint8_t{ 54 }, pattern.Swing);

    // The channel is the one the pattern had, not one the template picks.
    VERIFY_ARE_EQUAL(uint8_t{ 3 }, pattern.Channel);
    VERIFY_ARE_EQUAL(size_t{ 4 }, pattern.Lanes.size());

    auto const kick = LaneFor(pattern, 36);
    auto const snare = LaneFor(pattern, 38);
    auto const hat = LaneFor(pattern, 42);

    VERIFY_IS_NOT_NULL(kick);
    VERIFY_IS_NOT_NULL(snare);
    VERIFY_IS_NOT_NULL(hat);

    VERIFY_IS_TRUE((Indexes(*kick) == std::vector<uint16_t>{ 0, 3, 6, 10 }));
    VERIFY_ARE_EQUAL(Velocity(100), kick->Steps[0].Velocity);

    // "....X..g.g..X..g": accents on the backbeat and ghost notes between.
    VERIFY_IS_TRUE((Indexes(*snare) == std::vector<uint16_t>{ 4, 7, 9, 12, 15 }));
    VERIFY_ARE_EQUAL(Velocity(127), snare->Steps[0].Velocity);
    VERIFY_ARE_EQUAL(Velocity(40), snare->Steps[1].Velocity);

    // "xoxo...": the soft ones are softer.
    VERIFY_ARE_EQUAL(Velocity(100), hat->Steps[0].Velocity);
    VERIFY_ARE_EQUAL(Velocity(72), hat->Steps[1].Velocity);

    // Ratchets and chance come from their characters too.
    auto const trap = MakePatternFromTemplate(Template(L"Trap"), 0, 9);
    auto const rolls = LaneFor(trap, 42);
    VERIFY_IS_NOT_NULL(rolls);

    auto const roll3 = FindStep(*rolls, 11);
    auto const roll2 = FindStep(*rolls, 14);
    VERIFY_IS_NOT_NULL(roll3);
    VERIFY_IS_NOT_NULL(roll2);
    VERIFY_ARE_EQUAL(uint8_t{ 3 }, roll3->Ratchet);
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, roll2->Ratchet);

    auto const lofi = MakePatternFromTemplate(Template(L"LoFi"), 0, 9);
    auto const maybe = FindStep(*LaneFor(lofi, 42), 3);
    VERIFY_IS_NOT_NULL(maybe);
    VERIFY_ARE_EQUAL(uint8_t{ 50 }, maybe->Chance);

    // An eighth-note template keeps its step size.
    auto const shuffle = MakePatternFromTemplate(Template(L"Shuffle"), 0, 9);
    VERIFY_ARE_EQUAL(uint16_t{ 8 }, shuffle.StepCount);
    VERIFY_ARE_EQUAL(TicksPerQuarterNote / 2, shuffle.StepTicks);
    VERIFY_ARE_EQUAL(uint8_t{ 67 }, shuffle.Swing);
}

void PatternTemplatesTests::ADrumTemplatePlays()
{
    Sequence sequence{};
    sequence.Tempo = { TempoPoint{ 0, 120.0, false } };

    Clip clip{};
    clip.Id = L"c-t";
    clip.Kind = ClipKind::Pattern;
    clip.Name = L"Beat";
    clip.Seed = 1234;
    clip.Pattern = MakePatternFromTemplate(Template(L"FourOnTheFloor"), 0, 9);
    sequence.Clips = { clip };

    Track track{};
    track.Id = L"t-1";
    track.Name = L"Drums";
    track.Destination.Endpoint = EndpointRef{ L"Drum machine", L"" };
    track.Timeline = { Placement{ L"c-t", 0, Bar } };
    sequence.Tracks = { track };

    NormalizeSequence(sequence);

    std::vector<RenderedMessage> messages{};
    RenderTrack(sequence, sequence.Tracks[0], 0, Bar, messages, RenderOptions{});

    std::vector<int64_t> kicks{};
    size_t hats{ 0 };

    for (auto const& message : messages)
    {
        if (message.Kind != RenderedKind::NoteOn)
        {
            continue;
        }

        auto const note = static_cast<uint8_t>((message.Words[0] >> 8) & 0x7F);
        auto const channel = static_cast<uint8_t>((message.Words[0] >> 16) & 0x0F);

        VERIFY_ARE_EQUAL(uint8_t{ 9 }, channel);

        if (note == 36)
        {
            kicks.push_back(message.Tick);
        }
        else if (note == 42)
        {
            ++hats;
        }
    }

    VERIFY_IS_TRUE((kicks == std::vector<int64_t>{ 0, 960, 1920, 2880 }));
    VERIFY_ARE_EQUAL(size_t{ 8 }, hats);
}

void PatternTemplatesTests::AMelodicTemplateFollowsTheKey()
{
    auto const& acid = Template(L"AcidTechno");

    auto const inC = MakePatternFromTemplate(acid, 0, 1);
    auto const inD = MakePatternFromTemplate(acid, 2, 1);
    auto const inA = MakePatternFromTemplate(acid, 9, 1);

    VERIFY_IS_TRUE(inC.Mode == PatternMode::Melodic);
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, inC.Channel);
    VERIFY_ARE_EQUAL(size_t{ 1 }, inC.Lanes.size());
    VERIFY_ARE_EQUAL(uint16_t{ 16 }, inC.StepCount);

    // "0! 0 12~ 0 ..." in C from C1 (36).
    auto const& steps = inC.Lanes[0].Steps;
    VERIFY_ARE_EQUAL(uint8_t{ 36 }, steps[0].Note);
    VERIFY_IS_TRUE(steps[0].Accent);
    VERIFY_ARE_EQUAL(uint8_t{ 48 }, steps[2].Note);
    VERIFY_IS_TRUE(steps[2].Slide);
    VERIFY_IS_FALSE(steps[1].Accent || steps[1].Slide);

    // D is two semitones up; A is the A below, nearer the template's C than the one above.
    VERIFY_ARE_EQUAL(steps.size(), inD.Lanes[0].Steps.size());
    VERIFY_ARE_EQUAL(steps.size(), inA.Lanes[0].Steps.size());

    for (size_t i = 0; i < steps.size(); ++i)
    {
        VERIFY_ARE_EQUAL(steps[i].Index, inD.Lanes[0].Steps[i].Index);
        VERIFY_ARE_EQUAL(static_cast<int32_t>(steps[i].Note) + 2, static_cast<int32_t>(inD.Lanes[0].Steps[i].Note));
        VERIFY_ARE_EQUAL(static_cast<int32_t>(steps[i].Note) - 3, static_cast<int32_t>(inA.Lanes[0].Steps[i].Note));
    }

    VERIFY_ARE_EQUAL(uint8_t{ 42 }, TemplateRootNote(acid, 6));
    VERIFY_ARE_EQUAL(uint8_t{ 31 }, TemplateRootNote(acid, 7));
    VERIFY_ARE_EQUAL(uint8_t{ 36 }, TemplateRootNote(acid, 12));

    // A sequence template sits an octave higher.
    VERIFY_ARE_EQUAL(uint8_t{ 48 }, TemplateRootNote(Template(L"BerlinSchool"), 0));
}

void PatternTemplatesTests::HeldNotesAccentsAndSlides()
{
    PatternTemplate source{};
    source.Id = L"Test";
    source.Mode = PatternMode::Melodic;
    source.Group = PatternTemplateGroup::Basslines;
    source.StepTicks = TicksPerQuarterNote / 2;
    source.NoteLength = 60;

    // A hold after a rest holds nothing; "-5" is a note, "-" a hold; "!~" and "~!" both work.
    source.Melody = L". - 0! - - 7~ -5 12!~";

    auto const pattern = MakePatternFromTemplate(source, 0, 0);
    auto const& steps = pattern.Lanes[0].Steps;

    VERIFY_ARE_EQUAL(uint16_t{ 8 }, pattern.StepCount);
    VERIFY_ARE_EQUAL(TicksPerQuarterNote / 2, pattern.StepTicks);
    VERIFY_ARE_EQUAL(size_t{ 4 }, steps.size());

    VERIFY_ARE_EQUAL(uint16_t{ 2 }, steps[0].Index);
    VERIFY_ARE_EQUAL(uint8_t{ 36 }, steps[0].Note);
    VERIFY_ARE_EQUAL(uint16_t{ 260 }, steps[0].Length);
    VERIFY_IS_TRUE(steps[0].Accent);

    VERIFY_ARE_EQUAL(uint16_t{ 5 }, steps[1].Index);
    VERIFY_ARE_EQUAL(uint8_t{ 43 }, steps[1].Note);
    VERIFY_ARE_EQUAL(uint16_t{ 60 }, steps[1].Length);
    VERIFY_IS_TRUE(steps[1].Slide);

    VERIFY_ARE_EQUAL(uint16_t{ 6 }, steps[2].Index);
    VERIFY_ARE_EQUAL(uint8_t{ 31 }, steps[2].Note);

    VERIFY_ARE_EQUAL(uint16_t{ 7 }, steps[3].Index);
    VERIFY_ARE_EQUAL(uint8_t{ 48 }, steps[3].Note);
    VERIFY_IS_TRUE(steps[3].Accent && steps[3].Slide);

    // A word that isn't a note is a rest, and the rest of the line still reads.
    source.Melody = L"0 x7 7";
    auto const odd = MakePatternFromTemplate(source, 0, 0);
    VERIFY_IS_TRUE((Indexes(odd.Lanes[0]) == std::vector<uint16_t>{ 0, 2 }));
}

void PatternTemplatesTests::OverlayKeepsWhatWasThere()
{
    auto pattern = MakeDrumPattern();
    VERIFY_IS_FALSE(PatternHasSteps(pattern));

    // The customer's own: a kick on step 2, an accented snare on 6 and a cowbell lane.
    SetStepOn(pattern.Lanes[0], 2, true, 36);
    SetStepOn(pattern.Lanes[1], 6, true, 38)->Velocity = Velocity(127);

    auto cowbell = MakePatternLane(56, 16);
    SetStepOn(cowbell, 5, true, 56);
    pattern.Lanes.push_back(cowbell);
    pattern.Swing = 57;
    pattern.Channel = 9;

    VERIFY_IS_TRUE(PatternHasSteps(pattern));

    auto const laneCount = pattern.Lanes.size();
    auto const kickId = pattern.Lanes[0].Id;

    OverlayPattern(pattern, MakePatternFromTemplate(Template(L"House"), 0, 9));

    // The kick lane takes the template's kicks and keeps its own, and keeps its id.
    auto const kick = LaneFor(pattern, 36);
    VERIFY_IS_NOT_NULL(kick);
    VERIFY_ARE_EQUAL(kickId, kick->Id);
    VERIFY_IS_TRUE((Indexes(*kick) == std::vector<uint16_t>{ 0, 2, 4, 8, 12 }));

    // House has no snare, so the snare lane is as it was.
    auto const snare = LaneFor(pattern, 38);
    VERIFY_IS_TRUE((Indexes(*snare) == std::vector<uint16_t>{ 6 }));
    VERIFY_ARE_EQUAL(Velocity(127), snare->Steps[0].Velocity);

    // The clap is a new lane; the hats go onto the lanes that were there.
    VERIFY_ARE_EQUAL(laneCount + 1, pattern.Lanes.size());
    VERIFY_IS_TRUE((Indexes(*LaneFor(pattern, 39)) == std::vector<uint16_t>{ 4, 12 }));
    VERIFY_ARE_EQUAL(size_t{ 8 }, LaneFor(pattern, 42)->Steps.size());
    VERIFY_ARE_EQUAL(size_t{ 4 }, LaneFor(pattern, 46)->Steps.size());
    VERIFY_IS_TRUE((Indexes(*LaneFor(pattern, 56)) == std::vector<uint16_t>{ 5 }));

    // The pattern's own settings stay.
    VERIFY_ARE_EQUAL(uint8_t{ 57 }, pattern.Swing);
    VERIFY_ARE_EQUAL(uint8_t{ 9 }, pattern.Channel);
    VERIFY_ARE_EQUAL(uint16_t{ 16 }, pattern.StepCount);

    // Where both have a step, the template's wins.
    auto const kickStep = FindStep(*kick, 0);
    VERIFY_ARE_EQUAL(Velocity(100), kickStep->Velocity);
}

void PatternTemplatesTests::OverlayKeepsTimeAcrossStepSizes()
{
    // An eighth-note pattern takes a sixteenth-note template: each step keeps its time.
    auto eighths = MakeDrumPattern();
    eighths.StepTicks = TicksPerQuarterNote / 2;
    SetPatternStepCount(eighths, 8);

    OverlayPattern(eighths, MakePatternFromTemplate(Template(L"Rock"), 0, 9));

    VERIFY_ARE_EQUAL(uint16_t{ 8 }, eighths.StepCount);
    VERIFY_ARE_EQUAL(TicksPerQuarterNote / 2, eighths.StepTicks);

    // "x.......x.x....." is sixteenths 0, 8 and 10: eighths 0, 4 and 5.
    VERIFY_IS_TRUE((Indexes(*LaneFor(eighths, 36)) == std::vector<uint16_t>{ 0, 4, 5 }));
    VERIFY_IS_TRUE((Indexes(*LaneFor(eighths, 42)) == std::vector<uint16_t>{ 0, 1, 2, 3, 4, 5, 6, 7 }));

    // A short sixteenth-note pattern takes an eighth-note template, and gets longer to fit it.
    auto sixteenths = MakeDrumPattern();
    SetPatternStepCount(sixteenths, 8);

    OverlayPattern(sixteenths, MakePatternFromTemplate(Template(L"Shuffle"), 0, 9));

    VERIFY_ARE_EQUAL(uint16_t{ 16 }, sixteenths.StepCount);
    VERIFY_ARE_EQUAL(TicksPerQuarterNote / 4, sixteenths.StepTicks);
    VERIFY_IS_TRUE((Indexes(*LaneFor(sixteenths, 36)) == std::vector<uint16_t>{ 0, 8 }));
    VERIFY_IS_TRUE((Indexes(*LaneFor(sixteenths, 38)) == std::vector<uint16_t>{ 4, 12 }));

    for (auto const& lane : sixteenths.Lanes)
    {
        VERIFY_ARE_EQUAL(uint16_t{ 16 }, LaneStepCount(sixteenths, lane));
    }

    // Its notes keep their length in time: half an eighth is a whole sixteenth.
    VERIFY_ARE_EQUAL(uint16_t{ 100 }, LaneFor(sixteenths, 36)->Steps[0].Length);
}

void PatternTemplatesTests::OverlayOnAMelodicPattern()
{
    auto pattern = MakeMelodicPattern();
    SetStepOn(pattern.Lanes[0], 0, true, 62);
    SetStepOn(pattern.Lanes[0], 1, true, 60);

    auto const laneId = pattern.Lanes[0].Id;

    // "0 . 12 . 0 . 12 ..." in C from C1.
    OverlayPattern(pattern, MakePatternFromTemplate(Template(L"OctaveDisco"), 0, 0));

    VERIFY_ARE_EQUAL(size_t{ 1 }, pattern.Lanes.size());
    VERIFY_ARE_EQUAL(laneId, pattern.Lanes[0].Id);

    auto const& lane = pattern.Lanes[0];
    VERIFY_ARE_EQUAL(size_t{ 9 }, lane.Steps.size());

    // The template's note wins where both have one; the pattern's own note between stays.
    VERIFY_ARE_EQUAL(uint8_t{ 36 }, FindStep(lane, 0)->Note);
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, FindStep(lane, 1)->Note);
    VERIFY_ARE_EQUAL(uint8_t{ 48 }, FindStep(lane, 2)->Note);
    VERIFY_ARE_EQUAL(uint8_t{ 48 }, FindStep(lane, 14)->Note);
}
