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
