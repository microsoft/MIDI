// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SequenceMergeTests.h"
#include "SequenceTestData.h"

#include "SequenceEdits.h"
#include "SequenceMerge.h"

#include <algorithm>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    constexpr int64_t Bar = TicksPerQuarterNote * 4;

    Clip NotesClip(std::wstring id, int64_t length, bool loop, std::vector<Note> notes)
    {
        Clip clip{};
        clip.Id = std::move(id);
        clip.Name = clip.Id;
        clip.Length = length;
        clip.Loop = loop;
        clip.Notes = std::move(notes);
        return clip;
    }

    Track MakeTestTrack(std::wstring id, uint32_t color, int8_t channel)
    {
        Track track{};
        track.Id = std::move(id);
        track.Name = track.Id;
        track.Color = color;
        track.Destination.Endpoint = EndpointRef{ L"Synth", L"" };
        track.Destination.Channel = channel;
        return track;
    }

    std::function<std::wstring()> Ids()
    {
        auto next = std::make_shared<int>(0);
        return [next]() { return L"new-" + std::to_wstring(++*next); };
    }

    std::wstring Joined(std::wstring const& a, std::wstring const& b)
    {
        return a + L" + " + b;
    }

    Note const* NoteAt(Clip const& clip, int64_t tick, uint8_t number)
    {
        auto const found = std::find_if(clip.Notes.begin(), clip.Notes.end(), [&](Note const& n) { return n.Tick == tick && n.Number == number; });
        return found == clip.Notes.end() ? nullptr : &*found;
    }
}

void SequenceMergeTests::OverlappingSameNotesBecomeOneLongerNote()
{
    auto const merged = MergeOverlappingNotes({
        testdata::MakeNote(0, 480, 60, 0x8000),
        testdata::MakeNote(240, 480, 60, 0xF000),
        testdata::MakeNote(720, 240, 60),       // starts as the others end: a note of its own
        testdata::MakeNote(240, 480, 64),       // another note
        testdata::MakeNote(100, 50, 60, 0x8000, 1),     // another channel
    });

    VERIFY_ARE_EQUAL(size_t{ 4 }, merged.size());

    auto const first = std::find_if(merged.begin(), merged.end(), [](Note const& n) { return n.Number == 60 && n.Channel == 0 && n.Tick == 0; });
    VERIFY_IS_TRUE(first != merged.end());
    VERIFY_ARE_EQUAL(int64_t{ 720 }, first->Length);
    VERIFY_ARE_EQUAL(uint16_t{ 0x8000 }, first->Velocity);

    // Sorted as a clip keeps them.
    VERIFY_IS_TRUE(std::is_sorted(merged.begin(), merged.end(), [](Note const& a, Note const& b) { return a.Tick < b.Tick; }));
}

void SequenceMergeTests::MergedClipsKeepTheirSpacing()
{
    Sequence sequence{};
    auto track = MakeTestTrack(L"t", 0xFF8A65, 0);
    sequence.Clips = {
        NotesClip(L"a", Bar, false, { testdata::MakeNote(0, 480, 60), testdata::MakeNote(960, 480, 62) }),
        NotesClip(L"b", Bar, false, { testdata::MakeNote(480, 240, 67) }),
        NotesClip(L"c", Bar, false, { testdata::MakeNote(0, 240, 72) }),
    };
    track.Timeline = { Placement{ L"a", Bar, 0 }, Placement{ L"c", Bar * 2, 0 }, Placement{ L"b", Bar * 4, 0 } };
    sequence.Tracks = { track };

    auto const merged = MergePlacements(sequence, L"t", { 2, 0 }, L"m", L"Merged");
    VERIFY_IS_TRUE(merged.has_value());

    auto const& clip = merged->Made;
    VERIFY_ARE_EQUAL(Bar * 4, clip.Length);
    VERIFY_IS_FALSE(clip.Loop);
    VERIFY_ARE_EQUAL(size_t{ 3 }, clip.Notes.size());
    VERIFY_IS_NOT_NULL(NoteAt(clip, 0, 60));
    VERIFY_IS_NOT_NULL(NoteAt(clip, 960, 62));
    VERIFY_IS_NOT_NULL(NoteAt(clip, Bar * 3 + 480, 67));

    // The clip in between stays where it was, and the merged one starts at the first's place.
    auto const& timeline = sequence.Tracks[0].Timeline;
    VERIFY_ARE_EQUAL(size_t{ 2 }, timeline.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"m" }, timeline[0].ClipId);
    VERIFY_ARE_EQUAL(Bar, timeline[0].Tick);
    VERIFY_ARE_EQUAL(std::wstring{ L"c" }, timeline[1].ClipId);
    VERIFY_ARE_EQUAL(size_t{ 0 }, merged->Placement);
    VERIFY_IS_NOT_NULL(FindClip(sequence, L"m"));

    // One placement alone isn't a merge.
    VERIFY_IS_FALSE(MergePlacements(sequence, L"t", { 0 }, L"x", L"X").has_value());
}

void SequenceMergeTests::MergingALoopingClipCopiesEachPass()
{
    Sequence sequence{};
    auto track = MakeTestTrack(L"t", 0xFF8A65, 0);
    sequence.Clips = {
        NotesClip(L"loop", Bar / 2, true, { testdata::MakeNote(0, Bar, 60) }),     // runs past its loop point
        NotesClip(L"one", Bar, false, { testdata::MakeNote(0, 240, 72) }),
    };
    track.Timeline = { Placement{ L"loop", 0, Bar * 2 }, Placement{ L"one", Bar * 2, 0 } };
    sequence.Tracks = { track };

    auto const merged = MergePlacements(sequence, L"t", { 0, 1 }, L"m", L"Merged");
    VERIFY_IS_TRUE(merged.has_value());

    // Four passes of the loop, each note ending where its pass does, then the other clip.
    auto const& clip = merged->Made;
    VERIFY_ARE_EQUAL(size_t{ 5 }, clip.Notes.size());

    for (int64_t pass = 0; pass < 4; ++pass)
    {
        auto const note = NoteAt(clip, pass * (Bar / 2), 60);
        VERIFY_IS_NOT_NULL(note);
        VERIFY_ARE_EQUAL(Bar / 2, note->Length);
    }

    VERIFY_IS_NOT_NULL(NoteAt(clip, Bar * 2, 72));
}

void SequenceMergeTests::SplittingANotesClipCutsItInTwo()
{
    Sequence sequence{};
    auto track = MakeTestTrack(L"t", 0xFF8A65, 0);
    sequence.Clips = { NotesClip(L"a", Bar, false, { testdata::MakeNote(0, 480, 60), testdata::MakeNote(1920, 960, 64), testdata::MakeNote(2880, 480, 67) }) };
    track.Timeline = { Placement{ L"a", Bar, 0 } };
    sequence.Tracks = { track };

    auto const added = SplitPlacement(sequence, L"t", 0, Bar + 2400, Ids(), L"Right");
    VERIFY_ARE_EQUAL(size_t{ 2 }, added.size());

    auto const& timeline = sequence.Tracks[0].Timeline;
    VERIFY_ARE_EQUAL(size_t{ 2 }, timeline.size());
    VERIFY_ARE_EQUAL(Bar, timeline[0].Tick);
    VERIFY_ARE_EQUAL(Bar + 2400, timeline[1].Tick);

    // The left part keeps what's before the cut; the note across it ends there.
    auto const left = FindClip(sequence, timeline[0].ClipId);
    VERIFY_IS_NOT_NULL(left);
    VERIFY_ARE_EQUAL(int64_t{ 2400 }, left->Length);
    VERIFY_ARE_EQUAL(size_t{ 2 }, left->Notes.size());
    VERIFY_ARE_EQUAL(int64_t{ 480 }, NoteAt(*left, 1920, 64)->Length);

    // The right part has the rest of that note, and what came after.
    auto const right = FindClip(sequence, timeline[1].ClipId);
    VERIFY_IS_NOT_NULL(right);
    VERIFY_ARE_EQUAL(std::wstring{ L"Right" }, right->Name);
    VERIFY_ARE_EQUAL(Bar - 2400, right->Length);
    VERIFY_ARE_EQUAL(size_t{ 2 }, right->Notes.size());
    VERIFY_ARE_EQUAL(int64_t{ 480 }, NoteAt(*right, 0, 64)->Length);
    VERIFY_IS_NOT_NULL(NoteAt(*right, 480, 67));

    // The original clip is no longer used, so it can go.
    auto const removed = RemoveUnusedClips(sequence);
    VERIFY_ARE_EQUAL(size_t{ 1 }, removed.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"a" }, removed[0].Id);
}

void SequenceMergeTests::SplittingAGeneratorKeepsItPlayingOnTheLeft()
{
    Sequence sequence{};
    auto track = MakeTestTrack(L"t", 0xFF8A65, 0);
    auto generator = MakeGeneratorClip(L"Euclid", GeneratorType::Euclidean, Bar);
    generator.Id = L"g";
    sequence.Clips = { generator };
    track.Timeline = { Placement{ L"g", 0, Bar * 2 } };
    sequence.Tracks = { track };
    NormalizeSequence(sequence);

    auto const added = SplitPlacement(sequence, L"t", 0, Bar, Ids(), L"Right");
    VERIFY_ARE_EQUAL(size_t{ 1 }, added.size());

    auto const& timeline = sequence.Tracks[0].Timeline;
    VERIFY_ARE_EQUAL(std::wstring{ L"g" }, timeline[0].ClipId);
    VERIFY_ARE_EQUAL(Bar, timeline[0].Length);

    // The right part is the notes the generator played in its second bar.
    auto const right = FindClip(sequence, timeline[1].ClipId);
    VERIFY_IS_NOT_NULL(right);
    VERIFY_IS_TRUE(right->Kind == ClipKind::Notes);
    VERIFY_IS_FALSE(right->Notes.empty());
}

void SequenceMergeTests::SplittingOutsideAPlacementDoesNothing()
{
    Sequence sequence{};
    auto track = MakeTestTrack(L"t", 0xFF8A65, 0);
    sequence.Clips = { NotesClip(L"a", Bar, false, { testdata::MakeNote(0, 480, 60) }) };
    track.Timeline = { Placement{ L"a", Bar, 0 } };
    sequence.Tracks = { track };

    VERIFY_IS_TRUE(SplitPlacement(sequence, L"t", 0, Bar, Ids(), L"R").empty());
    VERIFY_IS_TRUE(SplitPlacement(sequence, L"t", 0, Bar * 2, Ids(), L"R").empty());
    VERIFY_IS_TRUE(SplitPlacement(sequence, L"t", 3, Bar + 10, Ids(), L"R").empty());
    VERIFY_ARE_EQUAL(size_t{ 1 }, sequence.Tracks[0].Timeline.size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, sequence.Clips.size());
}

void SequenceMergeTests::MergingATrackKeepsTheDestinationsSettings()
{
    Sequence sequence{};
    auto into = MakeTestTrack(L"into", 0x5BC0EB, 3);
    auto from = MakeTestTrack(L"from", 0xFF8A65, 9);
    from.Lanes = { MakeLane(LaneTarget::Controller, 1, 0), MakeLane(LaneTarget::Controller, 7, 0) };
    into.Lanes = { MakeLane(LaneTarget::Controller, 7, 0) };
    from.Tags = { Tag{ 0, L"Theirs" } };
    sequence.Clips = { NotesClip(L"a", Bar, false, { testdata::MakeNote(0, 480, 60) }), NotesClip(L"b", Bar, false, { testdata::MakeNote(0, 480, 36) }) };
    into.Timeline = { Placement{ L"a", 0, 0 } };
    from.Timeline = { Placement{ L"b", Bar * 2, 0 } };
    sequence.Tracks = { into, from };

    auto const result = MergeTrackInto(sequence, L"from", L"into", Ids(), Joined);
    VERIFY_IS_TRUE(result.Merged);
    VERIFY_ARE_EQUAL(size_t{ 1 }, result.LanesSkipped);

    // The source is gone; the destination keeps where it plays, its channel and its color.
    VERIFY_ARE_EQUAL(size_t{ 1 }, sequence.Tracks.size());
    auto const& track = sequence.Tracks[0];
    VERIFY_ARE_EQUAL(std::wstring{ L"into" }, track.Id);
    VERIFY_ARE_EQUAL(uint32_t{ 0x5BC0EB }, track.Color);
    VERIFY_ARE_EQUAL(int8_t{ 3 }, track.Destination.Channel);

    // Placements that don't overlap move across still linked to their clips.
    VERIFY_ARE_EQUAL(size_t{ 2 }, track.Timeline.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"b" }, track.Timeline[1].ClipId);
    VERIFY_IS_TRUE(result.Added.empty());

    VERIFY_ARE_EQUAL(size_t{ 2 }, track.Lanes.size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, track.Tags.size());

    // A track can't merge into itself or a folder.
    auto folder = MakeTestTrack(L"f", 0, 0);
    folder.IsFolder = true;
    sequence.Tracks.push_back(folder);
    VERIFY_IS_FALSE(MergeTrackInto(sequence, L"into", L"into", Ids(), Joined).Merged);
    VERIFY_IS_FALSE(MergeTrackInto(sequence, L"into", L"f", Ids(), Joined).Merged);
}

void SequenceMergeTests::MergingATrackMergesWhereBothPlay()
{
    Sequence sequence{};
    auto into = MakeTestTrack(L"into", 0x5BC0EB, 3);
    auto from = MakeTestTrack(L"from", 0xFF8A65, 9);
    sequence.Clips = {
        NotesClip(L"a", Bar, false, { testdata::MakeNote(0, 960, 60), testdata::MakeNote(1920, 480, 64) }),
        NotesClip(L"b", Bar, false, { testdata::MakeNote(480, 960, 60), testdata::MakeNote(0, 480, 36) }),
    };
    into.Timeline = { Placement{ L"a", 0, 0 } };
    from.Timeline = { Placement{ L"b", Bar / 2, 0 } };
    sequence.Tracks = { into, from };

    auto const result = MergeTrackInto(sequence, L"from", L"into", Ids(), Joined);
    VERIFY_IS_TRUE(result.Merged);
    VERIFY_ARE_EQUAL(size_t{ 1 }, result.Added.size());

    auto const& track = sequence.Tracks[0];
    VERIFY_ARE_EQUAL(size_t{ 1 }, track.Timeline.size());
    VERIFY_ARE_EQUAL(int64_t{ 0 }, track.Timeline[0].Tick);

    auto const clip = FindClip(sequence, track.Timeline[0].ClipId);
    VERIFY_IS_NOT_NULL(clip);
    VERIFY_ARE_EQUAL(std::wstring{ L"a + b" }, clip->Name);
    VERIFY_ARE_EQUAL(Bar + Bar / 2, clip->Length);

    // Their 60 at 2400 starts after ours ends at 960, so it's a note of its own, and their 36
    // comes along.
    VERIFY_IS_NOT_NULL(NoteAt(*clip, 0, 60));
    VERIFY_ARE_EQUAL(int64_t{ 960 }, NoteAt(*clip, 0, 60)->Length);
    VERIFY_IS_NOT_NULL(NoteAt(*clip, 2400, 60));
    VERIFY_IS_NOT_NULL(NoteAt(*clip, 1920, 36));
    VERIFY_IS_NOT_NULL(NoteAt(*clip, 1920, 64));

    // Where the same note overlaps, it's one longer note.
    Sequence overlap{};
    auto x = MakeTestTrack(L"x", 0, 0);
    auto y = MakeTestTrack(L"y", 0, 0);
    overlap.Clips = { NotesClip(L"p", Bar, false, { testdata::MakeNote(0, 960, 60) }), NotesClip(L"q", Bar, false, { testdata::MakeNote(0, 960, 60) }) };
    x.Timeline = { Placement{ L"p", 0, 0 } };
    y.Timeline = { Placement{ L"q", 480, 0 } };
    overlap.Tracks = { x, y };

    VERIFY_IS_TRUE(MergeTrackInto(overlap, L"y", L"x", Ids(), Joined).Merged);
    auto const joined = FindClip(overlap, overlap.Tracks[0].Timeline[0].ClipId);
    VERIFY_ARE_EQUAL(size_t{ 1 }, joined->Notes.size());
    VERIFY_ARE_EQUAL(int64_t{ 1440 }, joined->Notes[0].Length);
}

void SequenceMergeTests::MergingATrackFillsAndMergesSlots()
{
    Sequence sequence{};
    auto into = MakeTestTrack(L"into", 0x5BC0EB, 3);
    auto from = MakeTestTrack(L"from", 0xFF8A65, 9);
    sequence.Clips = {
        NotesClip(L"a", Bar, true, { testdata::MakeNote(0, 240, 60) }),
        NotesClip(L"b", Bar * 2, true, { testdata::MakeNote(0, 240, 36) }),
        NotesClip(L"c", Bar, true, { testdata::MakeNote(0, 240, 72) }),
    };
    into.Slots = { L"a", L"", L"" };
    from.Slots = { L"b", L"c", L"", L"a" };
    sequence.Tracks = { into, from };

    auto const result = MergeTrackInto(sequence, L"from", L"into", Ids(), Joined);
    VERIFY_IS_TRUE(result.Merged);

    auto const& slots = sequence.Tracks[0].Slots;
    VERIFY_ARE_EQUAL(size_t{ 4 }, slots.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"c" }, slots[1]);
    VERIFY_IS_TRUE(slots[2].empty());
    VERIFY_ARE_EQUAL(std::wstring{ L"a" }, slots[3]);

    // Both had a clip: one looping clip as long as the longer, with the shorter one going round.
    auto const merged = FindClip(sequence, slots[0]);
    VERIFY_IS_NOT_NULL(merged);
    VERIFY_IS_TRUE(merged->Loop);
    VERIFY_ARE_EQUAL(Bar * 2, merged->Length);
    VERIFY_IS_NOT_NULL(NoteAt(*merged, 0, 60));
    VERIFY_IS_NOT_NULL(NoteAt(*merged, Bar, 60));
    VERIFY_IS_NOT_NULL(NoteAt(*merged, 0, 36));
}

void SequenceMergeTests::GhostNotesComeFromUnmutedTracksOnly()
{
    Sequence sequence{};
    sequence.Clips = {
        NotesClip(L"mine", Bar, false, { testdata::MakeNote(0, 480, 60) }),
        NotesClip(L"bass", Bar, false, { testdata::MakeNote(0, 480, 36), testdata::MakeNote(1440, 960, 38) }),
        NotesClip(L"quiet", Bar, false, { testdata::MakeNote(0, 480, 80) }),
    };

    auto mine = MakeTestTrack(L"mine", 0x111111, 0);
    mine.Timeline = { Placement{ L"mine", Bar, 0 } };

    // Starts half a bar before the edited clip; its second note runs into it.
    auto bass = MakeTestTrack(L"bass", 0x00FF00, 1);
    bass.Timeline = { Placement{ L"bass", Bar / 2, 0 } };

    auto quiet = MakeTestTrack(L"quiet", 0x0000FF, 2);
    quiet.Muted = true;
    quiet.Timeline = { Placement{ L"quiet", Bar, 0 } };

    Track folder{};
    folder.Id = L"folder";
    folder.IsFolder = true;
    folder.Children = { bass };

    sequence.Tracks = { mine, folder, quiet };

    auto ghosts = NotesOtherTracksPlay(sequence, L"mine", Bar, Bar * 2);
    VERIFY_ARE_EQUAL(size_t{ 1 }, ghosts.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"bass" }, ghosts[0].TrackId);
    VERIFY_ARE_EQUAL(uint32_t{ 0x00FF00 }, ghosts[0].Color);
    VERIFY_ARE_EQUAL(size_t{ 1 }, ghosts[0].Notes.size());
    // It started before the clip, so it's cut to begin where the clip does.
    VERIFY_ARE_EQUAL(int64_t{ 0 }, ghosts[0].Notes[0].Tick);
    VERIFY_ARE_EQUAL(int64_t{ Bar / 2 + 1440 + 960 - Bar }, ghosts[0].Notes[0].Length);
    VERIFY_ARE_EQUAL(uint8_t{ 38 }, ghosts[0].Notes[0].Number);

    // A solo elsewhere doesn't hide anything: every unmuted track still shows.
    sequence.Tracks[2].Muted = false;
    sequence.Tracks[2].Soloed = true;
    ghosts = NotesOtherTracksPlay(sequence, L"mine", Bar, Bar * 2);
    VERIFY_ARE_EQUAL(size_t{ 2 }, ghosts.size());

    // A muted folder hides what's in it.
    sequence.Tracks[2].Soloed = false;
    sequence.Tracks[1].Muted = true;
    ghosts = NotesOtherTracksPlay(sequence, L"mine", Bar, Bar * 2);
    VERIFY_ARE_EQUAL(size_t{ 1 }, ghosts.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"quiet" }, ghosts[0].TrackId);
}