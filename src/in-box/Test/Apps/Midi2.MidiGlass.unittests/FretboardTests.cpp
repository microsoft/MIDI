// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The fretboard: tunings, which note each fret plays, where everything is drawn and what a
// finger lands on, one note per string with hammer-ons, slides and strums, what goes on the
// wire for each output, the buttons and pickers that change it, the file, and the editor.

#include "FretboardTests.h"

#include "Fretboard.h"
#include "FretboardChanges.h"
#include "StringVoices.h"
#include "BindingEngine.h"
#include "EditorController.h"
#include "LayoutModel.h"
#include "LayoutSerializer.h"
#include "PadGrid.h"
#include "PadGridChanges.h"

#include <algorithm>
#include <array>
#include <cmath>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    glass::LayoutDocument DocumentWith(_In_ std::vector<glass::Control> controls)
    {
        glass::LayoutDocument document{};

        document.Name = L"Frets";
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

    glass::PadGridTargets Only(_In_ std::wstring const& id)
    {
        return glass::PadGridTargets{ false, { id } };
    }

    glass::FretboardSpec Guitar()
    {
        return glass::NormalizedFretboard(glass::FretboardSpec{});
    }

    glass::Control Fretboard(_In_ std::wstring const& id, _In_ int32_t channel = 0, _In_ bool midi1 = false)
    {
        glass::Control control{};

        control.Id = id;
        control.Kind = glass::ControlKind::Fretboard;
        control.Label = id;
        control.Width = 760;
        control.Height = 300;
        control.Fretboard = Guitar();

        glass::ControlMessage row{};
        row.Trigger = glass::MessageTrigger::Changes;
        row.Kind = glass::MessageKind::Note;
        row.DeviceName = L"Synth";
        row.ChannelIndex = channel;
        row.UseMidi1Protocol = midi1;

        control.Messages.push_back(row);

        return control;
    }

    glass::Control PadGrid(_In_ std::wstring const& id)
    {
        glass::Control control{};

        control.Id = id;
        control.Kind = glass::ControlKind::NotePads;
        control.Width = 400;
        control.Height = 200;

        return control;
    }

    glass::Control Picker(_In_ std::wstring const& id, _In_ glass::SwitchPicks picks, _In_ glass::PadGridTargets const& targets)
    {
        glass::Control control{};

        control.Id = id;
        control.Kind = glass::ControlKind::Switch;
        control.Switch.Picks = picks;
        control.Switch.Targets = targets;

        if (picks == glass::SwitchPicks::Scale)
        {
            control.Switch.Scales = glass::DefaultPickerScales();
        }

        return control;
    }

    // What one call wrote, as a list.
    using Actions = std::vector<glass::StringAction>;

    template <typename TCall>
    Actions Run(_In_ TCall const& call)
    {
        std::array<glass::StringAction, glass::MaximumFretStrings> buffer{};

        auto const count = call(std::span<glass::StringAction>{ buffer.data(), buffer.size() });

        return Actions(buffer.begin(), buffer.begin() + count);
    }

    bool Is(_In_ glass::StringAction const& action, _In_ glass::StringActionKind kind, _In_ int32_t note)
    {
        return action.Kind == kind && action.Note == note;
    }

    uint32_t TypeOf(_In_ uint32_t word) noexcept { return word >> 28; }
    uint32_t StatusOf(_In_ uint32_t word) noexcept { return (word >> 20) & 0xF; }
    uint32_t ChannelOf(_In_ uint32_t word) noexcept { return (word >> 16) & 0xF; }
}

// ------------------------------------------------------------------ tunings and frets

void FretboardTests::TuningsPutTheLowStringAtTheBottom()
{
    auto const standard = glass::TuningStrings(glass::FretTuning::Standard);

    VERIFY_ARE_EQUAL(size_t{ 6 }, standard.size());
    VERIFY_ARE_EQUAL(64, standard.front().Note);
    VERIFY_ARE_EQUAL(40, standard.back().Note);

    // Every named tuning has strings and is known by its strings.
    for (int32_t tuning = 0; tuning < glass::FretTuningPresetCount; ++tuning)
    {
        auto const strings = glass::TuningStrings(static_cast<glass::FretTuning>(tuning));

        VERIFY_IS_FALSE(strings.empty());
        VERIFY_IS_TRUE(glass::MatchingTuning(strings) == static_cast<glass::FretTuning>(tuning));
    }

    VERIFY_ARE_EQUAL(size_t{ 4 }, glass::TuningStrings(glass::FretTuning::Bass).size());
    VERIFY_ARE_EQUAL(size_t{ 7 }, glass::TuningStrings(glass::FretTuning::SevenString).size());
    VERIFY_ARE_EQUAL(35, glass::TuningStrings(glass::FretTuning::SevenString).back().Note);
    VERIFY_IS_TRUE(glass::TuningStrings(glass::FretTuning::Custom).empty());

    // A ukulele's bottom string is tuned high.
    auto const ukulele = glass::TuningStrings(glass::FretTuning::Ukulele);

    VERIFY_ARE_EQUAL(67, ukulele.back().Note);
    VERIFY_IS_GREATER_THAN(ukulele.back().Note, ukulele[2].Note);

    // A new fretboard is a guitar in standard tuning.
    VERIFY_IS_TRUE(Guitar().Strings == standard);
    VERIFY_IS_TRUE(Guitar().Tuning == glass::FretTuning::Standard);
    VERIFY_IS_TRUE(Guitar().Strum == glass::StrumSide::Right);
}

void FretboardTests::EditingAStringMakesTheTuningCustom()
{
    auto strings = glass::TuningStrings(glass::FretTuning::Standard);

    strings.back().Note = 38;
    VERIFY_IS_TRUE(glass::MatchingTuning(strings) == glass::FretTuning::DropD);

    strings.front().Note = 63;
    VERIFY_IS_TRUE(glass::MatchingTuning(strings) == glass::FretTuning::Custom);

    // The name always follows the strings, whatever a file says.
    glass::FretboardSpec spec{};
    spec.Tuning = glass::FretTuning::Standard;
    spec.Strings = glass::TuningStrings(glass::FretTuning::OpenG);

    VERIFY_IS_TRUE(glass::NormalizedFretboard(spec).Tuning == glass::FretTuning::OpenG);

    // The same notes in another order are not the same tuning.
    auto reversed = glass::TuningStrings(glass::FretTuning::Standard);
    std::reverse(reversed.begin(), reversed.end());

    VERIFY_IS_TRUE(glass::MatchingTuning(reversed) == glass::FretTuning::Custom);
}

void FretboardTests::AFretCountsSemitonesFromTheOpenString()
{
    auto const spec = Guitar();

    VERIFY_ARE_EQUAL(40, glass::FretNote(spec, 5, 0));
    VERIFY_ARE_EQUAL(43, glass::FretNote(spec, 5, 3));
    VERIFY_ARE_EQUAL(76, glass::FretNote(spec, 0, 12));
    VERIFY_ARE_EQUAL(86, glass::FretNote(spec, 0, 22));

    // Past the last fret, on a string that isn't there, or off the top of the note range.
    VERIFY_ARE_EQUAL(-1, glass::FretNote(spec, 0, 23));
    VERIFY_ARE_EQUAL(-1, glass::FretNote(spec, 6, 0));
    VERIFY_ARE_EQUAL(-1, glass::FretNote(spec, -1, 0));

    auto high = spec;
    high.Strings[0].Note = 120;

    VERIFY_ARE_EQUAL(127, glass::FretNote(high, 0, 7));
    VERIFY_ARE_EQUAL(-1, glass::FretNote(high, 0, 8));
}

void FretboardTests::TheCapoMovesTheOpenStringAndSilencesTheFretsBehindIt()
{
    auto spec = Guitar();
    spec.Capo = 2;

    VERIFY_ARE_EQUAL(42, glass::FretNote(spec, 5, 0));
    VERIFY_ARE_EQUAL(-1, glass::FretNote(spec, 5, 1));
    VERIFY_ARE_EQUAL(42, glass::FretNote(spec, 5, 2));
    VERIFY_ARE_EQUAL(43, glass::FretNote(spec, 5, 3));
}

void FretboardTests::ABanjosShortStringStartsAtTheFifthFret()
{
    glass::FretboardSpec spec{};
    spec.Strings = glass::TuningStrings(glass::FretTuning::Banjo);
    spec = glass::NormalizedFretboard(spec);

    VERIFY_ARE_EQUAL(size_t{ 5 }, spec.Strings.size());
    VERIFY_ARE_EQUAL(67, spec.Strings[4].Note);
    VERIFY_ARE_EQUAL(5, spec.Strings[4].StartFret);

    VERIFY_ARE_EQUAL(67, glass::FretNote(spec, 4, 0));
    VERIFY_ARE_EQUAL(-1, glass::FretNote(spec, 4, 3));
    VERIFY_ARE_EQUAL(-1, glass::FretNote(spec, 4, 5));
    VERIFY_ARE_EQUAL(68, glass::FretNote(spec, 4, 6));

    // A capo below where it starts doesn't reach it; one above does.
    spec.Capo = 2;
    VERIFY_ARE_EQUAL(67, glass::FretNote(spec, 4, 0));
    VERIFY_ARE_EQUAL(64, glass::FretNote(spec, 0, 0));

    spec.Capo = 7;
    VERIFY_ARE_EQUAL(69, glass::FretNote(spec, 4, 0));
}

void FretboardTests::MutedOutOfKeyFretsPlayNothing()
{
    auto spec = Guitar();
    spec.KeyRoot = 4;
    spec.Scale = glass::MusicalScale::MinorPentatonic;
    spec.OutOfKey = glass::OutOfKeyFrets::Mute;

    VERIFY_IS_TRUE(glass::FretPlays(spec, 40));
    VERIFY_IS_FALSE(glass::FretPlays(spec, 41));
    VERIFY_IS_FALSE(glass::FretPlays(spec, -1));

    spec.OutOfKey = glass::OutOfKeyFrets::Dim;
    VERIFY_IS_TRUE(glass::FretPlays(spec, 41));

    spec.OutOfKey = glass::OutOfKeyFrets::Mute;
    spec.KeyRoot = glass::NoKey;
    VERIFY_IS_TRUE(glass::FretPlays(spec, 41));
}

void FretboardTests::AStrangersFileIsKeptInsideTheLimits()
{
    glass::FretboardSpec spec{};
    spec.Frets = 99;
    spec.FirstFret = 50;
    spec.LastFret = 0;
    spec.Capo = 40;
    spec.BendRangeSemitones = 0;
    spec.KeyRoot = 13;

    for (int32_t index = 0; index < 20; ++index)
    {
        glass::FretString string{};
        string.Note = 200;
        string.StartFret = 90;
        spec.Strings.push_back(string);
    }

    auto const kept = glass::NormalizedFretboard(spec);

    VERIFY_ARE_EQUAL(glass::MaximumFrets, kept.Frets);
    VERIFY_ARE_EQUAL(static_cast<size_t>(glass::MaximumFretStrings), kept.Strings.size());
    VERIFY_ARE_EQUAL(127, kept.Strings[0].Note);
    VERIFY_ARE_EQUAL(kept.Frets - 1, kept.Strings[0].StartFret);
    VERIFY_ARE_EQUAL(kept.Frets - 1, kept.FirstFret);
    VERIFY_IS_GREATER_THAN_OR_EQUAL(kept.LastFret, std::max(kept.FirstFret, 1));
    VERIFY_IS_LESS_THAN_OR_EQUAL(kept.LastFret, kept.Frets);
    VERIFY_ARE_EQUAL(kept.Frets - 1, kept.Capo);
    VERIFY_ARE_EQUAL(glass::MinimumFretBendSemitones, kept.BendRangeSemitones);
    VERIFY_ARE_EQUAL(glass::NoKey, kept.KeyRoot);
    VERIFY_IS_TRUE(kept.Tuning == glass::FretTuning::Custom);

    // No strings at all gets the strings the tuning names.
    glass::FretboardSpec bass{};
    bass.Tuning = glass::FretTuning::Bass;

    VERIFY_IS_TRUE(glass::NormalizedFretboard(bass).Strings == glass::TuningStrings(glass::FretTuning::Bass));
}

// ------------------------------------------------------------------ where things are

void FretboardTests::StringsRunTopToBottomWithTheStrumAreaOnTheRight()
{
    auto const layout = glass::LayOutFretboard(Guitar(), 760, 300);

    VERIFY_IS_FALSE(layout.Mirrored);
    VERIFY_IS_TRUE(layout.HasStrum);
    VERIFY_IS_TRUE(layout.HasStrip);
    VERIFY_ARE_EQUAL(760.0, layout.StrumRight);
    VERIFY_IS_GREATER_THAN(layout.StrumLeft, layout.NeckRight);
    VERIFY_ARE_EQUAL(0.0, layout.OpenLeft);
    VERIFY_ARE_EQUAL(layout.OpenRight, layout.NeckLeft);

    VERIFY_ARE_EQUAL(size_t{ 6 }, layout.StringY.size());

    for (size_t index = 1; index < layout.StringY.size(); ++index)
    {
        VERIFY_IS_GREATER_THAN(layout.StringY[index], layout.StringY[index - 1]);
    }

    // The low E, last in the list, is the string drawn at the bottom.
    VERIFY_IS_LESS_THAN(layout.StringY.back(), layout.StripTop);

    VERIFY_ARE_EQUAL(1, layout.FirstCell);
    VERIFY_ARE_EQUAL(12, layout.LastCell);
    VERIFY_ARE_EQUAL(size_t{ 13 }, layout.WireX.size());
    VERIFY_IS_LESS_THAN(std::abs(layout.WireX.front() - layout.NeckLeft), 0.001);
    VERIFY_IS_LESS_THAN(std::abs(layout.WireX.back() - layout.NeckRight), 0.001);

    // No strum area: the neck runs to the edge.
    auto none = Guitar();
    none.Strum = glass::StrumSide::None;

    auto const plain = glass::LayOutFretboard(none, 760, 300);

    VERIFY_IS_FALSE(plain.HasStrum);
    VERIFY_ARE_EQUAL(760.0, plain.NeckRight);

    // Too short for the strip, and the strings take the whole height.
    auto const short_ = glass::LayOutFretboard(Guitar(), 760, 60);

    VERIFY_IS_FALSE(short_.HasStrip);
    VERIFY_ARE_EQUAL(60.0, short_.StringsBottom);
}

void FretboardTests::AStrumAreaOnTheLeftTurnsTheNeckRound()
{
    auto spec = Guitar();
    spec.Strum = glass::StrumSide::Left;

    auto const layout = glass::LayOutFretboard(spec, 760, 300);

    VERIFY_IS_TRUE(layout.Mirrored);
    VERIFY_ARE_EQUAL(0.0, layout.StrumLeft);
    VERIFY_ARE_EQUAL(760.0, layout.OpenRight);
    VERIFY_IS_LESS_THAN(std::abs(layout.WireX.front() - layout.NeckRight), 0.001);

    for (size_t wire = 1; wire < layout.WireX.size(); ++wire)
    {
        VERIFY_IS_LESS_THAN(layout.WireX[wire], layout.WireX[wire - 1]);
    }

    // The strings stay where they were: only the neck turns round.
    auto const right = glass::LayOutFretboard(Guitar(), 760, 300);

    VERIFY_IS_TRUE(layout.StringY == right.StringY);

    auto const hit = glass::FretAtPoint(layout, glass::FretCenterX(layout, 3), layout.StringY[5]);

    VERIFY_IS_TRUE(hit.Region == glass::FretRegion::Neck);
    VERIFY_ARE_EQUAL(3, hit.Fret);
    VERIFY_ARE_EQUAL(5, hit.String);
}

void FretboardTests::FretsGetCloserUpTheNeck()
{
    auto const layout = glass::LayOutFretboard(Guitar(), 760, 300);

    for (size_t wire = 2; wire < layout.WireX.size(); ++wire)
    {
        VERIFY_IS_LESS_THAN(layout.WireX[wire] - layout.WireX[wire - 1], layout.WireX[wire - 1] - layout.WireX[wire - 2]);
    }

    // Each fret is a twelfth root of two shorter than the one before, as on a real neck.
    auto const first = layout.WireX[1] - layout.WireX[0];
    auto const second = layout.WireX[2] - layout.WireX[1];

    VERIFY_IS_LESS_THAN(std::abs(first / second - std::pow(2.0, 1.0 / 12.0)), 0.0001);
}

void FretboardTests::APointFindsItsStringAndFret()
{
    auto const layout = glass::LayOutFretboard(Guitar(), 760, 300);

    auto const fret = glass::FretAtPoint(layout, glass::FretCenterX(layout, 3), layout.StringY[5]);

    VERIFY_IS_TRUE(fret.Region == glass::FretRegion::Neck);
    VERIFY_ARE_EQUAL(5, fret.String);
    VERIFY_ARE_EQUAL(3, fret.Fret);
    VERIFY_IS_LESS_THAN(std::abs(fret.FretPosition - 3.0), 0.001);

    auto const open = glass::FretAtPoint(layout, (layout.OpenLeft + layout.OpenRight) / 2.0, layout.StringY[0]);

    VERIFY_IS_TRUE(open.Region == glass::FretRegion::Open);
    VERIFY_ARE_EQUAL(0, open.String);
    VERIFY_ARE_EQUAL(0, open.Fret);

    auto const strum = glass::FretAtPoint(layout, (layout.StrumLeft + layout.StrumRight) / 2.0, layout.StringY[2]);

    VERIFY_IS_TRUE(strum.Region == glass::FretRegion::Strum);
    VERIFY_ARE_EQUAL(2, strum.String);

    auto const strip = glass::FretAtPoint(layout, 200, layout.Height - 2);

    VERIFY_IS_TRUE(strip.Region == glass::FretRegion::Strip);

    // Between two strings, a point belongs to the nearer one: nothing falls through.
    auto const between = (layout.StringY[0] + layout.StringY[1]) / 2.0;

    VERIFY_ARE_EQUAL(0, glass::FretAtPoint(layout, 300, between - 1.0).String);
    VERIFY_ARE_EQUAL(1, glass::FretAtPoint(layout, 300, between + 1.0).String);

    // Off the control.
    VERIFY_IS_TRUE(glass::FretAtPoint(layout, -1, 10).Region == glass::FretRegion::None);
    VERIFY_IS_TRUE(glass::FretAtPoint(layout, 10, 400).Region == glass::FretRegion::None);
}

void FretboardTests::ASmoothSlideFollowsTheFingerBetweenFrets()
{
    auto const layout = glass::LayOutFretboard(Guitar(), 760, 300);

    auto const halfway = (glass::FretCenterX(layout, 3) + glass::FretCenterX(layout, 4)) / 2.0;

    VERIFY_IS_LESS_THAN(std::abs(glass::FretPositionAtX(layout, halfway) - 3.5), 0.001);
    VERIFY_IS_LESS_THAN(std::abs(glass::FretPositionAtX(layout, glass::FretCenterX(layout, 7)) - 7.0), 0.001);
    VERIFY_ARE_EQUAL(0.0, glass::FretPositionAtX(layout, 0.0));
    VERIFY_ARE_EQUAL(12.0, glass::FretPositionAtX(layout, layout.NeckRight + 5.0));

    VERIFY_ARE_EQUAL(0.0, glass::SlideBend(glass::FretSlide::Off, 5, 7, 7.2));
    VERIFY_ARE_EQUAL(2.0, glass::SlideBend(glass::FretSlide::Stepped, 5, 7, 7.2));
    VERIFY_IS_LESS_THAN(std::abs(glass::SlideBend(glass::FretSlide::Smooth, 5, 7, 7.2) - 2.2), 0.0001);
    VERIFY_ARE_EQUAL(-2.0, glass::SlideBend(glass::FretSlide::Stepped, 5, 3, 3.0));
}

void FretboardTests::AStrumCrossesStringsInTheOrderTheFingerMeetsThem()
{
    auto const layout = glass::LayOutFretboard(Guitar(), 760, 300);

    std::array<int32_t, glass::MaximumFretStrings> crossed{};

    auto count = glass::StringsCrossed(layout, 0.0, layout.StringsBottom, crossed);

    VERIFY_ARE_EQUAL(6u, count);

    for (uint32_t index = 0; index < count; ++index)
    {
        VERIFY_ARE_EQUAL(static_cast<int32_t>(index), crossed[index]);
    }

    count = glass::StringsCrossed(layout, layout.StringsBottom, 0.0, crossed);

    VERIFY_ARE_EQUAL(6u, count);
    VERIFY_ARE_EQUAL(5, crossed[0]);
    VERIFY_ARE_EQUAL(0, crossed[5]);

    // A short sweep crosses only what it passes, and resting on a string crosses nothing.
    count = glass::StringsCrossed(layout, layout.StringY[1] + 1.0, layout.StringY[3], crossed);

    VERIFY_ARE_EQUAL(2u, count);
    VERIFY_ARE_EQUAL(2, crossed[0]);
    VERIFY_ARE_EQUAL(3, crossed[1]);

    VERIFY_ARE_EQUAL(0u, glass::StringsCrossed(layout, 100.0, 100.0, crossed));
}

void FretboardTests::AFasterStrumIsLouder()
{
    auto const slow = glass::StrumVelocity(300.0, 600.0);
    auto const fast = glass::StrumVelocity(300.0, 60.0);

    VERIFY_IS_LESS_THAN(slow, fast);
    VERIFY_IS_LESS_THAN_OR_EQUAL(fast, 1.0);
    VERIFY_ARE_EQUAL(1.0, glass::StrumVelocity(1000.0, 10.0));
    VERIFY_ARE_EQUAL(0.3, glass::StrumVelocity(0.0, 10.0));

    // Which way the finger goes doesn't matter.
    VERIFY_ARE_EQUAL(slow, glass::StrumVelocity(-300.0, 600.0));

    // Two events in the same instant don't divide by zero.
    VERIFY_ARE_EQUAL(1.0, glass::StrumVelocity(50.0, 0.0));
}

void FretboardTests::PushingAcrossAStringBendsItUpEitherWay()
{
    constexpr double Pitch = 40.0;

    VERIFY_ARE_EQUAL(0.0, glass::CrossBend(2, 0.0, Pitch));
    VERIFY_ARE_EQUAL(0.0, glass::CrossBend(2, Pitch * glass::BendDeadZone, Pitch));
    VERIFY_ARE_EQUAL(2.0, glass::CrossBend(2, Pitch, Pitch));
    VERIFY_ARE_EQUAL(2.0, glass::CrossBend(2, -Pitch, Pitch));
    VERIFY_ARE_EQUAL(2.0, glass::CrossBend(2, Pitch * 3.0, Pitch));

    auto const half = glass::CrossBend(2, Pitch * 0.55, Pitch);

    VERIFY_IS_GREATER_THAN(half, 0.9);
    VERIFY_IS_LESS_THAN(half, 1.1);

    VERIFY_ARE_EQUAL(0.0, glass::CrossBend(2, 10.0, 0.0));
}

void FretboardTests::TheNeckStripMovesTheFretsShowingAndKeepsTheirCount()
{
    auto const spec = Guitar();
    auto const layout = glass::LayOutFretboard(spec, 760, 300);

    // The far end of the strip shows the last twelve frets.
    auto const end = glass::NeckWindowAt(spec, layout, layout.NeckRight);

    VERIFY_ARE_EQUAL(11, end);

    auto const moved = glass::WithNeckAt(spec, end);

    VERIFY_ARE_EQUAL(11, moved.FirstFret);
    VERIFY_ARE_EQUAL(22, moved.LastFret);

    auto const middle = glass::WithNeckAt(spec, 5);

    VERIFY_ARE_EQUAL(5, middle.FirstFret);
    VERIFY_ARE_EQUAL(16, middle.LastFret);

    // Frets 0 and 1 both start at the nut, so asking for 0 changes nothing.
    auto const nut = glass::WithNeckAt(spec, 0);

    VERIFY_ARE_EQUAL(0, nut.FirstFret);
    VERIFY_ARE_EQUAL(12, nut.LastFret);

    // The start of the strip is the nut, wherever the strum area is.
    VERIFY_ARE_EQUAL(1, glass::NeckWindowAt(spec, layout, 0.0));

    auto left = spec;
    left.Strum = glass::StrumSide::Left;

    auto const mirrored = glass::LayOutFretboard(left, 760, 300);

    VERIFY_ARE_EQUAL(1, glass::NeckWindowAt(left, mirrored, 760.0));
    VERIFY_ARE_EQUAL(11, glass::NeckWindowAt(left, mirrored, mirrored.NeckLeft));
}

// ------------------------------------------------------------------ one note per string

namespace
{
    glass::StringVoices VoicesFor(_In_ glass::FretboardSpec const& spec)
    {
        glass::StringVoices voices{};
        voices.Configure(spec);
        return voices;
    }

    glass::FretboardSpec WhileHeld()
    {
        auto spec = Guitar();
        spec.Sustain = glass::FretSustain::WhileHeld;
        return spec;
    }
}

void FretboardTests::ATapPlaysAndLiftingEndsItWhileHeld()
{
    auto voices = VoicesFor(WhileHeld());

    auto const press = Run([&](auto actions) { return voices.Press(1, 5, 3, 3.0, 0.8, actions); });

    // The bend goes first, so a channel left bent by the last note starts this one in tune.
    VERIFY_ARE_EQUAL(size_t{ 2 }, press.size());
    VERIFY_IS_TRUE(Is(press[0], glass::StringActionKind::Bend, 43));
    VERIFY_ARE_EQUAL(0.0, press[0].Value);
    VERIFY_IS_TRUE(Is(press[1], glass::StringActionKind::NoteOn, 43));
    VERIFY_ARE_EQUAL(0.8, press[1].Value);
    VERIFY_ARE_EQUAL(5, static_cast<int32_t>(press[1].String));
    VERIFY_IS_TRUE(voices.IsSounding(5));
    VERIFY_ARE_EQUAL(3, voices.HeldFret(5));

    // The same finger can't land twice.
    VERIFY_IS_TRUE(Run([&](auto actions) { return voices.Press(1, 4, 3, 3.0, 0.8, actions); }).empty());

    auto const release = Run([&](auto actions) { return voices.Release(1, actions); });

    VERIFY_ARE_EQUAL(size_t{ 1 }, release.size());
    VERIFY_IS_TRUE(Is(release[0], glass::StringActionKind::NoteOff, 43));
    VERIFY_IS_FALSE(voices.IsSounding(5));
    VERIFY_ARE_EQUAL(-1, voices.HeldFret(5));
    VERIFY_ARE_EQUAL(size_t{ 0 }, voices.FingerCount());
}

void FretboardTests::AStringRingsUntilItIsPlayedAgain()
{
    auto voices = VoicesFor(Guitar());

    Run([&](auto actions) { return voices.Press(1, 5, 3, 3.0, 1.0, actions); });

    VERIFY_IS_TRUE(Run([&](auto actions) { return voices.Release(1, actions); }).empty());
    VERIFY_IS_TRUE(voices.IsSounding(5));
    VERIFY_ARE_EQUAL(43, voices.SoundingNote(5));

    // Played again: the old note ends, then the new one starts.
    auto const again = Run([&](auto actions) { return voices.Press(2, 5, 5, 5.0, 1.0, actions); });

    VERIFY_ARE_EQUAL(size_t{ 3 }, again.size());
    VERIFY_IS_TRUE(Is(again[0], glass::StringActionKind::NoteOff, 43));
    VERIFY_IS_TRUE(Is(again[1], glass::StringActionKind::Bend, 45));
    VERIFY_IS_TRUE(Is(again[2], glass::StringActionKind::NoteOn, 45));

    // Another string rings on its own.
    Run([&](auto actions) { return voices.Press(3, 0, 0, 0.0, 1.0, actions); });
    Run([&](auto actions) { return voices.Release(3, actions); });

    VERIFY_IS_TRUE(voices.IsSounding(0));
    VERIFY_IS_TRUE(voices.IsSounding(5));
}

void FretboardTests::AHammerOnIsLegatoAndAPullOffGoesBack()
{
    auto voices = VoicesFor(WhileHeld());

    Run([&](auto actions) { return voices.Press(1, 5, 5, 5.0, 1.0, actions); });

    // The new note starts before the old one ends, so a mono instrument slurs.
    auto const hammer = Run([&](auto actions) { return voices.Press(2, 5, 7, 7.0, 0.6, actions); });

    VERIFY_ARE_EQUAL(size_t{ 3 }, hammer.size());
    VERIFY_IS_TRUE(Is(hammer[0], glass::StringActionKind::Bend, 47));
    VERIFY_IS_TRUE(Is(hammer[1], glass::StringActionKind::NoteOn, 47));
    VERIFY_IS_TRUE(Is(hammer[2], glass::StringActionKind::NoteOff, 45));

    auto const pull = Run([&](auto actions) { return voices.Release(2, actions); });

    VERIFY_ARE_EQUAL(size_t{ 3 }, pull.size());
    VERIFY_IS_TRUE(Is(pull[1], glass::StringActionKind::NoteOn, 45));
    VERIFY_IS_TRUE(Is(pull[2], glass::StringActionKind::NoteOff, 47));
    VERIFY_ARE_EQUAL(45, voices.SoundingNote(5));

    auto const last = Run([&](auto actions) { return voices.Release(1, actions); });

    VERIFY_ARE_EQUAL(size_t{ 1 }, last.size());
    VERIFY_IS_TRUE(Is(last[0], glass::StringActionKind::NoteOff, 45));
}

void FretboardTests::AFretBehindAHigherOneSoundsNothing()
{
    auto voices = VoicesFor(WhileHeld());

    Run([&](auto actions) { return voices.Press(1, 5, 7, 7.0, 1.0, actions); });

    VERIFY_IS_TRUE(Run([&](auto actions) { return voices.Press(2, 5, 5, 5.0, 1.0, actions); }).empty());
    VERIFY_ARE_EQUAL(47, voices.SoundingNote(5));
    VERIFY_ARE_EQUAL(7, voices.HeldFret(5));

    VERIFY_IS_TRUE(Run([&](auto actions) { return voices.Release(2, actions); }).empty());
    VERIFY_ARE_EQUAL(47, voices.SoundingNote(5));

    // A finger on another string is a note of its own.
    VERIFY_ARE_EQUAL(size_t{ 2 }, Run([&](auto actions) { return voices.Press(3, 4, 7, 7.0, 1.0, actions); }).size());
}

void FretboardTests::WithoutHammerOnsEveryChangeIsANewAttack()
{
    auto spec = WhileHeld();
    spec.HammerOns = false;

    auto voices = VoicesFor(spec);

    Run([&](auto actions) { return voices.Press(1, 5, 5, 5.0, 1.0, actions); });

    auto const higher = Run([&](auto actions) { return voices.Press(2, 5, 7, 7.0, 1.0, actions); });

    VERIFY_ARE_EQUAL(size_t{ 3 }, higher.size());
    VERIFY_IS_TRUE(Is(higher[0], glass::StringActionKind::NoteOff, 45));
    VERIFY_IS_TRUE(Is(higher[2], glass::StringActionKind::NoteOn, 47));

    auto const back = Run([&](auto actions) { return voices.Release(2, actions); });

    VERIFY_ARE_EQUAL(size_t{ 3 }, back.size());
    VERIFY_IS_TRUE(Is(back[0], glass::StringActionKind::NoteOff, 47));
    VERIFY_IS_TRUE(Is(back[2], glass::StringActionKind::NoteOn, 45));
}

void FretboardTests::ASteppedSlideBendsAndAnOffSlideReplays()
{
    auto voices = VoicesFor(WhileHeld());

    Run([&](auto actions) { return voices.Press(1, 5, 5, 5.0, 1.0, actions); });

    // Moving about on the same fret sends nothing.
    VERIFY_IS_TRUE(Run([&](auto actions) { return voices.Move(1, 5, 5.2, 0.0, actions); }).empty());

    auto const slid = Run([&](auto actions) { return voices.Move(1, 7, 7.0, 0.0, actions); });

    VERIFY_ARE_EQUAL(size_t{ 1 }, slid.size());
    VERIFY_IS_TRUE(Is(slid[0], glass::StringActionKind::Bend, 45));
    VERIFY_ARE_EQUAL(2.0, slid[0].Value);

    // Pushing across bends on top of the slide.
    auto const pushed = Run([&](auto actions) { return voices.Move(1, 7, 7.0, 0.5, actions); });

    VERIFY_ARE_EQUAL(size_t{ 1 }, pushed.size());
    VERIFY_ARE_EQUAL(2.5, pushed[0].Value);

    // Smooth follows the finger between frets.
    auto smoothSpec = WhileHeld();
    smoothSpec.Slide = glass::FretSlide::Smooth;

    auto smooth = VoicesFor(smoothSpec);

    Run([&](auto actions) { return smooth.Press(1, 5, 5, 5.0, 1.0, actions); });

    auto const glide = Run([&](auto actions) { return smooth.Move(1, 6, 6.5, 0.0, actions); });

    VERIFY_ARE_EQUAL(size_t{ 1 }, glide.size());
    VERIFY_ARE_EQUAL(1.5, glide[0].Value);

    // Off plays a new note at each fret.
    auto offSpec = WhileHeld();
    offSpec.Slide = glass::FretSlide::Off;

    auto off = VoicesFor(offSpec);

    Run([&](auto actions) { return off.Press(1, 5, 5, 5.0, 1.0, actions); });

    auto const replay = Run([&](auto actions) { return off.Move(1, 6, 6.0, 0.0, actions); });

    VERIFY_ARE_EQUAL(size_t{ 3 }, replay.size());
    VERIFY_IS_TRUE(Is(replay[0], glass::StringActionKind::NoteOff, 45));
    VERIFY_IS_TRUE(Is(replay[2], glass::StringActionKind::NoteOn, 46));
}

void FretboardTests::StrummingFretsSilentlyAndPlaysTheFrettedNotes()
{
    auto spec = WhileHeld();
    spec.Play = glass::FretPlayMode::Strum;

    auto voices = VoicesFor(spec);

    // An A string fretted at 2 is a B, and fretting it is silent.
    VERIFY_IS_TRUE(Run([&](auto actions) { return voices.Press(1, 4, 2, 2.0, 1.0, actions); }).empty());
    VERIFY_IS_FALSE(voices.IsSounding(4));

    auto const strummed = Run([&](auto actions) { return voices.Strum(4, 0.9, actions); });

    VERIFY_ARE_EQUAL(size_t{ 2 }, strummed.size());
    VERIFY_IS_TRUE(Is(strummed[1], glass::StringActionKind::NoteOn, 47));
    VERIFY_ARE_EQUAL(0.9, strummed[1].Value);

    // Nothing fretted: the open string.
    auto const open = Run([&](auto actions) { return voices.Strum(5, 0.9, actions); });

    VERIFY_IS_TRUE(Is(open[1], glass::StringActionKind::NoteOn, 40));

    // Strummed again, the ringing note ends first.
    auto const again = Run([&](auto actions) { return voices.Strum(4, 0.7, actions); });

    VERIFY_ARE_EQUAL(size_t{ 3 }, again.size());
    VERIFY_IS_TRUE(Is(again[0], glass::StringActionKind::NoteOff, 47));

    // Lifting the fretting finger ends its string, while held; the open string rings on.
    auto const lifted = Run([&](auto actions) { return voices.Release(1, actions); });

    VERIFY_ARE_EQUAL(size_t{ 1 }, lifted.size());
    VERIFY_IS_TRUE(Is(lifted[0], glass::StringActionKind::NoteOff, 47));
    VERIFY_IS_TRUE(voices.IsSounding(5));

    // With hammer-ons, fretting a string that is ringing changes its note without an attack.
    Run([&](auto actions) { return voices.Strum(3, 0.9, actions); });

    auto const hammered = Run([&](auto actions) { return voices.Press(2, 3, 2, 2.0, 1.0, actions); });

    VERIFY_ARE_EQUAL(size_t{ 3 }, hammered.size());
    VERIFY_IS_TRUE(Is(hammered[1], glass::StringActionKind::NoteOn, 52));
    VERIFY_IS_TRUE(Is(hammered[2], glass::StringActionKind::NoteOff, 50));
}

void FretboardTests::MuteStopsEveryString()
{
    auto voices = VoicesFor(Guitar());

    for (int32_t string = 0; string < 6; ++string)
    {
        Run([&](auto actions) { return voices.Strum(string, 1.0, actions); });
    }

    // A finger still down stays down.
    Run([&](auto actions) { return voices.Press(9, 2, 4, 4.0, 1.0, actions); });

    auto const muted = Run([&](auto actions) { return voices.MuteAll(actions); });

    VERIFY_ARE_EQUAL(size_t{ 6 }, muted.size());

    for (auto const& action : muted)
    {
        VERIFY_IS_TRUE(action.Kind == glass::StringActionKind::NoteOff);
    }

    for (int32_t string = 0; string < 6; ++string)
    {
        VERIFY_IS_FALSE(voices.IsSounding(string));
    }

    VERIFY_ARE_EQUAL(size_t{ 1 }, voices.FingerCount());
    VERIFY_IS_TRUE(Run([&](auto actions) { return voices.MuteAll(actions); }).empty());

    // Everything lifted and stopped, as when the layout closes.
    Run([&](auto actions) { return voices.Strum(0, 1.0, actions); });

    VERIFY_ARE_EQUAL(size_t{ 1 }, Run([&](auto actions) { return voices.ReleaseAll(actions); }).size());
    VERIFY_ARE_EQUAL(size_t{ 0 }, voices.FingerCount());
}

void FretboardTests::TwoStringsOnOneNoteShareItOnOneChannel()
{
    auto spec = WhileHeld();
    spec.Output = glass::FretOutput::PerNoteBend;

    auto voices = VoicesFor(spec);

    // The open high E, then the B string at the fifth fret: the same E.
    Run([&](auto actions) { return voices.Strum(0, 1.0, actions); });

    VERIFY_IS_TRUE(Run([&](auto actions) { return voices.Press(1, 1, 5, 5.0, 1.0, actions); }).empty());
    VERIFY_IS_TRUE(voices.IsSounding(1));

    // Lifting it leaves the note to the string still holding it.
    VERIFY_IS_TRUE(Run([&](auto actions) { return voices.Release(1, actions); }).empty());

    auto const muted = Run([&](auto actions) { return voices.MuteAll(actions); });

    VERIFY_ARE_EQUAL(size_t{ 1 }, muted.size());
    VERIFY_IS_TRUE(Is(muted[0], glass::StringActionKind::NoteOff, 64));

    // On channels of their own, the two are two notes.
    auto own = VoicesFor(WhileHeld());

    Run([&](auto actions) { return own.Strum(0, 1.0, actions); });

    VERIFY_ARE_EQUAL(size_t{ 2 }, Run([&](auto actions) { return own.Press(1, 1, 5, 5.0, 1.0, actions); }).size());
}

void FretboardTests::AMutedFretDampsTheString()
{
    auto spec = Guitar();
    spec.KeyRoot = 0;
    spec.Scale = glass::MusicalScale::Major;
    spec.OutOfKey = glass::OutOfKeyFrets::Mute;

    auto voices = VoicesFor(spec);

    // F sharp is not in C major.
    VERIFY_IS_TRUE(Run([&](auto actions) { return voices.Press(1, 5, 2, 2.0, 1.0, actions); }).empty());
    VERIFY_IS_FALSE(voices.IsSounding(5));

    Run([&](auto actions) { return voices.Release(1, actions); });
    Run([&](auto actions) { return voices.Strum(5, 1.0, actions); });

    VERIFY_IS_TRUE(voices.IsSounding(5));

    // Tapping a muted fret on a ringing string stops it, like a hand on the string.
    auto const damped = Run([&](auto actions) { return voices.Press(2, 5, 2, 2.0, 1.0, actions); });

    VERIFY_ARE_EQUAL(size_t{ 1 }, damped.size());
    VERIFY_IS_TRUE(Is(damped[0], glass::StringActionKind::NoteOff, 40));
    VERIFY_IS_FALSE(voices.IsSounding(5));
}

// ------------------------------------------------------------------ on the wire

namespace
{
    // What the engine builds for one fretboard request on a fretboard whose note row is on this
    // channel.
    std::vector<glass::PreparedSend> Wire(_In_ glass::StringSend const& request, _In_ int32_t channel, _In_ bool midi1 = false)
    {
        auto const document = DocumentWith({ Fretboard(L"frets", channel, midi1) });

        glass::BindingEngine engine{};
        engine.Prepare(document, OneDevice());

        std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

        auto const count = engine.EvaluateString(0, request, sends);

        return std::vector<glass::PreparedSend>(sends.begin(), sends.begin() + count);
    }

    glass::StringAction Action(_In_ glass::StringActionKind kind, _In_ uint8_t string, _In_ uint8_t note, _In_ double value)
    {
        return glass::StringAction{ kind, string, note, value };
    }
}

void FretboardTests::EachStringGetsAChannelOfItsOwn()
{
    auto const output = glass::FretOutput::ChannelPerString;

    // String 4 of a fretboard whose note row is on channel 3 (2 counted from 0) plays on 7.
    auto const on = Wire(glass::StringSendFor(output, Action(glass::StringActionKind::NoteOn, 4, 45, 1.0)), 2);

    VERIFY_ARE_EQUAL(size_t{ 1 }, on.size());
    VERIFY_ARE_EQUAL(4u, TypeOf(on[0].Words[0]));
    VERIFY_ARE_EQUAL(0x9u, StatusOf(on[0].Words[0]));
    VERIFY_ARE_EQUAL(6u, ChannelOf(on[0].Words[0]));
    VERIFY_ARE_EQUAL(45u, (on[0].Words[0] >> 8) & 0x7F);

    // Half the sensitivity up is three quarters of the way up the 32 bit range.
    auto const bend = Wire(glass::StringSendFor(output, Action(glass::StringActionKind::Bend, 4, 45, 12.0)), 2);

    VERIFY_ARE_EQUAL(0xEu, StatusOf(bend[0].Words[0]));
    VERIFY_ARE_EQUAL(6u, ChannelOf(bend[0].Words[0]));
    VERIFY_ARE_EQUAL(0xC0000000u, bend[0].Words[1]);

    // Before the first note, its channel is told how far a bend reaches: registered controller
    // 0, in semitones in the top seven bits.
    glass::StringSend setup{};

    VERIFY_IS_TRUE(glass::StringSetupFor(output, 4, setup));
    VERIFY_IS_TRUE(setup.Kind == glass::StringSendKind::BendSensitivity);

    auto const told = Wire(setup, 2);

    VERIFY_ARE_EQUAL(0x2u, StatusOf(told[0].Words[0]));
    VERIFY_ARE_EQUAL(6u, ChannelOf(told[0].Words[0]));
    VERIFY_ARE_EQUAL(0u, told[0].Words[0] & 0xFFFF);
    VERIFY_ARE_EQUAL(24u << 25, told[0].Words[1]);

    // Past channel 16 the strings come round again.
    auto const wrapped = Wire(glass::StringSendFor(output, Action(glass::StringActionKind::NoteOn, 15, 45, 1.0)), 2);

    VERIFY_ARE_EQUAL(1u, ChannelOf(wrapped[0].Words[0]));

    // A note off is a note off on the same channel.
    auto const off = Wire(glass::StringSendFor(output, Action(glass::StringActionKind::NoteOff, 4, 45, 0.0)), 2);

    VERIFY_ARE_EQUAL(0x8u, StatusOf(off[0].Words[0]));
    VERIFY_ARE_EQUAL(6u, ChannelOf(off[0].Words[0]));
}

void FretboardTests::MpeTellsOnlyTheManagerChannelTheSensitivity()
{
    auto const output = glass::FretOutput::Mpe;

    // The note row's channel is the manager; strings play on the member channels above it.
    VERIFY_ARE_EQUAL(1, static_cast<int32_t>(glass::StringChannelOffset(output, 0)));
    VERIFY_ARE_EQUAL(6, static_cast<int32_t>(glass::StringChannelOffset(output, 5)));

    auto const on = Wire(glass::StringSendFor(output, Action(glass::StringActionKind::NoteOn, 0, 64, 1.0)), 0);

    VERIFY_ARE_EQUAL(1u, ChannelOf(on[0].Words[0]));

    // Every string's setup is the same message, on the manager channel, at the profile's 48.
    for (int32_t string = 0; string < 6; ++string)
    {
        glass::StringSend setup{};

        VERIFY_IS_TRUE(glass::StringSetupFor(output, string, setup));
        VERIFY_ARE_EQUAL(0, static_cast<int32_t>(setup.ChannelOffset));
        VERIFY_ARE_EQUAL(glass::FretMpeBendSensitivity, setup.Value);
    }

    glass::StringSend setup{};
    glass::StringSetupFor(output, 3, setup);

    auto const told = Wire(setup, 0);

    VERIFY_ARE_EQUAL(0u, ChannelOf(told[0].Words[0]));
    VERIFY_ARE_EQUAL(48u << 25, told[0].Words[1]);

    // A bend is ordinary pitch bend on the member channel, out of 48.
    auto const bend = Wire(glass::StringSendFor(output, Action(glass::StringActionKind::Bend, 0, 64, 24.0)), 0);

    VERIFY_ARE_EQUAL(0xEu, StatusOf(bend[0].Words[0]));
    VERIFY_ARE_EQUAL(1u, ChannelOf(bend[0].Words[0]));
    VERIFY_ARE_EQUAL(0xC0000000u, bend[0].Words[1]);
}

void FretboardTests::PerNoteBendStaysOnOneChannel()
{
    auto const output = glass::FretOutput::PerNoteBend;

    for (int32_t string = 0; string < 12; ++string)
    {
        VERIFY_ARE_EQUAL(0, static_cast<int32_t>(glass::StringChannelOffset(output, string)));
    }

    auto const bend = Wire(glass::StringSendFor(output, Action(glass::StringActionKind::Bend, 3, 52, 0.0)), 5);

    VERIFY_ARE_EQUAL(0x6u, StatusOf(bend[0].Words[0]));
    VERIFY_ARE_EQUAL(5u, ChannelOf(bend[0].Words[0]));
    VERIFY_ARE_EQUAL(52u, (bend[0].Words[0] >> 8) & 0x7F);
    VERIFY_ARE_EQUAL(0x80000000u, bend[0].Words[1]);

    glass::StringSend setup{};

    VERIFY_IS_TRUE(glass::StringSetupFor(output, 3, setup));
    VERIFY_IS_TRUE(setup.Kind == glass::StringSendKind::PerNoteBendSensitivity);

    auto const told = Wire(setup, 5);

    VERIFY_ARE_EQUAL(0x2u, StatusOf(told[0].Words[0]));
    VERIFY_ARE_EQUAL(7u, told[0].Words[0] & 0x7F);

    // A MIDI 1.0 row has no per-note bend, so it sends nothing rather than something wrong.
    VERIFY_IS_TRUE(Wire(glass::StringSendFor(output, Action(glass::StringActionKind::Bend, 3, 52, 1.0)), 5, true).empty());
}

void FretboardTests::AMidi1RowBendsIn14Bits()
{
    auto const output = glass::FretOutput::ChannelPerString;

    auto const centered = Wire(glass::StringSendFor(output, Action(glass::StringActionKind::Bend, 1, 59, 0.0)), 0, true);

    VERIFY_ARE_EQUAL(size_t{ 1 }, centered.size());
    VERIFY_ARE_EQUAL(1u, centered[0].WordCount);
    VERIFY_ARE_EQUAL(2u, TypeOf(centered[0].Words[0]));
    VERIFY_ARE_EQUAL(0xEu, StatusOf(centered[0].Words[0]));
    VERIFY_ARE_EQUAL(1u, ChannelOf(centered[0].Words[0]));
    VERIFY_ARE_EQUAL(0u, (centered[0].Words[0] >> 8) & 0x7F);
    VERIFY_ARE_EQUAL(64u, centered[0].Words[0] & 0x7F);

    // All the way up is 16383.
    auto const top = Wire(glass::StringSendFor(output, Action(glass::StringActionKind::Bend, 1, 59, 30.0)), 0, true);

    VERIFY_ARE_EQUAL(0x7Fu, (top[0].Words[0] >> 8) & 0x7F);
    VERIFY_ARE_EQUAL(0x7Fu, top[0].Words[0] & 0x7F);

    // The sensitivity still goes as a MIDI 2.0 registered controller, which Windows turns into
    // the four control changes a MIDI 1.0 device expects.
    glass::StringSend setup{};
    glass::StringSetupFor(output, 1, setup);

    auto const told = Wire(setup, 0, true);

    VERIFY_ARE_EQUAL(4u, TypeOf(told[0].Words[0]));
    VERIFY_ARE_EQUAL(0x2u, StatusOf(told[0].Words[0]));
}

// ------------------------------------------------------------------ buttons, pickers, readouts and the file

void FretboardTests::CapoAndNeckButtonsChangeOnlyTheFretboardsTheyTarget()
{
    auto const document = DocumentWith({ Fretboard(L"a"), Fretboard(L"b"), PadGrid(L"grid") });

    glass::PadGridStates states{};

    VERIFY_IS_TRUE(glass::ApplyPadGridFunction(document, Only(L"a"), glass::PadGridFunction::CapoUp, states));
    VERIFY_ARE_EQUAL(1, glass::StateOf(states, L"a").Capo);
    VERIFY_IS_TRUE(states.find(L"b") == states.end());
    VERIFY_IS_TRUE(states.find(L"grid") == states.end());

    auto const* const a = document.FindControl(L"a");

    VERIFY_ARE_EQUAL(1, glass::WithFretState(a->Fretboard, glass::StateOf(states, L"a")).Capo);

    VERIFY_IS_TRUE(glass::ApplyPadGridFunction(document, Only(L"a"), glass::PadGridFunction::CapoDown, states));
    VERIFY_IS_FALSE(glass::ApplyPadGridFunction(document, Only(L"a"), glass::PadGridFunction::CapoDown, states));

    // At the nut there is nowhere further down to go.
    VERIFY_IS_FALSE(glass::ApplyPadGridFunction(document, Only(L"a"), glass::PadGridFunction::DownTheNeck, states));
    VERIFY_IS_TRUE(glass::ApplyPadGridFunction(document, Only(L"a"), glass::PadGridFunction::UpTheNeck, states));

    auto const moved = glass::WithFretState(a->Fretboard, glass::StateOf(states, L"a"));

    VERIFY_ARE_EQUAL(2, moved.FirstFret);
    VERIFY_ARE_EQUAL(13, moved.LastFret);

    // Dragging the neck strip lands in the same state.
    auto const dragged = glass::MovedNeckState(a->Fretboard, glass::PadGridState{}, 2);

    VERIFY_ARE_EQUAL(2, dragged.NeckFirst);

    // Mute changes nothing about where a fretboard is; the player stops its strings.
    VERIFY_IS_FALSE(glass::ApplyPadGridFunction(document, Only(L"a"), glass::PadGridFunction::MuteStrings, states));

    VERIFY_IS_TRUE(glass::ApplyPadGridFunction(document, Only(L"a"), glass::PadGridFunction::Reset, states));
    VERIFY_IS_TRUE(states.empty());

    // The capo stops short of the last fret.
    glass::PadGridState high{};
    high.Capo = 21;

    VERIFY_IS_TRUE(glass::ChangedFretState(a->Fretboard, high, glass::PadGridFunction::CapoUp) == high);
}

void FretboardTests::PadButtonsLeaveFretboardsAlone()
{
    auto keyed = Fretboard(L"keyed");
    keyed.Fretboard.KeyRoot = 4;
    keyed.Fretboard.Scale = glass::MusicalScale::MinorPentatonic;

    auto const document = DocumentWith({ keyed, Fretboard(L"plain"), PadGrid(L"grid") });

    glass::PadGridStates states{};

    VERIFY_IS_TRUE(glass::ApplyPadGridFunction(document, {}, glass::PadGridFunction::OctaveUp, states));
    VERIFY_IS_TRUE(states.find(L"grid") != states.end());
    VERIFY_IS_TRUE(states.find(L"keyed") == states.end());
    VERIFY_IS_TRUE(states.find(L"plain") == states.end());

    // A capo button aimed at every grid leaves the pads alone.
    states.clear();

    VERIFY_IS_TRUE(glass::ApplyPadGridFunction(document, {}, glass::PadGridFunction::CapoUp, states));
    VERIFY_IS_TRUE(states.find(L"grid") == states.end());
    VERIFY_ARE_EQUAL(1, glass::StateOf(states, L"plain").Capo);

    // The next root moves the dots of a fretboard that shows a key, and only that one.
    states.clear();

    glass::ApplyPadGridFunction(document, {}, glass::PadGridFunction::NextRoot, states);

    VERIFY_ARE_EQUAL(5, glass::StateOf(states, L"keyed").Root);
    VERIFY_IS_TRUE(states.find(L"plain") == states.end());
    VERIFY_ARE_EQUAL(5, glass::WithFretState(keyed.Fretboard, glass::StateOf(states, L"keyed")).KeyRoot);
}

void FretboardTests::ATuningPickerSetsAndShowsTheTuning()
{
    auto const picker = Picker(L"tuning", glass::SwitchPicks::Tuning, Only(L"a"));
    auto const document = DocumentWith({ picker, Fretboard(L"a"), Fretboard(L"b") });

    VERIFY_ARE_EQUAL(glass::FretTuningPresetCount, glass::SwitchPositionCount(picker));
    VERIFY_IS_TRUE(glass::IsKeyPicker(picker));

    glass::PadGridStates states{};

    VERIFY_ARE_EQUAL(0, glass::PickerPositionFor(document, picker, states));

    VERIFY_IS_TRUE(glass::ApplyPick(document, picker, 1, states));
    VERIFY_IS_TRUE(states.find(L"b") == states.end());

    auto const* const a = document.FindControl(L"a");
    auto const tuned = glass::WithFretState(a->Fretboard, glass::StateOf(states, L"a"));

    VERIFY_IS_TRUE(tuned.Strings == glass::TuningStrings(glass::FretTuning::DropD));
    VERIFY_ARE_EQUAL(1, glass::PickerPositionFor(document, picker, states));

    // A bass tuning brings its own number of strings.
    glass::ApplyPick(document, picker, static_cast<int32_t>(glass::FretTuning::Bass), states);

    VERIFY_ARE_EQUAL(size_t{ 4 }, glass::WithFretState(a->Fretboard, glass::StateOf(states, L"a")).Strings.size());

    // A fretboard in a tuning of its own lights no position.
    auto custom = Fretboard(L"custom");
    custom.Fretboard.Strings[0].Note = 63;
    custom.Fretboard = glass::NormalizedFretboard(custom.Fretboard);

    auto const customPicker = Picker(L"tuning", glass::SwitchPicks::Tuning, Only(L"custom"));

    VERIFY_ARE_EQUAL(-1, glass::PickerPositionFor(DocumentWith({ customPicker, custom }), customPicker, {}));

    // A root picker aimed only at a fretboard shows and moves its dots.
    auto keyed = Fretboard(L"keyed");
    keyed.Fretboard.KeyRoot = 2;

    auto const root = Picker(L"root", glass::SwitchPicks::RootNote, Only(L"keyed"));
    auto const keyedDocument = DocumentWith({ root, keyed });

    glass::PadGridStates keyedStates{};

    VERIFY_ARE_EQUAL(2, glass::PickerPositionFor(keyedDocument, root, keyedStates));
    VERIFY_IS_TRUE(glass::ApplyPick(keyedDocument, root, 7, keyedStates));
    VERIFY_ARE_EQUAL(7, glass::PickerPositionFor(keyedDocument, root, keyedStates));
}

void FretboardTests::ReadoutsShowTheCapoAndTheTuning()
{
    glass::Control readout{};
    readout.Id = L"readout";
    readout.Kind = glass::ControlKind::Readout;
    readout.Readout.Source = glass::ReadoutSource::Capo;
    readout.Readout.Targets = Only(L"a");

    VERIFY_IS_TRUE(glass::ReadsAPadGrid(readout));

    auto const document = DocumentWith({ readout, Fretboard(L"a") });

    glass::PadGridStates states{};

    auto reading = glass::ReadFretboard(document, Only(L"a"), states);

    VERIFY_IS_TRUE(reading.HasFretboard);
    VERIFY_ARE_EQUAL(0, reading.Capo);
    VERIFY_IS_TRUE(reading.Tuning == glass::FretTuning::Standard);

    glass::ApplyPadGridFunction(document, Only(L"a"), glass::PadGridFunction::CapoUp, states);

    VERIFY_ARE_EQUAL(1, glass::ReadFretboard(document, Only(L"a"), states).Capo);

    // Aimed at nothing, it reads nothing.
    VERIFY_IS_FALSE(glass::ReadFretboard(document, glass::PadGridTargets{ false, {} }, states).HasFretboard);
}

void FretboardTests::AFretboardSurvivesARoundTrip()
{
    auto control = Fretboard(L"banjo");

    control.Fretboard.Strings = glass::TuningStrings(glass::FretTuning::Banjo);
    control.Fretboard.Tuning = glass::FretTuning::Banjo;
    control.Fretboard.Capo = 3;
    control.Fretboard.FirstFret = 5;
    control.Fretboard.LastFret = 9;
    control.Fretboard.Strum = glass::StrumSide::Left;
    control.Fretboard.Play = glass::FretPlayMode::Strum;
    control.Fretboard.Sustain = glass::FretSustain::WhileHeld;
    control.Fretboard.KeyRoot = 4;
    control.Fretboard.Scale = glass::MusicalScale::MinorPentatonic;
    control.Fretboard.OutOfKey = glass::OutOfKeyFrets::Dim;
    control.Fretboard.BendRangeSemitones = 3;
    control.Fretboard.Slide = glass::FretSlide::Smooth;
    control.Fretboard.HammerOns = false;
    control.Fretboard.Output = glass::FretOutput::Mpe;

    auto picker = Picker(L"tuning", glass::SwitchPicks::Tuning, Only(L"banjo"));

    glass::Control readout{};
    readout.Id = L"readout";
    readout.Kind = glass::ControlKind::Readout;
    readout.Readout.Source = glass::ReadoutSource::Tuning;

    glass::Control button{};
    button.Id = L"mute";
    button.Kind = glass::ControlKind::Button;

    glass::ControlMessage row{};
    row.Trigger = glass::MessageTrigger::TurnsOn;
    row.Kind = glass::MessageKind::PadGrid;
    row.PadFunction = glass::PadGridFunction::MuteStrings;
    row.PadTargets = Only(L"banjo");
    button.Messages.push_back(row);

    auto const text = glass::WriteLayoutToJson(DocumentWith({ control, picker, readout, button }));

    VERIFY_IS_TRUE(text.find(L"\"kind\": \"fretboard\"") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"tuning\": \"banjo\"") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"startFret\": 5") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"strumSide\": \"left\"") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"output\": \"mpe\"") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"picks\": \"tuning\"") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"source\": \"tuning\"") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"function\": \"muteStrings\"") != std::wstring::npos);

    auto const reread = glass::ReadLayoutFromJson(text);

    VERIFY_IS_TRUE(reread.Succeeded);

    auto const& back = reread.Document.FindControl(L"banjo")->Fretboard;

    VERIFY_IS_TRUE(back.Strings == control.Fretboard.Strings);
    VERIFY_IS_TRUE(back.Tuning == glass::FretTuning::Banjo);
    VERIFY_ARE_EQUAL(3, back.Capo);
    VERIFY_ARE_EQUAL(5, back.FirstFret);
    VERIFY_ARE_EQUAL(9, back.LastFret);
    VERIFY_IS_TRUE(back.Strum == glass::StrumSide::Left);
    VERIFY_IS_TRUE(back.Play == glass::FretPlayMode::Strum);
    VERIFY_IS_TRUE(back.Sustain == glass::FretSustain::WhileHeld);
    VERIFY_ARE_EQUAL(4, back.KeyRoot);
    VERIFY_IS_TRUE(back.Scale == glass::MusicalScale::MinorPentatonic);
    VERIFY_IS_TRUE(back.OutOfKey == glass::OutOfKeyFrets::Dim);
    VERIFY_ARE_EQUAL(3, back.BendRangeSemitones);
    VERIFY_IS_TRUE(back.Slide == glass::FretSlide::Smooth);
    VERIFY_IS_FALSE(back.HammerOns);
    VERIFY_IS_TRUE(back.Output == glass::FretOutput::Mpe);

    VERIFY_ARE_EQUAL(text, glass::WriteLayoutToJson(reread.Document));

    // A fretboard a file wrote with no strings, and one string a newer build added a field to.
    auto const sparse = glass::ReadLayoutFromJson(
        LR"({ "fileVersion": 1, "name": "T", "pages": [ { "id": "p", "name": "P", "controls": [
            { "id": "a", "kind": "fretboard", "messages": [] },
            { "id": "b", "kind": "fretboard", "fretboard": { "strings": [ { "note": 50, "gauge": 10 } ], "frets": 500 }, "messages": [] } ] } ] })");

    VERIFY_IS_TRUE(sparse.Succeeded);

    auto const& a = sparse.Document.FindControl(L"a")->Fretboard;
    auto const& b = sparse.Document.FindControl(L"b")->Fretboard;

    VERIFY_IS_TRUE(a.Strings == glass::TuningStrings(glass::FretTuning::Standard));
    VERIFY_ARE_EQUAL(size_t{ 1 }, b.Strings.size());
    VERIFY_ARE_EQUAL(22, b.Frets);
    VERIFY_IS_TRUE(b.Tuning == glass::FretTuning::Custom);
    VERIFY_IS_TRUE(glass::WriteLayoutToJson(sparse.Document).find(L"gauge") != std::wstring::npos);
}

// ------------------------------------------------------------------ the editor

void FretboardTests::ANewFretboardIsAGuitarInStandardTuning()
{
    glass::EditorController controller{};
    controller.Load(DocumentWith({}));

    auto const id = controller.AddControl(glass::ControlKind::Fretboard, 100, 100);

    VERIFY_IS_FALSE(id.empty());

    auto const* const control = controller.Document().FindControl(id);

    VERIFY_IS_TRUE(control->Kind == glass::ControlKind::Fretboard);
    VERIFY_IS_TRUE(control->Fretboard.Strings == glass::TuningStrings(glass::FretTuning::Standard));
    VERIFY_ARE_EQUAL(40, control->Fretboard.Strings.back().Note);
    VERIFY_IS_TRUE(control->Fretboard.Strum == glass::StrumSide::Right);
    VERIFY_ARE_EQUAL(size_t{ 1 }, control->Messages.size());
    VERIFY_IS_TRUE(control->Messages[0].Kind == glass::MessageKind::Note);

    // Turning another control into one gives it strings and a note row too.
    auto const knob = controller.AddControl(glass::ControlKind::Knob, 100, 500);

    VERIFY_IS_TRUE(controller.SetControlKind(knob, glass::ControlKind::Fretboard));

    auto const* const turned = controller.Document().FindControl(knob);

    VERIFY_ARE_EQUAL(size_t{ 6 }, turned->Fretboard.Strings.size());
    VERIFY_IS_TRUE(std::any_of(turned->Messages.begin(), turned->Messages.end(),
        [](glass::ControlMessage const& message) { return message.Kind == glass::MessageKind::Note; }));
}

void FretboardTests::TheEditorKeepsAFretboardInsideItsLimits()
{
    glass::EditorController controller{};
    controller.Load(DocumentWith({}));

    auto const id = controller.AddControl(glass::ControlKind::Fretboard, 100, 100);

    auto wanted = controller.Document().FindControl(id)->Fretboard;
    wanted.Frets = 99;
    wanted.Capo = 50;

    VERIFY_IS_TRUE(controller.SetControlFretboard(id, wanted));

    auto const& kept = controller.Document().FindControl(id)->Fretboard;

    VERIFY_ARE_EQUAL(glass::MaximumFrets, kept.Frets);
    VERIFY_ARE_EQUAL(glass::MaximumFrets - 1, kept.Capo);

    // The same again is no edit.
    VERIFY_IS_FALSE(controller.SetControlFretboard(id, kept));

    // A fretboard never ends up with no strings.
    auto empty = kept;
    empty.Strings.clear();

    VERIFY_IS_FALSE(controller.SetControlFretboard(id, empty));

    // Reordering the strings is an edit, and the name follows the notes.
    auto reordered = kept;
    std::reverse(reordered.Strings.begin(), reordered.Strings.end());

    VERIFY_IS_TRUE(controller.SetControlFretboard(id, reordered));
    VERIFY_ARE_EQUAL(40, controller.Document().FindControl(id)->Fretboard.Strings.front().Note);
    VERIFY_IS_TRUE(controller.Document().FindControl(id)->Fretboard.Tuning == glass::FretTuning::Custom);

    // Not a fretboard: nothing to set.
    auto const knob = controller.AddControl(glass::ControlKind::Knob, 100, 500);

    VERIFY_IS_FALSE(controller.SetControlFretboard(knob, wanted));
}

void FretboardTests::AddingFretboardButtonsWiresThemToTheFretboard()
{
    glass::EditorController controller{};
    controller.Load(DocumentWith({}));

    auto const id = controller.AddControl(glass::ControlKind::Fretboard, 100, 100);
    auto const before = controller.Document().Pages[0].Controls.size();

    glass::EditorController::FretboardButtonLabels labels{};
    labels.Tuning = L"Tuning";
    labels.CapoDown = L"Capo down";
    labels.Capo = L"Capo";
    labels.CapoUp = L"Capo up";
    labels.DownTheNeck = L"Down the neck";
    labels.UpTheNeck = L"Up the neck";
    labels.Mute = L"Mute";

    auto const added = controller.AddFretboardButtons(id, labels);

    VERIFY_ARE_EQUAL(size_t{ 7 }, added.size());
    VERIFY_ARE_EQUAL(before + 7, controller.Document().Pages[0].Controls.size());

    auto const& document = controller.Document();
    auto const* const tuning = document.FindControl(added[0]);
    auto const* const down = document.FindControl(added[1]);
    auto const* const capo = document.FindControl(added[2]);
    auto const* const mute = document.FindControl(added[6]);
    auto const* const board = document.FindControl(id);

    VERIFY_IS_TRUE(tuning->Switch.Picks == glass::SwitchPicks::Tuning);
    VERIFY_IS_TRUE(tuning->Switch.Targets == Only(id));
    VERIFY_IS_TRUE(tuning->Messages.empty());
    VERIFY_ARE_EQUAL(std::wstring{ L"Tuning" }, tuning->Label);

    VERIFY_ARE_EQUAL(size_t{ 1 }, down->Messages.size());
    VERIFY_IS_TRUE(down->Messages[0].PadFunction == glass::PadGridFunction::CapoDown);
    VERIFY_IS_TRUE(down->Messages[0].PadTargets == Only(id));

    VERIFY_IS_TRUE(capo->Readout.Source == glass::ReadoutSource::Capo);
    VERIFY_IS_TRUE(capo->Readout.Targets == Only(id));
    VERIFY_IS_TRUE(mute->Messages[0].PadFunction == glass::PadGridFunction::MuteStrings);

    // The picker beside it, the row under it, and everything on the page.
    VERIFY_IS_GREATER_THAN_OR_EQUAL(tuning->X, board->X + board->Width);
    VERIFY_IS_GREATER_THAN(down->Y, board->Y + board->Height);

    for (auto const& added_id : added)
    {
        auto const* const control = document.FindControl(added_id);

        VERIFY_IS_LESS_THAN_OR_EQUAL(control->X + control->Width, static_cast<double>(document.PageWidth));
        VERIFY_IS_LESS_THAN_OR_EQUAL(control->Y + control->Height, static_cast<double>(document.PageHeight));
    }

    // Not a fretboard: nothing is added.
    auto const knob = controller.AddControl(glass::ControlKind::Knob, 900, 600);

    VERIFY_IS_TRUE(controller.AddFretboardButtons(knob, labels).empty());
}
