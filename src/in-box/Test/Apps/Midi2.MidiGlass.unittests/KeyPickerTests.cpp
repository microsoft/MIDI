// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Root and scale pickers: switches that pick the key of every pad grid while a layout runs. What
// a pick does to the notes, what each position picks, what the file keeps, and what the editor
// builds.

#include "KeyPickerTests.h"

#include "PadGrid.h"
#include "BindingEngine.h"
#include "ControlFactory.h"
#include "EditorController.h"
#include "LayoutModel.h"
#include "LayoutSerializer.h"

#include <algorithm>
#include <array>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    glass::LayoutDocument DocumentWith(_In_ std::vector<glass::Control> controls)
    {
        glass::LayoutDocument document{};

        document.Name = L"Pickers";
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

    glass::Control Grid(_In_ std::wstring const& id, _In_ int32_t root, _In_ glass::MusicalScale scale)
    {
        glass::Control control{};

        control.Id = id;
        control.Kind = glass::ControlKind::NotePads;
        control.Width = 428;
        control.Height = 164;
        control.Pads.KeyRoot = root;
        control.Pads.Scale = scale;
        control.Pads.Notes = glass::PadNotes::InKey;

        return control;
    }

    glass::Control Picker(_In_ std::wstring const& id, _In_ glass::SwitchPicks picks)
    {
        glass::Control control{};

        control.Id = id;
        control.Kind = glass::ControlKind::Switch;
        control.Width = 376;
        control.Height = 44;
        control.Switch.Picks = picks;

        if (picks == glass::SwitchPicks::Scale)
        {
            control.Switch.Scales = glass::DefaultPickerScales();
        }

        return control;
    }

    int32_t ScaleIndex(_In_ glass::MusicalScale scale)
    {
        return static_cast<int32_t>(scale);
    }

    std::vector<int32_t> BottomRow(_In_ glass::PadGridSpec const& spec)
    {
        std::vector<int32_t> notes{};

        for (int32_t column = 0; column < 8; ++column)
        {
            notes.push_back(glass::PadNote(spec, false, 0, column, 8));
        }

        return notes;
    }
}

// ------------------------------------------------------------ what a pick does to the pads

void KeyPickerTests::APickedKeyReplacesTheGridsKey()
{
    auto const grid = Grid(L"grid", 2, glass::MusicalScale::Major).Pads;

    auto picked = glass::WithPickedKey(grid, { 4, -1 });

    VERIFY_ARE_EQUAL(4, picked.KeyRoot);
    VERIFY_IS_TRUE(picked.Scale == glass::MusicalScale::Major);

    picked = glass::WithPickedKey(grid, { -1, ScaleIndex(glass::MusicalScale::Phrygian) });

    VERIFY_ARE_EQUAL(2, picked.KeyRoot);
    VERIFY_IS_TRUE(picked.Scale == glass::MusicalScale::Phrygian);

    // Nothing picked, or something no picker offers, leaves the grid as it was saved.
    for (auto const nothing : { glass::PickedKey{}, glass::PickedKey{ 12, 99 } })
    {
        picked = glass::WithPickedKey(grid, nothing);

        VERIFY_ARE_EQUAL(2, picked.KeyRoot);
        VERIFY_IS_TRUE(picked.Scale == glass::MusicalScale::Major);
    }

    // A grid with no key has nothing to change, so it plays every note whatever is picked.
    auto plain = grid;
    plain.KeyRoot = glass::NoKey;

    picked = glass::WithPickedKey(plain, { 4, ScaleIndex(glass::MusicalScale::Minor) });

    VERIFY_ARE_EQUAL(glass::NoKey, picked.KeyRoot);
    VERIFY_IS_TRUE(picked.Scale == glass::MusicalScale::Major);
}

void KeyPickerTests::InKeyPadsPlayOnlyTheScaleThatIsPicked()
{
    auto const grid = Grid(L"grid", 0, glass::MusicalScale::Major).Pads;

    // C major: C, D, E, F, G, A, B and C, with no sharps or flats.
    VERIFY_IS_TRUE((std::vector<int32_t>{ 48, 50, 52, 53, 55, 57, 59, 60 }) == BottomRow(grid));

    // Phrygian on the same root flattens the second, third, sixth and seventh.
    auto const phrygian = glass::WithPickedKey(grid, { -1, ScaleIndex(glass::MusicalScale::Phrygian) });

    VERIFY_IS_TRUE((std::vector<int32_t>{ 48, 49, 51, 53, 55, 56, 58, 60 }) == BottomRow(phrygian));

    // Lydian sharpens the fourth.
    auto const lydian = glass::WithPickedKey(grid, { -1, ScaleIndex(glass::MusicalScale::Lydian) });

    VERIFY_IS_TRUE((std::vector<int32_t>{ 48, 50, 52, 54, 55, 57, 59, 60 }) == BottomRow(lydian));

    // A new root moves the whole grid, and the first pad with it.
    auto const dPhrygian = glass::WithPickedKey(grid, { 2, ScaleIndex(glass::MusicalScale::Phrygian) });

    VERIFY_ARE_EQUAL(50, glass::FirstPadNote(dPhrygian));
    VERIFY_IS_TRUE((std::vector<int32_t>{ 50, 51, 53, 55, 57, 58, 60, 62 }) == BottomRow(dPhrygian));
}

void KeyPickerTests::PickersShowTheFirstGridsKeyUntilOneIsTouched()
{
    auto const document = DocumentWith({
        Picker(L"root", glass::SwitchPicks::RootNote),
        Grid(L"first", 2, glass::MusicalScale::Dorian),
        Grid(L"second", 5, glass::MusicalScale::Major) });

    auto const first = glass::FirstGridKey(document);

    VERIFY_ARE_EQUAL(2, first.Root);
    VERIFY_ARE_EQUAL(ScaleIndex(glass::MusicalScale::Dorian), first.Scale);

    // A root that has been picked shows, and the scale is still the grid's own.
    auto const shown = glass::ShownKey(document, { 7, -1 });

    VERIFY_ARE_EQUAL(7, shown.Root);
    VERIFY_ARE_EQUAL(ScaleIndex(glass::MusicalScale::Dorian), shown.Scale);

    // With no grid in a key there is nothing to show.
    auto const none = glass::FirstGridKey(DocumentWith({ Grid(L"plain", glass::NoKey, glass::MusicalScale::Major) }));

    VERIFY_ARE_EQUAL(-1, none.Root);
    VERIFY_ARE_EQUAL(-1, none.Scale);
}

// ------------------------------------------------------------------- the picker itself

void KeyPickerTests::EachPositionOfAPickerIsANoteOrAScale()
{
    auto const root = Picker(L"root", glass::SwitchPicks::RootNote);
    auto const scale = Picker(L"scale", glass::SwitchPicks::Scale);

    VERIFY_IS_TRUE(glass::IsKeyPicker(root));
    VERIFY_ARE_EQUAL(glass::RootPickerPositions, glass::SwitchPositionCount(root));
    VERIFY_ARE_EQUAL(7, glass::SwitchPositionCount(scale));

    VERIFY_ARE_EQUAL(3, glass::PickAt(root, 3).Root);
    VERIFY_ARE_EQUAL(-1, glass::PickAt(root, 3).Scale);
    VERIFY_ARE_EQUAL(ScaleIndex(glass::MusicalScale::Phrygian), glass::PickAt(scale, 3).Scale);
    VERIFY_ARE_EQUAL(-1, glass::PickAt(scale, 3).Root);

    // Off the end, or on a switch that sends, picks nothing.
    VERIFY_ARE_EQUAL(-1, glass::PickAt(root, 12).Root);
    VERIFY_ARE_EQUAL(-1, glass::PickAt(scale, -1).Scale);

    auto plain = root;
    plain.Switch.Picks = glass::SwitchPicks::Nothing;

    VERIFY_IS_FALSE(glass::IsKeyPicker(plain));
    VERIFY_ARE_EQUAL(-1, glass::PickAt(plain, 3).Root);

    // And back: the position that shows a key, or none when the picker does not offer it.
    glass::PickedKey const lydian{ 9, ScaleIndex(glass::MusicalScale::Lydian) };

    VERIFY_ARE_EQUAL(9, glass::PickerPosition(root, lydian));
    VERIFY_ARE_EQUAL(4, glass::PickerPosition(scale, lydian));
    VERIFY_ARE_EQUAL(glass::SwitchValueOf(9, 12), glass::PickerValue(root, lydian));

    glass::PickedKey const blues{ 0, ScaleIndex(glass::MusicalScale::Blues) };

    VERIFY_ARE_EQUAL(-1, glass::PickerPosition(scale, blues));
    VERIFY_ARE_EQUAL(-1.0, glass::PickerValue(scale, blues));

    // Short enough to print on a slice of the switch.
    VERIFY_ARE_EQUAL(std::wstring{ L"C" }, glass::RootPickerName(0));
    VERIFY_ARE_EQUAL(std::wstring{ L"E\u266D" }, glass::RootPickerName(3));
    VERIFY_ARE_EQUAL(std::wstring{ L"F\u266F" }, glass::RootPickerName(6));
    VERIFY_IS_TRUE(glass::RootPickerName(12).empty());
    VERIFY_ARE_EQUAL(std::wstring{ L"PadScalePhrygian" }, std::wstring{ glass::ScaleNameResourceKey(glass::MusicalScale::Phrygian) });
}

void KeyPickerTests::APickerSendsNothing()
{
    glass::Page page{};

    auto control = glass::MakeNewControl(glass::ControlKind::Switch, 0, 0, 1280, 800, L"Synth", page);

    VERIFY_ARE_EQUAL(size_t{ 3 }, control.Messages.size());

    control.Switch.Picks = glass::SwitchPicks::RootNote;

    glass::BindingEngine engine{};
    engine.Prepare(DocumentWith({ control }), OneDevice());

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    VERIFY_ARE_EQUAL(0u, engine.Evaluate(0, glass::MessageTrigger::Changes, 0.0, sends));
    VERIFY_ARE_EQUAL(0u, engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends));

    // Its rows wait in the file, and the same switch sends them again once it stops picking.
    control.Switch.Picks = glass::SwitchPicks::Nothing;
    engine.Prepare(DocumentWith({ control }), OneDevice());

    VERIFY_ARE_EQUAL(1u, engine.Evaluate(0, glass::MessageTrigger::Changes, 0.0, sends));
}

// -------------------------------------------------------------------------- the file

void KeyPickerTests::PickersSurviveARoundTrip()
{
    auto scale = Picker(L"scale", glass::SwitchPicks::Scale);
    scale.Switch.Scales = { glass::MusicalScale::Major, glass::MusicalScale::Dorian, glass::MusicalScale::Blues };

    auto const text = glass::WriteLayoutToJson(DocumentWith({
        Picker(L"root", glass::SwitchPicks::RootNote), scale, Grid(L"grid", 0, glass::MusicalScale::Major) }));

    VERIFY_IS_TRUE(text.find(L"\"picks\": \"rootNote\"") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"picks\": \"scale\"") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"blues\"") != std::wstring::npos);

    auto const reread = glass::ReadLayoutFromJson(text);
    VERIFY_IS_TRUE(reread.Succeeded);

    auto const* root = reread.Document.FindControl(L"root");
    auto const* back = reread.Document.FindControl(L"scale");

    VERIFY_IS_NOT_NULL(root);
    VERIFY_IS_NOT_NULL(back);

    VERIFY_IS_TRUE(root->Switch.Picks == glass::SwitchPicks::RootNote);
    VERIFY_IS_TRUE(root->Switch.Scales.empty());
    VERIFY_IS_TRUE(back->Switch.Picks == glass::SwitchPicks::Scale);
    VERIFY_IS_TRUE(back->Switch.Scales == scale.Switch.Scales);

    VERIFY_ARE_EQUAL(text, glass::WriteLayoutToJson(reread.Document));

    // A switch that sends is written exactly as it was before pickers existed.
    glass::Page page{};

    auto const plain = glass::MakeNewControl(glass::ControlKind::Switch, 0, 0, 1280, 800, L"Synth", page);
    auto const plainText = glass::WriteLayoutToJson(DocumentWith({ plain }));

    VERIFY_IS_TRUE(plainText.find(L"\"picks\"") == std::wstring::npos);
    VERIFY_IS_TRUE(plainText.find(L"\"scales\"") == std::wstring::npos);
}

void KeyPickerTests::PickersFromAFileAreBounded()
{
    auto const result = glass::ReadLayoutFromJson(
        LR"({ "fileVersion": 1, "name": "T", "pages": [ { "id": "p", "name": "P", "controls": [
            { "id": "a", "kind": "switch", "switch": { "positions": [], "picks": "tempo" } },
            { "id": "b", "kind": "switch", "switch": { "positions": [], "picks": "scale", "scales": [ "major", "sparkle", "major", 7 ] } },
            { "id": "c", "kind": "switch", "switch": { "positions": [], "picks": "scale", "scales": [ "lydian", "dorian", "lydian" ] } } ] } ] })");

    VERIFY_IS_TRUE(result.Succeeded);

    auto const& controls = result.Document.Pages[0].Controls;

    VERIFY_ARE_EQUAL(size_t{ 3 }, controls.size());

    // Something this build cannot pick is a switch that sends.
    VERIFY_IS_TRUE(controls[0].Switch.Picks == glass::SwitchPicks::Nothing);

    // One scale it knows is nothing to pick between, so it offers the usual seven.
    VERIFY_IS_TRUE(controls[1].Switch.Picks == glass::SwitchPicks::Scale);
    VERIFY_IS_TRUE(controls[1].Switch.Scales == glass::DefaultPickerScales());

    // Each scale once, in the file's own order.
    VERIFY_IS_TRUE((std::vector<glass::MusicalScale>{ glass::MusicalScale::Lydian, glass::MusicalScale::Dorian }) ==
        controls[2].Switch.Scales);
    VERIFY_ARE_EQUAL(2, glass::SwitchPositionCount(controls[2]));
}

// ------------------------------------------------------------------------ the editor

void KeyPickerTests::ASwitchThatStartsPickingGrowsToFitItsNames()
{
    glass::EditorController controller{};
    controller.Load(DocumentWith({}));

    auto const id = controller.AddControl(glass::ControlKind::Switch, 100, 100);

    VERIFY_IS_FALSE(id.empty());

    auto const switchOf = [&]() { return *controller.Document().FindControl(id); };
    auto const before = switchOf();
    auto const across = before.Width >= before.Height;

    auto const lengthOf = [across](glass::Control const& control) { return across ? control.Width : control.Height; };
    auto const thicknessOf = [across](glass::Control const& control) { return across ? control.Height : control.Width; };

    // Twelve notes, each with room for a letter and a flat.
    VERIFY_IS_TRUE(controller.SetSwitchPicks(id, glass::SwitchPicks::RootNote));

    auto const root = switchOf();

    VERIFY_IS_TRUE(root.Switch.Picks == glass::SwitchPicks::RootNote);
    VERIFY_ARE_EQUAL((std::max)(lengthOf(before), 12.0 * 30.0 + 16.0), lengthOf(root));
    VERIFY_ARE_EQUAL(thicknessOf(before), thicknessOf(root));

    // The same again is no edit.
    VERIFY_IS_FALSE(controller.SetSwitchPicks(id, glass::SwitchPicks::RootNote));

    // A scale picker starts with the seven modes.
    VERIFY_IS_TRUE(controller.SetSwitchPicks(id, glass::SwitchPicks::Scale));
    VERIFY_IS_TRUE(switchOf().Switch.Scales == glass::DefaultPickerScales());
    VERIFY_IS_TRUE(lengthOf(switchOf()) >= lengthOf(root));

    // Undo takes it back to the switch it was.
    VERIFY_IS_TRUE(controller.Undo());
    VERIFY_IS_TRUE(controller.Undo());
    VERIFY_IS_TRUE(switchOf().Switch.Picks == glass::SwitchPicks::Nothing);
    VERIFY_ARE_EQUAL(lengthOf(before), lengthOf(switchOf()));

    // Only a switch picks.
    auto const pads = controller.AddControl(glass::ControlKind::NotePads, 400, 400);

    VERIFY_IS_FALSE(controller.SetSwitchPicks(pads, glass::SwitchPicks::RootNote));
}

void KeyPickerTests::AScalePickerOffersTwoOrMoreScalesInOrder()
{
    glass::EditorController controller{};
    controller.Load(DocumentWith({}));

    auto const id = controller.AddControl(glass::ControlKind::Switch, 100, 100);

    VERIFY_IS_TRUE(controller.SetSwitchPicks(id, glass::SwitchPicks::Scale));

    // In the order the inspector lists them, each once.
    VERIFY_IS_TRUE(controller.SetSwitchScales(id,
        { glass::MusicalScale::Blues, glass::MusicalScale::Major, glass::MusicalScale::Blues }));

    auto const* control = controller.Document().FindControl(id);

    VERIFY_IS_TRUE((std::vector<glass::MusicalScale>{ glass::MusicalScale::Major, glass::MusicalScale::Blues }) ==
        control->Switch.Scales);
    VERIFY_ARE_EQUAL(2, glass::SwitchPositionCount(*control));

    // One scale is not a choice, and the same list again is no edit.
    VERIFY_IS_FALSE(controller.SetSwitchScales(id, { glass::MusicalScale::Dorian }));
    VERIFY_IS_FALSE(controller.SetSwitchScales(id, { glass::MusicalScale::Major, glass::MusicalScale::Blues }));
}

void KeyPickerTests::AddingPickersPutsOneOfEachBesideTheGrid()
{
    glass::EditorController controller{};
    controller.Load(DocumentWith({}));

    auto const grid = controller.AddControl(glass::ControlKind::NotePads, 300, 300);

    VERIFY_IS_FALSE(grid.empty());

    auto const pads = *controller.Document().FindControl(grid);
    auto const before = controller.Document().Pages[0].Controls.size();

    auto const added = controller.AddKeyPickers(grid, L"Root note", L"Scale");

    VERIFY_ARE_EQUAL(size_t{ 2 }, added.size());
    VERIFY_ARE_EQUAL(before + 2, controller.Document().Pages[0].Controls.size());

    auto const root = *controller.Document().FindControl(added[0]);
    auto const scale = *controller.Document().FindControl(added[1]);

    VERIFY_IS_TRUE(root.Switch.Picks == glass::SwitchPicks::RootNote);
    VERIFY_IS_TRUE(scale.Switch.Picks == glass::SwitchPicks::Scale);
    VERIFY_ARE_EQUAL(std::wstring{ L"Root note" }, root.Label);
    VERIFY_ARE_EQUAL(std::wstring{ L"Scale" }, scale.Label);
    VERIFY_IS_TRUE(root.Messages.empty());
    VERIFY_IS_TRUE(scale.Messages.empty());

    // The root across the top of the grid, the scale standing at its right, and neither on it.
    VERIFY_ARE_EQUAL(pads.X, root.X);
    VERIFY_IS_TRUE(root.Y + root.Height <= pads.Y);
    VERIFY_IS_TRUE(root.Width >= pads.Width);
    VERIFY_IS_TRUE(scale.X >= pads.X + pads.Width);
    VERIFY_ARE_EQUAL(pads.Y, scale.Y);
    VERIFY_IS_TRUE(scale.Height > scale.Width);

    // A layout has one of each, and one undo takes both away.
    VERIFY_IS_TRUE(controller.AddKeyPickers(grid, L"Root note", L"Scale").empty());
    VERIFY_IS_TRUE(controller.Undo());
    VERIFY_ARE_EQUAL(before, controller.Document().Pages[0].Controls.size());
}
