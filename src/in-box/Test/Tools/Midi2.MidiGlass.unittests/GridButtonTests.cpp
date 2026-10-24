// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Pad grids in rows and columns, and the grid buttons, pickers and readouts that change and show
// them while a layout runs: the arithmetic, which grids a change reaches, what the file keeps,
// and what the editor builds.

#include "GridButtonTests.h"

#include "PadGrid.h"
#include "PadGridChanges.h"
#include "ActionPlan.h"
#include "BindingEngine.h"
#include "ControlFactory.h"
#include "EditorController.h"
#include "LayoutModel.h"
#include "LayoutSerializer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    glass::LayoutDocument DocumentWith(_In_ std::vector<glass::Control> controls)
    {
        glass::LayoutDocument document{};

        document.Name = L"Grids";
        document.PageWidth = 1280;
        document.PageHeight = 800;
        document.CanvasWidth = 1280;
        document.CanvasHeight = 800;

        glass::DeviceEntry device{};
        device.Name = L"Synth";

        document.Devices.push_back(device);

        glass::Page page{};
        page.Id = L"page";
        page.Name = L"Page 1";
        page.Controls = std::move(controls);

        document.Pages.push_back(std::move(page));

        return document;
    }

    std::vector<glass::PreparedDestination> OneDevice()
    {
        glass::PreparedDestination destination{};

        destination.Name = L"Synth";
        destination.IsAvailable = true;

        return { destination };
    }

    glass::PadGridSpec Fixed(_In_ glass::PadArrangement arrangement, _In_ int32_t rows, _In_ int32_t perRow)
    {
        glass::PadGridSpec spec{};

        spec.Arrangement = arrangement;
        spec.Rows = rows;
        spec.PadsPerRow = perRow;
        spec.StartNote = 48;
        spec.RightInterval = 1;
        spec.RowInterval = 5;

        return spec;
    }

    glass::Control Grid(_In_ std::wstring const& id, _In_ int32_t root)
    {
        glass::Control control{};

        control.Id = id;
        control.Label = id;
        control.Kind = glass::ControlKind::NotePads;
        control.X = 100;
        control.Y = 100;
        control.Width = 400;
        control.Height = 300;
        control.Pads = Fixed(glass::PadArrangement::Rows, 8, 8);
        control.Pads.KeyRoot = root;

        return control;
    }

    glass::Control Picker(_In_ std::wstring const& id, _In_ glass::SwitchPicks picks, _In_ glass::PadGridTargets const& targets)
    {
        glass::Control control{};

        control.Id = id;
        control.Kind = glass::ControlKind::Switch;
        control.Width = 376;
        control.Height = 44;
        control.Switch.Picks = picks;
        control.Switch.Targets = targets;

        if (picks == glass::SwitchPicks::Scale)
        {
            control.Switch.Scales = glass::DefaultPickerScales();
        }

        return control;
    }

    glass::PadGridTargets Only(_In_ std::wstring const& id)
    {
        return glass::PadGridTargets{ false, { id } };
    }

    glass::PadGridState Change(
        _In_ glass::PadGridSpec const& built,
        _In_ glass::PadGridState const& state,
        _In_ glass::PadGridFunction function)
    {
        return glass::ChangedState(built, false, state, function);
    }

    int32_t FirstNote(_In_ glass::PadGridSpec const& built, _In_ glass::PadGridState const& state)
    {
        auto const spec = glass::WithGridState(built, false, state);

        return glass::PadNote(spec, false, 0, 0, spec.PadsPerRow);
    }
}

// ------------------------------------------------------------------ rows and columns

void GridButtonTests::RowsLayOutASetGridFromTheBottomLeft()
{
    auto const layout = glass::LayOutPadGrid(Fixed(glass::PadArrangement::Rows, 8, 8), false, 400, 400);

    VERIFY_ARE_EQUAL(size_t{ 64 }, layout.Cells.size());
    VERIFY_ARE_EQUAL(8, layout.Rows);
    VERIFY_ARE_EQUAL(8, layout.Columns);
    VERIFY_IS_FALSE(layout.Vertical);

    auto const& first = layout.Cells[0];
    auto const& next = layout.Cells[1];
    auto const& above = layout.Cells[8];

    VERIFY_ARE_EQUAL(48, first.Note);
    VERIFY_ARE_EQUAL(49, next.Note);
    VERIFY_ARE_EQUAL(53, above.Note);

    // Bottom left, the next pad to its right, the next row above it.
    VERIFY_IS_TRUE(first.CenterX < 200 && first.CenterY > 200);
    VERIFY_IS_TRUE(next.CenterX > first.CenterX);
    VERIFY_ARE_EQUAL(first.CenterY, next.CenterY);
    VERIFY_IS_TRUE(above.CenterY < first.CenterY);
    VERIFY_ARE_EQUAL(first.CenterX, above.CenterX);

    VERIFY_ARE_EQUAL(0, glass::PadAtPoint(layout, first.CenterX, first.CenterY));
    VERIFY_ARE_EQUAL(63, glass::PadAtPoint(layout, layout.Cells[63].CenterX, layout.Cells[63].CenterY));
}

void GridButtonTests::ColumnsTurnTheGridOnItsSide()
{
    auto spec = Fixed(glass::PadArrangement::Columns, 4, 6);
    spec.Glide = glass::PadGlide::PerNoteBend;

    auto const layout = glass::LayOutPadGrid(spec, false, 400, 300);

    VERIFY_ARE_EQUAL(size_t{ 24 }, layout.Cells.size());
    VERIFY_IS_TRUE(layout.Vertical);

    auto const& first = layout.Cells[0];
    auto const& up = layout.Cells[1];
    auto const& right = layout.Cells[6];

    // Pads run up a column, and columns stack to the right.
    VERIFY_ARE_EQUAL(first.CenterX, up.CenterX);
    VERIFY_IS_TRUE(up.CenterY < first.CenterY);
    VERIFY_IS_TRUE(right.CenterX > first.CenterX);
    VERIFY_ARE_EQUAL(first.CenterY, right.CenterY);

    VERIFY_ARE_EQUAL(48, first.Note);
    VERIFY_ARE_EQUAL(49, up.Note);
    VERIFY_ARE_EQUAL(53, right.Note);
    VERIFY_ARE_EQUAL(1, up.Column);
    VERIFY_ARE_EQUAL(1, right.Row);

    // A bend runs up the column, toward the next pad up.
    VERIFY_ARE_EQUAL(48.0, glass::PitchAtPoint(layout, 0, first.CenterX, first.CenterY));

    auto const halfway = glass::PitchAtPoint(layout, 0, first.CenterX, first.CenterY - layout.Pitch * 0.5);

    VERIFY_IS_TRUE(std::abs(halfway - 48.5) < 0.001);

    // Sideways is the next column, not a bend.
    VERIFY_ARE_EQUAL(48.0, glass::PitchAtPoint(layout, 0, first.CenterX + layout.Pitch * 0.4, first.CenterY));
}

void GridButtonTests::PadsStretchToFillTheControl()
{
    auto const wide = glass::LayOutPadGrid(Fixed(glass::PadArrangement::Rows, 2, 4), false, 400, 100);

    VERIFY_IS_TRUE(wide.PadWidth > wide.PadHeight * 1.5);

    // The whole control is used: a gap at either end, and nothing else.
    auto const& last = wide.Cells[7];

    VERIFY_IS_TRUE(std::abs((last.CenterX + wide.PadWidth * 0.5 + wide.Gap) - 400.0) < 0.01);
    VERIFY_IS_TRUE(std::abs((last.CenterY - wide.PadHeight * 0.5 - wide.Gap)) < 0.01);

    // Out toward the side of a wide pad is still that pad.
    auto const& first = wide.Cells[0];

    VERIFY_ARE_EQUAL(0, glass::PadAtPoint(wide, first.CenterX + wide.PadWidth * 0.45, first.CenterY));
    VERIFY_ARE_EQUAL(1, glass::PadAtPoint(wide, first.CenterX + wide.PadWidth * 0.55 + wide.Gap, first.CenterY));

    // Resizing changes the pads, never the grid.
    auto const tall = glass::LayOutPadGrid(Fixed(glass::PadArrangement::Rows, 2, 4), false, 100, 400);

    VERIFY_ARE_EQUAL(4, tall.Columns);
    VERIFY_ARE_EQUAL(2, tall.Rows);
    VERIFY_IS_TRUE(tall.PadHeight > tall.PadWidth);
}

void GridButtonTests::AGridNeverHasMoreThan1024Pads()
{
    VERIFY_ARE_EQUAL(16, glass::RowsAllowed(32, 64));
    VERIFY_ARE_EQUAL(32, glass::RowsAllowed(40, 32));
    VERIFY_ARE_EQUAL(1, glass::RowsAllowed(0, 8));

    auto const spec = Fixed(glass::PadArrangement::Rows, 32, 64);

    VERIFY_ARE_EQUAL(1024, glass::PadCountOf(spec));

    // A giant touch screen, every pad, and a finger moving across it.
    auto const started = std::chrono::steady_clock::now();
    auto const layout = glass::LayOutPadGrid(spec, false, 3840, 2160);
    auto const laidOut = std::chrono::steady_clock::now();

    VERIFY_ARE_EQUAL(size_t{ 1024 }, layout.Cells.size());

    int32_t found{ 0 };

    for (int32_t step = 0; step < 1000; ++step)
    {
        found += glass::PadAtPoint(layout, 3840.0 * step / 1000.0, 2160.0 * step / 1000.0) >= 0 ? 1 : 0;
    }

    auto const hitTested = std::chrono::steady_clock::now();

    auto const layoutMs = std::chrono::duration<double, std::milli>(laidOut - started).count();
    auto const hitMs = std::chrono::duration<double, std::milli>(hitTested - laidOut).count();

    Log::Comment(String().Format(
        L"1,024 pads: laid out in %.3f ms, 1,000 hit tests in %.3f ms (%.4f ms each).",
        layoutMs, hitMs, hitMs / 1000.0));

    VERIFY_IS_TRUE(found > 900);

    // Generous, so a busy build machine does not fail it. A finger moving needs well under 1 ms.
    VERIFY_IS_TRUE(hitMs / 1000.0 < 1.0);
}

void GridButtonTests::HexagonsInColumnsLieFlatSideUp()
{
    glass::PadGridSpec spec{};

    spec.Arrangement = glass::PadArrangement::Columns;
    spec.Rows = 3;
    spec.PadsPerRow = 5;

    auto const layout = glass::LayOutPadGrid(spec, true, 300, 300);

    VERIFY_IS_TRUE(layout.Vertical);
    VERIFY_IS_TRUE(layout.Hex);
    VERIFY_IS_TRUE(layout.PadWidth > layout.PadHeight);

    auto const& first = layout.Cells[0];
    auto const& up = layout.Cells[1];
    auto const& nextColumn = layout.Cells[5];

    VERIFY_IS_TRUE(std::abs(first.CenterX - up.CenterX) < 0.001);
    VERIFY_IS_TRUE(nextColumn.CenterX > first.CenterX);

    // Every second column sits half a pad up.
    VERIFY_IS_TRUE(std::abs((first.CenterY - nextColumn.CenterY) - layout.Pitch * 0.5) < 0.001);

    VERIFY_ARE_EQUAL(0, glass::PadAtPoint(layout, first.CenterX, first.CenterY));
}

void GridButtonTests::RowsSurviveARoundTripAndFlowStaysOutOfTheFile()
{
    auto grid = Grid(L"grid", 0);
    grid.Pads.Arrangement = glass::PadArrangement::Columns;
    grid.Pads.Rows = 5;
    grid.Pads.PadsPerRow = 12;
    grid.Pads.HighlightHeld = true;

    auto const text = glass::WriteLayoutToJson(DocumentWith({ grid }));

    VERIFY_IS_TRUE(text.find(L"\"arrangement\": \"columns\"") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"padsPerRow\": 12") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"highlightHeld\": true") != std::wstring::npos);

    auto const reread = glass::ReadLayoutFromJson(text);
    VERIFY_IS_TRUE(reread.Succeeded);

    auto const* back = reread.Document.FindControl(L"grid");
    VERIFY_IS_NOT_NULL(back);

    VERIFY_IS_TRUE(back->Pads.Arrangement == glass::PadArrangement::Columns);
    VERIFY_ARE_EQUAL(5, back->Pads.Rows);
    VERIFY_ARE_EQUAL(12, back->Pads.PadsPerRow);
    VERIFY_IS_TRUE(back->Pads.HighlightHeld);
    VERIFY_ARE_EQUAL(text, glass::WriteLayoutToJson(reread.Document));

    // A flowing grid is written exactly as it was before rows existed.
    auto flow = Grid(L"flow", 0);
    flow.Pads = glass::PadGridSpec{};

    auto const flowText = glass::WriteLayoutToJson(DocumentWith({ flow }));

    VERIFY_IS_TRUE(flowText.find(L"\"arrangement\"") == std::wstring::npos);
    VERIFY_IS_TRUE(flowText.find(L"\"padsPerRow\"") == std::wstring::npos);
    VERIFY_IS_TRUE(flowText.find(L"\"highlightHeld\"") == std::wstring::npos);

    // A stranger's file can't ask for more than the cap.
    auto const big = glass::ReadLayoutFromJson(
        LR"({ "fileVersion": 1, "name": "T", "pages": [ { "id": "p", "name": "P", "controls": [
            { "id": "a", "kind": "notePads", "pads": { "arrangement": "rows", "rows": 32, "padsPerRow": 64 } },
            { "id": "b", "kind": "notePads", "pads": { "arrangement": "rows", "rows": 900, "padsPerRow": 900 } } ] } ] })");

    VERIFY_IS_TRUE(big.Succeeded);
    VERIFY_ARE_EQUAL(16, big.Document.Pages[0].Controls[0].Pads.Rows);
    VERIFY_ARE_EQUAL(1024, glass::PadCountOf(big.Document.Pages[0].Controls[0].Pads));
    VERIFY_IS_TRUE(glass::PadCountOf(big.Document.Pages[0].Controls[1].Pads) <= glass::MaximumGridPads);
}

// ------------------------------------------------------------------ what a grid button does

void GridButtonTests::OctaveRowAndShiftButtonsMoveTheGrid()
{
    auto const built = Grid(L"grid", 0).Pads;

    VERIFY_ARE_EQUAL(48, FirstNote(built, {}));

    auto const octave = Change(built, {}, glass::PadGridFunction::OctaveUp);

    VERIFY_ARE_EQUAL(1, octave.Octaves);
    VERIFY_ARE_EQUAL(60, FirstNote(built, octave));

    // A row up moves every pad by the row offset, a fourth here.
    auto const row = Change(built, {}, glass::PadGridFunction::RowUp);

    VERIFY_ARE_EQUAL(1, row.Rows);
    VERIFY_ARE_EQUAL(53, FirstNote(built, row));

    VERIFY_ARE_EQUAL(49, FirstNote(built, Change(built, {}, glass::PadGridFunction::ShiftRight)));
    VERIFY_ARE_EQUAL(47, FirstNote(built, Change(built, {}, glass::PadGridFunction::SemitoneDown)));

    // In key, a semitone button moves one note of the scale.
    auto inKey = built;
    inKey.Notes = glass::PadNotes::InKey;

    auto const step = Change(inKey, {}, glass::PadGridFunction::SemitoneUp);

    VERIFY_ARE_EQUAL(50, FirstNote(inKey, step));
}

void GridButtonTests::AChangeThatLeavesTheNoteRangeIsNotMade()
{
    auto high = Grid(L"grid", 0).Pads;
    high.StartNote = 120;

    VERIFY_IS_TRUE(Change(high, {}, glass::PadGridFunction::OctaveUp) == glass::PadGridState{});

    auto low = Grid(L"grid", 0).Pads;
    low.StartNote = 0;

    VERIFY_IS_TRUE(Change(low, {}, glass::PadGridFunction::SemitoneDown) == glass::PadGridState{});
    VERIFY_IS_TRUE(Change(low, {}, glass::PadGridFunction::OctaveDown) == glass::PadGridState{});

    // The other way is fine.
    VERIFY_ARE_EQUAL(1, Change(low, {}, glass::PadGridFunction::OctaveUp).Octaves);
}

void GridButtonTests::InKeyOnlyNeedsAKey()
{
    auto plain = Grid(L"grid", glass::NoKey).Pads;

    VERIFY_IS_TRUE(Change(plain, {}, glass::PadGridFunction::InKeyOnlyOn) == glass::PadGridState{});

    auto const keyed = Grid(L"grid", 0).Pads;

    glass::PadGridState moved{};
    moved.Steps = 2;

    auto const on = Change(keyed, moved, glass::PadGridFunction::InKeyOnlyOn);

    VERIFY_ARE_EQUAL(1, on.InKeyOnly);

    // A step means something else once the notes change, so it starts again.
    VERIFY_ARE_EQUAL(0, on.Steps);
    VERIFY_IS_TRUE(glass::PlaysOnlyTheKey(glass::WithGridState(keyed, false, on)));

    // On again is no change; the toggle turns it off.
    VERIFY_IS_TRUE(Change(keyed, on, glass::PadGridFunction::InKeyOnlyOn) == on);
    VERIFY_ARE_EQUAL(0, Change(keyed, on, glass::PadGridFunction::InKeyOnlyToggle).InKeyOnly);
}

void GridButtonTests::NextRootAndNextScaleGoRoundAgain()
{
    auto const b = Grid(L"grid", 11).Pads;

    VERIFY_ARE_EQUAL(0, Change(b, {}, glass::PadGridFunction::NextRoot).Root);
    VERIFY_ARE_EQUAL(10, Change(b, {}, glass::PadGridFunction::PreviousRoot).Root);

    auto wholeTone = Grid(L"grid", 0).Pads;
    wholeTone.Scale = glass::MusicalScale::WholeTone;

    VERIFY_ARE_EQUAL(static_cast<int32_t>(glass::MusicalScale::Major),
        Change(wholeTone, {}, glass::PadGridFunction::NextScale).Scale);

    auto const major = Grid(L"grid", 0).Pads;

    VERIFY_ARE_EQUAL(static_cast<int32_t>(glass::MusicalScale::WholeTone),
        Change(major, {}, glass::PadGridFunction::PreviousScale).Scale);
    VERIFY_ARE_EQUAL(static_cast<int32_t>(glass::MusicalScale::Minor),
        Change(major, {}, glass::PadGridFunction::NextScale).Scale);

    // No key, nothing to move.
    VERIFY_IS_TRUE(Change(Grid(L"grid", glass::NoKey).Pads, {}, glass::PadGridFunction::NextRoot) == glass::PadGridState{});
}

void GridButtonTests::NextRowOffsetStepsThroughThePresets()
{
    auto const built = Grid(L"grid", 0).Pads;

    // A fourth goes to a fifth, an octave, then round to continuous.
    auto state = Change(built, {}, glass::PadGridFunction::NextRowOffset);

    VERIFY_ARE_EQUAL(7, glass::WithGridState(built, false, state).RowInterval);

    state = Change(built, state, glass::PadGridFunction::NextRowOffset);

    VERIFY_ARE_EQUAL(12, glass::WithGridState(built, false, state).RowInterval);

    state = Change(built, state, glass::PadGridFunction::NextRowOffset);

    VERIFY_ARE_EQUAL(0, glass::WithGridState(built, false, state).RowInterval);

    // Hexagons skip continuous.
    auto hexState = glass::ChangedState(built, true, {}, glass::PadGridFunction::NextRowOffset);
    hexState = glass::ChangedState(built, true, hexState, glass::PadGridFunction::NextRowOffset);
    hexState = glass::ChangedState(built, true, hexState, glass::PadGridFunction::NextRowOffset);

    VERIFY_ARE_EQUAL(4, glass::WithGridState(built, true, hexState).RowInterval);

    // In key the presets count notes of the scale: three steps goes to four.
    auto inKey = built;
    inKey.Notes = glass::PadNotes::InKey;

    auto const keyed = Change(inKey, {}, glass::PadGridFunction::NextRowOffset);

    VERIFY_ARE_EQUAL(4, glass::WithGridState(inKey, false, keyed).RowSteps);
}

void GridButtonTests::ResetPutsTheGridBack()
{
    auto const document = DocumentWith({ Grid(L"grid", 0) });

    glass::PadGridStates states{};

    VERIFY_IS_TRUE(glass::ApplyPadGridFunction(document, {}, glass::PadGridFunction::OctaveUp, states));
    VERIFY_IS_TRUE(glass::ApplyPadGridFunction(document, {}, glass::PadGridFunction::NextRoot, states));
    VERIFY_ARE_EQUAL(size_t{ 1 }, states.size());

    VERIFY_IS_TRUE(glass::ApplyPadGridFunction(document, {}, glass::PadGridFunction::Reset, states));
    VERIFY_IS_TRUE(states.empty());

    // Nothing to reset is no change.
    VERIFY_IS_FALSE(glass::ApplyPadGridFunction(document, {}, glass::PadGridFunction::Reset, states));
}

// ------------------------------------------------------------------ targets

void GridButtonTests::AButtonChangesOnlyTheGridsItTargets()
{
    auto const document = DocumentWith({ Grid(L"left", 0), Grid(L"right", 7) });

    glass::PadGridStates states{};

    VERIFY_IS_TRUE(glass::ApplyPadGridFunction(document, Only(L"right"), glass::PadGridFunction::OctaveDown, states));
    VERIFY_ARE_EQUAL(size_t{ 1 }, states.size());
    VERIFY_ARE_EQUAL(-1, glass::StateOf(states, L"right").Octaves);
    VERIFY_ARE_EQUAL(0, glass::StateOf(states, L"left").Octaves);

    VERIFY_IS_TRUE(glass::ApplyPadGridFunction(document, {}, glass::PadGridFunction::OctaveUp, states));
    VERIFY_ARE_EQUAL(1, glass::StateOf(states, L"left").Octaves);
    VERIFY_IS_TRUE(states.find(L"right") == states.end());

    // An empty list reaches no grid.
    VERIFY_IS_FALSE(glass::ApplyPadGridFunction(
        document, glass::PadGridTargets{ false, {} }, glass::PadGridFunction::OctaveUp, states));

    VERIFY_IS_TRUE(glass::TargetsGrid({}, L"anything"));
    VERIFY_IS_FALSE(glass::TargetsGrid(Only(L"left"), L"right"));
}

void GridButtonTests::APickerWithTargetsChangesAndShowsOnlyItsGrids()
{
    auto const rightRoot = Picker(L"rightRoot", glass::SwitchPicks::RootNote, Only(L"right"));
    auto const anyRoot = Picker(L"anyRoot", glass::SwitchPicks::RootNote, {});

    auto const document = DocumentWith({ Grid(L"left", 2), Grid(L"right", 7), rightRoot, anyRoot });

    glass::PadGridStates states{};

    // Each picker shows the grid it targets.
    VERIFY_ARE_EQUAL(7, glass::PickerPositionFor(document, rightRoot, states));
    VERIFY_ARE_EQUAL(2, glass::PickerPositionFor(document, anyRoot, states));

    VERIFY_IS_TRUE(glass::ApplyPick(document, rightRoot, 4, states));

    VERIFY_ARE_EQUAL(4, glass::StateOf(states, L"right").Root);
    VERIFY_IS_TRUE(states.find(L"left") == states.end());
    VERIFY_ARE_EQUAL(4, glass::PickerPositionFor(document, rightRoot, states));
    VERIFY_ARE_EQUAL(2, glass::PickerPositionFor(document, anyRoot, states));

    // The same pick again is no change.
    VERIFY_IS_FALSE(glass::ApplyPick(document, rightRoot, 4, states));

    VERIFY_IS_TRUE(glass::HasPickerFor(document, glass::SwitchPicks::RootNote, L"left"));
    VERIFY_IS_FALSE(glass::HasPickerFor(DocumentWith({ Grid(L"left", 2), rightRoot }), glass::SwitchPicks::RootNote, L"left"));
}

void GridButtonTests::InKeyRowOffsetAndOctavePickers()
{
    auto const inKey = Picker(L"inKey", glass::SwitchPicks::InKeyOnly, {});
    auto const rowOffset = Picker(L"rows", glass::SwitchPicks::RowOffset, {});
    auto const octave = Picker(L"octave", glass::SwitchPicks::Octave, {});

    VERIFY_ARE_EQUAL(glass::InKeyPickerPositions, glass::SwitchPositionCount(inKey));
    VERIFY_ARE_EQUAL(glass::RowOffsetPickerPositions, glass::SwitchPositionCount(rowOffset));
    VERIFY_ARE_EQUAL(glass::OctavePickerPositions, glass::SwitchPositionCount(octave));

    auto const document = DocumentWith({ Grid(L"grid", 0), inKey, rowOffset, octave });

    glass::PadGridStates states{};

    // As built: every note, a fourth, where it was built.
    VERIFY_ARE_EQUAL(0, glass::PickerPositionFor(document, inKey, states));
    VERIFY_ARE_EQUAL(2, glass::PickerPositionFor(document, rowOffset, states));
    VERIFY_ARE_EQUAL(2, glass::PickerPositionFor(document, octave, states));

    VERIFY_IS_TRUE(glass::ApplyPick(document, inKey, 1, states));
    VERIFY_IS_TRUE(glass::ApplyPick(document, rowOffset, 4, states));
    VERIFY_IS_TRUE(glass::ApplyPick(document, octave, 3, states));

    auto const state = glass::StateOf(states, L"grid");
    auto const spec = glass::WithGridState(Grid(L"grid", 0).Pads, false, state);

    VERIFY_IS_TRUE(glass::PlaysOnlyTheKey(spec));
    VERIFY_ARE_EQUAL(7, spec.RowSteps);
    VERIFY_ARE_EQUAL(1, state.Octaves);

    VERIFY_ARE_EQUAL(1, glass::PickerPositionFor(document, inKey, states));
    VERIFY_ARE_EQUAL(4, glass::PickerPositionFor(document, rowOffset, states));
    VERIFY_ARE_EQUAL(3, glass::PickerPositionFor(document, octave, states));

    // What each position prints is worked out from what it picks.
    VERIFY_ARE_EQUAL(std::wstring{ L"+1" }, glass::SignedNumberText(1));
    VERIFY_ARE_EQUAL(std::wstring{ L"\u22122" }, glass::SignedNumberText(-2));
    VERIFY_ARE_EQUAL(std::wstring{ L"0" }, glass::SignedNumberText(0));
}

void GridButtonTests::AReadoutReadsTheGridItTargets()
{
    glass::Control readout{};
    readout.Id = L"readout";
    readout.Kind = glass::ControlKind::Readout;
    readout.Readout.Source = glass::ReadoutSource::PadSummary;
    readout.Readout.Targets = Only(L"right");

    auto const document = DocumentWith({ Grid(L"left", 2), Grid(L"right", 7), readout });

    VERIFY_IS_TRUE(glass::ReadsAPadGrid(readout));

    glass::PadGridStates states{};

    auto reading = glass::ReadPadGrid(document, readout.Readout.Targets, states);

    VERIFY_IS_TRUE(reading.HasGrid);
    VERIFY_IS_TRUE(reading.HasKey);
    VERIFY_ARE_EQUAL(7, reading.Root);
    VERIFY_ARE_EQUAL(5, reading.RowOffset);

    VERIFY_IS_TRUE(glass::ApplyPadGridFunction(document, Only(L"right"), glass::PadGridFunction::OctaveUp, states));

    reading = glass::ReadPadGrid(document, readout.Readout.Targets, states);

    VERIFY_ARE_EQUAL(1, reading.Octaves);

    // A list of grids that are not there reads nothing.
    VERIFY_IS_FALSE(glass::ReadPadGrid(document, Only(L"gone"), states).HasGrid);

    auto plain = readout;
    plain.Readout.Source = glass::ReadoutSource::Value;

    VERIFY_IS_FALSE(glass::ReadsAPadGrid(plain));
}

// ------------------------------------------------------------------ sending and the file

void GridButtonTests::APadGridRowSendsNothingAndRunsAsAPlan()
{
    glass::Control button{};
    button.Id = L"button";
    button.Kind = glass::ControlKind::Button;

    glass::ControlMessage row{};
    row.Trigger = glass::MessageTrigger::TurnsOn;
    row.Kind = glass::MessageKind::PadGrid;
    row.DeviceName = L"Synth";
    row.PadFunction = glass::PadGridFunction::NextScale;
    row.PadTargets = Only(L"grid");

    button.Messages.push_back(row);

    auto const document = DocumentWith({ button, Grid(L"grid", 0) });

    VERIFY_IS_FALSE(glass::SendsToADevice(glass::MessageKind::PadGrid));

    glass::BindingEngine engine{};
    engine.Prepare(document, OneDevice());

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    VERIFY_ARE_EQUAL(0u, engine.EvaluatePress(0, glass::MessageTrigger::TurnsOn, true, 1.0, sends));

    glass::ActionPlanSet plans{};
    plans.Prepare(document, OneDevice());

    auto const* const plan = plans.Find(0, glass::MessageTrigger::TurnsOn);

    VERIFY_IS_NOT_NULL(plan);
    VERIFY_ARE_EQUAL(size_t{ 1 }, plan->Actions.size());
    VERIFY_IS_TRUE(plan->Actions[0].Kind == glass::ActionKind::ChangePadGrid);
    VERIFY_IS_TRUE(plan->Actions[0].PadFunction == glass::PadGridFunction::NextScale);
    VERIFY_IS_TRUE(plan->Actions[0].PadTargets == Only(L"grid"));

    VERIFY_IS_NULL(plans.Find(0, glass::MessageTrigger::TurnsOff));
}

void GridButtonTests::PadGridRowsSurviveARoundTrip()
{
    glass::Control button{};
    button.Id = L"button";
    button.Kind = glass::ControlKind::Button;

    glass::ControlMessage row{};
    row.Trigger = glass::MessageTrigger::TurnsOn;
    row.Kind = glass::MessageKind::PadGrid;
    row.PadFunction = glass::PadGridFunction::InKeyOnlyToggle;
    row.PadTargets = Only(L"grid");

    glass::ControlMessage everyGrid = row;
    everyGrid.PadFunction = glass::PadGridFunction::Reset;
    everyGrid.PadTargets = {};

    button.Messages = { row, everyGrid };

    auto const picker = Picker(L"picker", glass::SwitchPicks::Octave, glass::PadGridTargets{ false, {} });

    glass::Control readout{};
    readout.Id = L"readout";
    readout.Kind = glass::ControlKind::Readout;
    readout.Readout.Source = glass::ReadoutSource::PadKey;
    readout.Readout.Targets = Only(L"grid");

    auto const text = glass::WriteLayoutToJson(DocumentWith({ button, picker, readout, Grid(L"grid", 0) }));

    VERIFY_IS_TRUE(text.find(L"\"kind\": \"padGrid\"") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"function\": \"inKeyOnlyToggle\"") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"picks\": \"octave\"") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"source\": \"padKey\"") != std::wstring::npos);

    auto const reread = glass::ReadLayoutFromJson(text);
    VERIFY_IS_TRUE(reread.Succeeded);

    auto const* back = reread.Document.FindControl(L"button");
    VERIFY_IS_NOT_NULL(back);
    VERIFY_ARE_EQUAL(size_t{ 2 }, back->Messages.size());
    VERIFY_IS_TRUE(back->Messages[0].PadFunction == glass::PadGridFunction::InKeyOnlyToggle);
    VERIFY_IS_TRUE(back->Messages[0].PadTargets == Only(L"grid"));
    VERIFY_IS_TRUE(back->Messages[1].PadFunction == glass::PadGridFunction::Reset);
    VERIFY_IS_TRUE(back->Messages[1].PadTargets.AllGrids);

    // An empty list stays an empty list, not every grid.
    auto const* pickerBack = reread.Document.FindControl(L"picker");
    VERIFY_IS_FALSE(pickerBack->Switch.Targets.AllGrids);
    VERIFY_IS_TRUE(pickerBack->Switch.Targets.ControlIds.empty());

    auto const* readoutBack = reread.Document.FindControl(L"readout");
    VERIFY_IS_TRUE(readoutBack->Readout.Source == glass::ReadoutSource::PadKey);
    VERIFY_IS_TRUE(readoutBack->Readout.Targets == Only(L"grid"));

    VERIFY_ARE_EQUAL(text, glass::WriteLayoutToJson(reread.Document));

    // A change this build can't make is kept as it came and does nothing.
    auto const newer = glass::ReadLayoutFromJson(
        LR"({ "fileVersion": 1, "name": "T", "pages": [ { "id": "p", "name": "P", "controls": [
            { "id": "a", "kind": "button", "messages": [ { "trigger": "turnsOn", "kind": "padGrid", "function": "sparkle" } ] } ] } ] })");

    VERIFY_IS_TRUE(newer.Succeeded);
    VERIFY_IS_TRUE(newer.Document.Pages[0].Controls[0].Messages[0].Kind == glass::MessageKind::Unrecognized);
    VERIFY_IS_TRUE(glass::WriteLayoutToJson(newer.Document).find(L"sparkle") != std::wstring::npos);
}

// ------------------------------------------------------------------ the editor

void GridButtonTests::AddingGridButtonsPutsWiredControlsUnderTheGrid()
{
    glass::EditorController controller{};
    controller.Load(DocumentWith({}));

    auto const grid = controller.AddControl(glass::ControlKind::NotePads, 300, 200);

    VERIFY_IS_FALSE(grid.empty());

    auto const pads = *controller.Document().FindControl(grid);
    auto const before = controller.Document().Pages[0].Controls.size();

    glass::EditorController::GridButtonLabels labels{};
    labels.OctaveDown = L"Octave down";
    labels.OctaveUp = L"Octave up";
    labels.RowDown = L"Row down";
    labels.RowUp = L"Row up";
    labels.InKeyOnly = L"In key only";
    labels.Readout = L"Pad grid";
    labels.Reset = L"Reset";

    auto const added = controller.AddGridButtons(grid, {}, labels);

    VERIFY_ARE_EQUAL(size_t{ 7 }, added.size());
    VERIFY_ARE_EQUAL(before + 7, controller.Document().Pages[0].Controls.size());

    auto const* const down = controller.Document().FindControl(added[0]);
    auto const* const toggle = controller.Document().FindControl(added[4]);
    auto const* const readout = controller.Document().FindControl(added[5]);
    auto const* const reset = controller.Document().FindControl(added[6]);

    VERIFY_IS_TRUE(down->Kind == glass::ControlKind::Button);
    VERIFY_ARE_EQUAL(size_t{ 1 }, down->Messages.size());
    VERIFY_IS_TRUE(down->Messages[0].Kind == glass::MessageKind::PadGrid);
    VERIFY_IS_TRUE(down->Messages[0].PadFunction == glass::PadGridFunction::OctaveDown);
    VERIFY_IS_TRUE(down->Messages[0].PadTargets == Only(grid));
    VERIFY_ARE_EQUAL(std::wstring{ L"Octave down" }, down->Label);

    VERIFY_IS_TRUE(toggle->Switch.Picks == glass::SwitchPicks::InKeyOnly);
    VERIFY_IS_TRUE(toggle->Switch.Targets == Only(grid));
    VERIFY_IS_TRUE(toggle->Messages.empty());

    VERIFY_IS_TRUE(readout->Readout.Source == glass::ReadoutSource::PadSummary);
    VERIFY_IS_TRUE(reset->Messages[0].PadFunction == glass::PadGridFunction::Reset);

    // In a row under the grid, left to right, none on top of another.
    for (size_t index = 0; index < added.size(); ++index)
    {
        auto const* const control = controller.Document().FindControl(added[index]);

        VERIFY_IS_TRUE(control->Y >= pads.Y + pads.Height);

        if (index > 0)
        {
            auto const* const left = controller.Document().FindControl(added[index - 1]);

            VERIFY_IS_TRUE(control->X >= left->X + left->Width);
        }
    }

    // One undo takes them all away.
    VERIFY_IS_TRUE(controller.Undo());
    VERIFY_ARE_EQUAL(before, controller.Document().Pages[0].Controls.size());

    // Only what is asked for.
    glass::EditorController::GridButtonChoices only{};
    only.Octave = false;
    only.Rows = false;
    only.InKeyOnly = false;
    only.Readout = false;

    VERIFY_ARE_EQUAL(size_t{ 1 }, controller.AddGridButtons(grid, only, labels).size());

    only.Reset = false;

    VERIFY_IS_TRUE(controller.AddGridButtons(grid, only, labels).empty());
}

void GridButtonTests::PickersAddedForASecondGridTargetOnlyThatGrid()
{
    glass::EditorController controller{};
    controller.Load(DocumentWith({}));

    auto const first = controller.AddControl(glass::ControlKind::NotePads, 100, 100);
    auto const second = controller.AddControl(glass::ControlKind::NotePads, 100, 500);

    auto const firstPickers = controller.AddKeyPickers(first, L"Root note", L"Scale");

    VERIFY_ARE_EQUAL(size_t{ 2 }, firstPickers.size());
    VERIFY_IS_TRUE(controller.Document().FindControl(firstPickers[0])->Switch.Targets.AllGrids);

    // Every grid already has a picker, so there is nothing to add.
    VERIFY_IS_TRUE(controller.AddKeyPickers(second, L"Root note", L"Scale").empty());

    // Once the first pickers change only the first grid, the second can have its own.
    for (auto const& id : firstPickers)
    {
        VERIFY_IS_TRUE(controller.SetSwitchTargets(id, Only(first)));
    }

    auto const secondPickers = controller.AddKeyPickers(second, L"Root note", L"Scale");

    VERIFY_ARE_EQUAL(size_t{ 2 }, secondPickers.size());

    for (auto const& id : secondPickers)
    {
        VERIFY_IS_TRUE(controller.Document().FindControl(id)->Switch.Targets == Only(second));
    }

    // A target that isn't a pad grid is left out, which here leaves the list as it was.
    VERIFY_IS_FALSE(controller.SetSwitchTargets(secondPickers[0], glass::PadGridTargets{ false, { second, secondPickers[1] } }));
    VERIFY_IS_TRUE(controller.Document().FindControl(secondPickers[0])->Switch.Targets == Only(second));
    VERIFY_IS_TRUE(controller.SetSwitchTargets(secondPickers[0], glass::PadGridTargets{ false, { secondPickers[1] } }));
    VERIFY_IS_TRUE(controller.Document().FindControl(secondPickers[0])->Switch.Targets.ControlIds.empty());
}

void GridButtonTests::TheEditorKeepsRowsWithinTheCap()
{
    glass::EditorController controller{};
    controller.Load(DocumentWith({}));

    auto const grid = controller.AddControl(glass::ControlKind::NotePads, 100, 100);
    auto pads = controller.Document().FindControl(grid)->Pads;

    pads.Arrangement = glass::PadArrangement::Rows;
    pads.Rows = 40;
    pads.PadsPerRow = 64;
    pads.ShiftSteps = 5;

    VERIFY_IS_TRUE(controller.SetControlPads(grid, pads));

    auto const& stored = controller.Document().FindControl(grid)->Pads;

    VERIFY_IS_TRUE(stored.Arrangement == glass::PadArrangement::Rows);
    VERIFY_ARE_EQUAL(16, stored.Rows);
    VERIFY_ARE_EQUAL(64, stored.PadsPerRow);

    // Only grid buttons move a grid, and only while the layout runs.
    VERIFY_ARE_EQUAL(0, stored.ShiftSteps);

    // The same again is no edit.
    VERIFY_IS_FALSE(controller.SetControlPads(grid, stored));

    // A readout can show a grid.
    auto const readout = controller.AddControl(glass::ControlKind::Readout, 600, 100);

    glass::ReadoutSpec wanted{};
    wanted.Source = glass::ReadoutSource::Octave;
    wanted.Targets = Only(grid);

    VERIFY_IS_TRUE(controller.SetReadout(readout, wanted));
    VERIFY_IS_FALSE(controller.SetReadout(readout, wanted));
    VERIFY_IS_FALSE(controller.SetReadout(grid, wanted));
}
