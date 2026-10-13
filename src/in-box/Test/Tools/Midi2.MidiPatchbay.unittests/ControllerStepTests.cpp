// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "ControllerStepTests.h"
#include "TestMessages.h"

#include "ControllerSteps.h"
#include "PatchSerializer.h"
#include "ProcessingBlock.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace midipatchbay;
using namespace patchbaytests;

namespace
{
    constexpr uint8_t NoteOff = 0x8;
    constexpr uint8_t NoteOn = 0x9;
    constexpr uint8_t ControlChange = 0xB;
    constexpr uint8_t ProgramChange = 0xC;
    constexpr uint8_t PitchBend = 0xE;

    // What a step sent, in order.
    struct Sent
    {
        std::vector<Message> Messages{};

        ControllerSink Sink()
        {
            return [this](uint32_t const* words, uint8_t count)
                {
                    Message message{};
                    std::copy_n(words, count, message.Words.begin());
                    message.Count = count;

                    Messages.push_back(message);
                };
        }

        std::vector<Message> Take()
        {
            auto taken = std::move(Messages);
            Messages.clear();
            return taken;
        }
    };

    Message FromWords(uint32_t const* words, uint8_t count)
    {
        Message message{};
        std::copy_n(words, count, message.Words.begin());
        message.Count = count;
        return message;
    }

    ControllerReading Resting()
    {
        ControllerReading reading{};
        reading.Connected = true;
        return reading;
    }

    ControllerMessage Note(uint8_t note)
    {
        auto message = ControllerMessageFor(ControllerMessageKind::Note);
        message.Number = note;
        return message;
    }

    ControllerMessage Program(ProgramStep step, uint8_t lowest, uint8_t highest, bool wraps)
    {
        auto message = ControllerMessageFor(ControllerMessageKind::ProgramChange);
        message.Step = step;
        message.LowestProgram = lowest;
        message.HighestProgram = highest;
        message.Wraps = wraps;
        message.Midi1Protocol = true;
        return message;
    }

    // MIDI 2.0 note on and off, as BuildControllerWords makes them.
    Message Midi2NoteOn(uint8_t note, uint32_t velocity16)
    {
        return Midi2(0, NoteOn, 0, note, 0, velocity16 << 16);
    }

    uint32_t Velocity16(int32_t hundredths)
    {
        return midiapp::ScaleToBits(hundredths / static_cast<double>(FullScaleHundredths), 16);
    }

    uint8_t ProgramOf(Message const& message)
    {
        return static_cast<uint8_t>((message.Words[0] >> 8) & 0x7F);
    }

    ControllerAxisSettings PitchBendStick()
    {
        ControllerAxisSettings settings{};
        settings.Control = ControllerControl::LeftStickX;
        settings.Target = { midiapp::ValueMessageKind::PitchBend, 0, 0, 0, false };
        settings.IntervalMilliseconds = 0;
        return settings;
    }
}

// ------------------------------------------------------------------ readings and Learn

void ControllerStepTests::XInputReadingsRunFromZeroToOne()
{
    // A and D-pad up held, the left trigger all the way in, the left stick pushed up and left.
    auto const reading = ControllerReadingFromXInput(0x1000 | 0x0001, 255, 0, -32768, 32767, 0, 0);

    VERIFY_IS_TRUE(reading.Connected);
    VERIFY_IS_TRUE(ControlIsDown(reading, ControllerControl::A));
    VERIFY_IS_TRUE(ControlIsDown(reading, ControllerControl::DPadUp));
    VERIFY_IS_FALSE(ControlIsDown(reading, ControllerControl::B));
    VERIFY_IS_FALSE(ControlIsDown(reading, ControllerControl::DPadDown));

    VERIFY_ARE_EQUAL(1.0, ControlValue(reading, ControllerControl::LeftTrigger));
    VERIFY_ARE_EQUAL(0.0, ControlValue(reading, ControllerControl::RightTrigger));
    VERIFY_ARE_EQUAL(0.0, ControlValue(reading, ControllerControl::LeftStickX));
    VERIFY_ARE_EQUAL(1.0, ControlValue(reading, ControllerControl::LeftStickY));
    VERIFY_IS_TRUE(std::abs(ControlValue(reading, ControllerControl::RightStickX) - 0.5) < 0.0001);

    // A trigger is a button past half way, and so is a stick pushed toward one end.
    VERIFY_IS_TRUE(ControlIsDown(reading, ControllerControl::LeftTrigger));
    VERIFY_IS_FALSE(ControlIsDown(reading, ControllerControl::RightTrigger));
    VERIFY_IS_TRUE(ControlIsDown(reading, ControllerControl::LeftStickUp));
    VERIFY_IS_TRUE(ControlIsDown(reading, ControllerControl::LeftStickLeft));
    VERIFY_IS_FALSE(ControlIsDown(reading, ControllerControl::LeftStickDown));
    VERIFY_IS_FALSE(ControlIsDown(reading, ControllerControl::LeftStickRight));
    VERIFY_IS_FALSE(ControlIsDown(reading, ControllerControl::RightStickUp));
    VERIFY_ARE_EQUAL(1.0, ControlValue(reading, ControllerControl::LeftStickUp));

    // A controller that went away has nothing down.
    auto gone = reading;
    gone.Connected = false;

    VERIFY_IS_FALSE(ControlIsDown(gone, ControllerControl::A));
    VERIFY_IS_FALSE(ControlIsDown(gone, ControllerControl::LeftTrigger));

    VERIFY_ARE_EQUAL(0.5, ControlRestValue(ControllerControl::LeftStickY));
    VERIFY_ARE_EQUAL(0.0, ControlRestValue(ControllerControl::RightTrigger));
}

void ControllerStepTests::EveryControlAndMessageHasItsOwnKey()
{
    std::set<std::wstring> keys{};
    size_t pressable{ 0 };
    size_t slidable{ 0 };

    for (size_t i = 0; i < ControllerControlCount; i++)
    {
        auto const control = static_cast<ControllerControl>(i);
        auto const key = std::wstring{ ControllerControlKey(control) };

        VERIFY_IS_FALSE(key.empty());
        VERIFY_IS_TRUE(keys.insert(key).second);

        auto const back = ControllerControlFromKey(key);

        VERIFY_IS_TRUE(back.has_value());
        VERIFY_IS_TRUE(*back == control);

        pressable += CanBePressed(control) ? 1 : 0;
        slidable += CanSlide(control) ? 1 : 0;
    }

    // 14 buttons, 2 triggers and 8 stick directions; 2 triggers and 4 stick axes.
    VERIFY_ARE_EQUAL(size_t{ 24 }, pressable);
    VERIFY_ARE_EQUAL(size_t{ 6 }, slidable);
    VERIFY_IS_FALSE(ControllerControlFromKey(L"A").has_value());

    std::set<std::wstring> messageKeys{};

    for (size_t i = 0; i < ControllerMessageKindCount; i++)
    {
        auto const kind = static_cast<ControllerMessageKind>(i);
        auto const key = std::wstring{ ControllerMessageKindKey(kind) };

        VERIFY_IS_TRUE(messageKeys.insert(key).second);
        VERIFY_IS_TRUE(ControllerMessageKindFromKey(key) == kind);
    }

    VERIFY_IS_FALSE(ControllerMessageKindFromKey(L"sysex").has_value());
}

void ControllerStepTests::LearnFindsTheControlThatMoved()
{
    auto const rest = Resting();

    auto pressed = rest;
    pressed.Buttons = 0x2000;

    VERIFY_IS_TRUE(FindMovedControl(rest, pressed, true) == ControllerControl::B);

    // Already down before Learn looked is not a press.
    VERIFY_IS_FALSE(FindMovedControl(pressed, pressed, true).has_value());

    // A stick pushed down is a stick direction for a button, and the stick for a stick step.
    auto pushed = rest;
    pushed.LeftStickY = 0.0;

    VERIFY_IS_TRUE(FindMovedControl(rest, pushed, true) == ControllerControl::LeftStickDown);
    VERIFY_IS_TRUE(FindMovedControl(rest, pushed, false) == ControllerControl::LeftStickY);
    VERIFY_IS_FALSE(FindMovedControl(pushed, pushed, true).has_value());

    // A trigger works for both.
    auto squeezed = rest;
    squeezed.RightTrigger = 0.9;

    VERIFY_IS_TRUE(FindMovedControl(rest, squeezed, true) == ControllerControl::RightTrigger);
    VERIFY_IS_TRUE(FindMovedControl(rest, squeezed, false) == ControllerControl::RightTrigger);

    // A stick that drifts a little is not a move, and a button is nothing to a stick step.
    auto drifted = rest;
    drifted.RightStickX = 0.6;

    VERIFY_IS_FALSE(FindMovedControl(rest, drifted, false).has_value());
    VERIFY_IS_FALSE(FindMovedControl(rest, drifted, true).has_value());
    VERIFY_IS_FALSE(FindMovedControl(rest, pressed, false).has_value());

    auto gone = pushed;
    gone.Connected = false;

    VERIFY_IS_FALSE(FindMovedControl(rest, gone, true).has_value());
}

// ------------------------------------------------------------------ the words

void ControllerStepTests::ANoteGoesOnAndOffAsMidi2()
{
    auto message = Note(60);
    message.Group = 2;
    message.Channel = 3;

    auto const words = BuildControllerWords(message, 0);

    VERIFY_ARE_EQUAL(uint8_t{ 2 }, words.OnCount);
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, words.OffCount);
    VERIFY_IS_TRUE(FromWords(words.On.data(), words.OnCount) == Midi2(2, NoteOn, 3, 60, 0, Velocity16(DefaultControllerVelocityHundredths) << 16));
    VERIFY_IS_TRUE(FromWords(words.Off.data(), words.OffCount) == Midi2(2, NoteOff, 3, 60, 0, 0));

    // The default velocity is 100 of 127, so a MIDI 1.0 device hears 100.
    VERIFY_ARE_EQUAL(uint32_t{ 100 }, Velocity16(DefaultControllerVelocityHundredths) >> 9);

    // A velocity of 0 would be a note off, so the quietest note on is 1.
    message.ValueHundredths = 0;

    auto const quiet = BuildControllerWords(message, 0);
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, quiet.On[1] >> 16);

    // No note off when it says not to.
    message.SendsRelease = false;
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, BuildControllerWords(message, 0).OffCount);
}

void ControllerStepTests::ANoteCanGoAsMidi1()
{
    auto message = Note(64);
    message.Midi1Protocol = true;

    auto const words = BuildControllerWords(message, 0);

    VERIFY_ARE_EQUAL(uint8_t{ 1 }, words.OnCount);
    VERIFY_IS_TRUE(FromWords(words.On.data(), words.OnCount) == Midi1(0, NoteOn, 0, 64, 100));
    VERIFY_IS_TRUE(FromWords(words.Off.data(), words.OffCount) == Midi1(0, NoteOff, 0, 64, 0));

    message.ValueHundredths = 0;
    VERIFY_IS_TRUE(FromWords(BuildControllerWords(message, 0).On.data(), 1) == Midi1(0, NoteOn, 0, 64, 1));
}

void ControllerStepTests::AValueUsesTheWholeMidi2Range()
{
    auto message = ControllerMessageFor(ControllerMessageKind::ControlChange);
    message.Number = 74;

    auto words = BuildControllerWords(message, 0);

    VERIFY_IS_TRUE(FromWords(words.On.data(), words.OnCount) == Midi2(0, ControlChange, 0, 74, 0, 0xFFFFFFFF));
    VERIFY_IS_TRUE(FromWords(words.Off.data(), words.OffCount) == Midi2(0, ControlChange, 0, 74, 0, 0));

    message.Midi1Protocol = true;
    words = BuildControllerWords(message, 0);

    VERIFY_IS_TRUE(FromWords(words.On.data(), words.OnCount) == Midi1(0, ControlChange, 0, 74, 127));

    // A pitch bend springs back to the exact middle.
    auto bend = ControllerMessageFor(ControllerMessageKind::PitchBend);
    words = BuildControllerWords(bend, 0);

    VERIFY_IS_TRUE(FromWords(words.Off.data(), words.OffCount) == Midi2(0, PitchBend, 0, 0, 0, 0x80000000));

    // An RPN sets a parameter and leaves it set.
    auto parameter = ControllerMessageFor(ControllerMessageKind::RegisteredController);
    parameter.Number = (1 << 7) | 2;
    words = BuildControllerWords(parameter, 0);

    VERIFY_ARE_EQUAL(uint8_t{ 2 }, words.OnCount);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, words.OffCount);
    VERIFY_ARE_EQUAL(uint32_t{ 0x40200102 }, words.On[0]);
}

void ControllerStepTests::AProgramChangeSendsNoBank()
{
    auto message = ControllerMessageFor(ControllerMessageKind::ProgramChange);
    message.Number = 5;

    auto words = BuildControllerWords(message, 5);

    VERIFY_IS_TRUE(FromWords(words.On.data(), words.OnCount) == Midi2(0, ProgramChange, 0, 0, 0, 0x05000000));
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, words.OffCount);

    message.Midi1Protocol = true;
    words = BuildControllerWords(message, 5);

    VERIFY_IS_TRUE(FromWords(words.On.data(), words.OnCount) == Midi1(0, ProgramChange, 0, 5, 0));
}

void ControllerStepTests::StartContinueAndStopAreSystemMessages()
{
    auto message = ControllerMessageFor(ControllerMessageKind::Start);
    message.Group = 3;

    auto words = BuildControllerWords(message, 0);

    VERIFY_IS_TRUE(FromWords(words.On.data(), words.OnCount) == System(3, 0xFA));
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, words.OffCount);

    VERIFY_IS_TRUE(FromWords(BuildControllerWords(ControllerMessageFor(ControllerMessageKind::Continue), 0).On.data(), 1) == System(0, 0xFB));
    VERIFY_IS_TRUE(FromWords(BuildControllerWords(ControllerMessageFor(ControllerMessageKind::Stop), 0).On.data(), 1) == System(0, 0xFC));
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, BuildControllerWords(ControllerMessageFor(ControllerMessageKind::Nothing), 0).OnCount);
}

void ControllerStepTests::TypedWordsMustBeOneWholeMessage()
{
    auto message = ControllerMessageFor(ControllerMessageKind::Words);

    message.WordCount = 2;
    message.Words = { 0x40903C00, 0xC8000000, 0, 0 };
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, BuildControllerWords(message, 0).OnCount);

    // A MIDI 2.0 message needs both of its words.
    message.WordCount = 1;
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, BuildControllerWords(message, 0).OnCount);

    message.Words = { 0x20903C64, 0, 0, 0 };
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, BuildControllerWords(message, 0).OnCount);

    message.WordCount = 4;
    message.Words = { 0x50000000, 1, 2, 3 };
    VERIFY_ARE_EQUAL(uint8_t{ 4 }, BuildControllerWords(message, 0).OnCount);

    message.WordCount = 3;
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, BuildControllerWords(message, 0).OnCount);
}

// ------------------------------------------------------------------ a button

void ControllerStepTests::APressSendsAndLettingGoEndsIt()
{
    ControllerButtonSettings settings{};
    settings.Pressed = Note(60);

    ControllerButtonMachine machine{ nullptr };
    Sent sent{};

    machine.Update(settings, 0, false, sent.Sink());
    VERIFY_ARE_EQUAL(size_t{ 0 }, sent.Take().size());

    machine.Update(settings, 10, true, sent.Sink());

    auto taken = sent.Take();
    VERIFY_ARE_EQUAL(size_t{ 1 }, taken.size());
    VERIFY_IS_TRUE(taken[0] == Midi2NoteOn(60, Velocity16(DefaultControllerVelocityHundredths)));

    // Still down is not another press, and nothing is held.
    machine.Update(settings, 2000, true, sent.Sink());
    VERIFY_ARE_EQUAL(size_t{ 0 }, sent.Take().size());

    machine.Update(settings, 2010, false, sent.Sink());

    taken = sent.Take();
    VERIFY_ARE_EQUAL(size_t{ 1 }, taken.size());
    VERIFY_IS_TRUE(taken[0] == Midi2(0, NoteOff, 0, 60, 0, 0));
}

void ControllerStepTests::HoldingSendsTheHeldMessageAndRepeats()
{
    ControllerButtonSettings settings{};
    settings.Pressed = Note(60);
    settings.Held = Note(72);
    settings.HoldMilliseconds = 500;
    settings.Repeats = true;
    settings.RepeatMilliseconds = 150;

    VERIFY_IS_TRUE(settings.SendsWhenHeld());

    ControllerButtonMachine machine{ nullptr };
    Sent sent{};

    machine.Update(settings, 1000, true, sent.Sink());
    VERIFY_ARE_EQUAL(size_t{ 1 }, sent.Take().size());

    machine.Update(settings, 1499, true, sent.Sink());
    VERIFY_ARE_EQUAL(size_t{ 0 }, sent.Take().size());

    machine.Update(settings, 1500, true, sent.Sink());

    auto taken = sent.Take();
    VERIFY_ARE_EQUAL(size_t{ 1 }, taken.size());
    VERIFY_ARE_EQUAL(uint32_t{ 72 }, (taken[0].Words[0] >> 8) & 0x7F);

    machine.Update(settings, 1649, true, sent.Sink());
    VERIFY_ARE_EQUAL(size_t{ 0 }, sent.Take().size());

    // A repeat ends the held note and starts it again.
    machine.Update(settings, 1650, true, sent.Sink());

    taken = sent.Take();
    VERIFY_ARE_EQUAL(size_t{ 2 }, taken.size());
    VERIFY_IS_TRUE(taken[0] == Midi2(0, NoteOff, 0, 72, 0, 0));
    VERIFY_ARE_EQUAL(uint32_t{ NoteOn }, (taken[1].Words[0] >> 20) & 0x0F);

    machine.Update(settings, 1800, true, sent.Sink());
    VERIFY_ARE_EQUAL(size_t{ 2 }, sent.Take().size());

    // Letting go ends both.
    machine.Update(settings, 1810, false, sent.Sink());

    taken = sent.Take();
    VERIFY_ARE_EQUAL(size_t{ 2 }, taken.size());
    VERIFY_IS_TRUE(taken[0] == Midi2(0, NoteOff, 0, 60, 0, 0));
    VERIFY_IS_TRUE(taken[1] == Midi2(0, NoteOff, 0, 72, 0, 0));

    // Without repeat it goes once.
    settings.Repeats = false;

    machine.Update(settings, 3000, true, sent.Sink());
    machine.Update(settings, 3500, true, sent.Sink());
    machine.Update(settings, 4500, true, sent.Sink());
    VERIFY_ARE_EQUAL(size_t{ 2 }, sent.Take().size());
}

void ControllerStepTests::TapOrHoldSendsOnlyOne()
{
    ControllerButtonSettings settings{};
    settings.Pressed = Note(60);
    settings.Held = Note(72);
    settings.HoldMilliseconds = 500;
    settings.TapOrHold = true;

    ControllerButtonMachine machine{ nullptr };
    Sent sent{};

    // A tap: nothing until it lets go, then the pressed note, ended a moment later.
    machine.Update(settings, 0, true, sent.Sink());
    VERIFY_ARE_EQUAL(size_t{ 0 }, sent.Take().size());

    machine.Update(settings, 100, false, sent.Sink());

    auto taken = sent.Take();
    VERIFY_ARE_EQUAL(size_t{ 1 }, taken.size());
    VERIFY_ARE_EQUAL(uint32_t{ 60 }, (taken[0].Words[0] >> 8) & 0x7F);

    machine.Update(settings, 100 + TapMessageMilliseconds - 1, false, sent.Sink());
    VERIFY_ARE_EQUAL(size_t{ 0 }, sent.Take().size());

    machine.Update(settings, 100 + TapMessageMilliseconds, false, sent.Sink());

    taken = sent.Take();
    VERIFY_ARE_EQUAL(size_t{ 1 }, taken.size());
    VERIFY_IS_TRUE(taken[0] == Midi2(0, NoteOff, 0, 60, 0, 0));

    // A hold: only the held note, and no pressed note when it lets go.
    machine.Update(settings, 1000, true, sent.Sink());
    machine.Update(settings, 1500, true, sent.Sink());

    taken = sent.Take();
    VERIFY_ARE_EQUAL(size_t{ 1 }, taken.size());
    VERIFY_ARE_EQUAL(uint32_t{ 72 }, (taken[0].Words[0] >> 8) & 0x7F);

    machine.Update(settings, 1600, false, sent.Sink());

    taken = sent.Take();
    VERIFY_ARE_EQUAL(size_t{ 1 }, taken.size());
    VERIFY_IS_TRUE(taken[0] == Midi2(0, NoteOff, 0, 72, 0, 0));
}

void ControllerStepTests::AToggleSwitchesOnEachPress()
{
    ControllerButtonSettings settings{};
    settings.Pressed = ControllerMessageFor(ControllerMessageKind::ControlChange);
    settings.Pressed.Midi1Protocol = true;
    settings.Toggles = true;

    // Held messages don't apply to a toggle.
    settings.Held = Note(72);
    VERIFY_IS_FALSE(settings.SendsWhenHeld());

    ControllerButtonMachine machine{ nullptr };
    Sent sent{};

    machine.Update(settings, 0, true, sent.Sink());

    auto taken = sent.Take();
    VERIFY_ARE_EQUAL(size_t{ 1 }, taken.size());
    VERIFY_IS_TRUE(taken[0] == Midi1(0, ControlChange, 0, 64, 127));

    machine.Update(settings, 5000, true, sent.Sink());
    machine.Update(settings, 5010, false, sent.Sink());
    VERIFY_ARE_EQUAL(size_t{ 0 }, sent.Take().size());

    machine.Update(settings, 6000, true, sent.Sink());

    taken = sent.Take();
    VERIFY_ARE_EQUAL(size_t{ 1 }, taken.size());
    VERIFY_IS_TRUE(taken[0] == Midi1(0, ControlChange, 0, 64, 0));

    machine.Update(settings, 6010, false, sent.Sink());
    VERIFY_ARE_EQUAL(size_t{ 0 }, sent.Take().size());

    // Left on when the patch stops: it is switched off.
    machine.Update(settings, 7000, true, sent.Sink());
    machine.Update(settings, 7010, false, sent.Sink());
    sent.Take();

    machine.Release(sent.Sink());

    taken = sent.Take();
    VERIFY_ARE_EQUAL(size_t{ 1 }, taken.size());
    VERIFY_IS_TRUE(taken[0] == Midi1(0, ControlChange, 0, 64, 0));
}

void ControllerStepTests::NextAndPreviousShareOnePlace()
{
    auto const place = std::make_shared<std::atomic<int32_t>>(-1);
    std::vector<std::pair<uint8_t, uint8_t>> asked{};

    ProgramPositionFor const programs = [&](uint8_t group, uint8_t channel)
        {
            asked.emplace_back(group, channel);
            return place;
        };

    ControllerButtonSettings up{};
    up.Pressed = Program(ProgramStep::Next, 10, 12, true);

    auto down = up;
    down.Pressed.Step = ProgramStep::Previous;

    ControllerButtonMachine upButton{ programs };
    ControllerButtonMachine downButton{ programs };
    Sent sent{};

    uint64_t now{ 0 };

    auto const press = [&](ControllerButtonMachine& machine, ControllerButtonSettings const& settings)
        {
            machine.Update(settings, now += 10, true, sent.Sink());
            machine.Update(settings, now += 10, false, sent.Sink());

            auto const taken = sent.Take();
            VERIFY_ARE_EQUAL(size_t{ 1 }, taken.size());

            return ProgramOf(taken[0]);
        };

    // The first press starts at the lowest.
    VERIFY_ARE_EQUAL(uint8_t{ 10 }, press(upButton, up));
    VERIFY_ARE_EQUAL(uint8_t{ 11 }, press(upButton, up));
    VERIFY_ARE_EQUAL(uint8_t{ 10 }, press(downButton, down));
    VERIFY_ARE_EQUAL(uint8_t{ 12 }, press(downButton, down));
    VERIFY_ARE_EQUAL(uint8_t{ 10 }, press(upButton, up));
    VERIFY_IS_TRUE(std::all_of(asked.begin(), asked.end(), [](auto const& a) { return a.first == 0 && a.second == 0; }));

    // Without going around, the ends stay put.
    VERIFY_ARE_EQUAL(uint8_t{ 12 }, SteppedProgram(12, ProgramStep::Next, 10, 12, false));
    VERIFY_ARE_EQUAL(uint8_t{ 10 }, SteppedProgram(10, ProgramStep::Previous, 10, 12, false));

    // Outside the range, or typed high to low.
    VERIFY_ARE_EQUAL(uint8_t{ 10 }, SteppedProgram(50, ProgramStep::Next, 10, 12, true));
    VERIFY_ARE_EQUAL(uint8_t{ 12 }, SteppedProgram(50, ProgramStep::Previous, 10, 12, true));
    VERIFY_ARE_EQUAL(uint8_t{ 11 }, SteppedProgram(10, ProgramStep::Next, 12, 10, true));
    VERIFY_ARE_EQUAL(uint8_t{ 10 }, SteppedProgram(-1, ProgramStep::Previous, 10, 12, true));
}

void ControllerStepTests::ReleaseEndsWhatIsStillOn()
{
    ControllerButtonSettings settings{};
    settings.Pressed = Note(60);

    ControllerButtonMachine machine{ nullptr };
    Sent sent{};

    machine.Update(settings, 0, true, sent.Sink());
    sent.Take();

    // Changed while it's down: the note that is sounding is the one that ends.
    settings.Pressed = Note(67);

    machine.Release(sent.Sink());

    auto taken = sent.Take();
    VERIFY_ARE_EQUAL(size_t{ 1 }, taken.size());
    VERIFY_IS_TRUE(taken[0] == Midi2(0, NoteOff, 0, 60, 0, 0));

    // Nothing is left to end.
    machine.Release(sent.Sink());
    VERIFY_ARE_EQUAL(size_t{ 0 }, sent.Take().size());

    // Down again after a release is a new press.
    machine.Update(settings, 100, true, sent.Sink());

    taken = sent.Take();
    VERIFY_ARE_EQUAL(size_t{ 1 }, taken.size());
    VERIFY_ARE_EQUAL(uint32_t{ 67 }, (taken[0].Words[0] >> 8) & 0x7F);
}

// ------------------------------------------------------------------ a stick or trigger

void ControllerStepTests::ADeadZoneAndAShapeChangeTheValue()
{
    ControllerAxisSettings trigger{};
    trigger.Control = ControllerControl::RightTrigger;
    trigger.DeadZoneHundredths = 1000;

    VERIFY_ARE_EQUAL(0.0, ShapedAxisValue(trigger, 0.05));
    VERIFY_IS_TRUE(std::abs(ShapedAxisValue(trigger, 0.55) - 0.5) < 0.000001);
    VERIFY_ARE_EQUAL(1.0, ShapedAxisValue(trigger, 1.0));

    auto stick = trigger;
    stick.Control = ControllerControl::LeftStickY;

    VERIFY_ARE_EQUAL(0.5, ShapedAxisValue(stick, 0.5));
    VERIFY_ARE_EQUAL(0.5, ShapedAxisValue(stick, 0.525));
    VERIFY_ARE_EQUAL(0.0, ShapedAxisValue(stick, 0.0));
    VERIFY_ARE_EQUAL(1.0, ShapedAxisValue(stick, 1.0));

    trigger.Shape.Invert = true;
    VERIFY_ARE_EQUAL(0.0, ShapedAxisValue(trigger, 1.0));

    // A 7 bit MIDI 1.0 value has 128 steps. Everything else is cut into 1024.
    midiapp::ValueMessageTarget target{ midiapp::ValueMessageKind::ControlChange, 0, 0, 1, true };
    VERIFY_ARE_EQUAL(int64_t{ 128 }, AxisStepCount(target));

    target.Midi1Protocol = false;
    VERIFY_ARE_EQUAL(int64_t{ 1024 }, AxisStepCount(target));

    target = { midiapp::ValueMessageKind::PitchBend, 0, 0, 0, true };
    VERIFY_ARE_EQUAL(int64_t{ 1024 }, AxisStepCount(target));
}

void ControllerStepTests::AStickSendsOnlyWhenItMovesAStep()
{
    auto const settings = PitchBendStick();

    ControllerAxisMachine machine{};
    Sent sent{};

    // Where it is when the patch starts is not a move.
    machine.Update(settings, 0, true, 0.3, sent.Sink());
    VERIFY_ARE_EQUAL(size_t{ 0 }, sent.Take().size());

    machine.Update(settings, 4, true, 0.3, sent.Sink());
    VERIFY_ARE_EQUAL(size_t{ 0 }, sent.Take().size());

    machine.Update(settings, 8, true, 1.0, sent.Sink());

    auto taken = sent.Take();
    VERIFY_ARE_EQUAL(size_t{ 1 }, taken.size());
    VERIFY_IS_TRUE(taken[0] == Midi2(0, PitchBend, 0, 0, 0, 0xFFFFFFFF));

    // Less than a step is nothing.
    machine.Update(settings, 12, true, 0.99999, sent.Sink());
    VERIFY_ARE_EQUAL(size_t{ 0 }, sent.Take().size());

    // Inside the dead zone is the exact middle.
    machine.Update(settings, 16, true, 0.52, sent.Sink());

    taken = sent.Take();
    VERIFY_ARE_EQUAL(size_t{ 1 }, taken.size());
    VERIFY_IS_TRUE(taken[0] == Midi2(0, PitchBend, 0, 0, 0, 0x80000000));

    machine.Update(settings, 20, true, 0.48, sent.Sink());
    VERIFY_ARE_EQUAL(size_t{ 0 }, sent.Take().size());
}

void ControllerStepTests::TheIntervalHoldsBackButNeverLosesTheLastMove()
{
    ControllerAxisSettings settings{};
    settings.Control = ControllerControl::LeftTrigger;
    settings.Target = { midiapp::ValueMessageKind::ControlChange, 0, 0, 2, true };
    settings.DeadZoneHundredths = 0;
    settings.IntervalMilliseconds = 50;

    ControllerAxisMachine machine{};
    Sent sent{};

    machine.Update(settings, 0, true, 0.0, sent.Sink());
    machine.Update(settings, 10, true, 0.5, sent.Sink());
    VERIFY_ARE_EQUAL(size_t{ 1 }, sent.Take().size());

    machine.Update(settings, 20, true, 0.6, sent.Sink());
    machine.Update(settings, 59, true, 0.6, sent.Sink());
    VERIFY_ARE_EQUAL(size_t{ 0 }, sent.Take().size());

    machine.Update(settings, 60, true, 0.6, sent.Sink());

    auto const taken = sent.Take();
    VERIFY_ARE_EQUAL(size_t{ 1 }, taken.size());
    VERIFY_IS_TRUE(taken[0] == Midi1(0, ControlChange, 0, 2, static_cast<uint8_t>(midiapp::ScaleToBits(0.6, 7))));
}

void ControllerStepTests::AStickGoesBackToRestWhenItStops()
{
    auto settings = PitchBendStick();

    ControllerAxisMachine machine{};
    Sent sent{};

    // Never moved: nothing to put back.
    machine.Update(settings, 0, true, 0.5, sent.Sink());
    machine.Release(settings, sent.Sink());
    VERIFY_ARE_EQUAL(size_t{ 0 }, sent.Take().size());

    machine.Update(settings, 0, true, 0.5, sent.Sink());
    machine.Update(settings, 10, true, 0.0, sent.Sink());
    VERIFY_ARE_EQUAL(size_t{ 1 }, sent.Take().size());

    machine.Release(settings, sent.Sink());

    auto taken = sent.Take();
    VERIFY_ARE_EQUAL(size_t{ 1 }, taken.size());
    VERIFY_IS_TRUE(taken[0] == Midi2(0, PitchBend, 0, 0, 0, 0x80000000));

    // A controller that goes away counts as let go, once.
    machine.Update(settings, 0, true, 0.5, sent.Sink());
    machine.Update(settings, 10, true, 1.0, sent.Sink());
    sent.Take();

    machine.Update(settings, 20, false, 0.5, sent.Sink());
    machine.Update(settings, 30, false, 0.5, sent.Sink());

    taken = sent.Take();
    VERIFY_ARE_EQUAL(size_t{ 1 }, taken.size());
    VERIFY_IS_TRUE(taken[0] == Midi2(0, PitchBend, 0, 0, 0, 0x80000000));

    // Not when it says not to.
    settings.ReturnsToRest = false;

    machine.Update(settings, 40, true, 1.0, sent.Sink());
    sent.Take();
    machine.Release(settings, sent.Sink());
    VERIFY_ARE_EQUAL(size_t{ 0 }, sent.Take().size());
}

// ------------------------------------------------------------------ the file

void ControllerStepTests::ControllerSettingsSurviveTheFile()
{
    auto button = DefaultBlockSettings(BlockKind::ControllerButton);

    button.ControllerButton.Player = 2;
    button.ControllerButton.Control = ControllerControl::LeftStickUp;
    button.ControllerButton.Pressed = ControllerMessageFor(ControllerMessageKind::ControlChange);
    button.ControllerButton.Pressed.Group = 3;
    button.ControllerButton.Pressed.Channel = 5;
    button.ControllerButton.Pressed.Number = 74;
    button.ControllerButton.Pressed.ValueHundredths = 2550;
    button.ControllerButton.Pressed.ReleaseHundredths = 1000;
    button.ControllerButton.Pressed.Midi1Protocol = true;
    button.ControllerButton.Held = Program(ProgramStep::Previous, 20, 10, false);
    button.ControllerButton.HoldMilliseconds = 750;
    button.ControllerButton.Repeats = true;
    button.ControllerButton.RepeatMilliseconds = 300;
    button.ControllerButton.TapOrHold = true;
    button.ControllerButton.Scale = ValueScale::SevenBit;

    auto read = BlockSettingsFromJson(BlockKind::ControllerButton, BlockSettingsToJson(BlockKind::ControllerButton, button));

    VERIFY_IS_TRUE(BlockSettingsSignature(BlockKind::ControllerButton, read) == BlockSettingsSignature(BlockKind::ControllerButton, button));
    VERIFY_IS_TRUE(read.ControllerButton.Held.LowestProgram == 20);

    // Typed words, and a note with no note off.
    button.ControllerButton.Pressed = ControllerMessageFor(ControllerMessageKind::Words);
    button.ControllerButton.Pressed.WordCount = 2;
    button.ControllerButton.Pressed.Words = { 0x40903C00, 0xC8000000, 0, 0 };
    button.ControllerButton.Held = Note(36);
    button.ControllerButton.Held.SendsRelease = false;
    button.ControllerButton.Held.ValueHundredths = 10000;

    read = BlockSettingsFromJson(BlockKind::ControllerButton, BlockSettingsToJson(BlockKind::ControllerButton, button));

    VERIFY_IS_TRUE(BlockSettingsSignature(BlockKind::ControllerButton, read) == BlockSettingsSignature(BlockKind::ControllerButton, button));

    auto axis = DefaultBlockSettings(BlockKind::ControllerAxis);

    VERIFY_IS_TRUE(axis.ControllerAxis.Target.Kind == midiapp::ValueMessageKind::PitchBend);

    axis.ControllerAxis.Player = 3;
    axis.ControllerAxis.Control = ControllerControl::RightTrigger;
    axis.ControllerAxis.Target = { midiapp::ValueMessageKind::AssignableController, 1, 2, 300, false };
    axis.ControllerAxis.DeadZoneHundredths = 2500;
    axis.ControllerAxis.Shape.Invert = true;
    axis.ControllerAxis.Shape.Curve = ValueCurve::FastRise;
    axis.ControllerAxis.Shape.InputMinimumHundredths = 1000;
    axis.ControllerAxis.Shape.OutputMaximumHundredths = 9000;
    axis.ControllerAxis.IntervalMilliseconds = 25;
    axis.ControllerAxis.ReturnsToRest = false;
    axis.ControllerAxis.Scale = ValueScale::SevenBit;

    auto const readAxis = BlockSettingsFromJson(BlockKind::ControllerAxis, BlockSettingsToJson(BlockKind::ControllerAxis, axis));

    VERIFY_IS_TRUE(BlockSettingsSignature(BlockKind::ControllerAxis, readAxis) == BlockSettingsSignature(BlockKind::ControllerAxis, axis));
    VERIFY_ARE_EQUAL(uint32_t{ 300 }, readAxis.ControllerAxis.Target.Number);

    // Both run on their own, with nothing going in.
    VERIFY_IS_TRUE(IsGenerator(BlockKind::ControllerButton));
    VERIFY_IS_TRUE(IsGenerator(BlockKind::ControllerAxis));
    VERIFY_IS_FALSE(HasInput(BlockKind::ControllerButton));
    VERIFY_IS_FALSE(HasInput(BlockKind::ControllerAxis));
}

void ControllerStepTests::BadValuesInTheFileFallBack()
{
    auto const button = ControllerButtonFromJson(json::JsonObject::Parse(
        LR"({"player":9,"control":"leftStickX","pressed":{"message":"bogus","channel":16,"number":200,"velocityPercent":150},)"
        LR"("held":{"message":"words","messageWords":[1077951488,"x",5]},"holdMilliseconds":-5,"repeatMilliseconds":1.5})"));

    VERIFY_ARE_EQUAL(uint8_t{ 0 }, button.Player);
    VERIFY_IS_TRUE(button.Control == ControllerControl::A);
    VERIFY_IS_TRUE(button.Pressed.Kind == ControllerMessageKind::Note);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, button.Pressed.Channel);
    VERIFY_ARE_EQUAL(uint32_t{ 60 }, button.Pressed.Number);
    VERIFY_ARE_EQUAL(DefaultControllerVelocityHundredths, button.Pressed.ValueHundredths);
    VERIFY_IS_TRUE(button.Held.Kind == ControllerMessageKind::Words);
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, button.Held.WordCount);
    VERIFY_ARE_EQUAL(uint32_t{ 1077951488 }, button.Held.Words[0]);
    VERIFY_ARE_EQUAL(DefaultHoldMilliseconds, button.HoldMilliseconds);
    VERIFY_ARE_EQUAL(DefaultRepeatMilliseconds, button.RepeatMilliseconds);

    // Below the shortest hold is the default too, not the shortest.
    VERIFY_ARE_EQUAL(DefaultHoldMilliseconds,
        ControllerButtonFromJson(json::JsonObject::Parse(LR"({"holdMilliseconds":50})")).HoldMilliseconds);
    VERIFY_ARE_EQUAL(250, ControllerButtonFromJson(json::JsonObject::Parse(LR"({"holdMilliseconds":250})")).HoldMilliseconds);
    VERIFY_ARE_EQUAL(2500, ControllerAxisFromJson(json::JsonObject::Parse(LR"({"deadZonePercent":25})")).DeadZoneHundredths);

    // A button control is no good for a stick step, a dead zone past half is the default, and an
    // NRPN is always MIDI 2.0.
    auto const axis = ControllerAxisFromJson(json::JsonObject::Parse(LR"({"control":"a","deadZonePercent":90,"message":"nrpn","midi1":true})"));

    VERIFY_IS_TRUE(axis.Control == ControllerControl::LeftStickX);
    VERIFY_ARE_EQUAL(DefaultDeadZoneHundredths, axis.DeadZoneHundredths);
    VERIFY_IS_TRUE(axis.Target.Kind == midiapp::ValueMessageKind::AssignableController);
    VERIFY_IS_FALSE(axis.Target.Midi1Protocol);

    // Nothing at all is a new step.
    auto const empty = ControllerButtonFromJson(json::JsonObject{});

    VERIFY_IS_TRUE(ControllerButtonSignature(empty) == ControllerButtonSignature(ControllerButtonSettings{}));
    VERIFY_IS_TRUE(ControllerAxisSignature(ControllerAxisFromJson(json::JsonObject{})) == ControllerAxisSignature(ControllerAxisSettings{}));
}

void ControllerStepTests::OnlyTheControllerAndControlRestartAStep()
{
    auto settings = DefaultBlockSettings(BlockKind::ControllerButton);
    auto const before = GeneratorRestartSignature(BlockKind::ControllerButton, settings);

    // A new message takes effect on the next press, with no restart.
    settings.ControllerButton.Pressed = Note(48);
    VERIFY_IS_TRUE(GeneratorRestartSignature(BlockKind::ControllerButton, settings) == before);

    settings.ControllerButton.Player = 1;
    VERIFY_IS_FALSE(GeneratorRestartSignature(BlockKind::ControllerButton, settings) == before);

    settings.ControllerButton.Player = 0;
    settings.ControllerButton.Control = ControllerControl::B;
    VERIFY_IS_FALSE(GeneratorRestartSignature(BlockKind::ControllerButton, settings) == before);

    auto axis = DefaultBlockSettings(BlockKind::ControllerAxis);
    auto const axisBefore = GeneratorRestartSignature(BlockKind::ControllerAxis, axis);

    axis.ControllerAxis.DeadZoneHundredths = 0;
    VERIFY_IS_TRUE(GeneratorRestartSignature(BlockKind::ControllerAxis, axis) == axisBefore);

    axis.ControllerAxis.Control = ControllerControl::RightStickY;
    VERIFY_IS_FALSE(GeneratorRestartSignature(BlockKind::ControllerAxis, axis) == axisBefore);
}

void ControllerStepTests::AFileWrittenTheWayTheGuideSaysReads()
{
    // The game controller examples in the guide for AI agents, the way an agent writes them.
    auto const patch = ReadPatchJson(LR"({
        "fileVersion": 2,
        "endpoints": [ { "id": "synth", "displayName": "Synth" }, { "id": "keys", "displayName": "Keys" } ],
        "blocks": [
            { "id": "next-program", "type": "controllerButton", "settings": { "player": 0, "control": "dpadUp",
              "pressed": { "message": "programChange", "step": "next", "lowest": 0, "highest": 7 },
              "held": { "message": "programChange", "step": "next", "lowest": 0, "highest": 7 },
              "holdMilliseconds": 500, "repeats": true, "repeatMilliseconds": 300 } },
            { "id": "x1", "type": "controllerButton", "settings": { "player": 1, "control": "leftStickUp", "pressed": { "message": "controlChange" } } },
            { "id": "bend", "type": "controllerAxis", "settings": { "player": 0, "control": "leftStickX", "message": "pitchBend", "channel": 0 } }
        ],
        "connections": [
            { "id": "c1", "source": "next-program", "destination": "synth" },
            { "id": "c2", "source": "x1", "destination": "synth" },
            { "id": "c3", "source": "bend", "destination": "synth" },
            { "id": "c4", "source": "keys", "destination": "bend" }
        ] })", L"Guide");

    VERIFY_IS_TRUE(patch.has_value());
    VERIFY_IS_FALSE(patch->IsFromNewerVersion);
    VERIFY_IS_NOT_NULL(patch->FindConnection(L"c3"));

    // Nothing goes into a game controller step.
    VERIFY_IS_NULL(patch->FindConnection(L"c4"));

    auto const* next = patch->FindBlock(L"next-program");
    VERIFY_IS_NOT_NULL(next);

    auto const& button = next->Settings.ControllerButton;
    VERIFY_IS_TRUE(button.Control == ControllerControl::DPadUp);
    VERIFY_IS_TRUE(button.Pressed.Kind == ControllerMessageKind::ProgramChange);
    VERIFY_IS_TRUE(button.Pressed.Step == ProgramStep::Next);
    VERIFY_ARE_EQUAL(uint8_t{ 7 }, button.Held.HighestProgram);
    VERIFY_IS_TRUE(button.SendsWhenHeld());
    VERIFY_IS_TRUE(button.Repeats);
    VERIFY_ARE_EQUAL(300, button.RepeatMilliseconds);

    // A control change with nothing else is the sustain pedal.
    auto const* x1 = patch->FindBlock(L"x1");
    VERIFY_IS_NOT_NULL(x1);
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, x1->Settings.ControllerButton.Player);
    VERIFY_IS_TRUE(x1->Settings.ControllerButton.Control == ControllerControl::LeftStickUp);
    VERIFY_ARE_EQUAL(uint32_t{ 64 }, x1->Settings.ControllerButton.Pressed.Number);
    VERIFY_ARE_EQUAL(FullScaleHundredths, x1->Settings.ControllerButton.Pressed.ValueHundredths);
    VERIFY_IS_TRUE(x1->Settings.ControllerButton.Pressed.SendsRelease);

    auto const* bend = patch->FindBlock(L"bend");
    VERIFY_IS_NOT_NULL(bend);
    VERIFY_IS_TRUE(bend->Settings.ControllerAxis.Target.Kind == midiapp::ValueMessageKind::PitchBend);
    VERIFY_IS_TRUE(bend->Settings.ControllerAxis.ReturnsToRest);
}
