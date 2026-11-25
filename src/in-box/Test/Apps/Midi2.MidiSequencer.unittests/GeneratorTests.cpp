// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "GeneratorTests.h"
#include "SequenceTestData.h"

#include "SequenceEdits.h"
#include "SequenceGenerator.h"
#include "SequenceRender.h"
#include "SequenceSerializer.h"
#include "SequenceUndo.h"

#include <algorithm>
#include <set>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    constexpr int64_t Bar = TicksPerQuarterNote * 4;
    constexpr int64_t Step = TicksPerQuarterNote / 4;

    bool Contains(std::wstring const& text, std::wstring_view part)
    {
        return text.find(part) != std::wstring::npos;
    }

    std::vector<int64_t> HitSteps(std::vector<bool> const& hits)
    {
        std::vector<int64_t> steps{};

        for (size_t i = 0; i < hits.size(); ++i)
        {
            if (hits[i])
            {
                steps.push_back(static_cast<int64_t>(i));
            }
        }

        return steps;
    }

    EuclidRing Ring(uint8_t note, uint8_t pulses, uint8_t steps, uint8_t chance = 100)
    {
        EuclidRing ring{};
        ring.Note = note;
        ring.Pulses = pulses;
        ring.Steps = steps;
        ring.Chance = chance;
        return ring;
    }

    // One track playing one generator clip from the start, for a number of bars.
    Sequence GeneratorSequence(GeneratorSettings generator, int64_t clipLength, int64_t bars, uint32_t seed = 4321)
    {
        Sequence sequence{};
        sequence.Tempo = { TempoPoint{ 0, 120.0, false } };

        Clip clip{};
        clip.Id = L"c-g";
        clip.Kind = ClipKind::Generator;
        clip.Name = L"Generated";
        clip.Seed = seed;
        clip.Length = clipLength;
        clip.Loop = true;
        clip.Generator = std::move(generator);
        sequence.Clips = { clip };

        Track track{};
        track.Id = L"t-1";
        track.Name = L"Arp";
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

    std::vector<int64_t> Ticks(std::vector<RenderedMessage> const& messages)
    {
        std::vector<int64_t> ticks{};

        for (auto const& message : messages)
        {
            ticks.push_back(message.Tick);
        }

        return ticks;
    }

    uint8_t NoteNumber(RenderedMessage const& message) noexcept { return static_cast<uint8_t>((message.Words[0] >> 8) & 0x7F); }
    uint16_t Velocity(RenderedMessage const& message) noexcept { return static_cast<uint16_t>(message.Words[1] >> 16); }

    // The note ons of one pass, moved back to the pass's start, so passes can be compared.
    std::vector<std::pair<int64_t, std::array<uint32_t, 4>>> Pass(std::vector<RenderedMessage> const& messages, int64_t start, int64_t length)
    {
        std::vector<std::pair<int64_t, std::array<uint32_t, 4>>> pass{};

        for (auto const& message : messages)
        {
            if (message.Tick >= start && message.Tick < start + length)
            {
                pass.emplace_back(message.Tick - start, message.Words);
            }
        }

        return pass;
    }

    bool SameMessages(std::vector<RenderedMessage> const& a, std::vector<RenderedMessage> const& b)
    {
        if (a.size() != b.size())
        {
            return false;
        }

        for (size_t i = 0; i < a.size(); ++i)
        {
            if (a[i].Tick != b[i].Tick || a[i].Kind != b[i].Kind || a[i].Words != b[i].Words)
            {
                return false;
            }
        }

        return true;
    }
}

void GeneratorTests::EuclideanRhythmsSpreadEvenly()
{
    VERIFY_IS_TRUE((std::vector<int64_t>{ 0, 4, 7, 10, 13 }) == HitSteps(EuclideanRhythm(5, 16, 0)));
    VERIFY_IS_TRUE((std::vector<int64_t>{ 0, 3, 6 }) == HitSteps(EuclideanRhythm(3, 8, 0)));

    // Rotate turns the ring: everything moves later, and wraps round.
    VERIFY_IS_TRUE((std::vector<int64_t>{ 1, 4, 7 }) == HitSteps(EuclideanRhythm(3, 8, 1)));
    VERIFY_IS_TRUE((std::vector<int64_t>{ 0, 2, 5 }) == HitSteps(EuclideanRhythm(3, 8, 2)));

    VERIFY_IS_TRUE(HitSteps(EuclideanRhythm(0, 8, 0)).empty());
    VERIFY_ARE_EQUAL(size_t{ 8 }, HitSteps(EuclideanRhythm(8, 8, 0)).size());
    VERIFY_ARE_EQUAL(size_t{ 8 }, HitSteps(EuclideanRhythm(20, 8, 0)).size());
}

void GeneratorTests::RingsRunTheirOwnLength()
{
    GeneratorSettings generator{};
    generator.Type = GeneratorType::Euclidean;
    generator.Rings = { Ring(50, 3, 8) };

    auto const ons = NoteOns(GeneratorSequence(generator, Bar, 2), 0, 2 * Bar);

    std::vector<int64_t> expected{};

    for (int64_t ring = 0; ring < 4; ++ring)
    {
        for (int64_t const hit : { 0, 3, 6 })
        {
            expected.push_back((ring * 8 + hit) * Step);
        }
    }

    VERIFY_IS_TRUE(expected == Ticks(ons));
    VERIFY_ARE_EQUAL(uint8_t{ 50 }, NoteNumber(ons[0]));

    // A ring of 12 against a one-bar clip comes round 4 steps earlier in each bar.
    generator.Rings = { Ring(50, 1, 12) };
    auto const drifting = NoteOns(GeneratorSequence(generator, Bar, 3), 0, 3 * Bar);
    VERIFY_IS_TRUE((std::vector<int64_t>{ 0, 12 * Step, 24 * Step, 36 * Step }) == Ticks(drifting));
}

void GeneratorTests::RingChanceRepeatsUnlessItChanges()
{
    GeneratorSettings generator{};
    generator.Type = GeneratorType::Euclidean;
    generator.Rings = { Ring(50, 16, 16, 50) };

    // Same: the seed decides once, and every pass plays the same hits.
    auto const same = NoteOns(GeneratorSequence(generator, Bar, 4), 0, 4 * Bar);
    VERIFY_IS_TRUE(Pass(same, 0, Bar) == Pass(same, Bar, Bar));
    VERIFY_IS_TRUE(Pass(same, 0, Bar) == Pass(same, 3 * Bar, Bar));
    VERIFY_IS_TRUE(same.size() > 4 && same.size() < 60);

    // Changes: the pass goes into the roll.
    generator.ChangesEachPass = true;
    auto const changes = NoteOns(GeneratorSequence(generator, Bar, 4), 0, 4 * Bar);
    VERIFY_IS_FALSE(Pass(changes, 0, Bar) == Pass(changes, Bar, Bar));

    // The drawn notes have every hit a ring could play, with its chance on them.
    auto const sequence = GeneratorSequence(generator, Bar, 1);
    VERIFY_ARE_EQUAL(size_t{ 16 }, sequence.Clips[0].Notes.size());
    VERIFY_ARE_EQUAL(uint8_t{ 50 }, sequence.Clips[0].Notes[0].Chance);
}

void GeneratorTests::AChanceMelodyStaysInItsScaleAndRange()
{
    auto generator = MakeChanceMelodyGenerator();
    VERIFY_ARE_EQUAL(static_cast<int>(ScaleKind::MinorPentatonic), static_cast<int>(generator.Scale));

    auto const ons = NoteOns(GeneratorSequence(generator, 2 * Bar, 8), 0, 8 * Bar);

    // About half of 128 steps.
    VERIFY_IS_TRUE(ons.size() > 128 * 3 / 10);
    VERIFY_IS_TRUE(ons.size() < 128 * 7 / 10);

    std::set<int32_t> const degrees{ 0, 3, 5, 7, 10 };

    for (auto const& on : ons)
    {
        auto const note = NoteNumber(on);
        VERIFY_IS_TRUE(note >= generator.LowNote && note <= generator.HighNote);
        VERIFY_IS_TRUE(degrees.contains((note - generator.ScaleRoot + 12) % 12));
        VERIFY_IS_TRUE(Velocity(on) >= generator.VelocityLow && Velocity(on) <= generator.VelocityHigh);
        VERIFY_ARE_EQUAL(int64_t{ 0 }, on.Tick % Step);
    }

    // The scale itself.
    VERIFY_IS_TRUE((std::vector<uint8_t>{ 64, 67, 69, 71, 74, 76 }) == ScaleNotes(4, ScaleKind::MinorPentatonic, 64, 76));
    VERIFY_IS_TRUE((std::vector<uint8_t>{ 61 }) == ScaleNotes(0, ScaleKind::Major, 61, 61));

    generator.Busy = 0;
    VERIFY_IS_TRUE(NoteOns(GeneratorSequence(generator, 2 * Bar, 2), 0, 2 * Bar).empty());

    generator.Busy = 100;
    VERIFY_ARE_EQUAL(size_t{ 32 }, NoteOns(GeneratorSequence(generator, 2 * Bar, 2), 0, 2 * Bar).size());
}

void GeneratorTests::KeepFromTheLastPass()
{
    auto generator = MakeChanceMelodyGenerator();
    generator.Busy = 70;

    // Everything kept: every pass is the first.
    generator.Keep = 100;
    auto ons = NoteOns(GeneratorSequence(generator, Bar, 4), 0, 4 * Bar);
    VERIFY_IS_TRUE(Pass(ons, 0, Bar) == Pass(ons, 3 * Bar, Bar));

    // Nothing kept: new every time.
    generator.Keep = 0;
    ons = NoteOns(GeneratorSequence(generator, Bar, 2), 0, 2 * Bar);
    VERIFY_IS_FALSE(Pass(ons, 0, Bar) == Pass(ons, Bar, Bar));

    // Same each pass, whatever Keep says.
    generator.ChangesEachPass = false;
    ons = NoteOns(GeneratorSequence(generator, Bar, 2), 0, 2 * Bar);
    VERIFY_IS_TRUE(Pass(ons, 0, Bar) == Pass(ons, Bar, Bar));

    // Three quarters kept: the next pass is mostly, but not all, the same.
    generator.ChangesEachPass = true;
    generator.Keep = 75;
    ons = NoteOns(GeneratorSequence(generator, Bar, 2), 0, 2 * Bar);

    auto const first = Pass(ons, 0, Bar);
    auto const second = Pass(ons, Bar, Bar);
    auto const shared = std::count_if(second.begin(), second.end(), [&first](auto const& note) { return std::find(first.begin(), first.end(), note) != first.end(); });

    Log::Comment(String().Format(L"%zu of %zu notes kept", static_cast<size_t>(shared), second.size()));
    VERIFY_IS_FALSE(first == second);
    VERIFY_IS_TRUE(static_cast<size_t>(shared) * 2 >= second.size());

    // Jumping back replays the same pass.
    auto const again = NoteOns(GeneratorSequence(generator, Bar, 2), Bar, 2 * Bar);
    VERIFY_IS_TRUE(second == Pass(again, Bar, Bar));
}

void GeneratorTests::AnArpeggiatorPlaysNothingYet()
{
    GeneratorSettings generator{};
    generator.Type = GeneratorType::Arpeggiator;
    generator.Rings = { Ring(50, 16, 16) };

    VERIFY_IS_TRUE(NoteOns(GeneratorSequence(generator, Bar, 2), 0, 2 * Bar).empty());
}

void GeneratorTests::APieceAtATimeMatchesTheWhole()
{
    for (auto const type : { GeneratorType::Euclidean, GeneratorType::ChanceMelody })
    {
        auto generator = type == GeneratorType::Euclidean ? MakeEuclideanGenerator() : MakeChanceMelodyGenerator();
        generator.ChangesEachPass = true;

        if (type == GeneratorType::Euclidean)
        {
            generator.Rings[1].Steps = 12;
            generator.Rings[2].Chance = 60;
        }

        auto const sequence = GeneratorSequence(generator, 2 * Bar, 8);
        auto const whole = NoteOns(sequence, 0, 8 * Bar);
        VERIFY_IS_TRUE(whole.size() > 20);

        std::vector<RenderedMessage> pieces{};

        for (int64_t from = 0; from < 8 * Bar; from += 37)
        {
            RenderTrack(sequence, sequence.Tracks[0], from, std::min(from + 37, 8 * Bar), pieces, RenderOptions{});
        }

        std::erase_if(pieces, [](RenderedMessage const& message) { return message.Kind != RenderedKind::NoteOn; });
        VERIFY_IS_TRUE(SameMessages(whole, pieces));
    }
}

void GeneratorTests::GeneratorsRoundTripThroughTheFile()
{
    std::wstring const text = LR"({
  "fileVersion": 1,
  "tracks": [],
  "clips": [
    { "id": "c-e", "kind": "generator", "name": "Euclid", "length": 3840, "loop": true, "seed": 11,
      "settings": { "type": "euclidean", "stepTicks": 240, "eachPass": "same", "channel": 2, "noteLength": 40,
        "rings": [ { "note": 52, "pulses": 5, "steps": 16, "rotate": 0, "velocity": 58982, "chance": 100 },
                   { "note": 59, "pulses": 7, "steps": 12, "rotate": 2, "velocity": 45875, "chance": 60 } ] } },
    { "id": "c-m", "kind": "generator", "name": "Wander", "length": 7680, "loop": true, "seed": 12,
      "notes": [ [0, 100, 0, 60, 1000] ],
      "settings": { "type": "chanceMelody", "stepTicks": 480, "eachPass": "changes", "channel": 0, "noteLength": 80,
        "scale": "dorian", "root": 2, "low": 50, "high": 74, "busy": 40, "keep": 60, "velocity": [30000, 60000] } },
    { "id": "c-a", "kind": "generator", "name": "Arp", "length": 3840, "loop": true,
      "settings": { "type": "arpeggiator", "stepTicks": 240, "eachPass": "same", "channel": 0, "noteLength": 60,
        "order": "upAndDown", "octaves": 2, "latch": true } }
  ]
})";

    Sequence read{};
    auto const result = ReadSequenceJson(text, read);
    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_ARE_EQUAL(size_t{ 0 }, result.SkippedItems);

    auto const& euclid = *read.Clips[0].Generator;
    VERIFY_ARE_EQUAL(static_cast<int>(GeneratorType::Euclidean), static_cast<int>(euclid.Type));
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, euclid.Channel);
    VERIFY_ARE_EQUAL(uint16_t{ 40 }, euclid.NoteLength);
    VERIFY_ARE_EQUAL(size_t{ 2 }, euclid.Rings.size());
    VERIFY_IS_TRUE(Ring(59, 7, 12, 60).Note == euclid.Rings[1].Note);
    VERIFY_ARE_EQUAL(uint8_t{ 12 }, euclid.Rings[1].Steps);
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, euclid.Rings[1].Rotate);
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, euclid.Rings[1].Chance);

    auto const& melody = *read.Clips[1].Generator;
    VERIFY_ARE_EQUAL(static_cast<int>(GeneratorType::ChanceMelody), static_cast<int>(melody.Type));
    VERIFY_IS_TRUE(melody.ChangesEachPass);
    VERIFY_ARE_EQUAL(static_cast<int>(ScaleKind::Dorian), static_cast<int>(melody.Scale));
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, melody.ScaleRoot);
    VERIFY_ARE_EQUAL(uint8_t{ 50 }, melody.LowNote);
    VERIFY_ARE_EQUAL(uint8_t{ 74 }, melody.HighNote);
    VERIFY_ARE_EQUAL(uint8_t{ 40 }, melody.Busy);
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, melody.Keep);
    VERIFY_ARE_EQUAL(uint16_t{ 30000 }, melody.VelocityLow);
    VERIFY_ARE_EQUAL(uint16_t{ 60000 }, melody.VelocityHigh);

    // The notes written with it are worked out again, not read.
    VERIFY_IS_FALSE(std::any_of(read.Clips[1].Notes.begin(), read.Clips[1].Notes.end(), [](Note const& note) { return note.Velocity == 1000; }));

    // The arpeggiator's settings, with what it plays from left out: the keys held.
    auto const& arpeggiator = *read.Clips[2].Generator;
    VERIFY_ARE_EQUAL(static_cast<int>(GeneratorType::Arpeggiator), static_cast<int>(arpeggiator.Type));
    VERIFY_ARE_EQUAL(static_cast<int>(ArpeggiatorSource::Held), static_cast<int>(arpeggiator.ArpSource));
    VERIFY_ARE_EQUAL(static_cast<int>(ArpeggiatorOrder::UpAndDown), static_cast<int>(arpeggiator.ArpOrder));
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, arpeggiator.ArpOctaves);
    VERIFY_IS_TRUE(arpeggiator.ArpLatch);

    auto const written = WriteSequenceJson(read);
    VERIFY_IS_TRUE(Contains(written, L"\"type\": \"euclidean\""));
    VERIFY_IS_TRUE(Contains(written, L"\"eachPass\": \"changes\""));
    VERIFY_IS_TRUE(Contains(written, L"\"scale\": \"dorian\""));
    VERIFY_IS_TRUE(Contains(written, L"\"velocity\": [30000, 60000]"));
    VERIFY_IS_TRUE(Contains(written, L"\"order\": \"upAndDown\""));
    VERIFY_IS_TRUE(Contains(written, L"\"source\": \"held\""));
    VERIFY_IS_TRUE(Contains(written, L"\"latch\": true"));
    VERIFY_IS_FALSE(Contains(written, L"\"notes\""));

    Sequence again{};
    VERIFY_IS_TRUE(ReadSequenceJson(written, again).Succeeded());
    VERIFY_ARE_EQUAL(written, WriteSequenceJson(again));
}

void GeneratorTests::DamagedGeneratorsAreClamped()
{
    std::wstring const text = LR"({
  "fileVersion": 1,
  "tracks": [],
  "clips": [
    { "id": "c-1", "kind": "generator", "name": "G", "length": 3840,
      "settings": { "type": "teleport", "stepTicks": 99999, "channel": 30, "noteLength": 0,
        "rings": [ { "note": 300, "pulses": 50, "steps": 100, "rotate": 200, "chance": 300 }, "junk" ],
        "scale": "wobbly", "root": 40, "low": 90, "high": 20, "busy": 200, "keep": -5, "velocity": [60000, 1000] } }
  ]
})";

    Sequence read{};
    auto const result = ReadSequenceJson(text, read);
    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_ARE_EQUAL(size_t{ 1 }, result.SkippedItems);

    auto const& generator = *read.Clips[0].Generator;
    VERIFY_ARE_EQUAL(static_cast<int>(GeneratorType::Euclidean), static_cast<int>(generator.Type));
    VERIFY_ARE_EQUAL(int64_t{ TicksPerQuarterNote * 4 }, generator.StepTicks);
    VERIFY_ARE_EQUAL(uint8_t{ 15 }, generator.Channel);
    VERIFY_ARE_EQUAL(uint16_t{ 1 }, generator.NoteLength);

    VERIFY_ARE_EQUAL(size_t{ 1 }, generator.Rings.size());
    VERIFY_ARE_EQUAL(uint8_t{ 127 }, generator.Rings[0].Note);
    VERIFY_ARE_EQUAL(MaximumRingSteps, generator.Rings[0].Steps);
    VERIFY_ARE_EQUAL(uint8_t{ 50 }, generator.Rings[0].Pulses);
    VERIFY_ARE_EQUAL(uint8_t{ 63 }, generator.Rings[0].Rotate);
    VERIFY_ARE_EQUAL(uint8_t{ 100 }, generator.Rings[0].Chance);

    VERIFY_ARE_EQUAL(static_cast<int>(ScaleKind::MinorPentatonic), static_cast<int>(generator.Scale));
    VERIFY_ARE_EQUAL(uint8_t{ 11 }, generator.ScaleRoot);
    VERIFY_ARE_EQUAL(uint8_t{ 20 }, generator.LowNote);
    VERIFY_ARE_EQUAL(uint8_t{ 90 }, generator.HighNote);
    VERIFY_ARE_EQUAL(uint8_t{ 100 }, generator.Busy);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, generator.Keep);
    VERIFY_ARE_EQUAL(uint16_t{ 1000 }, generator.VelocityLow);
    VERIFY_ARE_EQUAL(uint16_t{ 60000 }, generator.VelocityHigh);
}

void GeneratorTests::MakeNotesPlaysWhatTheGeneratorPlayed()
{
    // Rings of 16 and 12 the same each pass: they line up after 48 steps, three bars.
    GeneratorSettings generator{};
    generator.Type = GeneratorType::Euclidean;
    generator.Rings = { Ring(50, 5, 16), Ring(55, 5, 12, 40) };

    auto sequence = GeneratorSequence(generator, Bar, 6);
    auto const notes = MakeNotesFromGenerator(sequence.Clips[0], 4, 64 * Bar);
    VERIFY_ARE_EQUAL(static_cast<int>(ClipKind::Notes), static_cast<int>(notes.Kind));
    VERIFY_ARE_EQUAL(3 * Bar, notes.Length);
    VERIFY_ARE_EQUAL(static_cast<int>(ClipOrigin::Generated), static_cast<int>(notes.Origin));

    auto copy = sequence;
    copy.Clips.push_back(notes);
    copy.Tracks[0].Timeline = { Placement{ notes.Id, 0, 6 * Bar } };
    VERIFY_IS_TRUE(SameMessages(NoteOns(sequence, 0, 6 * Bar), NoteOns(copy, 0, 6 * Bar)));

    // A melody that changes gives the passes asked for.
    auto melody = GeneratorSequence(MakeChanceMelodyGenerator(), 2 * Bar, 8);
    auto const melodyNotes = MakeNotesFromGenerator(melody.Clips[0], 4, 64 * Bar);
    VERIFY_ARE_EQUAL(8 * Bar, melodyNotes.Length);

    auto melodyCopy = melody;
    melodyCopy.Clips.push_back(melodyNotes);
    melodyCopy.Tracks[0].Timeline = { Placement{ melodyNotes.Id, 0, 8 * Bar } };
    VERIFY_IS_TRUE(SameMessages(NoteOns(melody, 0, 8 * Bar), NoteOns(melodyCopy, 0, 8 * Bar)));

    // Never more than the ceiling, in whole passes.
    VERIFY_ARE_EQUAL(2 * Bar, MakeNotesFromGenerator(melody.Clips[0], 4, 3 * Bar).Length);
}

void GeneratorTests::GeneratorEditsUndo()
{
    auto sequence = GeneratorSequence(MakeEuclideanGenerator(), Bar, 1);
    auto& clip = sequence.Clips[0];
    auto const notesBefore = clip.Notes.size();

    auto const before = *clip.Generator;
    auto after = before;
    after.Rings.push_back(Ring(40, 16, 16));

    clip.Generator = after;
    RefreshGeneratorPreview(clip);

    UndoStack undo{};
    ChangeList changes{};
    changes.push_back(MakeClipGeneratorChange(clip.Id, before, after));
    undo.Commit(L"Edit generator", std::move(changes));

    VERIFY_ARE_EQUAL(notesBefore + 16, sequence.Clips[0].Notes.size());

    VERIFY_IS_TRUE(undo.Undo(sequence));
    VERIFY_ARE_EQUAL(notesBefore, sequence.Clips[0].Notes.size());

    VERIFY_IS_TRUE(undo.Redo(sequence));
    VERIFY_ARE_EQUAL(notesBefore + 16, sequence.Clips[0].Notes.size());

    // A new generator clip has the comp's settings and notes to draw.
    auto const made = MakeGeneratorClip(L"Euclid", GeneratorType::Euclidean, Bar);
    VERIFY_ARE_EQUAL(size_t{ 3 }, made.Generator->Rings.size());
    VERIFY_ARE_EQUAL(size_t{ 15 }, made.Notes.size());
}
