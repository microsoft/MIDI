// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "HarmonyFollowTests.h"

#include "Harmony.h"
#include "HarmonyFollow.h"
#include "SequenceArpeggiator.h"
#include "SequenceGenerator.h"
#include "SequenceRender.h"
#include "SequenceSerializer.h"
#include "StandardMidiFileBridge.h"
#include "midi_file_smf_reader.h"

#include <algorithm>
#include <set>
#include <span>
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

    constexpr uint8_t C = 0;
    constexpr uint8_t D = 2;
    constexpr uint8_t E = 4;
    constexpr uint8_t F = 5;
    constexpr uint8_t G = 7;
    constexpr uint8_t A = 9;
    constexpr uint8_t B = 11;

    KeyChange Key(uint8_t root, ScaleKind scale, int64_t tick = 0)
    {
        return KeyChange{ tick, root, scale };
    }

    ChordEvent Chord(uint8_t root, ChordType type, int64_t tick = 0, int64_t length = Bar)
    {
        ChordEvent chord{};
        chord.Tick = tick;
        chord.Length = length;
        chord.Root = root;
        chord.Type = type;
        return chord;
    }

    Note MakeNote(int64_t tick, uint8_t number)
    {
        Note note{};
        note.Tick = tick;
        note.Length = Beat / 2;
        note.Number = number;
        note.Velocity = 40000;
        return note;
    }

    // A sequence with one track that places the clip once at tick 0.
    Sequence OneClip(Clip clip, int64_t placementLength)
    {
        Sequence sequence{};
        clip.Id = L"clip";
        sequence.Clips.push_back(std::move(clip));

        Track track{};
        track.Id = L"track";
        track.Timeline.push_back(Placement{ L"clip", 0, placementLength });
        sequence.Tracks.push_back(std::move(track));
        return sequence;
    }

    // The note number and tick of each note on rendered.
    std::vector<std::pair<int64_t, uint8_t>> NotesOn(Sequence const& sequence, int64_t toTick)
    {
        std::vector<RenderedMessage> messages{};
        RenderTrack(sequence, sequence.Tracks.front(), 0, toTick, messages);

        std::vector<std::pair<int64_t, uint8_t>> notes{};

        for (auto const& message : messages)
        {
            if (message.Kind == RenderedKind::NoteOn)
            {
                notes.emplace_back(message.Tick, static_cast<uint8_t>((message.Words[0] >> 8) & 0x7F));
            }
        }

        return notes;
    }

    bool InScale(uint8_t note, uint8_t root, ScaleKind scale)
    {
        auto const notes = ScaleNotes(root, scale, 0, 127);
        return std::find(notes.begin(), notes.end(), note) != notes.end();
    }
}

void HarmonyFollowTests::ScaleStepsBetweenKeys()
{
    auto const cMajor = Key(C, ScaleKind::Major);

    // E4 in C major is F♯4 in D major and E♭4 in C minor.
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(66), MoveByScaleSteps(64, cMajor, Key(D, ScaleKind::Major)));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(63), MoveByScaleSteps(64, cMajor, Key(C, ScaleKind::NaturalMinor)));

    // C major to B major goes down a semitone, not up 11.
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(59), MoveByScaleSteps(60, cMajor, Key(B, ScaleKind::Major)));

    // To G major the root goes down a fourth: C4 is G3, E4 is B3.
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(55), MoveByScaleSteps(60, cMajor, Key(G, ScaleKind::Major)));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(59), MoveByScaleSteps(64, cMajor, Key(G, ScaleKind::Major)));

    // The same key is left alone, and every note of a scale lands in the other.
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(61), MoveByScaleSteps(61, cMajor, cMajor));

    for (auto const note : ScaleNotes(C, ScaleKind::Major, 36, 84))
    {
        VERIFY_IS_TRUE(InScale(MoveByScaleSteps(note, cMajor, Key(A, ScaleKind::HarmonicMinor)), A, ScaleKind::HarmonicMinor));
        VERIFY_IS_TRUE(InScale(MoveByScaleSteps(note, cMajor, Key(F, ScaleKind::Dorian)), F, ScaleKind::Dorian));
    }

    // Into a pentatonic scale, each step lands on a note of it.
    for (auto const note : ScaleNotes(C, ScaleKind::Major, 48, 72))
    {
        VERIFY_IS_TRUE(InScale(MoveByScaleSteps(note, cMajor, Key(E, ScaleKind::MinorPentatonic)), E, ScaleKind::MinorPentatonic));
    }

    // Into a scale with fewer notes on the same root, the notes it shares stay put: C–E–G is still
    // C–E–G in C major pentatonic, and F, which it doesn't have, goes down to E. G stays G in the
    // blues scale.
    auto const pentatonic = Key(C, ScaleKind::MajorPentatonic);
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(60), MoveByScaleSteps(60, cMajor, pentatonic));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(62), MoveByScaleSteps(62, cMajor, pentatonic));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(64), MoveByScaleSteps(64, cMajor, pentatonic));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(64), MoveByScaleSteps(65, cMajor, pentatonic));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(67), MoveByScaleSteps(67, cMajor, pentatonic));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(67), MoveByScaleSteps(67, cMajor, Key(C, ScaleKind::Blues)));
}

void HarmonyFollowTests::ScaleStepsKeepAlterationsAndRange()
{
    auto const cMajor = Key(C, ScaleKind::Major);

    // F♯4 (a semitone over F) in C major is G♯4 in D major (a semitone over G).
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(68), MoveByScaleSteps(66, cMajor, Key(D, ScaleKind::Major)));

    // Below the root, and at both ends.
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(1), MoveByScaleSteps(0, cMajor, Key(1, ScaleKind::Major)));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(127), MoveByScaleSteps(127, cMajor, Key(D, ScaleKind::Major)));

    for (int note = 0; note <= 127; ++note)
    {
        auto const moved = MoveByScaleSteps(static_cast<uint8_t>(note), Key(F, ScaleKind::Lydian), Key(B, ScaleKind::Blues));
        VERIFY_IS_TRUE(moved <= 127);
        VERIFY_IS_TRUE(std::abs(static_cast<int>(moved) - note) <= 12);
    }
}

void HarmonyFollowTests::NearestChordTones()
{
    auto const cMajor = Chord(C, ChordType::Major);

    VERIFY_ARE_EQUAL(static_cast<uint8_t>(64), NearestChordTone(64, cMajor));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(64), NearestChordTone(65, cMajor));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(67), NearestChordTone(69, cMajor));

    // D is as near C as E: the lower one.
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(60), NearestChordTone(62, cMajor));

    // The bass of a slash chord counts.
    auto slash = Chord(C, ChordType::Major);
    slash.Bass = D;
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(62), NearestChordTone(62, slash));
}

void HarmonyFollowTests::NotesClipFollowsTheKey()
{
    Clip clip{};
    clip.Kind = ClipKind::Notes;
    clip.Length = Bar;
    clip.Loop = true;
    clip.Notes = { MakeNote(0, 60), MakeNote(Beat, 64), MakeNote(Beat * 2, 67) };

    auto sequence = OneClip(clip, Bar * 2);

    HarmonyTrack harmony{};
    harmony.Keys = { Key(C, ScaleKind::Major), Key(D, ScaleKind::Major, Bar) };
    sequence.Harmony = harmony;

    // Off: as written in both bars.
    auto notes = NotesOn(sequence, Bar * 2);
    VERIFY_ARE_EQUAL(size_t{ 6 }, notes.size());
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(64), notes[4].second);

    // On, written in C major: the second pass, under D major, plays D, F♯ and A.
    sequence.Clips.front().FollowKey = true;
    sequence.Clips.front().WrittenRoot = C;
    sequence.Clips.front().WrittenScale = ScaleKind::Major;

    notes = NotesOn(sequence, Bar * 2);
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(60), notes[0].second);
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(62), notes[3].second);
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(66), notes[4].second);
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(69), notes[5].second);

    // Without a Key and chords row, it plays as written.
    sequence.Harmony.reset();
    notes = NotesOn(sequence, Bar * 2);
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(64), notes[4].second);
}

void HarmonyFollowTests::DrumPatternsNeverFollow()
{
    Clip clip{};
    clip.Kind = ClipKind::Pattern;
    clip.FollowKey = true;
    clip.WrittenRoot = C;

    PatternSettings pattern{};
    pattern.Mode = PatternMode::Drums;
    clip.Pattern = pattern;
    VERIFY_IS_FALSE(ClipFollowsKey(clip));

    clip.Pattern->Mode = PatternMode::Melodic;
    VERIFY_IS_TRUE(ClipFollowsKey(clip));

    Clip generator{};
    generator.Kind = ClipKind::Generator;
    generator.FollowKey = true;
    VERIFY_IS_FALSE(ClipFollowsKey(generator));

    VERIFY_IS_FALSE(GeneratorUsesKey(GeneratorType::Euclidean));
    VERIFY_IS_FALSE(GeneratorUsesKey(GeneratorType::LifeDrums));
    VERIFY_IS_FALSE(GeneratorUsesKey(GeneratorType::Arpeggiator));
    VERIFY_IS_TRUE(GeneratorUsesKey(GeneratorType::ChanceMelody));
    VERIFY_IS_TRUE(GeneratorUsesKey(GeneratorType::ChangeRinging));
}

void HarmonyFollowTests::GeneratorPlaysInTheSongKey()
{
    Clip clip{};
    clip.Kind = ClipKind::Generator;
    clip.Seed = 1234;
    clip.Generator = MakeChanceMelodyGenerator();
    clip.Generator->Busy = 90;
    clip.Length = GeneratorStepCount(clip) * clip.Generator->StepTicks;

    // E minor pentatonic has B, which F major doesn't.
    auto sequence = OneClip(clip, clip.Length * 4);
    HarmonyTrack harmony{};
    harmony.Keys = { Key(F, ScaleKind::Major) };
    sequence.Harmony = harmony;

    auto const playsB = [](std::vector<std::pair<int64_t, uint8_t>> const& notes)
    {
        return std::any_of(notes.begin(), notes.end(), [](auto const& n) { return n.second % 12 == B; });
    };

    auto notes = NotesOn(sequence, clip.Length * 4);
    VERIFY_IS_FALSE(notes.empty());
    VERIFY_IS_TRUE(playsB(notes));

    sequence.Clips.front().Generator->SongKey = true;
    notes = NotesOn(sequence, clip.Length * 4);
    VERIFY_IS_FALSE(notes.empty());

    for (auto const& [tick, number] : notes)
    {
        VERIFY_IS_TRUE(InScale(number, F, ScaleKind::Major));
    }

    // Without a key on the row, its own key.
    sequence.Harmony->Keys.clear();
    VERIFY_IS_TRUE(playsB(NotesOn(sequence, clip.Length * 4)));
}

void HarmonyFollowTests::ChordTonesOnTheBeat()
{
    Clip clip{};
    clip.Kind = ClipKind::Generator;
    clip.Seed = 99;
    clip.Generator = MakeChanceMelodyGenerator();
    clip.Generator->Busy = 100;
    clip.Generator->ChordTones = true;
    clip.Length = GeneratorStepCount(clip) * clip.Generator->StepTicks;

    auto sequence = OneClip(clip, clip.Length * 2);
    HarmonyTrack harmony{};
    harmony.Keys = { Key(E, ScaleKind::NaturalMinor) };
    harmony.Chords = { Chord(C, ChordType::Major, 0, clip.Length * 2) };
    sequence.Harmony = harmony;

    auto const notes = NotesOn(sequence, clip.Length * 2);
    size_t onBeat{ 0 };

    for (auto const& [tick, number] : notes)
    {
        if (tick % Beat == 0)
        {
            ++onBeat;
            auto const pc = number % 12;
            VERIFY_IS_TRUE(pc == C || pc == E || pc == G);
        }
    }

    VERIFY_IS_TRUE(onBeat > 0);
}

void HarmonyFollowTests::ArpeggiatorPlaysTheChords()
{
    Clip clip{};
    clip.Id = L"arp";
    clip.Kind = ClipKind::Generator;
    clip.Generator = MakeArpeggiatorGenerator();
    clip.Generator->ArpSource = ArpeggiatorSource::Chords;
    clip.Length = Bar * 2;

    Sequence sequence{};
    HarmonyTrack harmony{};
    harmony.Keys = { Key(A, ScaleKind::NaturalMinor) };
    harmony.Chords = { Chord(A, ChordType::Minor, 0, Bar), Chord(F, ChordType::Major, Bar, Bar) };
    sequence.Harmony = harmony;

    std::vector<Note> played{};
    ForEachArpeggiatorNote(sequence, clip, 0, Bar * 2, 0, 0, Bar * 2, nullptr, [&played](Note const& note) { played.push_back(note); });

    VERIFY_IS_FALSE(played.empty());

    for (auto const& note : played)
    {
        // Over the arpeggiator's octaves: the chord's notes, an octave or more up.
        auto const expected = ChordNotes(note.Tick < Bar ? harmony.Chords[0] : harmony.Chords[1]);
        VERIFY_IS_TRUE(std::any_of(expected.begin(), expected.end(), [&note](uint8_t n) { return n % 12 == note.Number % 12 && note.Number >= n; }));
    }

    // Each chord starts the order again: the first note of the second bar is F's lowest.
    auto const second = std::find_if(played.begin(), played.end(), [](Note const& note) { return note.Tick >= Bar; });
    VERIFY_IS_TRUE(second != played.end());
    VERIFY_ARE_EQUAL(ChordNotes(harmony.Chords[1]).front(), second->Number);

    // No chords, nothing to play.
    sequence.Harmony->Chords.clear();
    played.clear();
    ForEachArpeggiatorNote(sequence, clip, 0, Bar * 2, 0, 0, Bar * 2, nullptr, [&played](Note const& note) { played.push_back(note); });
    VERIFY_IS_TRUE(played.empty());
}

void HarmonyFollowTests::ArpeggiatorHearsTheKeyMove()
{
    // A chord clip written in C major that follows the key, under D major in its second bar.
    Clip chords{};
    chords.Kind = ClipKind::Notes;
    chords.Length = Bar;
    chords.Loop = true;
    chords.FollowKey = true;
    chords.WrittenRoot = C;
    chords.WrittenScale = ScaleKind::Major;

    for (auto const number : { 60, 64, 67 })
    {
        auto note = MakeNote(0, static_cast<uint8_t>(number));
        note.Length = Bar;
        chords.Notes.push_back(note);
    }

    auto sequence = OneClip(chords, Bar * 2);
    HarmonyTrack harmony{};
    harmony.Keys = { Key(C, ScaleKind::Major), Key(D, ScaleKind::Major, Bar) };
    sequence.Harmony = harmony;

    // What an arpeggiator following the track hears is what the track plays: D, F♯ and A there.
    auto const first = NotesSoundingAt(sequence, sequence.Tracks.front(), Beat);
    auto const second = NotesSoundingAt(sequence, sequence.Tracks.front(), Bar + Beat);

    VERIFY_ARE_EQUAL(size_t{ 3 }, first.Notes.size());
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(64), first.Notes[1].Number);
    VERIFY_ARE_EQUAL(size_t{ 3 }, second.Notes.size());
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(62), second.Notes[0].Number);
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(66), second.Notes[1].Number);
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(69), second.Notes[2].Number);
}

void HarmonyFollowTests::ChordsBecomeNotes()
{
    // Am for a bar from bar 3, then E7/G♯ for two.
    auto am = Chord(A, ChordType::Minor, Bar * 2, Bar);
    auto e7 = Chord(E, ChordType::Seventh, Bar * 3, Bar * 2);
    e7.Bass = static_cast<uint8_t>(8);

    auto const clip = ChordsAsNotes({ am, e7 }, 30000);
    VERIFY_IS_TRUE(clip.Kind == ClipKind::Notes);
    VERIFY_IS_FALSE(clip.Loop);
    VERIFY_ARE_EQUAL(Bar * 3, clip.Length);

    auto const amNotes = ChordNotes(am);
    auto const e7Notes = ChordNotes(e7);
    VERIFY_ARE_EQUAL(amNotes.size() + e7Notes.size(), clip.Notes.size());

    // From the first chord's start, each held for its chord's length, sorted.
    VERIFY_ARE_EQUAL(int64_t{ 0 }, clip.Notes.front().Tick);
    VERIFY_ARE_EQUAL(Bar, clip.Notes.front().Length);
    VERIFY_ARE_EQUAL(amNotes.front(), clip.Notes.front().Number);
    VERIFY_ARE_EQUAL(Bar, clip.Notes.back().Tick);
    VERIFY_ARE_EQUAL(Bar * 2, clip.Notes.back().Length);
    VERIFY_ARE_EQUAL(static_cast<uint16_t>(30000), clip.Notes.back().Velocity);
    VERIFY_IS_TRUE(std::is_sorted(clip.Notes.begin(), clip.Notes.end(), [](Note const& a, Note const& b) { return a.Tick != b.Tick ? a.Tick < b.Tick : a.Number < b.Number; }));

    // The G♯ bass is there.
    VERIFY_IS_TRUE(std::any_of(clip.Notes.begin(), clip.Notes.end(), [](Note const& note) { return note.Tick == Bar && note.Number % 12 == 8; }));

    // Nothing to copy: an empty clip a bar long.
    auto const empty = ChordsAsNotes({}, 30000);
    VERIFY_IS_TRUE(empty.Notes.empty());
    VERIFY_ARE_EQUAL(Bar, empty.Length);
}

void HarmonyFollowTests::KeySignatures()
{
    auto const check = [](KeyChange const& key, int8_t accidentals, bool minor)
    {
        auto const signature = KeySignatureOf(key);
        VERIFY_ARE_EQUAL(accidentals, signature.Accidentals);
        VERIFY_ARE_EQUAL(minor, signature.Minor);
    };

    check(Key(C, ScaleKind::Major), 0, false);
    check(Key(G, ScaleKind::Major), 1, false);
    check(Key(F, ScaleKind::Major), -1, false);
    check(Key(B, ScaleKind::Major), 5, false);
    check(Key(6, ScaleKind::Major), -6, false);       // G♭, as the row spells it
    check(Key(A, ScaleKind::NaturalMinor), 0, true);
    check(Key(D, ScaleKind::NaturalMinor), -1, true);
    check(Key(E, ScaleKind::HarmonicMinor), 1, true);

    // A mode goes as the key signature it shares.
    check(Key(D, ScaleKind::Dorian), 0, true);
    check(Key(G, ScaleKind::Mixolydian), 0, false);

    // Read back: major keys and natural minor ones.
    VERIFY_IS_TRUE((KeyFromSignature(KeySignature{ 2, false }, 10) == KeyChange{ 10, D, ScaleKind::Major }));
    VERIFY_IS_TRUE((KeyFromSignature(KeySignature{ -3, true }, 0) == KeyChange{ 0, 0, ScaleKind::NaturalMinor }));
    VERIFY_IS_TRUE((KeyFromSignature(KeySignature{ 7, false }, 0) == KeyChange{ 0, 1, ScaleKind::Major }));
    VERIFY_IS_TRUE((KeyFromSignature(KeySignature{ -7, true }, 0).Root == 8));

    // Every major and natural minor key comes back as it went.
    for (uint8_t root = 0; root < 12; ++root)
    {
        for (auto const scale : { ScaleKind::Major, ScaleKind::NaturalMinor })
        {
            auto const key = Key(root, scale, 480);
            VERIFY_IS_TRUE(KeyFromSignature(KeySignatureOf(key), 480) == key);
        }
    }
}

void HarmonyFollowTests::StandardMidiFileKeysRoundTrip()
{
    // A file needs something to play, so one note.
    Clip clip{};
    clip.Kind = ClipKind::Notes;
    clip.Length = Bar;
    clip.Notes = { MakeNote(0, 64) };

    auto sequence = OneClip(clip, Bar);
    sequence.Name = L"Keys";

    HarmonyTrack harmony{};
    harmony.Keys = { Key(E, ScaleKind::NaturalMinor), Key(G, ScaleKind::Major, Bar * 4), Key(D, ScaleKind::Dorian, Bar * 8) };
    harmony.Chords = { Chord(E, ChordType::Minor, 0, Bar) };
    sequence.Harmony = harmony;

    auto const exported = ExportStandardMidiFile(sequence, {}, ClipFileText{});
    VERIFY_IS_TRUE(exported.Succeeded);

    midifile::MidiSequence file{};
    auto const read = midifile::ParseStandardMidiFile(std::span<uint8_t const>{ exported.Bytes }, file);
    Log::Comment(String().Format(L"Read status %d at byte %llu, %u of %u tracks, %zu bytes", static_cast<int>(read.Status), read.ByteOffset, read.TracksRead, read.TracksDeclared, exported.Bytes.size()));
    VERIFY_IS_TRUE(read.Succeeded());
    VERIFY_ARE_EQUAL(size_t{ 3 }, file.KeySignatureMap.size());
    VERIFY_ARE_EQUAL(static_cast<int8_t>(1), file.KeySignatureMap[0].Accidentals);
    VERIFY_IS_TRUE(file.KeySignatureMap[0].IsMinor);

    auto const imported = ImportStandardMidiFile(file, L"keys.mid");
    VERIFY_IS_TRUE(imported.Harmony.has_value());
    VERIFY_ARE_EQUAL(size_t{ 3 }, imported.Harmony->Keys.size());
    VERIFY_IS_TRUE(imported.Harmony->Keys[0] == Key(E, ScaleKind::NaturalMinor));
    VERIFY_IS_TRUE(imported.Harmony->Keys[1] == Key(G, ScaleKind::Major, Bar * 4));

    // D Dorian shares C major's signature, written as minor: it comes back as A minor.
    VERIFY_IS_TRUE(imported.Harmony->Keys[2] == Key(A, ScaleKind::NaturalMinor, Bar * 8));

    // Chords have no standard event.
    VERIFY_IS_TRUE(imported.Harmony->Chords.empty());

    // A file without key signatures gives no row.
    sequence.Harmony.reset();
    midifile::MidiSequence plain{};
    VERIFY_IS_TRUE(midifile::ParseStandardMidiFile(std::span<uint8_t const>{ ExportStandardMidiFile(sequence, {}, ClipFileText{}).Bytes }, plain).Succeeded());
    VERIFY_IS_FALSE(ImportStandardMidiFile(plain, L"plain.mid").Harmony.has_value());
}

void HarmonyFollowTests::SettingsRoundTrip()
{
    Sequence original{};

    Clip notes{};
    notes.Id = L"notes";
    notes.Kind = ClipKind::Notes;
    notes.FollowKey = true;
    notes.WrittenRoot = 3;
    notes.WrittenScale = ScaleKind::Dorian;
    original.Clips.push_back(notes);

    Clip melody{};
    melody.Id = L"melody";
    melody.Kind = ClipKind::Generator;
    melody.Generator = MakeChanceMelodyGenerator();
    melody.Generator->SongKey = true;
    melody.Generator->ChordTones = true;
    original.Clips.push_back(melody);

    Clip arp{};
    arp.Id = L"arp";
    arp.Kind = ClipKind::Generator;
    arp.Generator = MakeArpeggiatorGenerator();
    arp.Generator->ArpSource = ArpeggiatorSource::Chords;
    original.Clips.push_back(arp);

    auto const text = WriteSequenceJson(original);
    VERIFY_IS_TRUE(text.find(L"\"followKey\": { \"root\": 3, \"scale\": \"dorian\" }") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"songKey\": true") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"chordTones\": true") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"source\": \"chords\"") != std::wstring::npos);

    Sequence read{};
    auto const result = ReadSequenceJson(text, read);
    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_ARE_EQUAL(size_t{ 0 }, result.SkippedItems);
    VERIFY_ARE_EQUAL(size_t{ 3 }, read.Clips.size());

    VERIFY_IS_TRUE(read.Clips[0].FollowKey);
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(3), read.Clips[0].WrittenRoot);
    VERIFY_IS_TRUE(read.Clips[0].WrittenScale == ScaleKind::Dorian);
    VERIFY_IS_TRUE(read.Clips[1].Generator->SongKey);
    VERIFY_IS_TRUE(read.Clips[1].Generator->ChordTones);
    VERIFY_IS_TRUE(read.Clips[2].Generator->ArpSource == ArpeggiatorSource::Chords);
    VERIFY_ARE_EQUAL(text, WriteSequenceJson(read));

    // Off by default, and not written.
    Sequence plain{};
    Clip quiet{};
    quiet.Id = L"quiet";
    plain.Clips.push_back(quiet);
    VERIFY_IS_TRUE(WriteSequenceJson(plain).find(L"followKey") == std::wstring::npos);
}
