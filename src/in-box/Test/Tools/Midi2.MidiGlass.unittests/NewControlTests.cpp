// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The controls added after the first pass: the two axis field, the ribbon, the piano keyboard
// and the clock generator, plus the stops any continuous control can have and what a meter or
// a lamp listens for.

#include "NewControlTests.h"

#include "InputRules.h"
#include "BindingEngine.h"
#include "LayoutModel.h"
#include "LayoutSerializer.h"
#include "ControlFactory.h"
#include "StepPattern.h"

#include <array>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    glass::LayoutDocument OneControl(_In_ glass::Control control)
    {
        glass::LayoutDocument document{};

        glass::DeviceEntry device{};
        device.Name = L"Synth";

        document.Devices.push_back(device);

        glass::Page page{};
        page.Id = L"page";
        page.Controls.push_back(std::move(control));

        document.Pages.push_back(std::move(page));

        return document;
    }

    std::vector<glass::PreparedDestination> OneDevice(_In_ bool available = true)
    {
        glass::PreparedDestination destination{};

        destination.Name = L"Synth";
        destination.IsAvailable = available;

        return { destination };
    }

    glass::ControlMessage ControlChangeOn(
        _In_ uint32_t number,
        _In_ glass::ValueAxis axis)
    {
        glass::ControlMessage message{};

        message.Trigger = glass::MessageTrigger::Changes;
        message.Kind = glass::MessageKind::ControlChange;
        message.DeviceName = L"Synth";
        message.Number = number;
        message.Axis = axis;

        return message;
    }

    glass::KeyboardSpec TwoOctavesFromC3()
    {
        glass::KeyboardSpec keyboard{};

        keyboard.KeyCount = 25;
        keyboard.LowestNote = 48;

        return keyboard;
    }
}

// ---------------------------------------------------------------------- two axis

void NewControlTests::ATwoAxisControlSetsAPositionOutright()
{
    VERIFY_IS_TRUE(glass::UsesAbsolutePosition(glass::ControlKind::XYPad));
    VERIFY_IS_TRUE(glass::UsesAbsolutePosition(glass::ControlKind::Joystick));
    VERIFY_IS_TRUE(glass::UsesAbsolutePosition(glass::ControlKind::Ribbon));

    // A knob still has no travel under the finger.
    VERIFY_IS_FALSE(glass::UsesAbsolutePosition(glass::ControlKind::Knob));
}

void NewControlTests::TheSecondAxisReadsBottomToTop()
{
    // Screen coordinates run downward and a joystick does not. This is the one piece of the
    // arithmetic that is easy to get backwards.
    VERIFY_ARE_EQUAL(1.0, glass::PositionToValueY(200.0, 0.0));
    VERIFY_ARE_EQUAL(0.0, glass::PositionToValueY(200.0, 200.0));
    VERIFY_ARE_EQUAL(0.5, glass::PositionToValueY(200.0, 100.0));
}

void NewControlTests::AJoystickTakesTwoAxesAndARibbonDoesNot()
{
    VERIFY_IS_TRUE(glass::UsesTwoAxes(glass::ControlKind::XYPad));
    VERIFY_IS_TRUE(glass::UsesTwoAxes(glass::ControlKind::Joystick));

    // A ribbon is a fader with no cap, not a pad.
    VERIFY_IS_FALSE(glass::UsesTwoAxes(glass::ControlKind::Ribbon));
    VERIFY_IS_FALSE(glass::UsesTwoAxes(glass::ControlKind::Fader));
}

void NewControlTests::OnlyTheMessagesOnThatAxisAreSent()
{
    glass::Control control{};

    control.Id = L"pad";
    control.Kind = glass::ControlKind::XYPad;
    control.Messages.push_back(ControlChangeOn(20, glass::ValueAxis::X));
    control.Messages.push_back(ControlChangeOn(21, glass::ValueAxis::Y));

    glass::BindingEngine engine{};
    engine.Prepare(OneControl(control), OneDevice());

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    auto written = engine.EvaluateAxis(
        0, glass::MessageTrigger::Changes, 1.0, glass::ValueAxis::X, sends);

    VERIFY_ARE_EQUAL(1u, written);
    VERIFY_ARE_EQUAL(20u, (sends[0].Words[0] >> 8) & 0x7F);

    written = engine.EvaluateAxis(
        0, glass::MessageTrigger::Changes, 1.0, glass::ValueAxis::Y, sends);

    VERIFY_ARE_EQUAL(1u, written);
    VERIFY_ARE_EQUAL(21u, (sends[0].Words[0] >> 8) & 0x7F);
}

void NewControlTests::AnAxisWithNoMessagesSendsNothing()
{
    glass::Control control{};

    control.Id = L"pad";
    control.Kind = glass::ControlKind::XYPad;
    control.Messages.push_back(ControlChangeOn(20, glass::ValueAxis::X));

    glass::BindingEngine engine{};
    engine.Prepare(OneControl(control), OneDevice());

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    VERIFY_ARE_EQUAL(0u, engine.EvaluateAxis(
        0, glass::MessageTrigger::Changes, 1.0, glass::ValueAxis::Y, sends));

    // And the plain Evaluate still drives the first axis, so every control that only ever had
    // one value behaves exactly as it did.
    VERIFY_ARE_EQUAL(1u, engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends));
}

// ------------------------------------------------------------------------- keys

void NewControlTests::AKeyboardOfTwentyFiveHasFifteenWhiteKeys()
{
    auto const keyboard = TwoOctavesFromC3();

    // 25 keys from C is two full octaves plus the top C: fifteen white, ten black. The white
    // count is what the drawing divides the width by, so it cannot be a simple fraction.
    auto const lowest = glass::KeyAtPosition(keyboard, 150.0, 60.0, 1.0, 55.0);
    auto const highest = glass::KeyAtPosition(keyboard, 150.0, 60.0, 149.0, 55.0);

    VERIFY_ARE_EQUAL(0, lowest);
    VERIFY_ARE_EQUAL(24, highest);
}

void NewControlTests::ATouchOnTheLeftEdgeIsTheLowestKey()
{
    auto const keyboard = TwoOctavesFromC3();

    VERIFY_ARE_EQUAL(0, glass::KeyAtPosition(keyboard, 150.0, 60.0, 0.0, 59.0));
}

void NewControlTests::ABlackKeyWinsOverTheWhiteOneBehindIt()
{
    auto const keyboard = TwoOctavesFromC3();

    // Fifteen white keys across 150 px is 10 px each, and C sharp sits on the boundary between
    // the first and second white keys. A touch near the top there is the black key.
    auto const key = glass::KeyAtPosition(keyboard, 150.0, 60.0, 10.0, 5.0);

    VERIFY_ARE_EQUAL(1, key);
}

void NewControlTests::TheLowerPartOfAWhiteKeyIsAlwaysWhite()
{
    auto const keyboard = TwoOctavesFromC3();

    // The same place across, but below where a black key reaches: the white key it sits on.
    auto const key = glass::KeyAtPosition(keyboard, 150.0, 60.0, 10.0, 55.0);

    VERIFY_ARE_EQUAL(2, key);
}

void NewControlTests::ATouchOutsideTheKeyboardIsNoKey()
{
    auto const keyboard = TwoOctavesFromC3();

    VERIFY_ARE_EQUAL(-1, glass::KeyAtPosition(keyboard, 150.0, 60.0, -1.0, 30.0));
    VERIFY_ARE_EQUAL(-1, glass::KeyAtPosition(keyboard, 150.0, 60.0, 151.0, 30.0));
    VERIFY_ARE_EQUAL(-1, glass::KeyAtPosition(keyboard, 150.0, 60.0, 30.0, 61.0));
}

void NewControlTests::VelocityRisesTowardTheFrontOfAKey()
{
    auto const keyboard = TwoOctavesFromC3();

    auto const back = glass::KeyVelocityFromPosition(keyboard, 0, 60.0, 0.0);
    auto const front = glass::KeyVelocityFromPosition(keyboard, 0, 60.0, 60.0);

    VERIFY_IS_TRUE(front > back);

    // A key that plays nothing reads as a control that missed, so there is a floor.
    VERIFY_IS_TRUE(back > 0.0);
    VERIFY_IS_TRUE(front <= 1.0);
}

void NewControlTests::AKeyPlaysItsOwnNoteRatherThanTheRowNumber()
{
    glass::Control control{};

    control.Id = L"keys";
    control.Kind = glass::ControlKind::PianoKeyboard;
    control.Keyboard = TwoOctavesFromC3();

    glass::ControlMessage message{};

    message.Trigger = glass::MessageTrigger::Changes;
    message.Kind = glass::MessageKind::Note;
    message.DeviceName = L"Synth";

    // Deliberately not the note that will be played. The key decides.
    message.Number = 36;

    control.Messages.push_back(std::move(message));

    glass::BindingEngine engine{};
    engine.Prepare(OneControl(control), OneDevice());

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    auto const written = engine.EvaluateNote(0, 60, 1.0, true, sends);

    VERIFY_ARE_EQUAL(1u, written);
    VERIFY_ARE_EQUAL(2u, sends[0].WordCount);

    // MIDI 2.0 note on: type 4, group 0, status 9, then the note in the first index byte.
    VERIFY_ARE_EQUAL(0x9u, (sends[0].Words[0] >> 20) & 0x0F);
    VERIFY_ARE_EQUAL(60u, (sends[0].Words[0] >> 8) & 0x7F);
}

namespace
{
    glass::Control NoteRowControl(glass::ControlKind kind, bool midi1)
    {
        glass::Control control{};

        control.Id = L"notes";
        control.Kind = kind;

        if (kind == glass::ControlKind::PianoKeyboard)
        {
            control.Keyboard.LowestNote = 48;
            control.Keyboard.KeyCount = 25;
        }

        glass::ControlMessage message{};

        message.Trigger = glass::MessageTrigger::Changes;
        message.Kind = glass::MessageKind::Note;
        message.DeviceName = L"Synth";
        message.Number = 60;
        message.UseMidi1Protocol = midi1;

        control.Messages.push_back(std::move(message));

        return control;
    }
}

void NewControlTests::ASoftKeyStillPlaysANoteOn()
{
    glass::BindingEngine engine{};
    engine.Prepare(OneControl(NoteRowControl(glass::ControlKind::PianoKeyboard, false)), OneDevice());

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    // The softest a key can be struck from its top edge. Read as a gate, as a note row bound to
    // a fader is, this was a note off and the key played nothing.
    auto const written = engine.EvaluateNote(0, 60, 0.2, true, sends);

    VERIFY_ARE_EQUAL(1u, written);
    VERIFY_ARE_EQUAL(0x9u, (sends[0].Words[0] >> 20) & 0x0F);

    // Soft, but not silent: about a fifth of the way up the velocity field.
    auto const velocity = sends[0].Words[1] >> 16;

    VERIFY_IS_TRUE(velocity > 0x2000u && velocity < 0x4000u);

    // And the release is still a note off.
    VERIFY_ARE_EQUAL(1u, engine.EvaluateNote(0, 60, 0.0, false, sends));
    VERIFY_ARE_EQUAL(0x8u, (sends[0].Words[0] >> 20) & 0x0F);
}

void NewControlTests::ALightPadPressStillPlaysANoteOn()
{
    glass::BindingEngine engine{};
    engine.Prepare(OneControl(NoteRowControl(glass::ControlKind::Pad, false)), OneDevice());

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    // A pen touching down lightly on a pad that plays from pressure.
    VERIFY_ARE_EQUAL(1u, engine.EvaluatePress(0, glass::MessageTrigger::Changes, true, 0.3, sends));
    VERIFY_ARE_EQUAL(0x9u, (sends[0].Words[0] >> 20) & 0x0F);
    VERIFY_ARE_EQUAL(60u, (sends[0].Words[0] >> 8) & 0x7F);

    VERIFY_ARE_EQUAL(1u, engine.EvaluatePress(0, glass::MessageTrigger::Changes, false, 0.0, sends));
    VERIFY_ARE_EQUAL(0x8u, (sends[0].Words[0] >> 20) & 0x0F);

    // A fader bound to a note row still reads its position as a gate.
    VERIFY_ARE_EQUAL(1u, engine.Evaluate(0, glass::MessageTrigger::Changes, 0.3, sends));
    VERIFY_ARE_EQUAL(0x8u, (sends[0].Words[0] >> 20) & 0x0F);
}

void NewControlTests::ANoteOnNeverGoesOutAtVelocityZero()
{
    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    {
        glass::BindingEngine engine{};
        engine.Prepare(OneControl(NoteRowControl(glass::ControlKind::PianoKeyboard, false)), OneDevice());

        VERIFY_ARE_EQUAL(1u, engine.EvaluateNote(0, 60, 0.0, true, sends));
        VERIFY_ARE_EQUAL(0x9u, (sends[0].Words[0] >> 20) & 0x0F);

        // Still at least one step once the service folds it to seven bits.
        VERIFY_IS_TRUE(((sends[0].Words[1] >> 16) >> 9) >= 1u);
    }

    {
        glass::BindingEngine engine{};
        engine.Prepare(OneControl(NoteRowControl(glass::ControlKind::PianoKeyboard, true)), OneDevice());

        VERIFY_ARE_EQUAL(1u, engine.EvaluateNote(0, 60, 0.0, true, sends));
        VERIFY_ARE_EQUAL(1u, sends[0].WordCount);
        VERIFY_ARE_EQUAL(0x9u, (sends[0].Words[0] >> 20) & 0x0F);
        VERIFY_ARE_EQUAL(1u, sends[0].Words[0] & 0x7F);
    }
}

// ------------------------------------------------------------------- the clock

void NewControlTests::AClockSendsOneWordPerDestination()
{
    glass::Control control{};

    control.Id = L"clock";
    control.Kind = glass::ControlKind::BeatClock;

    glass::ControlMessage message{};

    message.Kind = glass::MessageKind::RawUmp;
    message.DeviceName = L"Synth";

    control.Messages.push_back(std::move(message));

    glass::BindingEngine engine{};
    engine.Prepare(OneControl(control), OneDevice());

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    auto const written = engine.EvaluateSystemRealTime(0, 0xF8, sends);

    VERIFY_ARE_EQUAL(1u, written);
    VERIFY_ARE_EQUAL(1u, sends[0].WordCount);

    // Message type 1, group 0, then the status byte. No channel, nothing to scale.
    VERIFY_ARE_EQUAL(0x10F80000u, sends[0].Words[0]);
}

void NewControlTests::AClockSendsOnceToADeviceNamedTwice()
{
    glass::Control control{};

    control.Id = L"clock";
    control.Kind = glass::ControlKind::BeatClock;

    for (int i = 0; i < 3; ++i)
    {
        glass::ControlMessage message{};

        message.Kind = glass::MessageKind::RawUmp;
        message.DeviceName = L"Synth";

        control.Messages.push_back(std::move(message));
    }

    glass::BindingEngine engine{};
    engine.Prepare(OneControl(control), OneDevice());

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    // Three rows pointed at one device is one clock, not three. Otherwise the device runs at
    // three times the tempo.
    VERIFY_ARE_EQUAL(1u, engine.EvaluateSystemRealTime(0, 0xF8, sends));
}

void NewControlTests::AClockSendsNothingWhileTheDeviceIsGone()
{
    glass::Control control{};

    control.Id = L"clock";
    control.Kind = glass::ControlKind::BeatClock;

    glass::ControlMessage message{};

    message.Kind = glass::MessageKind::RawUmp;
    message.DeviceName = L"Synth";

    control.Messages.push_back(std::move(message));

    glass::BindingEngine engine{};
    engine.Prepare(OneControl(control), OneDevice(false));

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    VERIFY_ARE_EQUAL(0u, engine.EvaluateSystemRealTime(0, 0xF8, sends));
}

// ------------------------------------------------------------------------ stops

void NewControlTests::AListOfStopsIsCountedAsItStands()
{
    glass::Control control{};

    auto message = ControlChangeOn(7, glass::ValueAxis::X);

    message.Detents.Mode = glass::DetentMode::ExplicitValues;
    message.Detents.Stops = { 10.0, 17.0, 38.0, 39.0, 40.0, 57.0 };

    control.Messages.push_back(std::move(message));

    VERIFY_ARE_EQUAL(6, glass::DetentStopCount(control));
}

void NewControlTests::EvenStepsAreCountedFromTheMinimum()
{
    glass::Control control{};

    auto message = ControlChangeOn(7, glass::ValueAxis::X);

    message.Minimum = { 0.0, glass::ValueScaling::Fraction };
    message.Maximum = { 1.0, glass::ValueScaling::Fraction };
    message.Detents.Mode = glass::DetentMode::EvenSteps;
    message.Detents.Step = 0.25;

    control.Messages.push_back(std::move(message));

    // Both ends and three between them.
    VERIFY_ARE_EQUAL(5, glass::DetentStopCount(control));
}

void NewControlTests::ASmoothControlHasNoStops()
{
    glass::Control control{};

    control.Messages.push_back(ControlChangeOn(7, glass::ValueAxis::X));

    VERIFY_ARE_EQUAL(0, glass::DetentStopCount(control));
}

void NewControlTests::StopsAreParsedFromWhateverSeparatorWasTyped()
{
    auto const commas = glass::ParseStopList(L"10, 17, 38");
    auto const spaces = glass::ParseStopList(L"10 17 38");
    auto const mixed = glass::ParseStopList(L"10;17,  38");

    VERIFY_ARE_EQUAL(size_t{ 3 }, commas.size());
    VERIFY_IS_TRUE(commas == spaces);
    VERIFY_IS_TRUE(commas == mixed);
    VERIFY_ARE_EQUAL(17.0, commas[1]);
}

void NewControlTests::OneBadEntryDoesNotEmptyTheList()
{
    // A stray character at the end of a long list must not throw the whole list away. That is
    // the opposite of the rule for system exclusive, where one bad byte can brick a synth.
    auto const stops = glass::ParseStopList(L"10, 17, zz, 38");

    VERIFY_ARE_EQUAL(size_t{ 3 }, stops.size());
    VERIFY_ARE_EQUAL(38.0, stops[2]);
}

void NewControlTests::AStopListRoundTripsThroughItsText()
{
    std::vector<double> const stops{ 10.0, 17.0, 38.5, 57.0 };

    VERIFY_IS_TRUE(glass::ParseStopList(glass::FormatStopList(stops)) == stops);

    // Whole numbers come back without a decimal point, because a stop list is a row of values
    // out of a manual and "10.000000" is unreadable.
    VERIFY_ARE_EQUAL(std::wstring{ L"10, 17, 38.5, 57" }, glass::FormatStopList(stops));
}

void NewControlTests::ExactStopsArePrintedAsThemselves()
{
    glass::Control control{};

    auto message = ControlChangeOn(7, glass::ValueAxis::X);

    message.Minimum = { 0.0, glass::ValueScaling::Absolute };
    message.Maximum = { 127.0, glass::ValueScaling::Absolute };
    message.Detents.Mode = glass::DetentMode::ExplicitValues;
    message.Detents.Scaling = glass::ValueScaling::Absolute;
    message.Detents.Stops = { 10.0, 17.0, 57.0 };

    control.Messages.push_back(std::move(message));

    auto const labels = glass::DetentStopLabels(control);

    VERIFY_ARE_EQUAL(size_t{ 3 }, labels.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"10" }, labels[0]);
    VERIFY_ARE_EQUAL(std::wstring{ L"57" }, labels[2]);
}

void NewControlTests::FractionStopsArePrintedAsPercentages()
{
    glass::Control control{};

    auto message = ControlChangeOn(7, glass::ValueAxis::X);

    message.Detents.Mode = glass::DetentMode::EvenSteps;
    message.Detents.Step = 0.5;

    control.Messages.push_back(std::move(message));

    auto const labels = glass::DetentStopLabels(control);

    VERIFY_ARE_EQUAL(size_t{ 3 }, labels.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"0 %" }, labels[0]);
    VERIFY_ARE_EQUAL(std::wstring{ L"50 %" }, labels[1]);
    VERIFY_ARE_EQUAL(std::wstring{ L"100 %" }, labels[2]);
}

void NewControlTests::TooManyStopsAreNotLabeled()
{
    glass::Control control{};

    auto message = ControlChangeOn(7, glass::ValueAxis::X);

    message.Detents.Mode = glass::DetentMode::ExplicitValues;

    for (int32_t i = 0; i < glass::MaximumLabeledStops + 1; ++i)
    {
        message.Detents.Stops.push_back(static_cast<double>(i));
    }

    control.Messages.push_back(std::move(message));

    // The control still snaps to all of them; it just stops printing numbers that would run
    // into each other whatever size it is drawn at.
    VERIFY_ARE_EQUAL(glass::MaximumLabeledStops + 1, glass::DetentStopCount(control));
    VERIFY_IS_TRUE(glass::DetentStopLabels(control).empty());
}

// --------------------------------------------------- what a control listens for

namespace
{
    glass::Control LampWatching(
        _In_ glass::FeedbackMode mode,
        _In_ std::wstring const& deviceName,
        _In_ bool matchesChannel = false)
    {
        glass::Control control{};

        control.Id = L"lamp";
        control.Kind = glass::ControlKind::Lamp;

        control.Feedback.Enabled = true;
        control.Feedback.Mode = mode;
        control.Feedback.DeviceName = deviceName;
        control.Feedback.GroupIndex = glass::AllGroups;
        control.Feedback.ChannelIndex = 0;
        control.Feedback.MatchesChannel = matchesChannel;

        return control;
    }

    // A MIDI 1.0 control change, group 0, on the channel given.
    uint32_t ControlChangeWord(_In_ uint8_t channel) noexcept
    {
        return 0x20B00700u | (static_cast<uint32_t>(channel & 0x0F) << 16);
    }

    // A MIDI 1.0 note on, group 0, on the channel given.
    uint32_t NoteOnWord(_In_ uint8_t channel, _In_ uint8_t note, _In_ uint8_t velocity) noexcept
    {
        return 0x20900000u
            | (static_cast<uint32_t>(channel & 0x0F) << 16)
            | (static_cast<uint32_t>(note & 0x7F) << 8)
            | static_cast<uint32_t>(velocity & 0x7F);
    }

    // A system real time message in a group 0 UMP.
    uint32_t RealTimeWord(_In_ uint8_t status) noexcept
    {
        return 0x10000000u | (static_cast<uint32_t>(status) << 16);
    }

    // How many controls this message lights, and how the first of them was lit.
    uint32_t HitsFor(
        _In_ glass::BindingEngine const& engine,
        _In_ uint32_t word,
        _In_ int32_t destinationIndex,
        _Out_ glass::BindingEngine::FeedbackHitKind& firstKind) noexcept
    {
        std::array<glass::BindingEngine::FeedbackHit, 8> hits{};

        auto const count = engine.CollectFeedbackHits(&word, 1, destinationIndex, hits);

        firstKind = count > 0 ? hits[0].Kind : glass::BindingEngine::FeedbackHitKind::Pulse;

        return count;
    }

    uint32_t HitsFor(
        _In_ glass::BindingEngine const& engine,
        _In_ uint32_t word,
        _In_ int32_t destinationIndex) noexcept
    {
        glass::BindingEngine::FeedbackHitKind ignored{};

        return HitsFor(engine, word, destinationIndex, ignored);
    }
}

void NewControlTests::AnActivityLampTakesAnythingFromItsDevice()
{
    glass::BindingEngine engine{};

    engine.Prepare(
        OneControl(LampWatching(glass::FeedbackMode::AnyActivity, L"Synth")),
        OneDevice());

    std::array<glass::BindingEngine::FeedbackHit, 8> hits{};

    auto const word = ControlChangeWord(5);

    VERIFY_ARE_EQUAL(1u, engine.CollectFeedbackHits(&word, 1, 0, hits));
    VERIFY_ARE_EQUAL(size_t{ 0 }, hits[0].ControlIndex);
}

void NewControlTests::AnActivityLampCanBeNarrowedToOneChannel()
{
    glass::BindingEngine engine{};

    engine.Prepare(
        OneControl(LampWatching(glass::FeedbackMode::AnyActivity, L"Synth", true)),
        OneDevice());

    VERIFY_ARE_EQUAL(1u, HitsFor(engine, ControlChangeWord(0), 0));
    VERIFY_ARE_EQUAL(0u, HitsFor(engine, ControlChangeWord(5), 0));
}

void NewControlTests::AnActivityLampIgnoresAnotherDevice()
{
    glass::BindingEngine engine{};

    engine.Prepare(
        OneControl(LampWatching(glass::FeedbackMode::AnyActivity, L"Synth")),
        OneDevice());

    auto const word = ControlChangeWord(0);

    // Destination 1 is not the one the lamp named.
    VERIFY_ARE_EQUAL(0u, HitsFor(engine, word, 1));

    // A lamp naming no device at all takes traffic from anything on the layout.
    glass::BindingEngine anywhere{};

    anywhere.Prepare(
        OneControl(LampWatching(glass::FeedbackMode::AnyActivity, L"")),
        OneDevice());

    VERIFY_ARE_EQUAL(1u, HitsFor(anywhere, word, 1));
}

void NewControlTests::ANoteLampTakesOnlyNotes()
{
    glass::BindingEngine engine{};

    engine.Prepare(
        OneControl(LampWatching(glass::FeedbackMode::Notes, L"Synth")),
        OneDevice());

    glass::BindingEngine::FeedbackHitKind kind{};

    // A note on blinks it for its hold time. A note off is the end of something rather than
    // the start of it, so it is left alone, and a controller is not a note at all.
    VERIFY_ARE_EQUAL(1u, HitsFor(engine, NoteOnWord(0, 60, 100), 0, kind));
    VERIFY_IS_TRUE(kind == glass::BindingEngine::FeedbackHitKind::Pulse);

    VERIFY_ARE_EQUAL(0u, HitsFor(engine, NoteOnWord(0, 60, 0), 0));
    VERIFY_ARE_EQUAL(0u, HitsFor(engine, ControlChangeWord(0), 0));
}

void NewControlTests::AControllerLampTakesOnlyControlChanges()
{
    glass::BindingEngine engine{};

    engine.Prepare(
        OneControl(LampWatching(glass::FeedbackMode::ControlChanges, L"Synth")),
        OneDevice());

    VERIFY_ARE_EQUAL(1u, HitsFor(engine, ControlChangeWord(0), 0));
    VERIFY_ARE_EQUAL(0u, HitsFor(engine, NoteOnWord(0, 60, 100), 0));
}

void NewControlTests::ATransportLampLatchesOnStartAndClearsOnStop()
{
    glass::BindingEngine engine{};

    engine.Prepare(
        OneControl(LampWatching(glass::FeedbackMode::Transport, L"Synth")),
        OneDevice());

    glass::BindingEngine::FeedbackHitKind kind{};

    VERIFY_ARE_EQUAL(1u, HitsFor(engine, RealTimeWord(0xFA), 0, kind));
    VERIFY_IS_TRUE(kind == glass::BindingEngine::FeedbackHitKind::On);

    VERIFY_ARE_EQUAL(1u, HitsFor(engine, RealTimeWord(0xFB), 0, kind));
    VERIFY_IS_TRUE(kind == glass::BindingEngine::FeedbackHitKind::On);

    VERIFY_ARE_EQUAL(1u, HitsFor(engine, RealTimeWord(0xFC), 0, kind));
    VERIFY_IS_TRUE(kind == glass::BindingEngine::FeedbackHitKind::Off);

    // A clock message is not transport, and must not flicker a running light.
    VERIFY_ARE_EQUAL(0u, HitsFor(engine, RealTimeWord(0xF8), 0));
}

void NewControlTests::ABeatLampCountsClockMessages()
{
    glass::BindingEngine engine{};

    engine.Prepare(
        OneControl(LampWatching(glass::FeedbackMode::Tempo, L"Synth")),
        OneDevice());

    glass::BindingEngine::FeedbackHitKind kind{};

    // Every clock is reported. Counting twenty four of them to a beat is the player's job,
    // because the engine keeps no state between messages.
    VERIFY_ARE_EQUAL(1u, HitsFor(engine, RealTimeWord(0xF8), 0, kind));
    VERIFY_IS_TRUE(kind == glass::BindingEngine::FeedbackHitKind::ClockTick);

    VERIFY_ARE_EQUAL(0u, HitsFor(engine, ControlChangeWord(0), 0));
}

void NewControlTests::AMessageBindingIsNotLitByActivity()
{
    auto control = LampWatching(glass::FeedbackMode::Message, L"Synth");

    control.Feedback.Kind = glass::MessageKind::ControlChange;
    control.Feedback.GroupIndex = 0;
    control.Feedback.Number = 7;

    glass::BindingEngine engine{};
    engine.Prepare(OneControl(control), OneDevice());

    auto const word = ControlChangeWord(0);

    VERIFY_ARE_EQUAL(0u, HitsFor(engine, word, 0));

    // It still moves on the message it was actually pointed at.
    size_t controlIndex{ 99 };
    double value{ -1.0 };

    VERIFY_IS_TRUE(engine.TryResolveFeedback(&word, 1, controlIndex, value));
    VERIFY_ARE_EQUAL(size_t{ 0 }, controlIndex);
}

void NewControlTests::AnActivityBindingDoesNotMoveAControl()
{
    glass::BindingEngine engine{};

    engine.Prepare(
        OneControl(LampWatching(glass::FeedbackMode::AnyActivity, L"Synth")),
        OneDevice());

    auto const word = ControlChangeWord(0);

    size_t controlIndex{ 99 };
    double value{ -1.0 };

    // An activity light has no value to carry, so it must not be answered as a value move.
    VERIFY_IS_FALSE(engine.TryResolveFeedback(&word, 1, controlIndex, value));
}

// ------------------------------------------- cropping a picture or a video

namespace
{
    glass::Picture Cropped(
        _In_ glass::BackgroundFit fit,
        _In_ double zoom = 1.0,
        _In_ double centerX = 0.5,
        _In_ double centerY = 0.5)
    {
        glass::Picture picture{};

        picture.FileName = L"clip.mp4";
        picture.Fit = fit;
        picture.Zoom = zoom;
        picture.CenterX = centerX;
        picture.CenterY = centerY;

        return picture;
    }

    bool Near(_In_ double actual, _In_ double wanted) noexcept
    {
        return std::abs(actual - wanted) < 0.01;
    }
}

void NewControlTests::AFilledPictureCoversTheControl()
{
    // A wide clip in a tall narrow box. This is the slice out of the middle.
    auto const rect = glass::PictureCropRect(
        Cropped(glass::BackgroundFit::Fill), 90.0, 360.0, 1920.0, 1080.0);

    // Scaled by height, because that is the side that has to reach: 360 / 1080 = 1/3.
    VERIFY_IS_TRUE(Near(rect.Height, 360.0));
    VERIFY_IS_TRUE(Near(rect.Width, 640.0));

    // Covered in both directions, with the overflow split evenly.
    VERIFY_IS_TRUE(rect.X <= 0.0 && rect.X + rect.Width >= 90.0);
    VERIFY_IS_TRUE(Near(rect.X, (90.0 - 640.0) * 0.5));
    VERIFY_IS_TRUE(Near(rect.Y, 0.0));
}

void NewControlTests::AUniformPictureFitsInsideTheControl()
{
    auto const rect = glass::PictureCropRect(
        Cropped(glass::BackgroundFit::Uniform), 400.0, 400.0, 1920.0, 1080.0);

    // Scaled by width this time, because nothing may be cut off.
    VERIFY_IS_TRUE(Near(rect.Width, 400.0));
    VERIFY_IS_TRUE(Near(rect.Height, 225.0));

    // Nothing hangs over the edge, and the empty space is shared top and bottom.
    VERIFY_IS_TRUE(Near(rect.X, 0.0));
    VERIFY_IS_TRUE(Near(rect.Y, (400.0 - 225.0) * 0.5));
}

void NewControlTests::AStretchedPictureTakesTheControlsShape()
{
    auto const rect = glass::PictureCropRect(
        Cropped(glass::BackgroundFit::Stretch), 300.0, 120.0, 1920.0, 1080.0);

    VERIFY_IS_TRUE(Near(rect.Width, 300.0));
    VERIFY_IS_TRUE(Near(rect.Height, 120.0));
    VERIFY_IS_TRUE(Near(rect.X, 0.0));
    VERIFY_IS_TRUE(Near(rect.Y, 0.0));
}

void NewControlTests::ZoomMakesThePictureLarger()
{
    auto const once = glass::PictureCropRect(
        Cropped(glass::BackgroundFit::Fill, 1.0), 200.0, 200.0, 400.0, 400.0);

    auto const thrice = glass::PictureCropRect(
        Cropped(glass::BackgroundFit::Fill, 3.0), 200.0, 200.0, 400.0, 400.0);

    VERIFY_IS_TRUE(Near(once.Width, 200.0));
    VERIFY_IS_TRUE(Near(thrice.Width, 600.0));

    // Past the top of the range it stops growing rather than running away.
    auto const silly = glass::PictureCropRect(
        Cropped(glass::BackgroundFit::Fill, 500.0), 200.0, 200.0, 400.0, 400.0);

    VERIFY_IS_TRUE(Near(silly.Width, 200.0 * glass::MaximumPictureZoom));
}

void NewControlTests::TheMiddleDecidesWhichSliceIsShown()
{
    // A square hole in a wide clip: 600 wide rendered, 200 of it on show.
    auto const left = glass::PictureCropRect(
        Cropped(glass::BackgroundFit::Fill, 1.0, 0.0, 0.5), 200.0, 200.0, 600.0, 200.0);

    auto const middle = glass::PictureCropRect(
        Cropped(glass::BackgroundFit::Fill, 1.0, 0.5, 0.5), 200.0, 200.0, 600.0, 200.0);

    auto const right = glass::PictureCropRect(
        Cropped(glass::BackgroundFit::Fill, 1.0, 1.0, 0.5), 200.0, 200.0, 600.0, 200.0);

    // Zero shows the left edge, one shows the right edge, and half is the middle.
    VERIFY_IS_TRUE(Near(left.X, 0.0));
    VERIFY_IS_TRUE(Near(middle.X, -200.0));
    VERIFY_IS_TRUE(Near(right.X, -400.0));
}

void NewControlTests::PanningCannotUncoverTheControl()
{
    // The case that showed up on screen: zoomed in and pushed almost to a corner.
    auto const rect = glass::PictureCropRect(
        Cropped(glass::BackgroundFit::Fill, 2.0, 0.9, 0.1), 120.0, 120.0, 640.0, 480.0);

    VERIFY_IS_TRUE(rect.Width >= 120.0);
    VERIFY_IS_TRUE(rect.Height >= 120.0);

    // No bare strip on any side, however far the middle was dragged.
    VERIFY_IS_TRUE(rect.X <= 0.0);
    VERIFY_IS_TRUE(rect.Y <= 0.0);
    VERIFY_IS_TRUE(rect.X + rect.Width >= 120.0);
    VERIFY_IS_TRUE(rect.Y + rect.Height >= 120.0);
}

void NewControlTests::ASmallPictureSitsWhereTheMiddleSays()
{
    // A 100 by 50 picture in a 400 square control leaves 300 across and 350 down to spare.
    auto const corner = glass::PictureCropRect(
        Cropped(glass::BackgroundFit::Centered, 1.0, 0.0, 0.0), 400.0, 400.0, 100.0, 50.0);

    auto const middle = glass::PictureCropRect(
        Cropped(glass::BackgroundFit::Centered, 1.0, 0.5, 0.5), 400.0, 400.0, 100.0, 50.0);

    auto const opposite = glass::PictureCropRect(
        Cropped(glass::BackgroundFit::Centered, 1.0, 1.0, 1.0), 400.0, 400.0, 100.0, 50.0);

    VERIFY_IS_TRUE(Near(middle.Width, 100.0));
    VERIFY_IS_TRUE(Near(middle.Height, 50.0));

    // Nothing to crop, so the same numbers place it instead: zero against the top left, one
    // against the bottom right, and half in the middle, which is where every old layout had it.
    VERIFY_IS_TRUE(Near(corner.X, 0.0));
    VERIFY_IS_TRUE(Near(corner.Y, 0.0));
    VERIFY_IS_TRUE(Near(middle.X, 150.0));
    VERIFY_IS_TRUE(Near(middle.Y, 175.0));
    VERIFY_IS_TRUE(Near(opposite.X, 300.0));
    VERIFY_IS_TRUE(Near(opposite.Y, 350.0));
}

void NewControlTests::AlignmentAndCropShareOneNumberPerAxis()
{
    // A wide picture in a square control, fitted: it fills across and leaves space above and
    // below. Right and bottom puts it against the right edge, which it already touches, and
    // down at the bottom of the spare space.
    auto const rect = glass::PictureCropRect(
        Cropped(glass::BackgroundFit::Uniform, 1.0, 1.0, 1.0), 200.0, 200.0, 400.0, 200.0);

    VERIFY_IS_TRUE(Near(rect.Width, 200.0));
    VERIFY_IS_TRUE(Near(rect.Height, 100.0));
    VERIFY_IS_TRUE(Near(rect.X, 0.0));
    VERIFY_IS_TRUE(Near(rect.Y, 100.0));

    // Zoomed to twice, the same numbers now crop across and still sit at the bottom.
    auto const zoomed = glass::PictureCropRect(
        Cropped(glass::BackgroundFit::Uniform, 2.0, 1.0, 1.0), 200.0, 200.0, 400.0, 200.0);

    VERIFY_IS_TRUE(Near(zoomed.Width, 400.0));
    VERIFY_IS_TRUE(Near(zoomed.Height, 200.0));
    VERIFY_IS_TRUE(Near(zoomed.X, -200.0));
    VERIFY_IS_TRUE(Near(zoomed.Y, 0.0));
}

void NewControlTests::AnUndecodedPictureFillsTheControl()
{
    // What a video looks like between the element appearing and the first frame arriving.
    auto const rect = glass::PictureCropRect(
        Cropped(glass::BackgroundFit::Fill), 320.0, 240.0, 0.0, 0.0);

    VERIFY_IS_TRUE(Near(rect.Width, 320.0));
    VERIFY_IS_TRUE(Near(rect.Height, 240.0));
    VERIFY_IS_TRUE(Near(rect.X, 0.0));
    VERIFY_IS_TRUE(Near(rect.Y, 0.0));
}

// --------------------------------------------------------------- wheel and switch

namespace
{
    // A seven bit value from a MIDI 1.0 message, or the 32 bit one from a MIDI 2.0 message.
    uint32_t SentValue(_In_ glass::PreparedSend const& send) noexcept
    {
        return send.WordCount >= 2 ? send.Words[1] : (send.Words[0] & 0x7F);
    }
}

void NewControlTests::ANewWheelIsAPitchWheel()
{
    glass::Page page{};

    auto const wheel = glass::MakeNewControl(glass::ControlKind::Wheel, 0, 0, 1280, 800, L"Synth", page);

    // Centered, and back to the middle when the thumb comes off.
    VERIFY_IS_TRUE(wheel.ReturnsToDefault);
    VERIFY_ARE_EQUAL(0.5, wheel.DefaultValue);
    VERIFY_ARE_EQUAL(size_t{ 1 }, wheel.Messages.size());
    VERIFY_ARE_EQUAL(static_cast<int>(glass::MessageKind::PitchBend), static_cast<int>(wheel.Messages[0].Kind));

    // Turned, not jumped to.
    VERIFY_IS_FALSE(glass::UsesAbsolutePosition(glass::ControlKind::Wheel));

    auto const read = glass::ReadLayoutFromJson(glass::WriteLayoutToJson(OneControl(wheel)));

    VERIFY_IS_TRUE(read.Succeeded);
    VERIFY_ARE_EQUAL(
        static_cast<int>(glass::ControlKind::Wheel),
        static_cast<int>(read.Document.Pages.front().Controls.front().Kind));
}

void NewControlTests::ASwitchPicksThePositionUnderTheFinger()
{
    // Three slices across 300: up to 100, up to 200, and the rest.
    VERIFY_ARE_EQUAL(0.0, glass::SwitchValueAtPoint(300.0, 40.0, 50.0, 20.0, 3));
    VERIFY_ARE_EQUAL(0.5, glass::SwitchValueAtPoint(300.0, 40.0, 150.0, 20.0, 3));
    VERIFY_ARE_EQUAL(1.0, glass::SwitchValueAtPoint(300.0, 40.0, 299.0, 20.0, 3));

    // A finger that slides off either end is still on the end position.
    VERIFY_ARE_EQUAL(1.0, glass::SwitchValueAtPoint(300.0, 40.0, 900.0, 20.0, 3));
    VERIFY_ARE_EQUAL(0.0, glass::SwitchValueAtPoint(300.0, 40.0, -30.0, 20.0, 3));

    // Upright, the first position is at the top, the way a list reads.
    VERIFY_ARE_EQUAL(0.0, glass::SwitchValueAtPoint(40.0, 300.0, 20.0, 10.0, 3));
    VERIFY_ARE_EQUAL(1.0, glass::SwitchValueAtPoint(40.0, 300.0, 20.0, 290.0, 3));

    // And a value from anywhere lands on the nearest position.
    VERIFY_ARE_EQUAL(1, glass::SwitchPositionAt(0.45, 3));
    VERIFY_ARE_EQUAL(2, glass::SwitchPositionAt(7.0, 3));
}

void NewControlTests::ASwitchSendsOnlyTheRowForItsPosition()
{
    // Built the way the palette builds one: three positions, a row each on one controller.
    glass::Page page{};

    auto const control = glass::MakeNewControl(glass::ControlKind::Switch, 0, 0, 1280, 800, L"Synth", page);

    VERIFY_ARE_EQUAL(3, glass::SwitchPositionCount(control));
    VERIFY_ARE_EQUAL(size_t{ 3 }, control.Messages.size());

    glass::BindingEngine engine{};
    engine.Prepare(OneControl(control), OneDevice());

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    VERIFY_ARE_EQUAL(1u, engine.Evaluate(0, glass::MessageTrigger::Changes, 0.0, sends));
    auto const bottom = SentValue(sends[0]);

    VERIFY_ARE_EQUAL(1u, engine.Evaluate(0, glass::MessageTrigger::Changes, 0.5, sends));
    auto const middle = SentValue(sends[0]);

    VERIFY_ARE_EQUAL(1u, engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends));
    auto const top = SentValue(sends[0]);

    VERIFY_IS_TRUE(bottom < middle);
    VERIFY_IS_TRUE(middle < top);

    // Near a position is that position.
    VERIFY_ARE_EQUAL(1u, engine.Evaluate(0, glass::MessageTrigger::Changes, 0.45, sends));
    VERIFY_ARE_EQUAL(middle, SentValue(sends[0]));
}

void NewControlTests::ASwitchSurvivesSavingAndLoading()
{
    glass::Page page{};

    auto control = glass::MakeNewControl(glass::ControlKind::Switch, 0, 0, 1280, 800, L"Synth", page);

    control.Switch.Positions = { L"Saw", L"Square", L"Noise" };

    auto const read = glass::ReadLayoutFromJson(glass::WriteLayoutToJson(OneControl(control)));

    VERIFY_IS_TRUE(read.Succeeded);

    auto const& loaded = read.Document.Pages.front().Controls.front();

    VERIFY_ARE_EQUAL(static_cast<int>(glass::ControlKind::Switch), static_cast<int>(loaded.Kind));
    VERIFY_ARE_EQUAL(size_t{ 3 }, loaded.Switch.Positions.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"Noise" }, loaded.Switch.Positions[2]);
    VERIFY_ARE_EQUAL(size_t{ 3 }, loaded.Messages.size());
    VERIFY_ARE_EQUAL(2, loaded.Messages[2].Position);
}

// ------------------------------------------------------------------- the step sequencer

namespace
{
    glass::StepsSpec StepsOf(int32_t count, glass::StepDirection direction)
    {
        glass::StepsSpec spec{};

        spec.Direction = direction;

        for (int32_t index = 0; index < count; ++index)
        {
            glass::SequencerStep step{};
            step.Note = 60 + index;
            spec.Pattern.push_back(step);
        }

        return spec;
    }

    std::vector<int32_t> Walk(glass::StepsSpec const& spec, uint64_t count)
    {
        std::vector<int32_t> steps{};
        uint32_t random{ 1 };

        for (uint64_t at = 0; at < count; ++at)
        {
            steps.push_back(glass::StepIndexAt(spec, at, random));
        }

        return steps;
    }
}

void NewControlTests::StepsWalkForwardBackwardAndBothWays()
{
    VERIFY_IS_TRUE((std::vector<int32_t>{ 0, 1, 2, 3, 0, 1 }) ==
        Walk(StepsOf(4, glass::StepDirection::Forward), 6));

    VERIFY_IS_TRUE((std::vector<int32_t>{ 3, 2, 1, 0, 3, 2 }) ==
        Walk(StepsOf(4, glass::StepDirection::Backward), 6));

    // Up and back without playing either end twice in a row.
    VERIFY_IS_TRUE((std::vector<int32_t>{ 0, 1, 2, 3, 2, 1, 0, 1, 2 }) ==
        Walk(StepsOf(4, glass::StepDirection::PingPong), 9));

    // One step is always that step, whichever way it walks.
    VERIFY_IS_TRUE((std::vector<int32_t>{ 0, 0, 0 }) ==
        Walk(StepsOf(1, glass::StepDirection::PingPong), 3));
}

void NewControlTests::RandomStepsStayOnThePattern()
{
    auto const steps = Walk(StepsOf(5, glass::StepDirection::Random), 200);

    std::array<int32_t, 5> seen{};

    for (auto const step : steps)
    {
        VERIFY_IS_TRUE(step >= 0 && step < 5);
        seen[static_cast<size_t>(step)]++;
    }

    // Every step comes up, which is the difference between random and stuck.
    for (auto const count : seen)
    {
        VERIFY_IS_TRUE(count > 0);
    }
}

void NewControlTests::SwingHoldsBackEverySecondStep()
{
    auto spec = StepsOf(8, glass::StepDirection::Forward);

    // Sixteenth notes at 120 beats a minute: an eighth of a second each.
    auto const step = glass::StepMicroseconds(spec, 120.0);

    VERIFY_ARE_EQUAL(125000.0, step);

    VERIFY_ARE_EQUAL(125000.0, glass::StepStartMicroseconds(spec, 1, step));

    // As far as it goes: the first of each pair takes three quarters of it.
    spec.Swing = 0.75;

    VERIFY_ARE_EQUAL(0.0, glass::StepStartMicroseconds(spec, 0, step));
    VERIFY_ARE_EQUAL(187500.0, glass::StepStartMicroseconds(spec, 1, step));
    VERIFY_ARE_EQUAL(250000.0, glass::StepStartMicroseconds(spec, 2, step));
    VERIFY_ARE_EQUAL(437500.0, glass::StepStartMicroseconds(spec, 3, step));
}

void NewControlTests::AStepsNoteEndsBeforeTheNextStepStarts()
{
    auto spec = StepsOf(8, glass::StepDirection::Forward);

    auto const step = glass::StepMicroseconds(spec, 120.0);

    spec.Gate = 0.5;
    VERIFY_ARE_EQUAL(62500.0, glass::GateMicroseconds(spec, 0, step));

    // Held for the whole step, it still lets go before the next note starts.
    spec.Gate = 1.0;

    for (uint64_t count = 0; count < 4; ++count)
    {
        auto const ends = glass::StepStartMicroseconds(spec, count, step) + glass::GateMicroseconds(spec, count, step);

        VERIFY_IS_TRUE(ends < glass::StepStartMicroseconds(spec, count + 1, step));
    }

    // With swing, the note before a late step is the one that gets longer.
    spec.Gate = 0.5;
    spec.Swing = 0.75;

    VERIFY_IS_TRUE(glass::GateMicroseconds(spec, 0, step) > glass::GateMicroseconds(spec, 1, step));
}

void NewControlTests::ANewStepsControlPlaysAnArpeggioOnANoteRow()
{
    glass::Page page{};

    auto const control = glass::MakeNewControl(glass::ControlKind::Steps, 0, 0, 1280, 800, L"Synth", page);

    VERIFY_ARE_EQUAL(glass::DefaultSequencerSteps, glass::SequencerStepCount(control.Steps));
    VERIFY_ARE_EQUAL(48, control.Steps.Pattern.front().Note);
    VERIFY_ARE_EQUAL(60, control.Steps.Pattern[4].Note);

    VERIFY_ARE_EQUAL(size_t{ 1 }, control.Messages.size());
    VERIFY_ARE_EQUAL(static_cast<int>(glass::MessageKind::Note), static_cast<int>(control.Messages.front().Kind));

    // Each step plays its own note on that row, the way a key does.
    glass::BindingEngine engine{};
    engine.Prepare(OneControl(control), OneDevice());

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    auto const& third = control.Steps.Pattern[2];

    VERIFY_ARE_EQUAL(1u, engine.EvaluateNote(0, static_cast<uint16_t>(third.Note), third.Velocity, true, sends));
    VERIFY_ARE_EQUAL(0x9u, (sends[0].Words[0] >> 20) & 0x0F);
    VERIFY_ARE_EQUAL(static_cast<uint32_t>(third.Note), (sends[0].Words[0] >> 8) & 0x7F);
}

void NewControlTests::AStepsControlSurvivesSavingAndLoading()
{
    glass::Page page{};

    auto control = glass::MakeNewControl(glass::ControlKind::Steps, 0, 0, 1280, 800, L"Synth", page);

    control.Steps.Pattern.resize(3);
    control.Steps.Pattern[1].On = false;
    control.Steps.Pattern[2].Note = 67;
    control.Steps.Pattern[2].Velocity = 0.25;
    control.Steps.StepsPerBeat = 3.0;
    control.Steps.Gate = 0.9;
    control.Steps.Swing = 0.6;
    control.Steps.Direction = glass::StepDirection::PingPong;
    control.Steps.Latching = false;
    control.Steps.StartsRunning = true;

    auto const read = glass::ReadLayoutFromJson(glass::WriteLayoutToJson(OneControl(control)));

    VERIFY_IS_TRUE(read.Succeeded);

    auto const& loaded = read.Document.Pages.front().Controls.front();

    VERIFY_ARE_EQUAL(static_cast<int>(glass::ControlKind::Steps), static_cast<int>(loaded.Kind));
    VERIFY_ARE_EQUAL(size_t{ 3 }, loaded.Steps.Pattern.size());
    VERIFY_IS_FALSE(loaded.Steps.Pattern[1].On);
    VERIFY_ARE_EQUAL(67, loaded.Steps.Pattern[2].Note);
    VERIFY_ARE_EQUAL(0.25, loaded.Steps.Pattern[2].Velocity);
    VERIFY_ARE_EQUAL(3.0, loaded.Steps.StepsPerBeat);
    VERIFY_ARE_EQUAL(0.9, loaded.Steps.Gate);
    VERIFY_ARE_EQUAL(0.6, loaded.Steps.Swing);
    VERIFY_ARE_EQUAL(static_cast<int>(glass::StepDirection::PingPong), static_cast<int>(loaded.Steps.Direction));
    VERIFY_IS_FALSE(loaded.Steps.Latching);
    VERIFY_IS_TRUE(loaded.Steps.StartsRunning);
}

void NewControlTests::AStepsFileCannotAskForTooMuch()
{
    // A layout from a stranger: far too many steps, notes and settings outside every range.
    std::wstring pattern{};

    for (int32_t index = 0; index < 200; ++index)
    {
        pattern += (index == 0 ? L"" : L",");
        pattern += L"{ \"note\": 300, \"velocity\": 9 }";
    }

    auto const json =
        L"{ \"fileVersion\": 1, \"name\": \"Strange\", \"pages\": [ { \"id\": \"p\", \"name\": \"P\", \"controls\": ["
        L"{ \"id\": \"s\", \"kind\": \"steps\", \"x\": 0, \"y\": 0, \"width\": 100, \"height\": 40,"
        L"  \"sequencer\": { \"pattern\": [" + pattern + L"], \"stepsPerBeat\": 1000, \"gate\": -3, \"swing\": 5,"
        L"  \"direction\": \"sideways\" } } ] } ] }";

    auto const read = glass::ReadLayoutFromJson(json);

    VERIFY_IS_TRUE(read.Succeeded);

    auto const& steps = read.Document.Pages.front().Controls.front().Steps;

    VERIFY_ARE_EQUAL(static_cast<size_t>(glass::MaximumSequencerSteps), steps.Pattern.size());

    // A whole number outside its range is refused and the default used, the way every other
    // whole number in the file is read. A fraction is held to its range.
    VERIFY_ARE_EQUAL(60, steps.Pattern.front().Note);
    VERIFY_ARE_EQUAL(1.0, steps.Pattern.front().Velocity);
    VERIFY_ARE_EQUAL(glass::MaximumStepsPerBeat, steps.StepsPerBeat);
    VERIFY_ARE_EQUAL(glass::MinimumStepGate, steps.Gate);
    VERIFY_ARE_EQUAL(glass::MaximumStepSwing, steps.Swing);
    VERIFY_ARE_EQUAL(static_cast<int>(glass::StepDirection::Forward), static_cast<int>(steps.Direction));
}
