// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "StrumTests.h"

#include "NoteTools.h"
#include "SequenceArpeggiator.h"
#include "SequenceSerializer.h"
#include "SequenceStrum.h"
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
    constexpr int64_t Bar = TicksPerQuarterNote * 4;

    bool ByTimeThenNote(Note const& a, Note const& b) noexcept
    {
        return a.Tick != b.Tick ? a.Tick < b.Tick : a.Number < b.Number;
    }

    // A bar of C major on the Key and chords row, and a Strum with nothing random about it: one
    // down stroke a beat, 40 ticks from first note to last, evenly.
    Sequence SongWithChord(ChordType type = ChordType::Major, uint8_t root = 0)
    {
        Sequence sequence{};
        sequence.Harmony = HarmonyTrack{};
        sequence.Harmony->Chords.push_back(ChordEvent{ 0, Bar, root, type });
        return sequence;
    }

    Clip StrumClip(wchar_t const* pattern = L"D")
    {
        Clip clip{};
        clip.Id = L"strum";
        clip.Kind = ClipKind::Generator;
        clip.Length = Bar;
        clip.Seed = 1;
        clip.Generator = MakeStrumGenerator();

        auto& generator = *clip.Generator;
        generator.StrumPattern = pattern;
        generator.StepTicks = TicksPerQuarterNote;
        generator.StrumInTicks = true;
        generator.StrumSpreadTicks = 40;
        generator.StrumCurve = StrumShape::Even;
        generator.StrumHand = 0;
        generator.StrumTilt = 0;
        return clip;
    }

    std::vector<Note> Play(Sequence const& sequence, Clip const& clip, int64_t from = 0, int64_t to = Bar)
    {
        std::vector<Note> notes{};
        ForEachStrumNote(sequence, clip, 0, clip.Length, 0, from, to, [&notes](Note const& note) { notes.push_back(note); });
        std::stable_sort(notes.begin(), notes.end(), ByTimeThenNote);
        return notes;
    }

    std::vector<int32_t> Numbers(std::vector<StrungNote> const& strung)
    {
        std::vector<int32_t> numbers{};

        for (auto const& note : strung)
        {
            numbers.push_back(note.Number);
        }

        return numbers;
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

    std::wstring Listed(std::vector<int32_t> const& numbers)
    {
        std::wstring text{};

        for (auto const number : numbers)
        {
            text += std::to_wstring(number) + L" ";
        }

        return text;
    }

    void SameNumbers(std::vector<int32_t> const& actual, std::vector<int32_t> const& expected)
    {
        VERIFY_IS_TRUE(actual == expected, (L"got " + Listed(actual) + L"expected " + Listed(expected)).c_str());
    }

    std::vector<int32_t> Guitar(std::vector<uint8_t> const& classes, uint8_t bass)
    {
        return Numbers(VoiceChord(StrumInstrument::Guitar, classes, bass, {}));
    }

    std::vector<Note> Applied(std::vector<Note> notes, NoteToolResult const& result)
    {
        for (auto const& removed : result.Removed)
        {
            auto const found = std::find(notes.begin(), notes.end(), removed);
            VERIFY_IS_TRUE(found != notes.end());

            if (found != notes.end())
            {
                notes.erase(found);
            }
        }

        notes.insert(notes.end(), result.Added.begin(), result.Added.end());
        std::stable_sort(notes.begin(), notes.end(), ByTimeThenNote);
        return notes;
    }
}

void StrumTests::CurvesStartAtZeroAndEndAtOne()
{
    for (auto const shape : { StrumShape::Even, StrumShape::SpeedingUp, StrumShape::SlowingDown, StrumShape::Ease, StrumShape::Rake })
    {
        VERIFY_ARE_EQUAL(0.0, StrumCurve(shape, 0.0, 0.8));
        VERIFY_ARE_EQUAL(1.0, StrumCurve(shape, 1.0, 0.8));

        // Never goes back on itself: the notes stay in strum order.
        auto previous = 0.0;

        for (int32_t i = 1; i <= 20; ++i)
        {
            auto const value = StrumCurve(shape, i / 20.0, 1.0);
            VERIFY_IS_GREATER_THAN_OR_EQUAL(value, previous);
            previous = value;
        }
    }

    // Speeding up leaves more time between the first notes than the last.
    VERIFY_IS_GREATER_THAN(StrumCurve(StrumShape::SpeedingUp, 0.25, 0.5), 0.25);
    VERIFY_IS_GREATER_THAN(StrumCurve(StrumShape::Rake, 1.0 / 3.0, 1.0), 0.6);
}

void StrumTests::GuitarPlaysOpenAndBarreShapes()
{
    // C x32010, G 320003, D xx0232, E minor 022000, A minor x02210, F 133211.
    SameNumbers(Guitar({ 0, 4, 7 }, 0), std::vector<int32_t>{ 48, 52, 55, 60, 64 });
    SameNumbers(Guitar({ 7, 11, 2 }, 7), std::vector<int32_t>{ 43, 47, 50, 55, 59, 67 });
    SameNumbers(Guitar({ 2, 6, 9 }, 2), std::vector<int32_t>{ 50, 57, 62, 66 });
    SameNumbers(Guitar({ 4, 7, 11 }, 4), std::vector<int32_t>{ 40, 47, 52, 55, 59, 64 });
    SameNumbers(Guitar({ 9, 0, 4 }, 9), std::vector<int32_t>{ 45, 52, 57, 60, 64 });
    SameNumbers(Guitar({ 5, 9, 0 }, 5), std::vector<int32_t>{ 41, 48, 53, 57, 60, 65 });

    // The strings are numbered from the low E, and C leaves it out.
    auto const c = VoiceChord(StrumInstrument::Guitar, { 0, 4, 7 }, 0, {});
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, c.front().String);
    VERIFY_ARE_EQUAL(uint8_t{ 5 }, c.back().String);

    // The bass of C/E is on the lowest string that sounds.
    VERIFY_ARE_EQUAL(4, Guitar({ 0, 4, 7 }, 4).front() % 12);
}

void StrumTests::UkuleleAndPianoVoicings()
{
    // C on a ukulele is 0003, its G string first.
    SameNumbers(Numbers(VoiceChord(StrumInstrument::Ukulele, { 0, 4, 7 }, 0, {})), std::vector<int32_t>{ 67, 60, 64, 72 });

    // C/E on a piano: E in the left hand, C major close together around middle C in the right.
    SameNumbers(Numbers(VoiceChord(StrumInstrument::Piano, { 0, 4, 7 }, 4, {})), std::vector<int32_t>{ 40, 55, 60, 64 });

    // As written plays the notes it's given, lowest first, each once.
    SameNumbers(Numbers(VoiceChord(StrumInstrument::AsWritten, { 0, 4, 7 }, 0, { 64, 60, 67, 60 })), std::vector<int32_t>{ 60, 64, 67 });
}

void StrumTests::DownStrokeSpreadsLowToHigh()
{
    auto const sequence = SongWithChord();
    auto const notes = Play(sequence, StrumClip(L"D"));

    VERIFY_ARE_EQUAL(size_t{ 20 }, notes.size(), L"five strings on each of four beats");

    std::vector<Note> first(notes.begin(), notes.begin() + 5);
    SameNumbers(Numbers(first), std::vector<int32_t>{ 48, 52, 55, 60, 64 });

    for (size_t i = 0; i < first.size(); ++i)
    {
        VERIFY_ARE_EQUAL(static_cast<int64_t>(i) * 10, first[i].Tick);
        VERIFY_ARE_EQUAL(uint16_t{ 52000 }, first[i].Velocity);
    }

    VERIFY_ARE_EQUAL(int64_t{ TicksPerQuarterNote }, notes[5].Tick);

    // Nothing without chords.
    VERIFY_IS_TRUE(Play(Sequence{}, StrumClip(L"D")).empty());
}

void StrumTests::UpStrokeHitsTheTopStrings()
{
    auto const sequence = SongWithChord();
    auto const notes = Play(sequence, StrumClip(L"U"), 0, TicksPerQuarterNote);

    // The top four strings, the high E first, spread over 70% of the down stroke's time.
    SameNumbers(Numbers(notes), std::vector<int32_t>{ 64, 60, 55, 52 });
    VERIFY_ARE_EQUAL(int64_t{ 0 }, notes.front().Tick);
    VERIFY_ARE_EQUAL(int64_t{ 28 }, notes.back().Tick);
    VERIFY_ARE_EQUAL(uint16_t{ 44200 }, notes.front().Velocity);
}

void StrumTests::SpreadInMillisecondsFollowsTheTempo()
{
    auto sequence = SongWithChord();
    auto clip = StrumClip(L"D");
    clip.Generator->StrumInTicks = false;
    clip.Generator->StrumSpreadMs = 30;

    // 30 ms at 120 BPM is 57.6 ticks; at 60 BPM a tick lasts twice as long, so half as many.
    auto const atOneTwenty = Play(sequence, clip, 0, TicksPerQuarterNote);
    auto const fast = atOneTwenty.back().Tick - atOneTwenty.front().Tick;
    VERIFY_IS_TRUE(fast >= 57 && fast <= 58, std::to_wstring(fast).c_str());

    sequence.Tempo = { TempoPoint{ 0, 60.0, false } };
    auto const atSixty = Play(sequence, clip, 0, TicksPerQuarterNote);
    auto const slow = atSixty.back().Tick - atSixty.front().Tick;
    VERIFY_IS_TRUE(slow >= 28 && slow <= 29, std::to_wstring(slow).c_str());
}

void StrumTests::LastNoteLandsOnTheBeatAndBassStays()
{
    auto const sequence = SongWithChord();
    auto clip = StrumClip(L"D");
    clip.Generator->StrumLand = StrumLands::Last;
    clip.Generator->StrumKeepBass = true;

    auto const notes = Play(sequence, clip, 900, 1000);
    VERIFY_ARE_EQUAL(size_t{ 5 }, notes.size());

    auto const find = [&notes](uint8_t number)
    {
        return std::find_if(notes.begin(), notes.end(), [number](Note const& note) { return note.Number == number; })->Tick;
    };

    VERIFY_ARE_EQUAL(int64_t{ 960 }, find(48), L"the bass on the beat");
    VERIFY_ARE_EQUAL(int64_t{ 920 }, find(52), L"the strum starts early");
    VERIFY_ARE_EQUAL(int64_t{ 960 }, find(64), L"and its last note lands on the beat");
}

void StrumTests::StringsRingUntilTheNextStroke()
{
    auto const sequence = SongWithChord();
    auto clip = StrumClip(L"D-U-");
    clip.Generator->StepTicks = TicksPerQuarterNote / 2;

    auto const notes = Play(sequence, clip, 0, 1);
    auto const firstStroke = Play(sequence, clip, 0, 100);

    auto const lengthOf = [&firstStroke](uint8_t number)
    {
        auto const note = std::find_if(firstStroke.begin(), firstStroke.end(), [number](Note const& n) { return n.Number == number && n.Tick < 100; });
        return note->Tick + note->Length;
    };

    VERIFY_ARE_EQUAL(size_t{ 1 }, notes.size());

    // The A string isn't hit by the up stroke, so it rings until the next down stroke.
    VERIFY_ARE_EQUAL(int64_t{ 1920 }, lengthOf(48));
    VERIFY_ARE_EQUAL(int64_t{ 960 }, lengthOf(64));

    // Without ring, each note is half a step.
    clip.Generator->StrumRing = false;
    VERIFY_ARE_EQUAL(int64_t{ 240 }, Play(sequence, clip, 0, 1).front().Length);
}

void StrumTests::RasgueadoAndTremoloFillTheStep()
{
    auto const sequence = SongWithChord();

    auto const rasgueado = Play(sequence, StrumClip(L"R---"));
    VERIFY_ARE_EQUAL(size_t{ 20 }, rasgueado.size(), L"four flicks of five strings");

    // Speeding up: the flicks come closer together.
    std::vector<int64_t> starts{};

    for (auto const& note : rasgueado)
    {
        if (note.Number == 48)
        {
            starts.push_back(note.Tick);
        }
    }

    VERIFY_ARE_EQUAL(size_t{ 4 }, starts.size());
    VERIFY_IS_GREATER_THAN(starts[1] - starts[0], starts[3] - starts[2]);

    auto const tremolo = Play(sequence, StrumClip(L"T---"));
    VERIFY_ARE_EQUAL(size_t{ 18 }, tremolo.size(), L"down, up, down, up: 5 + 4 + 5 + 4");
}

void StrumTests::KeySwitchesForMutedStrokes()
{
    auto const sequence = SongWithChord();
    auto clip = StrumClip(L"Dx");
    clip.Generator->StrumMute = StrumMuting::KeySwitch;
    clip.Generator->StrumMuteKey = 24;
    clip.Generator->StrumOpenKey = 26;

    std::vector<Note> switches{};

    for (auto const& note : Play(sequence, clip))
    {
        if (note.Number == 24 || note.Number == 26)
        {
            switches.push_back(note);
        }
    }

    VERIFY_ARE_EQUAL(size_t{ 4 }, switches.size());
    VERIFY_ARE_EQUAL(uint8_t{ 26 }, switches[0].Number);
    VERIFY_ARE_EQUAL(int64_t{ 0 }, switches[0].Tick);
    VERIFY_ARE_EQUAL(uint8_t{ 24 }, switches[1].Number);
    VERIFY_ARE_EQUAL(int64_t{ 959 }, switches[1].Tick, L"just before the muted stroke");
    VERIFY_ARE_EQUAL(uint8_t{ 26 }, switches[2].Number);

    // Short muting sends none, and keeps the muted stroke short.
    clip.Generator->StrumMute = StrumMuting::Short;
    auto const plain = Play(sequence, clip, 960, 1000);
    VERIFY_IS_TRUE(std::none_of(plain.begin(), plain.end(), [](Note const& note) { return note.Number == 24 || note.Number == 26; }));
    VERIFY_IS_LESS_THAN(plain.front().Length, int64_t{ 60 });
}

void StrumTests::SameNotesInAnyWindows()
{
    auto const sequence = SongWithChord(ChordType::Minor, 9);
    auto clip = StrumClip(L"D-DU-UDx");
    clip.Generator->StepTicks = TicksPerQuarterNote / 2;
    clip.Generator->StrumLand = StrumLands::Middle;
    clip.Generator->StrumHand = 60;
    clip.Generator->StrumTilt = 20;

    auto const whole = Play(sequence, clip);

    std::vector<Note> pieces{};

    for (auto const& [from, to] : { std::pair<int64_t, int64_t>{ 0, 1000 }, { 1000, 1450 }, { 1450, 2500 }, { 2500, Bar } })
    {
        auto const piece = Play(sequence, clip, from, to);
        pieces.insert(pieces.end(), piece.begin(), piece.end());
    }

    std::stable_sort(pieces.begin(), pieces.end(), ByTimeThenNote);
    VERIFY_IS_FALSE(whole.empty());
    VERIFY_IS_TRUE(whole == pieces);
}

void StrumTests::FollowsAnotherTrack()
{
    Sequence sequence{};

    Clip held{};
    held.Id = L"held";
    held.Length = Bar;
    held.Notes = { MakeNote(0, Bar, 57), MakeNote(0, Bar, 60), MakeNote(0, Bar, 64) };
    sequence.Clips.push_back(held);

    Track track{};
    track.Id = L"piano";
    track.Timeline.push_back(Placement{ L"held", 0, 0 });
    sequence.Tracks.push_back(track);

    auto clip = StrumClip(L"D");
    clip.Generator->ArpSource = ArpeggiatorSource::Track;
    clip.Generator->ArpTrackId = L"piano";

    // The track plays A minor, which a guitar holds as x02210.
    auto const notes = Play(sequence, clip, 0, TicksPerQuarterNote);
    SameNumbers(Numbers(notes), std::vector<int32_t>{ 45, 52, 57, 60, 64 });

    // As written strums the track's own notes.
    clip.Generator->Instrument = StrumInstrument::AsWritten;
    SameNumbers(Numbers(Play(sequence, clip, 0, TicksPerQuarterNote)), std::vector<int32_t>{ 57, 60, 64 });

    // A Strum doesn't follow another Strum's track.
    sequence.Clips.push_back(clip);
    sequence.Tracks[0].Timeline.push_back(Placement{ L"strum", Bar, 0 });
    VERIFY_IS_TRUE(NotesSoundingAt(sequence, sequence.Tracks[0], Bar + 10).Notes.empty());
}

void StrumTests::MakeNotesBakesTheStrum()
{
    auto const sequence = SongWithChord();
    auto clip = StrumClip(L"D-DU");
    clip.Generator->StrumHand = 40;

    auto const made = MakeNotesFromArpeggiator(sequence, clip, Placement{ clip.Id, 0, 0 });
    VERIFY_IS_TRUE(made.Kind == ClipKind::Notes);

    auto played = Play(sequence, clip);
    auto baked = made.Notes;
    std::stable_sort(baked.begin(), baked.end(), ByTimeThenNote);
    VERIFY_IS_TRUE(played == baked);
}

void StrumTests::StrumSettingsRoundTrip()
{
    Sequence sequence{};

    auto strum = StrumClip(L"R-DUR-DU");
    auto& generator = *strum.Generator;
    generator.Instrument = StrumInstrument::Ukulele;
    generator.StrumInTicks = false;
    generator.StrumSpreadMs = 44;
    generator.StrumSpreadTicks = 77;
    generator.StrumCurve = StrumShape::Rake;
    generator.StrumBend = 33;
    generator.StrumLand = StrumLands::Middle;
    generator.StrumKeepBass = true;
    generator.StrumRing = false;
    generator.StrumTilt = -12;
    generator.StrumHand = 45;
    generator.StrumBurst = 6;
    generator.StrumBurstCurve = StrumShape::SlowingDown;
    generator.StrumMute = StrumMuting::KeySwitch;
    generator.StrumMuteKey = 30;
    generator.StrumOpenKey = 31;
    sequence.Clips.push_back(strum);

    Clip notes{};
    notes.Id = L"notes";
    auto strummed = MakeNote(10, 470, 64);
    strummed.StrumOffset = 10;
    auto early = MakeNote(0, 500, 60);
    early.StrumOffset = -12;
    notes.Notes = { early, strummed };
    sequence.Clips.push_back(notes);

    Track track{};
    track.Id = L"t";
    track.Timeline = { Placement{ L"strum", 0, 0 }, Placement{ L"notes", Bar, 0 } };
    sequence.Tracks.push_back(track);

    auto const text = WriteSequenceJson(sequence);

    Sequence read{};
    VERIFY_IS_TRUE(ReadSequenceJson(text, read).Succeeded());

    auto const readStrum = FindClip(read, L"strum");
    VERIFY_IS_NOT_NULL(readStrum);
    VERIFY_IS_TRUE(readStrum->Generator.has_value());

    auto expected = generator;
    expected.Unknown = nullptr;
    auto actual = *readStrum->Generator;
    actual.Unknown = nullptr;
    VERIFY_IS_TRUE(expected.Type == actual.Type && expected.Instrument == actual.Instrument && expected.StrumPattern == actual.StrumPattern &&
        expected.StrumInTicks == actual.StrumInTicks && expected.StrumSpreadMs == actual.StrumSpreadMs && expected.StrumSpreadTicks == actual.StrumSpreadTicks &&
        expected.StrumCurve == actual.StrumCurve && expected.StrumBend == actual.StrumBend && expected.StrumLand == actual.StrumLand &&
        expected.StrumKeepBass == actual.StrumKeepBass && expected.StrumRing == actual.StrumRing && expected.StrumTilt == actual.StrumTilt &&
        expected.StrumHand == actual.StrumHand && expected.StrumBurst == actual.StrumBurst && expected.StrumBurstCurve == actual.StrumBurstCurve &&
        expected.StrumMute == actual.StrumMute && expected.StrumMuteKey == actual.StrumMuteKey && expected.StrumOpenKey == actual.StrumOpenKey &&
        expected.ArpSource == actual.ArpSource && expected.VelocityLow == actual.VelocityLow && expected.VelocityHigh == actual.VelocityHigh);

    auto const readNotes = FindClip(read, L"notes");
    VERIFY_IS_NOT_NULL(readNotes);
    VERIFY_ARE_EQUAL(size_t{ 2 }, readNotes->Notes.size());
    VERIFY_ARE_EQUAL(-12, readNotes->Notes[0].StrumOffset);
    VERIFY_ARE_EQUAL(10, readNotes->Notes[1].StrumOffset);

    // A file that comes back the same.
    VERIFY_ARE_EQUAL(text, WriteSequenceJson(read));
}

void StrumTests::PianoRollStrumRedoesAndComesBack()
{
    NoteToolContext context{};
    std::vector<Note> const original{ MakeNote(960, 480, 48), MakeNote(960, 480, 52), MakeNote(960, 480, 55), MakeNote(1920, 240, 72) };

    StrumSettings settings{};
    settings.InTicks = true;
    settings.SpreadTicks = 30;
    settings.Shape = StrumShape::Even;

    auto const down = Applied(original, Strum(original, context, settings));
    VERIFY_ARE_EQUAL(int64_t{ 960 }, down[0].Tick);
    VERIFY_ARE_EQUAL(int64_t{ 975 }, down[1].Tick);
    VERIFY_ARE_EQUAL(int64_t{ 990 }, down[2].Tick);
    VERIFY_ARE_EQUAL(int64_t{ 1440 }, down[2].Tick + down[2].Length, L"the end stays");
    VERIFY_ARE_EQUAL(30, down[2].StrumOffset);
    VERIFY_ARE_EQUAL(0, down[3].StrumOffset, L"a single note isn't strummed");
    VERIFY_ARE_EQUAL(size_t{ 2 }, CountStrummed(down));

    // Strumming again starts from the chord as it was, so it replaces the first strum.
    settings.Direction = StrumDirection::Up;
    auto const up = Applied(down, Strum(down, context, settings));
    SameNumbers(Numbers(up), std::vector<int32_t>{ 55, 52, 48, 72 });
    VERIFY_ARE_EQUAL(int64_t{ 990 }, up[2].Tick);

    // Remove strum puts every note back exactly.
    auto restored = Applied(up, RemoveStrum(up));
    auto sortedOriginal = original;
    std::stable_sort(sortedOriginal.begin(), sortedOriginal.end(), ByTimeThenNote);
    VERIFY_IS_TRUE(restored == sortedOriginal);

    // Another tool that moves a strummed note makes it forget the strum.
    QuantizeSettings quantize{};
    auto const quantized = Applied(down, Quantize(down, context, quantize));
    VERIFY_ARE_EQUAL(size_t{ 0 }, CountStrummed(quantized));
}
