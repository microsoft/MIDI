// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "ActionPlanTests.h"

#include "ActionPlan.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    std::vector<glass::PreparedDestination> OneDevice()
    {
        glass::PreparedDestination destination{};

        destination.Name = L"Synth";
        destination.IsAvailable = true;

        return { destination };
    }

    glass::LayoutDocument DocumentWithOneControl(_In_ glass::Control control)
    {
        glass::LayoutDocument document{};

        document.Name = L"Test";

        glass::DeviceEntry device{};
        device.Name = L"Synth";
        document.Devices.push_back(device);

        glass::Page page{};
        page.Id = L"page-1";
        page.Name = L"Page 1";
        page.Controls.push_back(std::move(control));

        document.Pages.push_back(std::move(page));

        return document;
    }

    glass::Control ButtonSending(_In_ glass::ControlMessage message)
    {
        glass::Control control{};

        control.Id = L"control-1";
        control.Kind = glass::ControlKind::Button;
        control.Messages.push_back(std::move(message));

        return control;
    }

    glass::ControlMessage SysExMessage(_In_ std::vector<uint8_t> bytes)
    {
        glass::ControlMessage message{};

        message.Trigger = glass::MessageTrigger::TurnsOn;
        message.Kind = glass::MessageKind::SystemExclusive;
        message.DeviceName = L"Synth";
        message.SystemExclusive = std::move(bytes);

        return message;
    }
}

// ---- system exclusive into UMP ----

void ActionPlanTests::AShortDumpIsOneCompletePacket()
{
    std::vector<uint32_t> words{};

    // Three payload bytes fit in one packet, so the status is "complete in one" rather than a
    // start with no end behind it.
    VERIFY_IS_TRUE(glass::PacketizeSystemExclusive({ 0x7E, 0x7F, 0x06 }, 0, words));

    VERIFY_ARE_EQUAL(size_t{ 2 }, words.size());
    VERIFY_ARE_EQUAL(0x30037E7Fu, words[0]);
    VERIFY_ARE_EQUAL(0x06000000u, words[1]);
}

void ActionPlanTests::ALongDumpIsStartContinueEnd()
{
    std::vector<uint8_t> bytes(14, 0x01);

    std::vector<uint32_t> words{};

    VERIFY_IS_TRUE(glass::PacketizeSystemExclusive(bytes, 0, words));

    // Fourteen bytes is six, six and two.
    VERIFY_ARE_EQUAL(size_t{ 6 }, words.size());

    VERIFY_ARE_EQUAL(0x1u, (words[0] >> 20) & 0xF);
    VERIFY_ARE_EQUAL(0x2u, (words[2] >> 20) & 0xF);
    VERIFY_ARE_EQUAL(0x3u, (words[4] >> 20) & 0xF);

    VERIFY_ARE_EQUAL(6u, (words[0] >> 16) & 0xF);
    VERIFY_ARE_EQUAL(6u, (words[2] >> 16) & 0xF);
    VERIFY_ARE_EQUAL(2u, (words[4] >> 16) & 0xF);
}

void ActionPlanTests::TheWrapperBytesAreOptional()
{
    // A dump copied from a manual may or may not carry its own wrapper. Both mean the same
    // thing, and neither 0xF0 nor 0xF7 belongs in a UMP packet.
    std::vector<uint32_t> wrapped{};
    std::vector<uint32_t> bare{};

    VERIFY_IS_TRUE(glass::PacketizeSystemExclusive({ 0xF0, 0x7E, 0x7F, 0x06, 0xF7 }, 0, wrapped));
    VERIFY_IS_TRUE(glass::PacketizeSystemExclusive({ 0x7E, 0x7F, 0x06 }, 0, bare));

    VERIFY_ARE_EQUAL(bare.size(), wrapped.size());
    VERIFY_ARE_EQUAL(bare[0], wrapped[0]);
    VERIFY_ARE_EQUAL(bare[1], wrapped[1]);
}

void ActionPlanTests::ADumpWithAStatusByteInItIsRefusedWhole()
{
    std::vector<uint32_t> words{};

    // Sending the part before the bad byte would leave a synthesizer waiting for an end that
    // never comes, so nothing goes at all.
    VERIFY_IS_FALSE(glass::PacketizeSystemExclusive({ 0x7E, 0x90, 0x06 }, 0, words));
    VERIFY_ARE_EQUAL(size_t{ 0 }, words.size());
}

void ActionPlanTests::AnEmptyDumpSendsNothing()
{
    std::vector<uint32_t> words{};

    VERIFY_IS_FALSE(glass::PacketizeSystemExclusive({}, 0, words));
    VERIFY_IS_FALSE(glass::PacketizeSystemExclusive({ 0xF0, 0xF7 }, 0, words));
}

void ActionPlanTests::TheGroupReachesEveryPacket()
{
    std::vector<uint8_t> bytes(14, 0x01);

    std::vector<uint32_t> words{};

    VERIFY_IS_TRUE(glass::PacketizeSystemExclusive(bytes, 5, words));

    for (size_t index = 0; index < words.size(); index += 2)
    {
        VERIFY_ARE_EQUAL(5u, (words[index] >> 24) & 0xF);
    }
}

void ActionPlanTests::TheLastPacketCarriesOnlyTheBytesItHas()
{
    std::vector<uint32_t> words{};

    VERIFY_IS_TRUE(glass::PacketizeSystemExclusive({ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }, 0, words));

    VERIFY_ARE_EQUAL(size_t{ 4 }, words.size());
    VERIFY_ARE_EQUAL(1u, (words[2] >> 16) & 0xF);

    // Everything after the one byte it has is zero, not whatever was in the buffer.
    VERIFY_ARE_EQUAL(0x07u, (words[2] >> 8) & 0xFF);
    VERIFY_ARE_EQUAL(0x00u, words[2] & 0xFF);
    VERIFY_ARE_EQUAL(0x00000000u, words[3]);
}

// ---- what a control does beyond its immediate messages ----

void ActionPlanTests::AChannelVoiceMessageMakesNoPlan()
{
    glass::ControlMessage message{};

    message.Trigger = glass::MessageTrigger::TurnsOn;
    message.Kind = glass::MessageKind::Note;
    message.DeviceName = L"Synth";
    message.Number = 60;

    auto const document = DocumentWithOneControl(ButtonSending(message));

    glass::ActionPlanSet plans{};
    plans.Prepare(document, OneDevice());

    // A note leaves in the pointer handler with no clock in the way. Putting it in a plan as
    // well would send it twice.
    VERIFY_ARE_EQUAL(size_t{ 0 }, plans.PlanCount());
    VERIFY_IS_NULL(plans.Find(0, glass::MessageTrigger::TurnsOn));
}

void ActionPlanTests::ASystemExclusiveMessageMakesAPlan()
{
    auto const document = DocumentWithOneControl(ButtonSending(SysExMessage({ 0x7E, 0x7F, 0x06 })));

    glass::ActionPlanSet plans{};
    plans.Prepare(document, OneDevice());

    auto const* const plan = plans.Find(0, glass::MessageTrigger::TurnsOn);

    VERIFY_IS_NOT_NULL(plan);
    VERIFY_ARE_EQUAL(size_t{ 1 }, plan->Actions.size());
    VERIFY_IS_TRUE(plan->Actions[0].Kind == glass::ActionKind::Send);
    VERIFY_ARE_EQUAL(0, plan->Actions[0].DestinationIndex);
    VERIFY_ARE_EQUAL(size_t{ 2 }, plan->Actions[0].Words.size());
}

void ActionPlanTests::APlanIsFoundByItsOwnTrigger()
{
    auto const document = DocumentWithOneControl(ButtonSending(SysExMessage({ 0x7E, 0x7F, 0x06 })));

    glass::ActionPlanSet plans{};
    plans.Prepare(document, OneDevice());

    VERIFY_IS_NOT_NULL(plans.Find(0, glass::MessageTrigger::TurnsOn));
    VERIFY_IS_NULL(plans.Find(0, glass::MessageTrigger::TurnsOff));
    VERIFY_IS_NULL(plans.Find(0, glass::MessageTrigger::Changes));
}

void ActionPlanTests::ARawMessageTravelsThroughUntouched()
{
    glass::ControlMessage message{};

    message.Trigger = glass::MessageTrigger::TurnsOn;
    message.Kind = glass::MessageKind::RawUmp;
    message.DeviceName = L"Synth";
    message.RawWords = { 0x40903C00, 0xFFFF0000 };

    auto const document = DocumentWithOneControl(ButtonSending(message));

    glass::ActionPlanSet plans{};
    plans.Prepare(document, OneDevice());

    auto const* const plan = plans.Find(0, glass::MessageTrigger::TurnsOn);

    VERIFY_IS_NOT_NULL(plan);
    VERIFY_ARE_EQUAL(size_t{ 2 }, plan->Actions[0].Words.size());
    VERIFY_ARE_EQUAL(0x40903C00u, plan->Actions[0].Words[0]);
    VERIFY_ARE_EQUAL(0xFFFF0000u, plan->Actions[0].Words[1]);
}

void ActionPlanTests::AMessageNamingAMissingDeviceKeepsItsPlace()
{
    auto message = SysExMessage({ 0x7E, 0x7F, 0x06 });
    message.DeviceName = L"Gone";

    auto const document = DocumentWithOneControl(ButtonSending(message));

    glass::ActionPlanSet plans{};
    plans.Prepare(document, OneDevice());

    auto const* const plan = plans.Find(0, glass::MessageTrigger::TurnsOn);

    // The row is kept rather than dropped, so the editor can still show it and say what is
    // wrong. The runner skips it because the destination is -1.
    VERIFY_IS_NOT_NULL(plan);
    VERIFY_ARE_EQUAL(-1, plan->Actions[0].DestinationIndex);
}

// ---- sequences ----

namespace
{
    glass::SequenceStep NoteStep(_In_ uint32_t number)
    {
        glass::SequenceStep step{};

        step.Kind = glass::SequenceStepKind::SendSystemExclusive;
        step.Message.Kind = glass::MessageKind::SystemExclusive;
        step.Message.DeviceName = L"Synth";
        step.Message.SystemExclusive = { static_cast<uint8_t>(number & 0x7F) };

        return step;
    }

    glass::SequenceStep WaitStep(_In_ uint32_t milliseconds)
    {
        glass::SequenceStep step{};

        step.Kind = glass::SequenceStepKind::Wait;
        step.WaitMilliseconds = milliseconds;

        return step;
    }

    glass::LayoutDocument DocumentPlaying(_In_ glass::Sequence sequence)
    {
        glass::ControlMessage message{};

        message.Trigger = glass::MessageTrigger::TurnsOn;
        message.Kind = glass::MessageKind::Sequence;
        message.DeviceName = L"Synth";
        message.SequenceName = sequence.Name;

        auto document = DocumentWithOneControl(ButtonSending(message));

        document.Sequences.push_back(std::move(sequence));

        return document;
    }
}

void ActionPlanTests::ASequenceBecomesItsSteps()
{
    glass::Sequence sequence{};

    sequence.Name = L"Intro";
    sequence.Steps.push_back(NoteStep(1));
    sequence.Steps.push_back(NoteStep(2));
    sequence.Steps.push_back(NoteStep(3));

    auto const document = DocumentPlaying(sequence);

    glass::ActionPlanSet plans{};
    plans.Prepare(document, OneDevice());

    auto const* const plan = plans.Find(0, glass::MessageTrigger::TurnsOn);

    VERIFY_IS_NOT_NULL(plan);
    VERIFY_ARE_EQUAL(size_t{ 3 }, plan->Actions.size());
}

void ActionPlanTests::AWaitBecomesAWaitAction()
{
    glass::Sequence sequence{};

    sequence.Name = L"Intro";
    sequence.Steps.push_back(NoteStep(1));
    sequence.Steps.push_back(WaitStep(250));
    sequence.Steps.push_back(NoteStep(2));

    auto const document = DocumentPlaying(sequence);

    glass::ActionPlanSet plans{};
    plans.Prepare(document, OneDevice());

    auto const* const plan = plans.Find(0, glass::MessageTrigger::TurnsOn);

    VERIFY_IS_NOT_NULL(plan);
    VERIFY_ARE_EQUAL(size_t{ 3 }, plan->Actions.size());
    VERIFY_IS_TRUE(plan->Actions[1].Kind == glass::ActionKind::Wait);
    VERIFY_ARE_EQUAL(250u, plan->Actions[1].WaitMilliseconds);
}

void ActionPlanTests::ARepeatBlockIsFlattened()
{
    glass::Sequence sequence{};

    glass::SequenceStep start{};
    start.Kind = glass::SequenceStepKind::RepeatBlockStart;
    start.RepeatCount = 3;

    glass::SequenceStep end{};
    end.Kind = glass::SequenceStepKind::RepeatBlockEnd;

    sequence.Name = L"Roll";
    sequence.Steps.push_back(start);
    sequence.Steps.push_back(NoteStep(1));
    sequence.Steps.push_back(WaitStep(100));
    sequence.Steps.push_back(end);
    sequence.Steps.push_back(NoteStep(9));

    auto const document = DocumentPlaying(sequence);

    glass::ActionPlanSet plans{};
    plans.Prepare(document, OneDevice());

    auto const* const plan = plans.Find(0, glass::MessageTrigger::TurnsOn);

    // Flattened at prepare time, so the runner has no control flow at all and the plan can be
    // read straight off in the editor.
    VERIFY_IS_NOT_NULL(plan);
    VERIFY_ARE_EQUAL(size_t{ 7 }, plan->Actions.size());
    VERIFY_IS_TRUE(plan->Actions[6].Kind == glass::ActionKind::Send);
}

void ActionPlanTests::ASequenceNamingItselfTerminates()
{
    glass::Sequence sequence{};

    glass::SequenceStep recurse{};
    recurse.Kind = glass::SequenceStepKind::SendMidiMessage;
    recurse.Message.Kind = glass::MessageKind::Sequence;
    recurse.Message.SequenceName = L"Loop";

    sequence.Name = L"Loop";
    sequence.Steps.push_back(NoteStep(1));
    sequence.Steps.push_back(recurse);

    auto const document = DocumentPlaying(sequence);

    glass::ActionPlanSet plans{};
    plans.Prepare(document, OneDevice());

    // A layout from a stranger can do this on purpose. It has to stop, and it has to stop
    // without allocating forever.
    auto const* const plan = plans.Find(0, glass::MessageTrigger::TurnsOn);

    VERIFY_IS_NOT_NULL(plan);
    VERIFY_IS_LESS_THAN(plan->Actions.size(), glass::MaximumPlanActions + 1);
}

void ActionPlanTests::ASequenceThatIsNotThereIsSkipped()
{
    glass::ControlMessage message{};

    message.Trigger = glass::MessageTrigger::TurnsOn;
    message.Kind = glass::MessageKind::Sequence;
    message.DeviceName = L"Synth";
    message.SequenceName = L"Missing";

    auto const document = DocumentWithOneControl(ButtonSending(message));

    glass::ActionPlanSet plans{};
    plans.Prepare(document, OneDevice());

    VERIFY_IS_NULL(plans.Find(0, glass::MessageTrigger::TurnsOn));
}

void ActionPlanTests::APlanCannotGrowWithoutLimit()
{
    glass::Sequence sequence{};

    glass::SequenceStep start{};
    start.Kind = glass::SequenceStepKind::RepeatBlockStart;
    start.RepeatCount = 256;

    glass::SequenceStep end{};
    end.Kind = glass::SequenceStepKind::RepeatBlockEnd;

    sequence.Name = L"Forever";
    sequence.Steps.push_back(start);

    for (int32_t i = 0; i < 64; ++i)
    {
        sequence.Steps.push_back(NoteStep(1));
    }

    sequence.Steps.push_back(end);

    auto const document = DocumentPlaying(sequence);

    glass::ActionPlanSet plans{};
    plans.Prepare(document, OneDevice());

    auto const* const plan = plans.Find(0, glass::MessageTrigger::TurnsOn);

    VERIFY_IS_NOT_NULL(plan);
    VERIFY_ARE_EQUAL(glass::MaximumPlanActions, plan->Actions.size());
}

void ActionPlanTests::AStepCanMoveAnotherControl()
{
    glass::Sequence sequence{};

    glass::SequenceStep step{};
    step.Kind = glass::SequenceStepKind::SetControlValue;
    step.TargetControlId = L"control-1";
    step.TargetValue = 0.25;

    sequence.Name = L"Reset";
    sequence.Steps.push_back(step);

    auto const document = DocumentPlaying(sequence);

    glass::ActionPlanSet plans{};
    plans.Prepare(document, OneDevice());

    auto const* const plan = plans.Find(0, glass::MessageTrigger::TurnsOn);

    VERIFY_IS_NOT_NULL(plan);
    VERIFY_IS_TRUE(plan->Actions[0].Kind == glass::ActionKind::SetControlValue);
    VERIFY_ARE_EQUAL(0, plan->Actions[0].TargetControlIndex);
    VERIFY_ARE_EQUAL(0.25, plan->Actions[0].TargetValue);
}

namespace
{
    glass::SequenceStep ChannelVoiceStep(
        _In_ glass::MessageKind kind,
        _In_ uint32_t number,
        _In_ double level)
    {
        glass::SequenceStep step{};

        step.Kind = glass::SequenceStepKind::SendMidiMessage;
        step.DurationMilliseconds = 150;
        step.Message.Kind = kind;
        step.Message.DeviceName = L"Synth";
        step.Message.Number = number;
        step.Message.UseMidi1Protocol = true;
        step.Message.Minimum = { level, glass::ValueScaling::Absolute };
        step.Message.Maximum = { level, glass::ValueScaling::Absolute };

        return step;
    }
}

void ActionPlanTests::ANoteInASequenceIsBuiltIntoWords()
{
    // The defect this test exists for: a control's own rows leave in the pointer handler, so
    // the plan builder skipped every channel voice message. Nothing was holding a sequence
    // step, so a sequence of notes sent absolutely nothing and said nothing about it.
    glass::Sequence sequence{};

    sequence.Name = L"Riff";
    sequence.Steps.push_back(ChannelVoiceStep(glass::MessageKind::Note, 60, 100));

    auto const document = DocumentPlaying(sequence);

    glass::ActionPlanSet plans{};
    plans.Prepare(document, OneDevice());

    auto const* const plan = plans.Find(0, glass::MessageTrigger::TurnsOn);

    VERIFY_IS_NOT_NULL(plan);
    VERIFY_ARE_EQUAL(size_t{ 3 }, plan->Actions.size());

    // 20903C64 is note on, note 60, velocity 100, group 0, channel 1.
    VERIFY_IS_TRUE(plan->Actions[0].Kind == glass::ActionKind::Send);
    VERIFY_ARE_EQUAL(size_t{ 1 }, plan->Actions[0].Words.size());
    VERIFY_ARE_EQUAL(0x20903C64u, plan->Actions[0].Words[0]);
}

void ActionPlanTests::ANoteInASequenceAlwaysHasItsNoteOff()
{
    glass::Sequence sequence{};

    sequence.Name = L"Riff";
    sequence.Steps.push_back(ChannelVoiceStep(glass::MessageKind::Note, 60, 100));

    auto const document = DocumentPlaying(sequence);

    glass::ActionPlanSet plans{};
    plans.Prepare(document, OneDevice());

    auto const* const plan = plans.Find(0, glass::MessageTrigger::TurnsOn);

    VERIFY_IS_NOT_NULL(plan);

    // On, held, off. A step list is exactly where a hanging note is easy to forget, so the
    // pair goes in together rather than being something to remember.
    VERIFY_IS_TRUE(plan->Actions[1].Kind == glass::ActionKind::Wait);
    VERIFY_ARE_EQUAL(150u, plan->Actions[1].WaitMilliseconds);

    VERIFY_IS_TRUE(plan->Actions[2].Kind == glass::ActionKind::Send);
    VERIFY_ARE_EQUAL(0x20803C64u, plan->Actions[2].Words[0]);
}

void ActionPlanTests::AControlChangeInASequenceIsBuiltIntoWords()
{
    glass::Sequence sequence{};

    sequence.Name = L"Riff";
    sequence.Steps.push_back(ChannelVoiceStep(glass::MessageKind::ControlChange, 7, 64));

    auto const document = DocumentPlaying(sequence);

    glass::ActionPlanSet plans{};
    plans.Prepare(document, OneDevice());

    auto const* const plan = plans.Find(0, glass::MessageTrigger::TurnsOn);

    VERIFY_IS_NOT_NULL(plan);

    // One action, not three: only a note needs an end.
    VERIFY_ARE_EQUAL(size_t{ 1 }, plan->Actions.size());
    VERIFY_ARE_EQUAL(0x20B00740u, plan->Actions[0].Words[0]);
}

void ActionPlanTests::ARepeatedNoteIsPlayedEveryTime()
{
    // The shape the wire probe drives: two notes, then a block of one note repeated twice.
    glass::Sequence sequence{};

    glass::SequenceStep start{};
    start.Kind = glass::SequenceStepKind::RepeatBlockStart;
    start.RepeatCount = 2;

    glass::SequenceStep wait{};
    wait.Kind = glass::SequenceStepKind::Wait;
    wait.WaitMilliseconds = 120;

    glass::SequenceStep end{};
    end.Kind = glass::SequenceStepKind::RepeatBlockEnd;

    sequence.Name = L"Riff";
    sequence.Steps.push_back(ChannelVoiceStep(glass::MessageKind::Note, 60, 100));
    sequence.Steps.push_back(wait);
    sequence.Steps.push_back(ChannelVoiceStep(glass::MessageKind::Note, 64, 100));
    sequence.Steps.push_back(wait);
    sequence.Steps.push_back(start);
    sequence.Steps.push_back(ChannelVoiceStep(glass::MessageKind::Note, 67, 100));
    sequence.Steps.push_back(wait);
    sequence.Steps.push_back(end);

    auto const document = DocumentPlaying(sequence);

    glass::ActionPlanSet plans{};
    plans.Prepare(document, OneDevice());

    auto const* const plan = plans.Find(0, glass::MessageTrigger::TurnsOn);

    VERIFY_IS_NOT_NULL(plan);

    int32_t noteOns{ 0 };
    int32_t sixtySevens{ 0 };

    for (auto const& action : plan->Actions)
    {
        if (action.Kind != glass::ActionKind::Send || action.Words.empty())
        {
            continue;
        }

        if ((action.Words[0] & 0xFFF00000u) == 0x20900000u)
        {
            ++noteOns;

            if (((action.Words[0] >> 8) & 0x7F) == 67)
            {
                ++sixtySevens;
            }
        }
    }

    VERIFY_ARE_EQUAL(4, noteOns);
    VERIFY_ARE_EQUAL(2, sixtySevens);
}

void ActionPlanTests::AStepKindThisBuildDoesNotKnowSendsNothing()
{
    // The defect this exists for: an unknown name fell through to "send a message", and the
    // default message is a control change, so a file written by a newer build made this one
    // send controller 0 to somebody's desk.
    glass::Sequence sequence{};

    glass::SequenceStep mystery{};
    mystery.Kind = glass::SequenceStepKind::SendMidiMessage;
    mystery.UnrecognizedKind = L"somethingFromNextYear";
    mystery.Message.Kind = glass::MessageKind::ControlChange;
    mystery.Message.DeviceName = L"Synth";

    sequence.Name = L"Riff";
    sequence.Steps.push_back(mystery);
    sequence.Steps.push_back(ChannelVoiceStep(glass::MessageKind::ControlChange, 7, 64));

    auto const document = DocumentPlaying(sequence);

    glass::ActionPlanSet plans{};
    plans.Prepare(document, OneDevice());

    auto const* const plan = plans.Find(0, glass::MessageTrigger::TurnsOn);

    VERIFY_IS_NOT_NULL(plan);

    // The step around it still runs. Only the one nobody understands is skipped.
    VERIFY_ARE_EQUAL(size_t{ 1 }, plan->Actions.size());
    VERIFY_ARE_EQUAL(0x20B00740u, plan->Actions[0].Words[0]);
}
