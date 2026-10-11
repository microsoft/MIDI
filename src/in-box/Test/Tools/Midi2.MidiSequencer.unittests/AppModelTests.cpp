// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "AppModelTests.h"
#include "SequenceTestData.h"

#include "ArrangeLayout.h"
#include "RecordingTake.h"
#include "SampleSequence.h"
#include "SequenceEdits.h"
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

    Sequence FolderSequence()
    {
        Sequence sequence{};

        Track drums{};
        drums.Id = L"t-drums";
        drums.Name = L"Drums";

        Track bass{};
        bass.Id = L"t-bass";
        bass.Name = L"Bass";

        Track pad{};
        pad.Id = L"t-pad";
        pad.Name = L"Pad";

        Track synths{};
        synths.Id = L"f-synths";
        synths.Name = L"Synths";
        synths.IsFolder = true;
        synths.Open = true;
        synths.Children = { bass, pad };

        sequence.Tracks = { drums, synths };
        return sequence;
    }

    std::vector<std::wstring> TopLevelIds(Sequence const& sequence)
    {
        std::vector<std::wstring> ids{};

        for (auto const& track : sequence.Tracks)
        {
            ids.push_back(track.Id);
        }

        return ids;
    }

    uint32_t Midi1(uint8_t status, uint8_t channel, uint8_t data1, uint8_t data2)
    {
        return 0x20000000u | (static_cast<uint32_t>(status) << 20) | (static_cast<uint32_t>(channel) << 16) |
            (static_cast<uint32_t>(data1) << 8) | data2;
    }
}

void AppModelTests::ANewTrackAfterAnOpenFolderGoesInside()
{
    auto sequence = FolderSequence();

    auto track = MakeTrack(sequence, L"Lead", false);
    auto const id = track.Id;
    InsertTrackAfter(sequence, L"f-synths", std::move(track));

    VERIFY_ARE_EQUAL(size_t{ 2 }, sequence.Tracks.size());
    VERIFY_ARE_EQUAL(size_t{ 3 }, sequence.Tracks[1].Children.size());
    VERIFY_ARE_EQUAL(id, sequence.Tracks[1].Children[2].Id);

    // After an ordinary track it goes next to it.
    auto second = MakeTrack(sequence, L"Keys", false);
    auto const secondId = second.Id;
    InsertTrackAfter(sequence, L"t-drums", std::move(second));
    VERIFY_ARE_EQUAL(secondId, sequence.Tracks[1].Id);

    // A closed folder keeps it outside.
    sequence.Tracks[2].Open = false;
    auto third = MakeTrack(sequence, L"Arp", false);
    auto const thirdId = third.Id;
    InsertTrackAfter(sequence, L"f-synths", std::move(third));
    VERIFY_ARE_EQUAL(thirdId, sequence.Tracks[3].Id);
}

void AppModelTests::AFolderCantMoveIntoItself()
{
    auto sequence = FolderSequence();

    Track inner{};
    inner.Id = L"f-inner";
    inner.IsFolder = true;
    sequence.Tracks[1].Children.push_back(inner);

    VERIFY_IS_FALSE(MoveTrackToFolder(sequence, L"f-synths", L"f-synths"));
    VERIFY_IS_FALSE(MoveTrackToFolder(sequence, L"f-synths", L"f-inner"));
    VERIFY_ARE_EQUAL(size_t{ 2 }, sequence.Tracks.size());

    VERIFY_IS_TRUE(MoveTrackToFolder(sequence, L"t-drums", L"f-inner"));
    VERIFY_ARE_EQUAL(size_t{ 1 }, sequence.Tracks.size());
    VERIFY_IS_TRUE(IsInside(sequence, L"t-drums", L"f-synths"));

    VERIFY_IS_TRUE(MoveTrackToFolder(sequence, L"t-drums", L""));
    VERIFY_ARE_EQUAL(L"t-drums", sequence.Tracks.back().Id);
}

void AppModelTests::MovingTracksUpAndDownStaysInTheirList()
{
    auto sequence = FolderSequence();

    VERIFY_IS_TRUE(MoveTrackBy(sequence, L"t-pad", -1));
    VERIFY_ARE_EQUAL(L"t-pad", sequence.Tracks[1].Children[0].Id);

    // Already first in the folder: it doesn't jump out.
    VERIFY_IS_FALSE(MoveTrackBy(sequence, L"t-pad", -1));
    VERIFY_IS_FALSE(MoveTrackBy(sequence, L"t-unknown", 1));

    VERIFY_IS_TRUE(MoveTrackBy(sequence, L"t-drums", 1));
    VERIFY_IS_TRUE((TopLevelIds(sequence) == std::vector<std::wstring>{ L"f-synths", L"t-drums" }));
}

void AppModelTests::UnusedClipsAreRemovedAndUsedOnesKept()
{
    auto sequence = FolderSequence();

    Clip used = MakeNotesClip(L"Used", Bar);
    Clip slotted = MakeNotesClip(L"In a slot", Bar);
    Clip orphan = MakeNotesClip(L"Orphan", Bar);

    sequence.Tracks[0].Timeline.push_back(Placement{ used.Id, 0, 0 });
    sequence.Tracks[1].Children[0].Slots = { slotted.Id };
    sequence.Clips = { used, slotted, orphan };

    auto const removed = RemoveUnusedClips(sequence);

    VERIFY_ARE_EQUAL(size_t{ 1 }, removed.size());
    VERIFY_ARE_EQUAL(orphan.Id, removed[0].Id);
    VERIFY_ARE_EQUAL(size_t{ 2 }, sequence.Clips.size());
}

void AppModelTests::UndoPutsTheTrackTreeBack()
{
    auto sequence = FolderSequence();
    UndoStack undo{};

    auto const before = sequence.Tracks;
    RemoveTrack(sequence, L"t-bass");
    InsertTrackAfter(sequence, L"", MakeTrack(sequence, L"New", false));

    ChangeList changes{};
    changes.push_back(MakeTracksChange(before, sequence.Tracks));
    undo.Commit(L"Edit tracks", std::move(changes));

    VERIFY_IS_NULL(FindTrack(sequence, L"t-bass"));
    VERIFY_IS_TRUE(undo.Undo(sequence));
    VERIFY_IS_NOT_NULL(FindTrack(sequence, L"t-bass"));
    VERIFY_ARE_EQUAL(size_t{ 2 }, sequence.Tracks.size());

    VERIFY_IS_TRUE(undo.Redo(sequence));
    VERIFY_IS_NULL(FindTrack(sequence, L"t-bass"));
    VERIFY_ARE_EQUAL(size_t{ 3 }, sequence.Tracks.size());
}

void AppModelTests::UndoPutsARemovedClipBack()
{
    auto sequence = FolderSequence();
    auto clip = MakeNotesClip(L"Hook", Bar);
    clip.Notes = { testdata::MakeNote(0, 480, 60), testdata::MakeNote(480, 480, 62) };
    sequence.Clips = { clip };

    UndoStack undo{};
    ChangeList changes{};
    changes.push_back(MakeClipPresenceChange(clip, false));
    undo.ApplyAndCommit(sequence, L"Delete clip", std::move(changes));

    VERIFY_IS_TRUE(sequence.Clips.empty());
    VERIFY_IS_TRUE(undo.Undo(sequence));
    VERIFY_ARE_EQUAL(size_t{ 1 }, sequence.Clips.size());
    VERIFY_ARE_EQUAL(size_t{ 2 }, sequence.Clips[0].Notes.size());
}

void AppModelTests::ClipSettingsUndoLeavesTheNotes()
{
    auto sequence = FolderSequence();
    auto clip = MakeNotesClip(L"Hook", Bar);
    clip.Notes = { testdata::MakeNote(0, 480, 60) };
    sequence.Clips = { clip };

    auto after = clip;
    after.Name = L"Hook 2";
    after.Length = Bar * 2;
    after.Loop = false;

    UndoStack undo{};
    ChangeList changes{};
    changes.push_back(MakeClipSettingsChange(clip, after));
    undo.ApplyAndCommit(sequence, L"Clip settings", std::move(changes));

    VERIFY_ARE_EQUAL(L"Hook 2", sequence.Clips[0].Name);
    VERIFY_ARE_EQUAL(Bar * 2, sequence.Clips[0].Length);

    // A note added after the settings change survives undoing it.
    sequence.Clips[0].Notes.push_back(testdata::MakeNote(960, 480, 64));

    VERIFY_IS_TRUE(undo.Undo(sequence));
    VERIFY_ARE_EQUAL(L"Hook", sequence.Clips[0].Name);
    VERIFY_ARE_EQUAL(Bar, sequence.Clips[0].Length);
    VERIFY_IS_TRUE(sequence.Clips[0].Loop);
    VERIFY_ARE_EQUAL(size_t{ 2 }, sequence.Clips[0].Notes.size());
}

void AppModelTests::SnapGoesToTheNearestLine()
{
    VERIFY_ARE_EQUAL(int64_t{ 0 }, SnapTick(100, 240));
    VERIFY_ARE_EQUAL(int64_t{ 240 }, SnapTick(120, 240));
    VERIFY_ARE_EQUAL(int64_t{ 240 }, SnapTick(359, 240));
    VERIFY_ARE_EQUAL(int64_t{ 480 }, SnapTick(361, 240));
    VERIFY_ARE_EQUAL(int64_t{ 1234 }, SnapTick(1234, 0));
    VERIFY_ARE_EQUAL(int64_t{ -240 }, SnapTick(-200, 240));
}

void AppModelTests::ANewTrackGetsAFreeChannel()
{
    auto sequence = FolderSequence();
    EndpointRef synth{ L"General MIDI Synth", L"" };

    sequence.Tracks[0].Destination.Endpoint = synth;
    sequence.Tracks[0].Destination.Channel = 0;
    sequence.Tracks[1].Children[0].Destination.Endpoint = synth;
    sequence.Tracks[1].Children[0].Destination.Channel = 1;

    VERIFY_ARE_EQUAL(int8_t{ 2 }, NextFreeChannel(sequence, synth, 0));

    // Another group of the same device is free.
    VERIFY_ARE_EQUAL(int8_t{ 0 }, NextFreeChannel(sequence, synth, 1));

    // Channel 10 is skipped: General MIDI keeps it for drums.
    for (int8_t channel = 2; channel < 9; ++channel)
    {
        auto track = MakeTrack(sequence, L"More", false);
        track.Destination.Endpoint = synth;
        track.Destination.Channel = channel;
        sequence.Tracks.push_back(track);
    }

    VERIFY_ARE_EQUAL(int8_t{ 10 }, NextFreeChannel(sequence, synth, 0));

    VERIFY_ARE_EQUAL(std::wstring{ L"Track 1" }, NextName(sequence, L"Track"));
}

void AppModelTests::RecordedMidi1NotesBecomeMidi2Notes()
{
    std::vector<MeterChange> meter{ MeterChange{} };

    RecordingTake take{ 0 };

    auto const on = Midi1(0x9, 1, 60, 100);
    auto const off = Midi1(0x8, 1, 60, 64);
    take.Add(100, &on, 1);
    take.Add(580, &off, 1);

    auto const clip = take.Finish(600, meter, L"c-take", L"Take 1", L"Keystep 37");

    VERIFY_ARE_EQUAL(size_t{ 1 }, clip.Notes.size());
    VERIFY_ARE_EQUAL(int64_t{ 100 }, clip.Notes[0].Tick);
    VERIFY_ARE_EQUAL(int64_t{ 480 }, clip.Notes[0].Length);
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, clip.Notes[0].Channel);
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, clip.Notes[0].Number);
    VERIFY_ARE_EQUAL(static_cast<uint16_t>(ScaleUp(100, 7, 16)), clip.Notes[0].Velocity);
    VERIFY_ARE_EQUAL(static_cast<uint16_t>(ScaleUp(64, 7, 16)), clip.Notes[0].ReleaseVelocity);

    // Scaling down what was scaled up gives back what was played.
    VERIFY_ARE_EQUAL(uint32_t{ 100 }, ScaleDown(clip.Notes[0].Velocity, 16, 7));

    VERIFY_IS_TRUE(clip.Origin == ClipOrigin::Recorded);
    VERIFY_ARE_EQUAL(L"Keystep 37", clip.OriginDetail);
    VERIFY_IS_FALSE(clip.Loop);
}

void AppModelTests::AHeldNoteEndsWhereTheTakeStops()
{
    std::vector<MeterChange> meter{ MeterChange{} };

    RecordingTake take{ 0 };
    auto const on = Midi1(0x9, 0, 64, 90);
    take.Add(960, &on, 1);

    VERIFY_ARE_EQUAL(size_t{ 1 }, take.NotesSoFar(1440).size());
    VERIFY_ARE_EQUAL(int64_t{ 480 }, take.NotesSoFar(1440)[0].Length);

    auto const clip = take.Finish(2000, meter, L"c", L"Take", L"");
    VERIFY_ARE_EQUAL(int64_t{ 1040 }, clip.Notes[0].Length);
    VERIFY_ARE_EQUAL(Bar, clip.Length);
}

void AppModelTests::ATakeStartsAndEndsOnBars()
{
    std::vector<MeterChange> meter{ MeterChange{} };

    // Started a little into bar 2, played into bar 3.
    RecordingTake take{ Bar + 160 };
    auto const on = Midi1(0x9, 0, 60, 90);
    auto const off = Midi1(0x8, 0, 60, 0);
    take.Add(Bar + 200, &on, 1);
    take.Add(Bar * 2 + 100, &off, 1);

    auto const clip = take.Finish(Bar * 2 + 300, meter, L"c", L"Take", L"");

    VERIFY_ARE_EQUAL(Bar, take.PlacementTick());
    VERIFY_ARE_EQUAL(int64_t{ 200 }, clip.Notes[0].Tick);
    VERIFY_ARE_EQUAL(Bar * 2, clip.Length);
}

void AppModelTests::TheRecordFilterLeavesThingsOut()
{
    RecordFilter filter{};
    filter.Channels = 1u << 0;
    filter.Controllers = false;

    auto const noteChannel1 = Midi1(0x9, 0, 60, 90);
    auto const noteChannel2 = Midi1(0x9, 1, 60, 90);
    auto const controller = Midi1(0xB, 0, 1, 64);
    auto const bend = Midi1(0xE, 0, 0, 64);
    uint32_t const clock = 0x10F80000;
    uint32_t const sysex[2]{ 0x30016000, 0 };

    VERIFY_IS_TRUE(PassesRecordFilter(filter, &noteChannel1, 1));
    VERIFY_IS_FALSE(PassesRecordFilter(filter, &noteChannel2, 1));
    VERIFY_IS_FALSE(PassesRecordFilter(filter, &controller, 1));
    VERIFY_IS_TRUE(PassesRecordFilter(filter, &bend, 1));
    VERIFY_IS_FALSE(PassesRecordFilter(filter, &clock, 1));

    // System exclusive is off unless asked for.
    VERIFY_IS_FALSE(PassesRecordFilter(filter, sysex, 2));
    filter.SystemExclusive = true;
    VERIFY_IS_TRUE(PassesRecordFilter(filter, sysex, 2));

    filter.Group = 3;
    VERIFY_IS_FALSE(PassesRecordFilter(filter, &noteChannel1, 1));

    // A take only keeps what passes.
    RecordingTake take{ 0, RecordFilter{ -1, 1u << 0, true, false } };
    take.Add(0, &controller, 1);
    take.Add(0, &noteChannel2, 1);
    VERIFY_IS_TRUE(take.IsEmpty());
}

void AppModelTests::ARepeatedNoteOnRestartsTheNote()
{
    std::vector<MeterChange> meter{ MeterChange{} };

    RecordingTake take{ 0 };
    auto const on = Midi1(0x9, 0, 60, 90);
    auto const offByVelocity = Midi1(0x9, 0, 60, 0);

    take.Add(0, &on, 1);
    take.Add(240, &on, 1);
    take.Add(480, &offByVelocity, 1);

    auto const clip = take.Finish(500, meter, L"c", L"Take", L"");

    // Velocity 0 is a note off in MIDI 1.0, and a second note on ends the first note.
    VERIFY_ARE_EQUAL(size_t{ 2 }, clip.Notes.size());
    VERIFY_ARE_EQUAL(int64_t{ 240 }, clip.Notes[0].Length);
    VERIFY_ARE_EQUAL(int64_t{ 240 }, clip.Notes[1].Length);

    // The UMP spec turns that note on into a note off at the middle release velocity.
    VERIFY_ARE_EQUAL(uint16_t{ 0x8000 }, clip.Notes[1].ReleaseVelocity);
}

void AppModelTests::ASlotTakeLoopsAtItsExactLength()
{
    // Recorded into a launcher slot from bar 3, for two bars.
    RecordingTake take{ Bar * 2 };
    auto const on = Midi1(0x9, 0, 60, 90);
    auto const off = Midi1(0x8, 0, 60, 0);
    auto const held = Midi1(0x9, 0, 67, 90);
    auto const controller = Midi1(0xB, 0, 1, 64);

    // Played a hair early, which still counts as the first beat.
    take.Add(Bar * 2 - 30, &on, 1);
    take.Add(Bar * 2 + 100, &controller, 1);
    take.Add(Bar * 2 + 480, &off, 1);
    take.Add(Bar * 3 + 960, &held, 1);

    // Past the end: left out.
    take.Add(Bar * 4 + 10, &controller, 1);

    auto const soFar = take.LoopSoFar(Bar * 2, Bar * 3 + 1440);
    VERIFY_ARE_EQUAL(size_t{ 2 }, soFar.Notes.size());
    VERIFY_ARE_EQUAL(int64_t{ 480 }, soFar.Notes[1].Length);

    auto const clip = take.FinishLoop(Bar * 2, L"c", L"Take", L"Keystep 37");

    VERIFY_IS_TRUE(clip.Loop);
    VERIFY_ARE_EQUAL(Bar * 2, clip.Length);
    VERIFY_ARE_EQUAL(size_t{ 2 }, clip.Notes.size());
    VERIFY_ARE_EQUAL(int64_t{ 0 }, clip.Notes[0].Tick);
    VERIFY_ARE_EQUAL(int64_t{ 480 }, clip.Notes[0].Length);

    // Still held at the end, so it ends there.
    VERIFY_ARE_EQUAL(Bar + 960, clip.Notes[1].Tick);
    VERIFY_ARE_EQUAL(Bar - 960, clip.Notes[1].Length);

    VERIFY_ARE_EQUAL(size_t{ 1 }, clip.Events.size());
    VERIFY_ARE_EQUAL(take.StartTick(), take.PlacementTick());
    VERIFY_ARE_EQUAL(std::wstring{ L"Keystep 37" }, clip.OriginDetail);
}

void AppModelTests::PinnedRowsGoToTheTop()
{
    auto sequence = FolderSequence();
    sequence.Tracks[1].Children[1].Pinned = true;

    auto const layout = BuildArrangeLayout(sequence);

    // Tempo and meter first, then the pinned Pad with its folder's name.
    VERIFY_ARE_EQUAL(size_t{ 2 }, layout.Pinned.size());
    VERIFY_IS_TRUE(layout.Pinned[0].Kind == ArrangeRowKind::Tempo);
    VERIFY_ARE_EQUAL(L"t-pad", layout.Pinned[1].TrackId);
    VERIFY_ARE_EQUAL(L"Synths", layout.Pinned[1].FolderName);
    VERIFY_ARE_EQUAL(TempoRowHeight + TrackRowHeight, layout.PinnedHeight);

    // Drums, Synths, Bass, then the row to add a track. Pad isn't drawn twice.
    VERIFY_ARE_EQUAL(size_t{ 4 }, layout.Scrolling.size());
    VERIFY_ARE_EQUAL(L"t-bass", layout.Scrolling[2].TrackId);
    VERIFY_ARE_EQUAL(size_t{ 1 }, layout.Scrolling[2].Depth);
    VERIFY_IS_TRUE(layout.Scrolling[3].Kind == ArrangeRowKind::AddTrack);
}

void AppModelTests::AClosedFolderHidesItsTracks()
{
    auto sequence = FolderSequence();
    sequence.Tracks[1].Open = false;
    sequence.Tracks[1].Muted = true;

    auto layout = BuildArrangeLayout(sequence);
    VERIFY_ARE_EQUAL(size_t{ 3 }, layout.Scrolling.size());
    VERIFY_IS_NULL(layout.FindRow(L"t-bass"));

    sequence.Tracks[1].Open = true;
    layout = BuildArrangeLayout(sequence);

    auto const bass = layout.FindRow(L"t-bass");
    VERIFY_IS_NOT_NULL(bass);
    VERIFY_IS_TRUE(bass->InsideMutedFolder);
}

void AppModelTests::RowsAreFoundByPosition()
{
    auto const layout = BuildArrangeLayout(FolderSequence());

    VERIFY_IS_NULL(layout.ScrollingRowAt(-1));
    VERIFY_ARE_EQUAL(L"t-drums", layout.ScrollingRowAt(0)->TrackId);
    VERIFY_ARE_EQUAL(L"t-drums", layout.ScrollingRowAt(TrackRowHeight - 0.5)->TrackId);
    VERIFY_ARE_EQUAL(L"f-synths", layout.ScrollingRowAt(TrackRowHeight)->TrackId);
    VERIFY_IS_NULL(layout.ScrollingRowAt(layout.ScrollingHeight + 1));
    VERIFY_IS_TRUE(layout.PinnedRowAt(1)->Kind == ArrangeRowKind::Tempo);
}

void AppModelTests::TheSampleIsWholeAndSurvivesSaving()
{
    SampleText text{};
    text.SequenceName = L"Sample";
    text.SceneIntro = L"Intro";
    text.SceneGroove = L"Groove";
    text.SceneBreak = L"Break";
    text.SceneDrop = L"Drop";
    text.SceneOutro = L"Outro";
    text.Drums = L"Drums";
    text.Synths = L"Synths";
    text.Keys = L"Keys";
    text.BeatA = L"Beat A";
    text.BassLineA = L"Bass line A";

    EndpointRef const synth{ L"General MIDI Synth", L"synth-id" };
    auto const sample = MakeSampleSequence(text, synth);

    // Five scenes, and a slot for each on every track.
    VERIFY_ARE_EQUAL(size_t{ 5 }, sample.Scenes.size());
    VERIFY_ARE_EQUAL(size_t{ 5 }, sample.Tags.size());
    VERIFY_ARE_EQUAL(size_t{ 3 }, sample.Tempo.size());

    size_t tracks{ 0 };
    std::set<uint8_t> channels{};

    ForEachTrack(sample, [&](Track const& track, size_t)
    {
        if (track.IsFolder)
        {
            return true;
        }

        ++tracks;
        VERIFY_ARE_EQUAL(sample.Scenes.size(), track.Slots.size());
        VERIFY_IS_TRUE(track.Destination.Endpoint == synth);

        // Each part on its own channel, so they don't play each other's sounds.
        VERIFY_IS_TRUE(channels.insert(static_cast<uint8_t>(track.Destination.Channel)).second);

        // Nothing names a clip that isn't there.
        for (auto const& placement : track.Timeline)
        {
            VERIFY_IS_NOT_NULL(FindClip(sample, placement.ClipId));
        }

        for (auto const& slot : track.Slots)
        {
            VERIFY_IS_TRUE(slot.empty() || FindClip(sample, slot) != nullptr);
        }

        return true;
    });

    VERIFY_ARE_EQUAL(size_t{ 9 }, tracks);

    // Every clip is used, and every clip has notes in it.
    for (auto const& clip : sample.Clips)
    {
        VERIFY_IS_TRUE(CountClipUses(sample, clip.Id) > 0);
        VERIFY_IS_FALSE(clip.Notes.empty());
    }

    // The same sequence comes back from its file.
    Sequence read{};
    VERIFY_IS_TRUE(ReadSequenceJson(WriteSequenceJson(sample), read).Succeeded());
    VERIFY_ARE_EQUAL(sample.Clips.size(), read.Clips.size());
    VERIFY_ARE_EQUAL(WriteSequenceJson(sample), WriteSequenceJson(read));
}
