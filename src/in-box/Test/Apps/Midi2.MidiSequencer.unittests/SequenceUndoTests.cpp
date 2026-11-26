// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SequenceModelTests.h"
#include "SequenceTestData.h"

#include "SequenceUndo.h"

#include <algorithm>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    std::vector<std::unique_ptr<SequenceChange>> One(std::unique_ptr<SequenceChange> change)
    {
        std::vector<std::unique_ptr<SequenceChange>> changes{};
        changes.push_back(std::move(change));
        return changes;
    }

    std::unique_ptr<SequenceChange> TempoChange(Sequence const& sequence, std::vector<TempoPoint> after)
    {
        return std::make_unique<SnapshotChange<std::vector<TempoPoint>>>(
            [](Sequence& target) { return &target.Tempo; },
            sequence.Tempo,
            std::move(after),
            sizeof(TempoPoint) * 4);
    }
}

void SequenceUndoTests::NoteEditsUndoAndRedo()
{
    auto sequence = testdata::SampleSequence();
    auto const before = FindClip(sequence, L"c-bass-a")->Notes;

    // Move the second note later and add one.
    auto moved = before[1];
    moved.Tick += 240;
    auto const added = testdata::MakeNote(3000, 100, 47);

    UndoStack history{};
    history.ApplyAndCommit(sequence, L"Move notes", One(MakeNoteChange(L"c-bass-a", { before[1] }, { moved, added })));

    auto const& notes = FindClip(sequence, L"c-bass-a")->Notes;
    VERIFY_ARE_EQUAL(before.size() + 1, notes.size());
    VERIFY_IS_TRUE(std::find(notes.begin(), notes.end(), moved) != notes.end());
    VERIFY_IS_TRUE(std::find(notes.begin(), notes.end(), before[1]) == notes.end());
    VERIFY_IS_TRUE(std::is_sorted(notes.begin(), notes.end(), [](Note const& a, Note const& b) { return a.Tick < b.Tick; }));

    VERIFY_ARE_EQUAL(std::wstring{ L"Move notes" }, history.UndoName());
    VERIFY_IS_TRUE(history.Undo(sequence));
    VERIFY_IS_TRUE(before == FindClip(sequence, L"c-bass-a")->Notes);

    VERIFY_IS_TRUE(history.CanRedo());
    VERIFY_IS_TRUE(history.Redo(sequence));
    VERIFY_ARE_EQUAL(before.size() + 1, FindClip(sequence, L"c-bass-a")->Notes.size());

    VERIFY_IS_FALSE(history.Redo(sequence));
}

void SequenceUndoTests::ASnapshotUndoesAndRedoes()
{
    auto sequence = testdata::SampleSequence();
    auto const before = sequence.Tempo;

    std::vector<TempoPoint> after{ TempoPoint{ 0, 90.0, false } };

    UndoStack history{};
    history.ApplyAndCommit(sequence, L"Change tempo", One(TempoChange(sequence, after)));
    VERIFY_IS_TRUE(after == sequence.Tempo);

    history.Undo(sequence);
    VERIFY_IS_TRUE(before == sequence.Tempo);

    history.Redo(sequence);
    VERIFY_IS_TRUE(after == sequence.Tempo);
}

void SequenceUndoTests::ANewEditForgetsTheRedo()
{
    auto sequence = testdata::SampleSequence();

    UndoStack history{};
    history.ApplyAndCommit(sequence, L"First", One(TempoChange(sequence, { TempoPoint{ 0, 100.0, false } })));
    history.Undo(sequence);
    VERIFY_IS_TRUE(history.CanRedo());

    history.ApplyAndCommit(sequence, L"Second", One(TempoChange(sequence, { TempoPoint{ 0, 110.0, false } })));
    VERIFY_IS_FALSE(history.CanRedo());
    VERIFY_ARE_EQUAL(std::wstring{ L"Second" }, history.UndoName());
    VERIFY_ARE_EQUAL(size_t{ 1 }, history.UndoCount());

    // A step with nothing in it isn't a step.
    history.Commit(L"Nothing", {});
    VERIFY_ARE_EQUAL(size_t{ 1 }, history.UndoCount());
}

void SequenceUndoTests::TheHistoryStaysUnderItsCeiling()
{
    auto sequence = testdata::SampleSequence();

    std::vector<Note> many{};

    for (int i = 0; i < 100; ++i)
    {
        many.push_back(testdata::MakeNote(i * 10, 5, 60));
    }

    // Each step holds about a hundred notes, more than the whole ceiling.
    UndoStack history{ 1000 };

    for (int step = 0; step < 5; ++step)
    {
        history.ApplyAndCommit(sequence, L"Add notes", One(MakeNoteChange(L"c-bass-a", {}, many)));
    }

    // The newest step is always kept, so the last edit can still be undone.
    VERIFY_ARE_EQUAL(size_t{ 1 }, history.UndoCount());
    VERIFY_IS_TRUE(history.Undo(sequence));
    VERIFY_IS_FALSE(history.CanUndo());

    history.Clear();
    VERIFY_ARE_EQUAL(size_t{ 0 }, history.TotalCost());
}
