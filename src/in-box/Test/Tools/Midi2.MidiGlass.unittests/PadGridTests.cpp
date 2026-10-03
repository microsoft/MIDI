// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The note pads and hex pads: where each pad sits, what it plays, which pad a finger is on, the
// pitch under a finger that slides, who is holding which note, and what a slide puts on the
// wire.

#include "PadGridTests.h"

#include "PadGrid.h"
#include "PadVoices.h"
#include "BindingEngine.h"
#include "ControlFactory.h"
#include "EditorController.h"
#include "LayoutModel.h"
#include "LayoutSerializer.h"
#include "PageTemplates.h"

#include <array>
#include <cmath>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    constexpr double Tolerance = 0.0001;

    void VerifyNear(_In_ double expected, _In_ double actual)
    {
        VERIFY_IS_LESS_THAN(std::abs(expected - actual), Tolerance);
    }

    glass::LayoutDocument DocumentWith(_In_ std::vector<glass::Control> controls)
    {
        glass::LayoutDocument document{};

        document.Name = L"Pads";
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

    // One grid with a MIDI 2.0 row and a MIDI 1.0 row, both on the third channel.
    glass::Control GridWithBothProtocols()
    {
        glass::Control control{};

        control.Id = L"pads";
        control.Kind = glass::ControlKind::NotePads;

        glass::ControlMessage row{};

        row.Trigger = glass::MessageTrigger::Changes;
        row.Kind = glass::MessageKind::Note;
        row.DeviceName = L"Synth";
        row.ChannelIndex = 2;

        auto midi1 = row;
        midi1.UseMidi1Protocol = true;

        control.Messages.push_back(row);
        control.Messages.push_back(midi1);

        return control;
    }

    // The default grid, three rows of eight, laid out in the rectangle a new one gets.
    glass::PadGridLayout DefaultSquareLayout()
    {
        return glass::LayOutPadGrid(glass::PadGridSpec{}, false, 428.0, 164.0);
    }

    glass::PadGridSpec WickiHayden()
    {
        glass::PadGridSpec spec{};

        spec.RightInterval = glass::WickiHaydenLayout.RightInterval;
        spec.RowInterval = glass::WickiHaydenLayout.RowInterval;

        return spec;
    }

    bool Is(_In_ glass::PadAction const& action, _In_ glass::PadActionKind kind, _In_ int32_t note)
    {
        return action.Kind == kind && action.Note == note;
    }

    using Actions = std::array<glass::PadAction, glass::PadVoices::MaximumTouches>;
}

// -------------------------------------------------------------------- where the pads go

void PadGridTests::ANewGridIsThreeFullRowsOfEight()
{
    glass::Page page{};
    page.Id = L"page";

    auto const control = glass::MakeNewControl(
        glass::ControlKind::NotePads, 0, 0, glass::ReferencePageWidth, glass::ReferencePageHeight, L"Synth", page);

    VERIFY_ARE_EQUAL(24, control.Pads.PadCount);

    auto const layout = glass::LayOutPadGrid(control.Pads, false, control.Width, control.Height);

    VERIFY_ARE_EQUAL(8, layout.Columns);
    VERIFY_ARE_EQUAL(3, layout.Rows);

    // Drawn at the size it asked for, so the first resize flows the pads rather than jumping.
    VERIFY_ARE_EQUAL(control.Pads.PadSize, layout.PadWidth);

    // One row that says where to send. The pad decides the note.
    VERIFY_ARE_EQUAL(size_t{ 1 }, control.Messages.size());
    VERIFY_IS_TRUE(control.Messages[0].Kind == glass::MessageKind::Note);
}

void PadGridTests::ANewHexGridIsThreeFullRowsOfEight()
{
    glass::Page page{};
    page.Id = L"page";

    auto const control = glass::MakeNewControl(
        glass::ControlKind::HexPads, 0, 0, glass::ReferencePageWidth, glass::ReferencePageHeight, L"Synth", page);

    auto const layout = glass::LayOutPadGrid(control.Pads, true, control.Width, control.Height);

    VERIFY_ARE_EQUAL(8, layout.Columns);
    VERIFY_ARE_EQUAL(3, layout.Rows);
    VERIFY_ARE_EQUAL(control.Pads.PadSize, layout.PadWidth);

    // And it starts on the layout most hexagon players already know.
    VERIFY_ARE_EQUAL(glass::WickiHaydenLayout.RightInterval, control.Pads.RightInterval);
    VERIFY_ARE_EQUAL(glass::WickiHaydenLayout.RowInterval, control.Pads.RowInterval);
}

void PadGridTests::PadsFlowIntoMoreRowsAsTheControlNarrows()
{
    // Six 48 px pads and their gaps are 321.6 across; seven would need 374.4.
    auto const layout = glass::LayOutPadGrid(glass::PadGridSpec{}, false, 330.0, 220.0);

    VERIFY_ARE_EQUAL(6, layout.Columns);
    VERIFY_ARE_EQUAL(4, layout.Rows);
    VERIFY_ARE_EQUAL(48.0, layout.PadWidth);
    VERIFY_ARE_EQUAL(size_t{ 24 }, layout.Cells.size());
}

void PadGridTests::PadsShrinkRatherThanFallOffAShortControl()
{
    auto const layout = glass::LayOutPadGrid(glass::PadGridSpec{}, false, 428.0, 100.0);

    VERIFY_ARE_EQUAL(size_t{ 24 }, layout.Cells.size());
    VERIFY_IS_LESS_THAN(layout.PadWidth, 48.0);

    for (auto const& cell : layout.Cells)
    {
        VERIFY_IS_GREATER_THAN_OR_EQUAL(cell.CenterX - layout.PadWidth * 0.5, -Tolerance);
        VERIFY_IS_LESS_THAN_OR_EQUAL(cell.CenterX + layout.PadWidth * 0.5, 428.0 + Tolerance);
        VERIFY_IS_GREATER_THAN_OR_EQUAL(cell.CenterY - layout.PadHeight * 0.5, -Tolerance);
        VERIFY_IS_LESS_THAN_OR_EQUAL(cell.CenterY + layout.PadHeight * 0.5, 100.0 + Tolerance);
    }
}

void PadGridTests::TheFirstPadIsBottomLeft()
{
    auto const layout = DefaultSquareLayout();

    // The lowest note nearest the player, the way every grid controller has it.
    VERIFY_IS_LESS_THAN(layout.Cells[0].CenterX, layout.Cells[1].CenterX);
    VERIFY_IS_GREATER_THAN(layout.Cells[0].CenterY, layout.Cells[8].CenterY);

    VERIFY_ARE_EQUAL(1, layout.Cells[8].Row);
    VERIFY_ARE_EQUAL(0, layout.Cells[8].Column);
}

void PadGridTests::EverySecondHexRowSitsHalfAPadOver()
{
    auto const layout = glass::LayOutPadGrid(WickiHayden(), true, 456.0, 160.0);

    VerifyNear(layout.Pitch * 0.5, layout.Cells[8].CenterX - layout.Cells[0].CenterX);
    VerifyNear(0.0, layout.Cells[16].CenterX - layout.Cells[0].CenterX);

    // Nested: rows are closer together than a hexagon is tall.
    VERIFY_IS_LESS_THAN(layout.Cells[0].CenterY - layout.Cells[8].CenterY, layout.PadHeight);
}

// ---------------------------------------------------------------- what each pad plays

void PadGridTests::RowsAFourthApartRepeatNotes()
{
    glass::PadGridSpec const spec{};

    VERIFY_ARE_EQUAL(53, glass::PadNote(spec, false, 1, 0, 8));
    VERIFY_ARE_EQUAL(60, glass::PadNote(spec, false, 2, 2, 8));

    // The note a fourth up is also five pads along the row below.
    VERIFY_ARE_EQUAL(glass::PadNote(spec, false, 1, 0, 8), glass::PadNote(spec, false, 0, 5, 8));
}

void PadGridTests::ZeroCarriesEachRowOnFromTheLast()
{
    glass::PadGridSpec spec{};
    spec.RowInterval = 0;

    VERIFY_ARE_EQUAL(56, glass::PadNote(spec, false, 1, 0, 8));
    VERIFY_ARE_EQUAL(71, glass::PadNote(spec, false, 2, 7, 8));
}

void PadGridTests::TwoRowsUpOnWickiHaydenIsAnOctave()
{
    auto const spec = WickiHayden();

    VERIFY_ARE_EQUAL(55, glass::PadNote(spec, true, 1, 0, 8));
    VERIFY_ARE_EQUAL(60, glass::PadNote(spec, true, 2, 0, 8));
    VERIFY_ARE_EQUAL(62, glass::PadNote(spec, true, 2, 1, 8));
}

void PadGridTests::AMajorChordOnTheHarmonicTableIsThreePadsThatTouch()
{
    glass::PadGridSpec spec{};
    spec.RightInterval = glass::HarmonicTableLayout.RightInterval;
    spec.RowInterval = glass::HarmonicTableLayout.RowInterval;

    auto const layout = glass::LayOutPadGrid(spec, true, 456.0, 160.0);

    auto const& root = layout.Cells[0];
    auto const& third = layout.Cells[1];
    auto const& fifth = layout.Cells[8];

    VERIFY_ARE_EQUAL(48, root.Note);
    VERIFY_ARE_EQUAL(52, third.Note);
    VERIFY_ARE_EQUAL(55, fifth.Note);

    auto const apart = [](glass::PadCell const& a, glass::PadCell const& b)
        {
            return std::hypot(a.CenterX - b.CenterX, a.CenterY - b.CenterY);
        };

    VerifyNear(layout.Pitch, apart(root, third));
    VerifyNear(layout.Pitch, apart(root, fifth));
    VerifyNear(layout.Pitch, apart(third, fifth));
}

void PadGridTests::APadOffTheEndOfTheNoteRangePlaysNothing()
{
    glass::PadGridSpec spec{};
    spec.StartNote = 120;

    VERIFY_ARE_EQUAL(127, glass::PadNote(spec, false, 0, 7, 8));
    VERIFY_ARE_EQUAL(-1, glass::PadNote(spec, false, 1, 3, 8));

    // Below zero, which a hexagon row can reach by stepping left along its diagonal.
    glass::PadGridSpec low{};
    low.StartNote = 0;
    low.RightInterval = 7;
    low.RowInterval = 1;

    VERIFY_ARE_EQUAL(-1, glass::PadNote(low, true, 2, 0, 8));
}

// ------------------------------------------------------------- which pad a finger is on

void PadGridTests::ATouchInTheGapBelongsToTheNearerPad()
{
    auto const layout = DefaultSquareLayout();

    auto const left = layout.Cells[0].CenterX;
    auto const y = layout.Cells[0].CenterY;

    // 48 px pads with 4.8 px between them: the gap runs from 24 to 28.8 past the first center,
    // and its middle is 26.4.
    VERIFY_ARE_EQUAL(0, glass::PadAtPoint(layout, left + 25.0, y));
    VERIFY_ARE_EQUAL(1, glass::PadAtPoint(layout, left + 28.0, y));
}

void PadGridTests::ATouchPastAShortRowIsNoPad()
{
    glass::PadGridSpec spec{};
    spec.PadCount = 10;

    auto const layout = glass::LayOutPadGrid(spec, false, 428.0, 164.0);

    VERIFY_ARE_EQUAL(2, layout.Rows);

    // Above the sixth pad of the bottom row, where the short top row has nothing.
    VERIFY_ARE_EQUAL(-1, glass::PadAtPoint(layout, layout.Cells[5].CenterX, layout.Cells[8].CenterY));

    // And on the pads it does have.
    VERIFY_ARE_EQUAL(9, glass::PadAtPoint(layout, layout.Cells[9].CenterX, layout.Cells[9].CenterY));
}

void PadGridTests::ATouchNearAHexagonsPointIsThatHexagon()
{
    auto const layout = glass::LayOutPadGrid(WickiHayden(), true, 456.0, 160.0);

    auto const& cell = layout.Cells[0];

    VERIFY_ARE_EQUAL(0, glass::PadAtPoint(layout, cell.CenterX, cell.CenterY - layout.PadHeight * 0.5 + 1.0));
    VERIFY_ARE_EQUAL(0, glass::PadAtPoint(layout, cell.CenterX, cell.CenterY + layout.PadHeight * 0.5 - 1.0));
}

// --------------------------------------------------------- the pitch under a sliding finger

void PadGridTests::AFingerNearTheMiddleOfAPadIsInTune()
{
    auto const layout = DefaultSquareLayout();
    glass::PadGridSpec const spec{};

    auto const& cell = layout.Cells[1];

    VERIFY_ARE_EQUAL(49.0, glass::PitchAtPoint(layout, spec, 1, cell.CenterX));
    VERIFY_ARE_EQUAL(49.0, glass::PitchAtPoint(layout, spec, 1, cell.CenterX + layout.Pitch * 0.15));
    VERIFY_ARE_EQUAL(49.0, glass::PitchAtPoint(layout, spec, 1, cell.CenterX - layout.Pitch * 0.15));
}

void PadGridTests::ThePitchMeetsTheNextPadAtTheEdgeBetweenThem()
{
    auto const layout = DefaultSquareLayout();
    glass::PadGridSpec const spec{};

    auto const edge = layout.Cells[1].CenterX + layout.Pitch * 0.5;

    VerifyNear(49.5, glass::PitchAtPoint(layout, spec, 1, edge));
    VerifyNear(49.5, glass::PitchAtPoint(layout, spec, 2, edge));

    // Three quarters of the way out, half way between flat and the edge.
    VerifyNear(49.25, glass::PitchAtPoint(layout, spec, 1, layout.Cells[1].CenterX + layout.Pitch * 0.35));
}

void PadGridTests::TheEndOfARowHoldsItsOwnNote()
{
    auto const layout = DefaultSquareLayout();
    glass::PadGridSpec const spec{};

    VERIFY_ARE_EQUAL(55.0, glass::PitchAtPoint(layout, spec, 7, layout.Cells[7].CenterX + layout.Pitch * 0.4));
    VERIFY_ARE_EQUAL(48.0, glass::PitchAtPoint(layout, spec, 0, layout.Cells[0].CenterX - layout.Pitch * 0.4));
}

// -------------------------------------------------------------------- keys and names

void PadGridTests::TheRootHasARoleOfItsOwn()
{
    glass::PadGridSpec const spec{};

    VERIFY_IS_TRUE(glass::RoleOfNote(spec, 60) == glass::PadRole::Root);
    VERIFY_IS_TRUE(glass::RoleOfNote(spec, 72) == glass::PadRole::Root);
    VERIFY_IS_TRUE(glass::RoleOfNote(spec, 62) == glass::PadRole::InKey);
    VERIFY_IS_TRUE(glass::RoleOfNote(spec, 61) == glass::PadRole::OutOfKey);
}

void PadGridTests::WithNoKeyEveryPadIsInIt()
{
    glass::PadGridSpec spec{};
    spec.KeyRoot = glass::NoKey;

    VERIFY_IS_TRUE(glass::RoleOfNote(spec, 60) == glass::PadRole::InKey);
    VERIFY_IS_TRUE(glass::RoleOfNote(spec, 61) == glass::PadRole::InKey);
}

void PadGridTests::AMinorKeyHoldsItsFlatThird()
{
    glass::PadGridSpec spec{};
    spec.KeyRoot = 9;
    spec.Scale = glass::MusicalScale::Minor;

    VERIFY_IS_TRUE(glass::RoleOfNote(spec, 57) == glass::PadRole::Root);
    VERIFY_IS_TRUE(glass::RoleOfNote(spec, 60) == glass::PadRole::InKey);
    VERIFY_IS_TRUE(glass::RoleOfNote(spec, 61) == glass::PadRole::OutOfKey);
}

void PadGridTests::FlatKeysAreWrittenWithFlats()
{
    VERIFY_IS_TRUE(glass::KeyUsesFlats(5, glass::MusicalScale::Major));
    VERIFY_IS_TRUE(glass::KeyUsesFlats(10, glass::MusicalScale::Major));
    VERIFY_IS_TRUE(glass::KeyUsesFlats(2, glass::MusicalScale::Minor));
    VERIFY_IS_FALSE(glass::KeyUsesFlats(7, glass::MusicalScale::Major));
    VERIFY_IS_FALSE(glass::KeyUsesFlats(4, glass::MusicalScale::Minor));
    VERIFY_IS_FALSE(glass::KeyUsesFlats(0, glass::MusicalScale::Major));

    // A mode borrows the key signature of the major key it is a mode of.
    VERIFY_IS_FALSE(glass::KeyUsesFlats(2, glass::MusicalScale::Dorian));
    VERIFY_IS_FALSE(glass::KeyUsesFlats(7, glass::MusicalScale::Mixolydian));

    // Middle C is C3, the way the SDK's MidiMessageHelper names it by default.
    VERIFY_ARE_EQUAL(std::wstring{ L"B\u266D3" }, glass::PadNoteName(70, true));
    VERIFY_ARE_EQUAL(std::wstring{ L"C\u266F3" }, glass::PadNoteName(61, false));
    VERIFY_ARE_EQUAL(std::wstring{ L"C3" }, glass::PadNoteName(60, false));
    VERIFY_ARE_EQUAL(std::wstring{ L"C2" }, glass::PadNoteName(48, false));
    VERIFY_ARE_EQUAL(std::wstring{ L"C-2" }, glass::PadNoteName(0, false));
    VERIFY_ARE_EQUAL(std::wstring{ L"G8" }, glass::PadNoteName(127, false));
    VERIFY_IS_TRUE(glass::PadNoteName(128, false).empty());
}

// ------------------------------------------------------ fingers and the notes they hold

void PadGridTests::APressPlaysANoteAndAReleaseEndsIt()
{
    glass::PadVoices voices{};
    voices.Configure(glass::PadGlide::Off, 48);

    Actions actions{};

    VERIFY_ARE_EQUAL(1u, voices.Press(1, 60, 0.8, 60.0, actions));
    VERIFY_IS_TRUE(Is(actions[0], glass::PadActionKind::NoteOn, 60));
    VERIFY_ARE_EQUAL(0.8, actions[0].Value);

    VERIFY_ARE_EQUAL(1u, voices.Release(1, actions));
    VERIFY_IS_TRUE(Is(actions[0], glass::PadActionKind::NoteOff, 60));

    VERIFY_ARE_EQUAL(size_t{ 0 }, voices.HeldCount());
}

void PadGridTests::TwoFingersPlayAChord()
{
    glass::PadVoices voices{};
    voices.Configure(glass::PadGlide::Off, 48);

    Actions actions{};

    VERIFY_ARE_EQUAL(1u, voices.Press(1, 60, 1.0, 60.0, actions));
    VERIFY_ARE_EQUAL(1u, voices.Press(2, 64, 1.0, 64.0, actions));
    VERIFY_IS_TRUE(Is(actions[0], glass::PadActionKind::NoteOn, 64));

    VERIFY_ARE_EQUAL(1u, voices.Release(1, actions));
    VERIFY_IS_TRUE(Is(actions[0], glass::PadActionKind::NoteOff, 60));
    VERIFY_ARE_EQUAL(size_t{ 1 }, voices.HeldCount());
}

void PadGridTests::ASlideWithGlideOffEndsOneNoteBeforeStartingTheNext()
{
    glass::PadVoices voices{};
    voices.Configure(glass::PadGlide::Off, 48);

    Actions actions{};

    voices.Press(1, 60, 1.0, 60.0, actions);

    VERIFY_ARE_EQUAL(2u, voices.Move(1, 62, 1.0, 62.0, actions));
    VERIFY_IS_TRUE(Is(actions[0], glass::PadActionKind::NoteOff, 60));
    VERIFY_IS_TRUE(Is(actions[1], glass::PadActionKind::NoteOn, 62));

    // Moving about on the same pad changes nothing.
    VERIFY_ARE_EQUAL(0u, voices.Move(1, 62, 1.0, 62.3, actions));
}

void PadGridTests::APortamentoSlideNamesTheNoteItCameFrom()
{
    glass::PadVoices voices{};
    voices.Configure(glass::PadGlide::Portamento, 48);

    Actions actions{};

    voices.Press(1, 60, 1.0, 60.0, actions);

    VERIFY_ARE_EQUAL(3u, voices.Move(1, 62, 1.0, 62.0, actions));
    VERIFY_IS_TRUE(Is(actions[0], glass::PadActionKind::PortamentoFrom, 60));
    VERIFY_IS_TRUE(Is(actions[1], glass::PadActionKind::NoteOn, 62));
    VERIFY_IS_TRUE(Is(actions[2], glass::PadActionKind::NoteOff, 60));
}

void PadGridTests::AReleaseAfterASlideEndsTheNoteTheFingerIsOn()
{
    for (auto const glide : { glass::PadGlide::Off, glass::PadGlide::Portamento })
    {
        glass::PadVoices voices{};
        voices.Configure(glide, 48);

        Actions actions{};

        voices.Press(1, 60, 1.0, 60.0, actions);
        voices.Move(1, 62, 1.0, 62.0, actions);

        VERIFY_ARE_EQUAL(1u, voices.Release(1, actions));
        VERIFY_IS_TRUE(Is(actions[0], glass::PadActionKind::NoteOff, 62));
    }
}

void PadGridTests::TwoFingersOnOneNoteShareIt()
{
    glass::PadVoices voices{};
    voices.Configure(glass::PadGlide::Off, 48);

    Actions actions{};

    VERIFY_ARE_EQUAL(1u, voices.Press(1, 60, 1.0, 60.0, actions));

    // The same note under a second finger, which a grid of fourths has in two places. A second
    // note on would retrigger it, and the first release would cut off the second finger.
    VERIFY_ARE_EQUAL(0u, voices.Press(2, 60, 1.0, 60.0, actions));
    VERIFY_ARE_EQUAL(0u, voices.Release(1, actions));

    VERIFY_ARE_EQUAL(1u, voices.Release(2, actions));
    VERIFY_IS_TRUE(Is(actions[0], glass::PadActionKind::NoteOff, 60));
}

void PadGridTests::AGlideNeverStealsANoteSomebodyElseIsHolding()
{
    glass::PadVoices voices{};
    voices.Configure(glass::PadGlide::Portamento, 48);

    Actions actions{};

    voices.Press(1, 60, 1.0, 60.0, actions);
    voices.Press(2, 60, 1.0, 60.0, actions);

    // Gliding the one voice away would silence the other finger, so this is a plain new note.
    VERIFY_ARE_EQUAL(1u, voices.Move(1, 62, 1.0, 62.0, actions));
    VERIFY_IS_TRUE(Is(actions[0], glass::PadActionKind::NoteOn, 62));

    VERIFY_ARE_EQUAL(1u, voices.Release(2, actions));
    VERIFY_IS_TRUE(Is(actions[0], glass::PadActionKind::NoteOff, 60));

    VERIFY_ARE_EQUAL(1u, voices.Release(1, actions));
    VERIFY_IS_TRUE(Is(actions[0], glass::PadActionKind::NoteOff, 62));
}

void PadGridTests::ABendingNoteStartsInTuneAtItsOwnRange()
{
    glass::PadVoices voices{};
    voices.Configure(glass::PadGlide::PerNoteBend, 24);

    Actions actions{};

    VERIFY_ARE_EQUAL(3u, voices.Press(1, 60, 1.0, 60.0, actions));

    VERIFY_IS_TRUE(actions[0].Kind == glass::PadActionKind::BendRange);
    VERIFY_ARE_EQUAL(24.0, actions[0].Value);

    // Whatever the last finger on this note left its bend at, it starts in tune.
    VERIFY_IS_TRUE(Is(actions[1], glass::PadActionKind::Bend, 60));
    VERIFY_ARE_EQUAL(0.0, actions[1].Value);

    VERIFY_IS_TRUE(Is(actions[2], glass::PadActionKind::NoteOn, 60));
}

void PadGridTests::ABendFollowsTheFingerAndIgnoresTinyMoves()
{
    glass::PadVoices voices{};
    voices.Configure(glass::PadGlide::PerNoteBend, 48);

    Actions actions{};

    voices.Press(1, 60, 1.0, 60.0, actions);

    VERIFY_ARE_EQUAL(1u, voices.Move(1, 61, 1.0, 60.5, actions));
    VERIFY_IS_TRUE(Is(actions[0], glass::PadActionKind::Bend, 60));
    VERIFY_ARE_EQUAL(0.5, actions[0].Value);

    VERIFY_ARE_EQUAL(0u, voices.Move(1, 61, 1.0, 60.504, actions));

    VERIFY_ARE_EQUAL(1u, voices.Move(1, 62, 1.0, 62.0, actions));
    VERIFY_ARE_EQUAL(2.0, actions[0].Value);

    // The note a bending finger ends is the one it struck.
    VERIFY_ARE_EQUAL(1u, voices.Release(1, actions));
    VERIFY_IS_TRUE(Is(actions[0], glass::PadActionKind::NoteOff, 60));
}

void PadGridTests::ABendIsHeldAtItsRange()
{
    glass::PadVoices voices{};
    voices.Configure(glass::PadGlide::PerNoteBend, 2);

    Actions actions{};

    voices.Press(1, 60, 1.0, 60.0, actions);

    VERIFY_ARE_EQUAL(1u, voices.Move(1, 65, 1.0, 65.0, actions));
    VERIFY_ARE_EQUAL(2.0, actions[0].Value);
}

void PadGridTests::ReleasingEverythingEndsEveryNote()
{
    glass::PadVoices voices{};
    voices.Configure(glass::PadGlide::Off, 48);

    Actions actions{};

    voices.Press(1, 60, 1.0, 60.0, actions);
    voices.Press(2, 64, 1.0, 64.0, actions);
    voices.Press(3, 64, 1.0, 64.0, actions);

    VERIFY_ARE_EQUAL(2u, voices.ReleaseAll(actions));
    VERIFY_IS_TRUE(Is(actions[0], glass::PadActionKind::NoteOff, 60));
    VERIFY_IS_TRUE(Is(actions[1], glass::PadActionKind::NoteOff, 64));

    VERIFY_ARE_EQUAL(size_t{ 0 }, voices.HeldCount());

    // A finger lifting after everything was let go has nothing left to end.
    VERIFY_ARE_EQUAL(0u, voices.Release(1, actions));
}

// ------------------------------------------------------------------ what goes on the wire

void PadGridTests::PortamentoIsControlChange84CarryingTheNote()
{
    glass::BindingEngine engine{};
    engine.Prepare(DocumentWith({ GridWithBothProtocols() }), OneDevice());

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    VERIFY_ARE_EQUAL(2u, engine.EvaluatePortamento(0, 60, sends));

    // MIDI 2.0: the note in the top seven bits of the value, as the UMP specification puts it.
    VERIFY_ARE_EQUAL(2u, sends[0].WordCount);
    VERIFY_ARE_EQUAL(0x40B25400u, sends[0].Words[0]);
    VERIFY_ARE_EQUAL(0x78000000u, sends[0].Words[1]);

    // MIDI 1.0: B2 54 3C.
    VERIFY_ARE_EQUAL(1u, sends[1].WordCount);
    VERIFY_ARE_EQUAL(0x20B2543Cu, sends[1].Words[0]);
}

void PadGridTests::APerNoteBendIsCenteredAtHalfScale()
{
    VERIFY_ARE_EQUAL(0x80000000u, glass::PerNotePitchBendValue(0.0, 48.0));
    VERIFY_ARE_EQUAL(0xFFFFFFFFu, glass::PerNotePitchBendValue(48.0, 48.0));
    VERIFY_ARE_EQUAL(0x00000000u, glass::PerNotePitchBendValue(-48.0, 48.0));
    VERIFY_ARE_EQUAL(0xC0000000u, glass::PerNotePitchBendValue(24.0, 48.0));
    VERIFY_ARE_EQUAL(0x40000000u, glass::PerNotePitchBendValue(-24.0, 48.0));

    // Past the range is held at it, and no range at all is no bend.
    VERIFY_ARE_EQUAL(0xFFFFFFFFu, glass::PerNotePitchBendValue(100.0, 48.0));
    VERIFY_ARE_EQUAL(0x80000000u, glass::PerNotePitchBendValue(1.0, 0.0));
}

void PadGridTests::APerNoteBendOnlyGoesToMidi2Rows()
{
    glass::BindingEngine engine{};
    engine.Prepare(DocumentWith({ GridWithBothProtocols() }), OneDevice());

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    // MIDI 1.0 has no message that bends one note on its own, so that row sends nothing.
    VERIFY_ARE_EQUAL(1u, engine.EvaluatePerNotePitchBend(0, 60, 12.0, 48.0, sends));

    // Type 4, status 6, channel 3, note 60; a quarter of the way up.
    VERIFY_ARE_EQUAL(0x40623C00u, sends[0].Words[0]);
    VERIFY_ARE_EQUAL(0xA0000000u, sends[0].Words[1]);
}

void PadGridTests::TheBendRangeIsRegisteredControllerSeven()
{
    glass::BindingEngine engine{};
    engine.Prepare(DocumentWith({ GridWithBothProtocols() }), OneDevice());

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    VERIFY_ARE_EQUAL(1u, engine.EvaluatePerNoteBendRange(0, 48.0, sends));

    // Registered controller, bank 0, index 7, with 48 semitones in 7.25 fixed point.
    VERIFY_ARE_EQUAL(0x40220007u, sends[0].Words[0]);
    VERIFY_ARE_EQUAL(0x60000000u, sends[0].Words[1]);

    VERIFY_ARE_EQUAL(0x05000000u, glass::SemitonesAsPitch725(2.5));
}

// -------------------------------------------------------------- the file and the editor

void PadGridTests::PadsSurviveARoundTrip()
{
    glass::Control control{};

    control.Id = L"hex";
    control.Kind = glass::ControlKind::HexPads;
    control.Width = 456;
    control.Height = 160;

    auto& pads = control.Pads;

    pads.PadCount = 37;
    pads.PadSize = 40.5;
    pads.StartNote = 36;
    pads.RightInterval = 4;
    pads.RowInterval = 7;
    pads.KeyRoot = 6;
    pads.Scale = glass::MusicalScale::Dorian;
    pads.NoteNames = glass::PadNoteNames::BottomRight;
    pads.NoteNameSize = 11.0;
    pads.RootColor = L"#FF00AA";
    pads.InKeyColor = L"#00AAFF";
    pads.OutOfKeyColor = L"#EEEEEE";
    pads.PressedColor = L"#FFFFFF";
    pads.Glide = glass::PadGlide::PerNoteBend;
    pads.BendRangeSemitones = 12;

    auto const text = glass::WriteLayoutToJson(DocumentWith({ control }));

    VERIFY_IS_TRUE(text.find(L"\"kind\": \"hexPads\"") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"key\": \"fSharp\"") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"scale\": \"dorian\"") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"glide\": \"perNoteBend\"") != std::wstring::npos);

    auto const reread = glass::ReadLayoutFromJson(text);
    VERIFY_IS_TRUE(reread.Succeeded);

    auto const* back = reread.Document.FindControl(L"hex");
    VERIFY_IS_NOT_NULL(back);

    VERIFY_IS_TRUE(back->Kind == glass::ControlKind::HexPads);
    VERIFY_ARE_EQUAL(37, back->Pads.PadCount);
    VERIFY_ARE_EQUAL(40.5, back->Pads.PadSize);
    VERIFY_ARE_EQUAL(36, back->Pads.StartNote);
    VERIFY_ARE_EQUAL(4, back->Pads.RightInterval);
    VERIFY_ARE_EQUAL(7, back->Pads.RowInterval);
    VERIFY_ARE_EQUAL(6, back->Pads.KeyRoot);
    VERIFY_IS_TRUE(back->Pads.Scale == glass::MusicalScale::Dorian);
    VERIFY_IS_TRUE(back->Pads.NoteNames == glass::PadNoteNames::BottomRight);
    VERIFY_ARE_EQUAL(11.0, back->Pads.NoteNameSize);
    VERIFY_ARE_EQUAL(std::wstring{ L"#FF00AA" }, back->Pads.RootColor);
    VERIFY_ARE_EQUAL(std::wstring{ L"#00AAFF" }, back->Pads.InKeyColor);
    VERIFY_ARE_EQUAL(std::wstring{ L"#EEEEEE" }, back->Pads.OutOfKeyColor);
    VERIFY_ARE_EQUAL(std::wstring{ L"#FFFFFF" }, back->Pads.PressedColor);
    VERIFY_IS_TRUE(back->Pads.Glide == glass::PadGlide::PerNoteBend);
    VERIFY_ARE_EQUAL(12, back->Pads.BendRangeSemitones);

    VERIFY_ARE_EQUAL(text, glass::WriteLayoutToJson(reread.Document));
}

void PadGridTests::PadsFromAFileAreBounded()
{
    // A stranger's file must not be able to ask for a billion pads, and a key or a glide this
    // build has never heard of falls back rather than failing the file.
    auto const result = glass::ReadLayoutFromJson(
        LR"({ "fileVersion": 1, "name": "T", "pages": [ { "id": "p", "name": "P", "controls": [
            { "id": "a", "kind": "notePads", "pads": { "padCount": 1e9, "padSize": -5, "startNote": 300,
              "rightInterval": 99, "key": "h", "glide": "warp", "sparkle": 3 } } ] } ] })");

    VERIFY_IS_TRUE(result.Succeeded);

    auto const& pads = result.Document.Pages[0].Controls[0].Pads;

    VERIFY_ARE_EQUAL(24, pads.PadCount);
    VERIFY_ARE_EQUAL(glass::MinimumPadSize, pads.PadSize);
    VERIFY_ARE_EQUAL(48, pads.StartNote);
    VERIFY_ARE_EQUAL(1, pads.RightInterval);
    VERIFY_ARE_EQUAL(0, pads.KeyRoot);
    VERIFY_IS_TRUE(pads.Glide == glass::PadGlide::Off);

    // and what it did not understand goes back out with it
    VERIFY_IS_TRUE(glass::WriteLayoutToJson(result.Document).find(L"\"sparkle\"") != std::wstring::npos);
}

void PadGridTests::SwitchingBetweenSquareAndHexCarriesTheStartingLayout()
{
    glass::EditorController controller{};
    controller.Load(DocumentWith({}));

    auto const id = controller.AddControl(glass::ControlKind::NotePads, 100, 100);

    VERIFY_IS_FALSE(id.empty());

    auto const padsOf = [&]() { return controller.Document().FindControl(id)->Pads; };

    VERIFY_IS_TRUE(controller.SetControlKind(id, glass::ControlKind::HexPads));
    VERIFY_ARE_EQUAL(glass::WickiHaydenLayout.RightInterval, padsOf().RightInterval);
    VERIFY_ARE_EQUAL(glass::WickiHaydenLayout.RowInterval, padsOf().RowInterval);

    VERIFY_IS_TRUE(controller.SetControlKind(id, glass::ControlKind::NotePads));
    VERIFY_ARE_EQUAL(1, padsOf().RightInterval);
    VERIFY_ARE_EQUAL(5, padsOf().RowInterval);

    // A layout somebody chose is theirs, and goes across as it is.
    auto chosen = padsOf();
    chosen.RightInterval = 3;
    chosen.RowInterval = 4;

    VERIFY_IS_TRUE(controller.SetControlPads(id, chosen));
    VERIFY_IS_TRUE(controller.SetControlKind(id, glass::ControlKind::HexPads));
    VERIFY_ARE_EQUAL(3, padsOf().RightInterval);
    VERIFY_ARE_EQUAL(4, padsOf().RowInterval);
}
