// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "LearnCaptureTests.h"

#include "LearnCapture.h"

#include <cmath>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    glass::LearnedBinding Learn(_In_ std::vector<uint32_t> const& words)
    {
        glass::LearnedBinding learned{};

        VERIFY_IS_TRUE(glass::TryLearnFromWords(
            words.data(), static_cast<uint32_t>(words.size()), learned));

        return learned;
    }

    void VerifyNear(_In_ double expected, _In_ double actual)
    {
        VERIFY_IS_LESS_THAN(std::abs(expected - actual), 0.001);
    }
}

// ---- reading a message back into a binding ----

void LearnCaptureTests::LearnsAMidi1ControlChange()
{
    // 20B0077F reads back as control change 7, value 127, group 0, channel 1. Verified on the
    // wire in phase 4, which is why that word is the one used here.
    auto const learned = Learn({ 0x20B0077F });

    VERIFY_IS_TRUE(learned.Kind == glass::MessageKind::ControlChange);
    VERIFY_ARE_EQUAL(7u, learned.Number);
    VERIFY_ARE_EQUAL(0, learned.ChannelIndex);
    VerifyNear(1.0, learned.Value);
}

void LearnCaptureTests::LearnsAMidi2ControlChange()
{
    auto const learned = Learn({ 0x40B00700, 0x80000000 });

    VERIFY_IS_TRUE(learned.Kind == glass::MessageKind::ControlChange);
    VERIFY_ARE_EQUAL(7u, learned.Number);
    VerifyNear(0.5, learned.Value);
}

void LearnCaptureTests::LearnsANote()
{
    auto const midi1 = Learn({ 0x20903C7F });

    VERIFY_IS_TRUE(midi1.Kind == glass::MessageKind::Note);
    VERIFY_ARE_EQUAL(60u, midi1.Number);
    VerifyNear(1.0, midi1.Value);

    auto const midi2 = Learn({ 0x40903C00, 0xFFFF0000 });

    VERIFY_IS_TRUE(midi2.Kind == glass::MessageKind::Note);
    VERIFY_ARE_EQUAL(60u, midi2.Number);
    VerifyNear(1.0, midi2.Value);
}

void LearnCaptureTests::LearnsPitchBend()
{
    // 20E07F7F is the top of a 14 bit bend, and 20E00040 is the center.
    auto const top = Learn({ 0x20E07F7F });

    VERIFY_IS_TRUE(top.Kind == glass::MessageKind::PitchBend);
    VerifyNear(1.0, top.Value);

    auto const center = Learn({ 0x20E00040 });

    VerifyNear(0.5, center.Value);
}

void LearnCaptureTests::LearnsARegisteredController()
{
    // 40200509 is a registered controller, bank 5, index 9.
    auto const learned = Learn({ 0x40200509, 0x12345678 });

    VERIFY_IS_TRUE(learned.Kind == glass::MessageKind::RegisteredController);
    VERIFY_ARE_EQUAL(uint32_t{ (5u << 7) | 9u }, learned.Number);
}

void LearnCaptureTests::TakesTheGroupAndTheChannelToo()
{
    // Capturing only the number is why remapping a controller is usually an hour of typing.
    auto const learned = Learn({ 0x25B90A40 });

    VERIFY_ARE_EQUAL(5, learned.GroupIndex);
    VERIFY_ARE_EQUAL(9, learned.ChannelIndex);
    VERIFY_ARE_EQUAL(10u, learned.Number);
}

void LearnCaptureTests::IgnoresWhatCannotBeBound()
{
    glass::LearnedBinding learned{};

    // Clock, active sensing, system exclusive and stream messages all arrive constantly on a
    // busy port, and none of them is something a control can be bound to.
    uint32_t const clock[]{ 0x10F80000 };
    uint32_t const sysex[]{ 0x30037E7F, 0x06000000 };
    uint32_t const stream[]{ 0xF0000000, 0, 0, 0 };

    VERIFY_IS_FALSE(glass::TryLearnFromWords(clock, 1, learned));
    VERIFY_IS_FALSE(glass::TryLearnFromWords(sysex, 2, learned));
    VERIFY_IS_FALSE(glass::TryLearnFromWords(stream, 4, learned));
    VERIFY_IS_FALSE(glass::TryLearnFromWords(nullptr, 0, learned));
}

// ---- what not to take ----

void LearnCaptureTests::ANoteOffDoesNotArmAnything()
{
    // A note off and a control change at zero both arrive while somebody is still reaching for
    // the control they mean. Taking the first message that turns up would bind the wrong thing.
    VERIFY_IS_FALSE(glass::IsWorthLearning(Learn({ 0x20803C00 })));
    VERIFY_IS_FALSE(glass::IsWorthLearning(Learn({ 0x20B00700 })));
    VERIFY_IS_TRUE(glass::IsWorthLearning(Learn({ 0x20B00701 })));
}

void LearnCaptureTests::PitchBendAtCenterDoesNotArmAnything()
{
    // A wheel sitting at rest sends center constantly on some hardware, so center is the one
    // value that means nothing happened.
    VERIFY_IS_FALSE(glass::IsWorthLearning(Learn({ 0x20E00040 })));
    VERIFY_IS_TRUE(glass::IsWorthLearning(Learn({ 0x20E07F7F })));
    VERIFY_IS_TRUE(glass::IsWorthLearning(Learn({ 0x20E00000 })));
}

// ---- writing it back ----

void LearnCaptureTests::EveryFieldCanBeRefused()
{
    glass::LearnedBinding learned{};

    learned.DeviceName = L"Controller";
    learned.Kind = glass::MessageKind::Note;
    learned.GroupIndex = 3;
    learned.ChannelIndex = 9;
    learned.Number = 42;

    glass::ControlMessage message{};

    message.DeviceName = L"Synth";
    message.Kind = glass::MessageKind::ControlChange;
    message.GroupIndex = 0;
    message.ChannelIndex = 0;
    message.Number = 7;

    glass::LearnAcceptance accept{ false, false, false, false, false };

    glass::ApplyLearned(learned, accept, message);

    VERIFY_ARE_EQUAL(std::wstring{ L"Synth" }, message.DeviceName);
    VERIFY_IS_TRUE(message.Kind == glass::MessageKind::ControlChange);
    VERIFY_ARE_EQUAL(0, message.GroupIndex);
    VERIFY_ARE_EQUAL(0, message.ChannelIndex);
    VERIFY_ARE_EQUAL(7u, message.Number);
}

void LearnCaptureTests::LockingTheDeviceTakesOnlyTheNumber()
{
    glass::LearnedBinding learned{};

    learned.DeviceName = L"Controller";
    learned.Kind = glass::MessageKind::Note;
    learned.GroupIndex = 3;
    learned.ChannelIndex = 9;
    learned.Number = 42;

    glass::ControlMessage message{};

    message.DeviceName = L"Synth";
    message.Kind = glass::MessageKind::ControlChange;
    message.ChannelIndex = 2;
    message.Number = 7;

    glass::LearnAcceptance accept{};

    accept.Device = false;
    accept.Group = false;
    accept.Channel = false;
    accept.Kind = false;
    accept.Number = true;

    glass::ApplyLearned(learned, accept, message);

    VERIFY_ARE_EQUAL(std::wstring{ L"Synth" }, message.DeviceName);
    VERIFY_IS_TRUE(message.Kind == glass::MessageKind::ControlChange);
    VERIFY_ARE_EQUAL(2, message.ChannelIndex);
    VERIFY_ARE_EQUAL(42u, message.Number);
}

void LearnCaptureTests::LearningFeedbackTurnsItOn()
{
    glass::LearnedBinding learned{};

    learned.DeviceName = L"DAW";
    learned.Kind = glass::MessageKind::ControlChange;
    learned.Number = 30;

    glass::FeedbackBinding feedback{};

    VERIFY_IS_FALSE(feedback.Enabled);

    glass::ApplyLearned(learned, glass::LearnAcceptance{}, feedback);

    // Capturing a feedback binding and leaving it switched off would look exactly like it not
    // having worked.
    VERIFY_IS_TRUE(feedback.Enabled);
    VERIFY_ARE_EQUAL(30u, feedback.Number);
    VERIFY_ARE_EQUAL(std::wstring{ L"DAW" }, feedback.DeviceName);
}

void LearnCaptureTests::OneKnobHeldIsOneBinding()
{
    auto first = Learn({ 0x20B00710 });
    auto second = Learn({ 0x20B00740 });

    first.DeviceName = L"Controller";
    second.DeviceName = L"Controller";

    // A knob swept across its travel sends a hundred messages. During a bank learn they must
    // fill one control, not a hundred.
    VERIFY_IS_TRUE(glass::IsSameBinding(first, second));

    auto other = Learn({ 0x20B00840 });
    other.DeviceName = L"Controller";

    VERIFY_IS_FALSE(glass::IsSameBinding(first, other));
}
