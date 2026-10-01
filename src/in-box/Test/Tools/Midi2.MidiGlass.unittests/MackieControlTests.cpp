// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Mackie Control, and the per-device protocol it hangs off: what each function puts on the
// wire, what the DAW sends back, and what happens to the rows when a device changes protocol.

#include "MackieControlTests.h"

#include "ActionPlan.h"
#include "BindingEngine.h"
#include "EditorController.h"
#include "LayoutModel.h"
#include "LayoutSerializer.h"
#include "LayoutTemplates.h"
#include "MackieControl.h"

#include <algorithm>
#include <array>
#include <cwchar>
#include <limits>
#include <vector>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    using Sends = std::array<glass::PreparedSend, glass::MaximumSendsPerEvent>;

    // A MIDI 1.0 channel voice word on group 1: type 2, then the status, the channel and the two
    // data bytes.
    constexpr uint32_t Word(uint32_t status, uint32_t channel, uint32_t data1, uint32_t data2) noexcept
    {
        return 0x20000000u | (status << 20) | (channel << 16) | (data1 << 8) | data2;
    }

    glass::Control ControlSending(_In_ glass::ControlKind kind, _In_ glass::ControlMessage message)
    {
        glass::Control control{};

        control.Id = L"control";
        control.Kind = kind;
        control.Messages.push_back(std::move(message));

        return control;
    }

    glass::LayoutDocument DawLayout(
        _In_ glass::Control control,
        _In_ glass::DeviceProtocol protocol = glass::DeviceProtocol::MackieControl)
    {
        glass::LayoutDocument document{};

        document.Name = L"Mackie";

        glass::DeviceEntry device{};
        device.Name = L"DAW";
        device.Protocol = protocol;

        document.Devices.push_back(device);

        glass::Page page{};
        page.Id = L"page";
        page.Name = L"Page 1";
        page.Controls.push_back(std::move(control));

        document.Pages.push_back(std::move(page));

        return document;
    }

    std::vector<glass::PreparedDestination> DawHere()
    {
        glass::PreparedDestination daw{};

        daw.Name = L"DAW";
        daw.IsAvailable = true;

        return { daw };
    }

    glass::BindingEngine Prepared(
        _In_ glass::ControlKind kind,
        _In_ uint32_t function,
        _In_ glass::DeviceProtocol protocol = glass::DeviceProtocol::MackieControl)
    {
        glass::BindingEngine engine{};

        engine.Prepare(DawLayout(ControlSending(kind, glass::MakeMackieRow(function, L"DAW", 0)), protocol), DawHere());

        return engine;
    }

    glass::ControlMessage Row(
        _In_ glass::MessageTrigger trigger,
        _In_ glass::MessageKind kind,
        _In_ std::wstring const& device,
        _In_ int32_t channel,
        _In_ uint32_t number)
    {
        glass::ControlMessage message{};

        message.Trigger = trigger;
        message.Kind = kind;
        message.DeviceName = device;
        message.ChannelIndex = channel;
        message.Number = number;

        return message;
    }

    std::vector<uint32_t> FunctionsOffered(_In_ glass::ControlKind kind)
    {
        std::vector<uint32_t> functions{};

        for (auto const& entry : glass::MackiePickerFor(kind))
        {
            if (entry.HeadingKey == nullptr)
            {
                functions.push_back(entry.Function);
            }
        }

        return functions;
    }
}

// ------------------------------------------------------------------- the functions

void MackieControlTests::EveryFunctionReadsBackFromItsName()
{
    std::vector<uint32_t> functions{};

    for (uint32_t note = 0; note <= glass::MackieLastButtonNote; ++note)
    {
        functions.push_back(note);
    }

    for (uint32_t strip = 0; strip <= glass::MackieMasterStrip; ++strip)
    {
        functions.push_back(glass::MackieFaderBase + strip);
    }

    for (uint32_t strip = 0; strip < glass::MackieStripCount; ++strip)
    {
        functions.push_back(glass::MackieVPotBase + strip);
    }

    functions.push_back(glass::MackieJog);

    std::vector<std::wstring> names{};

    for (auto const function : functions)
    {
        auto const name = glass::MackieFunctionFileName(function);

        VERIFY_IS_FALSE(name.empty());
        VERIFY_ARE_EQUAL(function, glass::MackieFunctionFromFileName(name));
        VERIFY_IS_NOT_NULL(glass::DescribeMackieFunction(function).ResourceKey);
        VERIFY_IS_TRUE(std::find(names.begin(), names.end(), name) == names.end());

        names.push_back(name);
    }

    VERIFY_ARE_EQUAL(glass::MackieNoFunction, glass::MackieFunctionFromFileName(L"hui"));
    VERIFY_ARE_EQUAL(glass::MackieNoFunction, glass::MackieFunctionFromFileName(L"fader9"));
    VERIFY_ARE_EQUAL(glass::MackieNoFunction, glass::MackieFunctionFromFileName(L""));
    VERIFY_IS_TRUE(glass::ShapeOfMackieFunction(glass::MackieNoFunction) == glass::MackieShape::None);
}

void MackieControlTests::TheButtonMapIsTheOneDawsExpect()
{
    struct Expected
    {
        wchar_t const* Name;
        uint32_t Function;
    };

    constexpr Expected expected[]
    {
        { L"rec1", 0 }, { L"rec8", 7 }, { L"solo1", 8 }, { L"mute1", 16 }, { L"select8", 31 },
        { L"vpotPress1", 32 }, { L"assignTrack", 40 }, { L"bankLeft", 46 }, { L"bankRight", 47 },
        { L"channelLeft", 48 }, { L"flip", 50 }, { L"f1", 54 }, { L"f8", 61 }, { L"midiTracks", 62 },
        { L"user", 69 }, { L"shift", 70 }, { L"readOff", 74 }, { L"save", 80 }, { L"marker", 84 },
        { L"cycle", 86 }, { L"click", 89 }, { L"rewind", 91 }, { L"fastForward", 92 },
        { L"stop", 93 }, { L"play", 94 }, { L"record", 95 }, { L"up", 96 }, { L"right", 99 },
        { L"zoom", 100 }, { L"scrub", 101 }, { L"userSwitch2", 103 },
        { L"fader1", glass::MackieFaderBase }, { L"masterFader", glass::MackieFaderBase + 8 },
        { L"vpot1", glass::MackieVPotBase }, { L"vpot8", glass::MackieVPotBase + 7 },
        { L"jog", glass::MackieJog },
    };

    for (auto const& entry : expected)
    {
        VERIFY_ARE_EQUAL(entry.Function, glass::MackieFunctionFromFileName(entry.Name));
    }
}

void MackieControlTests::EachKindOfControlIsOfferedWhatItCanDo()
{
    auto buttons = FunctionsOffered(glass::ControlKind::Button);

    // Play leads, because the transport is what people look for first.
    VERIFY_ARE_EQUAL(94u, buttons.front());

    std::sort(buttons.begin(), buttons.end());
    buttons.erase(std::unique(buttons.begin(), buttons.end()), buttons.end());

    VERIFY_ARE_EQUAL(size_t{ glass::MackieLastButtonNote + 1 }, buttons.size());
    VERIFY_ARE_EQUAL(buttons.size(), FunctionsOffered(glass::ControlKind::Toggle).size());

    VERIFY_ARE_EQUAL(size_t{ 9 }, FunctionsOffered(glass::ControlKind::Fader).size());
    VERIFY_ARE_EQUAL(size_t{ 9 }, FunctionsOffered(glass::ControlKind::Knob).size());
    VERIFY_ARE_EQUAL(size_t{ 9 }, FunctionsOffered(glass::ControlKind::Turntable).size());
    VERIFY_IS_TRUE(FunctionsOffered(glass::ControlKind::XYPad).empty());

    VERIFY_IS_TRUE(glass::MackieFunctionFits(94, glass::ControlKind::Pad));
    VERIFY_IS_FALSE(glass::MackieFunctionFits(94, glass::ControlKind::Fader));
    VERIFY_IS_FALSE(glass::MackieFunctionFits(glass::MackieFaderBase, glass::ControlKind::Knob));
    VERIFY_IS_TRUE(glass::MackieFunctionFits(glass::MackieJog, glass::ControlKind::Wheel));
}

// ------------------------------------------------------------------- what goes out

void MackieControlTests::AButtonSendsAPressAndAReleaseAsNoteOns()
{
    auto engine = Prepared(glass::ControlKind::Button, 94);
    Sends sends{};

    VERIFY_ARE_EQUAL(1u, engine.EvaluatePress(0, glass::MessageTrigger::TurnsOn, true, 1.0, sends));
    VERIFY_ARE_EQUAL(1u, sends[0].WordCount);
    VERIFY_ARE_EQUAL(Word(0x9, 0, 94, 127), sends[0].Words[0]);

    // A soft press is still a whole press to a DAW.
    VERIFY_ARE_EQUAL(1u, engine.EvaluatePress(0, glass::MessageTrigger::TurnsOn, true, 0.1, sends));
    VERIFY_ARE_EQUAL(Word(0x9, 0, 94, 127), sends[0].Words[0]);

    // The release is a note on at velocity 0, the way the hardware sends it, not a note off.
    VERIFY_ARE_EQUAL(1u, engine.EvaluatePress(0, glass::MessageTrigger::TurnsOff, false, 0.0, sends));
    VERIFY_ARE_EQUAL(Word(0x9, 0, 94, 0), sends[0].Words[0]);

    VERIFY_ARE_EQUAL(0u, engine.EvaluatePress(0, glass::MessageTrigger::Changes, true, 1.0, sends));
}

void MackieControlTests::AToggleSendsAWholePressEachTime()
{
    auto engine = Prepared(glass::ControlKind::Toggle, 16);
    Sends sends{};

    for (auto const on : { true, false })
    {
        auto const trigger = on ? glass::MessageTrigger::TurnsOn : glass::MessageTrigger::TurnsOff;

        VERIFY_ARE_EQUAL(2u, engine.EvaluatePress(0, trigger, on, 1.0, sends));
        VERIFY_ARE_EQUAL(Word(0x9, 0, 16, 127), sends[0].Words[0]);
        VERIFY_ARE_EQUAL(Word(0x9, 0, 16, 0), sends[1].Words[0]);
    }
}

void MackieControlTests::AFaderSendsPitchBendAndItsTouchNote()
{
    auto engine = Prepared(glass::ControlKind::Fader, glass::MackieFaderBase + 2);
    Sends sends{};

    VERIFY_ARE_EQUAL(1u, engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends));
    VERIFY_ARE_EQUAL(Word(0xE, 2, 0x7F, 0x7F), sends[0].Words[0]);

    // 8192 is the middle: 0 in the low seven bits and 64 in the high ones.
    VERIFY_ARE_EQUAL(1u, engine.Evaluate(0, glass::MessageTrigger::Changes, 0.5, sends));
    VERIFY_ARE_EQUAL(Word(0xE, 2, 0x00, 0x40), sends[0].Words[0]);

    VERIFY_ARE_EQUAL(1u, engine.Evaluate(0, glass::MessageTrigger::Touched, 1.0, sends));
    VERIFY_ARE_EQUAL(Word(0x9, 0, 106, 127), sends[0].Words[0]);

    VERIFY_ARE_EQUAL(1u, engine.Evaluate(0, glass::MessageTrigger::Released, 0.0, sends));
    VERIFY_ARE_EQUAL(Word(0x9, 0, 106, 0), sends[0].Words[0]);
}

void MackieControlTests::TheMasterFaderIsTheNinth()
{
    auto engine = Prepared(glass::ControlKind::Fader, glass::MackieFaderBase + glass::MackieMasterStrip);
    Sends sends{};

    VERIFY_ARE_EQUAL(1u, engine.Evaluate(0, glass::MessageTrigger::Changes, 0.0, sends));
    VERIFY_ARE_EQUAL(Word(0xE, 8, 0, 0), sends[0].Words[0]);

    VERIFY_ARE_EQUAL(1u, engine.Evaluate(0, glass::MessageTrigger::Touched, 1.0, sends));
    VERIFY_ARE_EQUAL(Word(0x9, 0, 112, 127), sends[0].Words[0]);
}

void MackieControlTests::AVPotSendsTurnsRatherThanPositions()
{
    auto engine = Prepared(glass::ControlKind::Knob, glass::MackieVPotBase + 1);
    Sends sends{};

    VERIFY_IS_TRUE(engine.HasRelativeRows(0));
    VERIFY_ARE_EQUAL(0u, engine.Evaluate(0, glass::MessageTrigger::Changes, 0.7, sends));

    VERIFY_ARE_EQUAL(1u, engine.EvaluateRelative(0, 3, sends));
    VERIFY_ARE_EQUAL(Word(0xB, 0, 17, 3), sends[0].Words[0]);

    VERIFY_ARE_EQUAL(1u, engine.EvaluateRelative(0, -5, sends));
    VERIFY_ARE_EQUAL(Word(0xB, 0, 17, 0x45), sends[0].Words[0]);

    // However far it went, one message says at most 63.
    VERIFY_ARE_EQUAL(1u, engine.EvaluateRelative(0, 500, sends));
    VERIFY_ARE_EQUAL(Word(0xB, 0, 17, 0x3F), sends[0].Words[0]);

    VERIFY_ARE_EQUAL(0u, engine.EvaluateRelative(0, 0, sends));

    auto jog = Prepared(glass::ControlKind::Turntable, glass::MackieJog);

    VERIFY_ARE_EQUAL(1u, jog.EvaluateRelative(0, -1, sends));
    VERIFY_ARE_EQUAL(Word(0xB, 0, 60, 0x41), sends[0].Words[0]);

    VERIFY_IS_FALSE(Prepared(glass::ControlKind::Fader, glass::MackieFaderBase).HasRelativeRows(0));
}

void MackieControlTests::TurnsAreCountedWithoutLosingAny()
{
    constexpr double Tick = 1.0 / glass::MackieTicksPerTravel;

    double baseline{ 0.5 };

    // Half a tick is not a tick yet, and it is not lost either.
    VERIFY_ARE_EQUAL(0, glass::TakeRelativeTicks(baseline, 0.5 + Tick * 0.5));
    VERIFY_ARE_EQUAL(1, glass::TakeRelativeTicks(baseline, 0.5 + Tick * 1.5));
    VERIFY_ARE_EQUAL(2, glass::TakeRelativeTicks(baseline, 0.5 + Tick * 3.0));
    VERIFY_ARE_EQUAL(-3, glass::TakeRelativeTicks(baseline, 0.5));
    VERIFY_ARE_EQUAL(0.5, baseline);

    // A sweep from one end to the other says 63 at once, and the rest waits for the next send.
    baseline = 0.0;

    VERIFY_ARE_EQUAL(63, glass::TakeRelativeTicks(baseline, 1.0));
    VERIFY_ARE_EQUAL(1, glass::TakeRelativeTicks(baseline, 1.0));

    // With nothing to count from, the first position is where counting starts.
    baseline = std::numeric_limits<double>::quiet_NaN();

    VERIFY_ARE_EQUAL(0, glass::TakeRelativeTicks(baseline, 0.25));
    VERIFY_ARE_EQUAL(0.25, baseline);

    VERIFY_ARE_EQUAL(1u, static_cast<uint32_t>(glass::MackieTurnValue(1)));
    VERIFY_ARE_EQUAL(0x41u, static_cast<uint32_t>(glass::MackieTurnValue(-1)));
}

void MackieControlTests::APlainRowOnAMackieDeviceSendsNothing()
{
    glass::BindingEngine engine{};
    Sends sends{};

    engine.Prepare(
        DawLayout(ControlSending(
            glass::ControlKind::Button,
            Row(glass::MessageTrigger::Changes, glass::MessageKind::Note, L"DAW", 0, 94))),
        DawHere());

    VERIFY_ARE_EQUAL(0u, engine.EvaluatePress(0, glass::MessageTrigger::Changes, true, 1.0, sends));
}

void MackieControlTests::AFunctionOnAPlainDeviceSendsNothing()
{
    auto engine = Prepared(glass::ControlKind::Button, 94, glass::DeviceProtocol::Midi1);
    Sends sends{};

    VERIFY_ARE_EQUAL(0u, engine.EvaluatePress(0, glass::MessageTrigger::TurnsOn, true, 1.0, sends));
    VERIFY_ARE_EQUAL(0u, engine.EvaluatePress(0, glass::MessageTrigger::Changes, true, 1.0, sends));
}

void MackieControlTests::FunctionsStayOutOfTheStartupPass()
{
    auto control = ControlSending(glass::ControlKind::Fader, glass::MakeMackieRow(glass::MackieFaderBase, L"DAW", 0));

    control.SendsValueOnStart = true;
    control.DefaultValue = 0.75;
    control.Messages.push_back(Row(glass::MessageTrigger::Changes, glass::MessageKind::ControlChange, L"Synth", 0, 7));

    auto document = DawLayout(control);

    glass::DeviceEntry synth{};
    synth.Name = L"Synth";
    document.Devices.push_back(synth);

    auto destinations = DawHere();

    glass::PreparedDestination synthHere{};
    synthHere.Name = L"Synth";
    synthHere.IsAvailable = true;
    destinations.push_back(synthHere);

    glass::BindingEngine engine{};
    engine.Prepare(document, destinations);

    Sends sends{};

    // Opening a layout must not throw the DAW's faders somewhere, but the synth still gets its
    // starting value.
    VERIFY_ARE_EQUAL(1u, engine.EvaluateStartupValues(sends));
    VERIFY_ARE_EQUAL(1, sends[0].DestinationIndex);
}

// ------------------------------------------------------------------- what comes back

void MackieControlTests::TheDawLightsAButtonAndCanMakeItBlink()
{
    auto engine = Prepared(glass::ControlKind::Button, 94);

    size_t index{ 0 };
    double value{ 0.0 };
    bool blinks{ false };

    auto const resolve = [&](uint32_t word)
        {
            return engine.TryResolveFeedback(&word, 1, 0, index, value, blinks) && index == 0;
        };

    VERIFY_IS_TRUE(resolve(Word(0x9, 0, 94, 127)));
    VERIFY_ARE_EQUAL(1.0, value);
    VERIFY_IS_FALSE(blinks);

    VERIFY_IS_TRUE(resolve(Word(0x9, 0, 94, 1)));
    VERIFY_IS_TRUE(blinks);

    VERIFY_IS_TRUE(resolve(Word(0x9, 0, 94, 0)));
    VERIFY_ARE_EQUAL(0.0, value);
    VERIFY_IS_FALSE(blinks);

    VERIFY_IS_TRUE(resolve(Word(0x8, 0, 94, 64)));
    VERIFY_ARE_EQUAL(0.0, value);
    VERIFY_IS_FALSE(blinks);

    // Any odd velocity but 127 blinks, and any even one is off.
    VERIFY_IS_TRUE(resolve(Word(0x9, 0, 94, 63)));
    VERIFY_IS_TRUE(blinks);

    VERIFY_IS_TRUE(resolve(Word(0x9, 0, 94, 64)));
    VERIFY_ARE_EQUAL(0.0, value);
    VERIFY_IS_FALSE(blinks);

    VERIFY_IS_FALSE(resolve(Word(0x9, 0, 93, 127)));

    // The same, arriving as MIDI 2.0: a velocity of 0x0204 folds to 1.
    uint32_t const words[2]{ 0x40905E00u, 0x02040000u };

    VERIFY_IS_TRUE(engine.TryResolveFeedback(words, 2, 0, index, value, blinks));
    VERIFY_IS_TRUE(blinks);
}

void MackieControlTests::TheDawMovesAFader()
{
    auto engine = Prepared(glass::ControlKind::Fader, glass::MackieFaderBase + 2);

    size_t index{ 0 };
    double value{ 0.0 };
    bool blinks{ false };

    auto word = Word(0xE, 2, 0x7F, 0x7F);

    VERIFY_IS_TRUE(engine.TryResolveFeedback(&word, 1, 0, index, value, blinks));
    VERIFY_ARE_EQUAL(1.0, value);
    VERIFY_IS_FALSE(blinks);

    word = Word(0xE, 3, 0x00, 0x40);

    VERIFY_IS_FALSE(engine.TryResolveFeedback(&word, 1, 0, index, value, blinks));
}

void MackieControlTests::ALightOnlyAnswersItsOwnDevice()
{
    auto engine = Prepared(glass::ControlKind::Button, 94);

    size_t index{ 0 };
    double value{ 0.0 };
    bool blinks{ false };

    auto word = Word(0x9, 0, 94, 127);

    // Note 94 from a keyboard is a note, not the DAW saying it is playing.
    VERIFY_IS_FALSE(engine.TryResolveFeedback(&word, 1, 1, index, value, blinks));
    VERIFY_IS_FALSE(engine.TryResolveFeedback(&word, 1, index, value));
    VERIFY_IS_TRUE(engine.TryResolveFeedback(&word, 1, 0, index, value, blinks));
}

// ------------------------------------------------------------------- the device's protocol

void MackieControlTests::AMidi1DeviceGetsMidi1Words()
{
    auto rpn = Row(glass::MessageTrigger::Changes, glass::MessageKind::RegisteredController, L"DAW", 0, 0);
    auto control = ControlSending(
        glass::ControlKind::Fader,
        Row(glass::MessageTrigger::Changes, glass::MessageKind::ControlChange, L"DAW", 0, 7));

    control.Messages.push_back(rpn);

    Sends sends{};

    glass::BindingEngine engine{};
    engine.Prepare(DawLayout(control, glass::DeviceProtocol::Midi1), DawHere());

    VERIFY_ARE_EQUAL(2u, engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends));
    VERIFY_ARE_EQUAL(1u, sends[0].WordCount);
    VERIFY_ARE_EQUAL(Word(0xB, 0, 7, 127), sends[0].Words[0]);

    // An RPN has no single MIDI 1.0 message, so it goes as MIDI 2.0 and Windows makes the four.
    VERIFY_ARE_EQUAL(2u, sends[1].WordCount);
    VERIFY_ARE_EQUAL(0x4u, sends[1].Words[0] >> 28);

    engine.Prepare(DawLayout(control, glass::DeviceProtocol::Midi2), DawHere());

    VERIFY_ARE_EQUAL(2u, engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends));
    VERIFY_ARE_EQUAL(2u, sends[0].WordCount);
    VERIFY_ARE_EQUAL(0x4u, sends[0].Words[0] >> 28);
}

void MackieControlTests::ASequenceStepFollowsItsDevice()
{
    glass::Sequence sequence{};
    sequence.Name = L"Go";

    glass::SequenceStep step{};
    step.Kind = glass::SequenceStepKind::SendMidiMessage;
    step.Message = Row(glass::MessageTrigger::Changes, glass::MessageKind::ControlChange, L"DAW", 0, 7);
    step.Message.Maximum = { 100.0 / 127.0, glass::ValueScaling::Fraction };

    sequence.Steps.push_back(step);

    auto run = Row(glass::MessageTrigger::TurnsOn, glass::MessageKind::Sequence, L"DAW", 0, 0);
    run.SequenceName = L"Go";

    auto document = DawLayout(ControlSending(glass::ControlKind::Button, run), glass::DeviceProtocol::Midi1);
    document.Sequences.push_back(sequence);

    glass::ActionPlanSet plans{};
    plans.Prepare(document, DawHere());

    auto const* const plan = plans.Find(0, glass::MessageTrigger::TurnsOn);

    VERIFY_IS_NOT_NULL(plan);
    VERIFY_ARE_EQUAL(size_t{ 1 }, plan->Actions[0].Words.size());
    VERIFY_ARE_EQUAL(Word(0xB, 0, 7, 100), plan->Actions[0].Words[0]);
}

void MackieControlTests::TheProtocolAndTheFunctionsSurviveTheFile()
{
    auto document = DawLayout(ControlSending(glass::ControlKind::Button, glass::MakeMackieRow(94, L"DAW", 0)));

    glass::DeviceEntry synth{};
    synth.Name = L"Synth";
    synth.Protocol = glass::DeviceProtocol::Midi1;
    document.Devices.push_back(synth);

    glass::DeviceEntry plain{};
    plain.Name = L"Plain";
    document.Devices.push_back(plain);

    auto const json = glass::WriteLayoutToJson(document);

    // Written by name, never by the number behind it.
    VERIFY_IS_TRUE(json.find(L"\"play\"") != std::wstring::npos);
    VERIFY_IS_TRUE(json.find(L"\"midi1\"") != std::wstring::npos);

    // A MIDI 2.0 device, which is every device in every older file, writes nothing new.
    VERIFY_IS_TRUE(json.find(L"\"midi2\"") == std::wstring::npos);

    auto const reread = glass::ReadLayoutFromJson(json);

    VERIFY_IS_TRUE(reread.Succeeded);
    VERIFY_IS_TRUE(reread.Document.Devices[0].Protocol == glass::DeviceProtocol::MackieControl);
    VERIFY_IS_TRUE(reread.Document.Devices[1].Protocol == glass::DeviceProtocol::Midi1);
    VERIFY_IS_TRUE(reread.Document.Devices[2].Protocol == glass::DeviceProtocol::Midi2);

    auto const& row = reread.Document.Pages[0].Controls[0].Messages[0];

    VERIFY_IS_TRUE(row.Kind == glass::MessageKind::MackieControl);
    VERIFY_ARE_EQUAL(94u, row.Number);

    VERIFY_ARE_EQUAL(json, glass::WriteLayoutToJson(reread.Document));
}

void MackieControlTests::ANewerProtocolOrFunctionIsKeptAsItWas()
{
    auto json = glass::WriteLayoutToJson(
        DawLayout(ControlSending(glass::ControlKind::Button, glass::MakeMackieRow(94, L"DAW", 0))));

    auto const protocolAt = json.find(L"\"mackieControl\"", json.find(L"\"protocol\""));
    json.replace(protocolAt, std::wcslen(L"\"mackieControl\""), L"\"hui\"");

    auto const functionAt = json.find(L"\"play\"");
    json.replace(functionAt, std::wcslen(L"\"play\""), L"\"somethingNew\"");

    auto const reread = glass::ReadLayoutFromJson(json);

    VERIFY_IS_TRUE(reread.Succeeded);

    auto const& device = reread.Document.Devices[0];

    VERIFY_IS_TRUE(device.Protocol == glass::DeviceProtocol::Midi2);
    VERIFY_ARE_EQUAL(std::wstring{ L"hui" }, device.UnrecognizedProtocol);
    VERIFY_ARE_EQUAL(glass::MackieNoFunction, reread.Document.Pages[0].Controls[0].Messages[0].Number);

    auto const written = glass::WriteLayoutToJson(reread.Document);

    VERIFY_IS_TRUE(written.find(L"\"hui\"") != std::wstring::npos);
    VERIFY_IS_TRUE(written.find(L"\"somethingNew\"") != std::wstring::npos);
}

// ------------------------------------------------------------------- switching

namespace
{
    // A DAW port set to MIDI 1.0 and a synth, with the rows somebody wiring a Mackie Control
    // surface by hand would have made.
    glass::LayoutDocument HandWiredDaw()
    {
        using Trigger = glass::MessageTrigger;
        using Kind = glass::MessageKind;

        auto play = ControlSending(glass::ControlKind::Button, Row(Trigger::TurnsOn, Kind::Note, L"DAW", 0, 94));
        play.Id = L"play";
        play.Messages.push_back(Row(Trigger::TurnsOff, Kind::Note, L"DAW", 0, 94));
        play.Messages.push_back(Row(Trigger::Changes, Kind::ControlChange, L"Synth", 0, 20));

        auto fader = ControlSending(glass::ControlKind::Fader, Row(Trigger::Changes, Kind::PitchBend, L"DAW", 2, 0));
        fader.Id = L"fader";
        fader.Messages.push_back(Row(Trigger::Touched, Kind::Note, L"DAW", 0, 106));
        fader.Messages.push_back(Row(Trigger::Released, Kind::Note, L"DAW", 0, 106));

        auto pan = ControlSending(glass::ControlKind::Knob, Row(Trigger::Changes, Kind::ControlChange, L"DAW", 0, 16));
        pan.Id = L"pan";

        auto other = ControlSending(glass::ControlKind::Button, Row(Trigger::Changes, Kind::ControlChange, L"DAW", 0, 7));
        other.Id = L"other";

        auto document = DawLayout(play, glass::DeviceProtocol::Midi1);

        document.Pages[0].Controls.push_back(fader);
        document.Pages[0].Controls.push_back(pan);
        document.Pages[0].Controls.push_back(other);

        glass::DeviceEntry synth{};
        synth.Name = L"Synth";
        document.Devices.push_back(synth);

        return document;
    }
}

void MackieControlTests::SwitchingToMackieTurnsMatchingRowsIntoFunctions()
{
    glass::EditorController editor{};
    editor.Load(HandWiredDaw());

    VERIFY_IS_TRUE(editor.SetDeviceProtocol(L"DAW", glass::DeviceProtocol::MackieControl));
    VERIFY_IS_FALSE(editor.SetDeviceProtocol(L"DAW", glass::DeviceProtocol::MackieControl));

    auto const& controls = editor.Document().Pages[0].Controls;

    // The press row and the release row were one button all along. The synth is untouched.
    VERIFY_ARE_EQUAL(size_t{ 2 }, controls[0].Messages.size());
    VERIFY_IS_TRUE(controls[0].Messages[0].Kind == glass::MessageKind::MackieControl);
    VERIFY_ARE_EQUAL(94u, controls[0].Messages[0].Number);
    VERIFY_IS_TRUE(controls[0].Messages[1].Kind == glass::MessageKind::ControlChange);
    VERIFY_ARE_EQUAL(std::wstring{ L"Synth" }, controls[0].Messages[1].DeviceName);

    // A fader and its touch note are one fader.
    VERIFY_ARE_EQUAL(size_t{ 1 }, controls[1].Messages.size());
    VERIFY_ARE_EQUAL(glass::MackieFaderBase + 2, controls[1].Messages[0].Number);

    // A knob that now sends turns springs back to the middle.
    VERIFY_ARE_EQUAL(glass::MackieVPotBase, controls[2].Messages[0].Number);
    VERIFY_IS_TRUE(controls[2].ReturnsToDefault);
    VERIFY_ARE_EQUAL(0.5, controls[2].DefaultValue);

    // Nothing in Mackie Control is controller 7, so that row waits for somebody to pick.
    VERIFY_IS_TRUE(controls[3].Messages[0].Kind == glass::MessageKind::ControlChange);
    VERIFY_ARE_EQUAL(7u, controls[3].Messages[0].Number);

    VERIFY_IS_TRUE(editor.Undo());
    VERIFY_ARE_EQUAL(size_t{ 3 }, editor.Document().Pages[0].Controls[0].Messages.size());
    VERIFY_IS_TRUE(editor.Document().Devices[0].Protocol == glass::DeviceProtocol::Midi1);
}

void MackieControlTests::SwitchingBackTurnsFunctionsIntoPlainRows()
{
    glass::EditorController editor{};
    editor.Load(HandWiredDaw());

    VERIFY_IS_TRUE(editor.SetDeviceProtocol(L"DAW", glass::DeviceProtocol::MackieControl));
    VERIFY_IS_TRUE(editor.SetDeviceProtocol(L"DAW", glass::DeviceProtocol::Midi1));

    auto const& controls = editor.Document().Pages[0].Controls;

    // One row that follows the button, which is a press and a release.
    auto const& play = controls[0].Messages[0];

    VERIFY_IS_TRUE(play.Kind == glass::MessageKind::Note);
    VERIFY_IS_TRUE(play.Trigger == glass::MessageTrigger::Changes);
    VERIFY_ARE_EQUAL(94u, play.Number);

    VERIFY_IS_TRUE(controls[1].Messages[0].Kind == glass::MessageKind::PitchBend);
    VERIFY_ARE_EQUAL(2, controls[1].Messages[0].ChannelIndex);

    VERIFY_IS_TRUE(controls[2].Messages[0].Kind == glass::MessageKind::ControlChange);
    VERIFY_ARE_EQUAL(16u, controls[2].Messages[0].Number);
}

void MackieControlTests::SwitchingKeepsExactValuesMeaningful()
{
    auto row = Row(glass::MessageTrigger::Changes, glass::MessageKind::ControlChange, L"DAW", 0, 7);
    row.Maximum = { 100.0, glass::ValueScaling::Absolute };
    row.UseMidi1Protocol = true;

    glass::EditorController editor{};
    editor.Load(DawLayout(ControlSending(glass::ControlKind::Fader, row), glass::DeviceProtocol::Midi1));

    VERIFY_IS_TRUE(editor.SetDeviceProtocol(L"DAW", glass::DeviceProtocol::Midi2));

    auto const& moved = editor.Document().Pages[0].Controls[0].Messages[0];

    // 100 of 127 is still 100 of 127, now as a share of the range the device decides.
    VERIFY_IS_TRUE(moved.Maximum.Scaling == glass::ValueScaling::Fraction);
    VERIFY_ARE_EQUAL(100.0 / 127.0, moved.Maximum.Value);
    VERIFY_IS_FALSE(moved.UseMidi1Protocol);
}

void MackieControlTests::MovingARowToAMackieDeviceMakesItAFunction()
{
    auto document = HandWiredDaw();
    document.Devices[0].Protocol = glass::DeviceProtocol::MackieControl;

    // Note 93 on channel 1 means nothing to a synth, and Stop to a Mackie Control device.
    auto& row = document.Pages[0].Controls[0].Messages[2];
    row = Row(glass::MessageTrigger::Changes, glass::MessageKind::Note, L"Synth", 0, 93);

    glass::EditorController editor{};
    editor.Load(document);

    auto moved = row;
    moved.DeviceName = L"DAW";

    VERIFY_IS_TRUE(editor.SetMessage(L"play", 2, moved));

    auto const& result = editor.Document().Pages[0].Controls[0].Messages[2];

    VERIFY_IS_TRUE(result.Kind == glass::MessageKind::MackieControl);
    VERIFY_ARE_EQUAL(93u, result.Number);

    // And back again, it is the plain note it stands for.
    auto back = result;
    back.DeviceName = L"Synth";

    VERIFY_IS_TRUE(editor.SetMessage(L"play", 2, back));
    VERIFY_IS_TRUE(editor.Document().Pages[0].Controls[0].Messages[2].Kind == glass::MessageKind::Note);
}

void MackieControlTests::ANewControlWaitsForAFunction()
{
    auto document = DawLayout(glass::Control{});

    document.PageWidth = 1280;
    document.PageHeight = 800;
    document.CanvasWidth = 1280;
    document.CanvasHeight = 800;
    document.Pages[0].Controls.clear();

    glass::EditorController editor{};
    editor.Load(document);

    for (auto const kind : { glass::ControlKind::Button, glass::ControlKind::Fader, glass::ControlKind::Knob })
    {
        auto const id = editor.AddControl(kind, 100.0, 100.0);

        VERIFY_IS_FALSE(id.empty());

        auto const* const control = editor.Document().FindControl(id);

        VERIFY_IS_NOT_NULL(control);
        VERIFY_ARE_EQUAL(size_t{ 1 }, control->Messages.size());
        VERIFY_IS_TRUE(control->Messages[0].Kind == glass::MessageKind::MackieControl);
        VERIFY_ARE_EQUAL(glass::MackieNoFunction, control->Messages[0].Number);
        VERIFY_ARE_EQUAL(std::wstring{ L"DAW" }, control->Messages[0].DeviceName);
    }

    // Mackie Control has nothing for an XY pad, so it keeps what it was made with.
    auto const pad = editor.AddControl(glass::ControlKind::XYPad, 400.0, 100.0);

    for (auto const& row : editor.Document().FindControl(pad)->Messages)
    {
        VERIFY_IS_FALSE(row.Kind == glass::MessageKind::MackieControl);
    }
}

// ------------------------------------------------------------------- the starter

void MackieControlTests::TheStarterIsAWholeSurface()
{
    midiapp::EndpointMatch match{};

    auto const document = glass::BuildLayoutFromTemplate(
        glass::LayoutTemplateKind::MackieControl, L"Mackie", L"DAW", match,
        midiapp::EndpointMatchMode::EndpointName);

    VERIFY_IS_TRUE(document.Devices[0].Protocol == glass::DeviceProtocol::MackieControl);

    std::vector<uint32_t> functions{};
    size_t faders{ 0 };
    size_t turns{ 0 };

    for (auto const& control : document.Pages[0].Controls)
    {
        VERIFY_ARE_EQUAL(size_t{ 1 }, control.Messages.size());

        auto const& row = control.Messages[0];

        VERIFY_IS_TRUE(row.Kind == glass::MessageKind::MackieControl);
        VERIFY_IS_TRUE(glass::MackieFunctionFits(row.Number, control.Kind));

        auto const shape = glass::ShapeOfMackieFunction(row.Number);

        if (shape == glass::MackieShape::Fader)
        {
            ++faders;
        }

        // Every turn springs back, or it would run out of travel.
        if (shape == glass::MackieShape::Encoder)
        {
            ++turns;
            VERIFY_IS_TRUE(control.ReturnsToDefault);
            VERIFY_ARE_EQUAL(0.5, control.DefaultValue);
        }

        functions.push_back(row.Number);
    }

    // Eight strips of V-Pot, rec, solo, mute, select and fader; the master; six transport keys,
    // four for banks, five for the cursor, and the jog wheel.
    VERIFY_ARE_EQUAL(size_t{ 65 }, functions.size());
    VERIFY_ARE_EQUAL(size_t{ 9 }, faders);
    VERIFY_ARE_EQUAL(size_t{ 9 }, turns);

    std::sort(functions.begin(), functions.end());
    VERIFY_IS_TRUE(std::adjacent_find(functions.begin(), functions.end()) == functions.end());

    // And it plays: the first fader is pitch bend on channel 1.
    glass::PreparedDestination daw{};
    daw.Name = L"DAW";
    daw.IsAvailable = true;

    glass::BindingEngine engine{};
    engine.Prepare(document, { daw });

    Sends sends{};

    for (size_t index = 0; index < document.Pages[0].Controls.size(); ++index)
    {
        if (document.Pages[0].Controls[index].Messages[0].Number == glass::MackieFaderBase)
        {
            VERIFY_ARE_EQUAL(1u, engine.Evaluate(index, glass::MessageTrigger::Changes, 1.0, sends));
            VERIFY_ARE_EQUAL(Word(0xE, 0, 0x7F, 0x7F), sends[0].Words[0]);
        }
    }
}
