// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "BindingEngineTests.h"

#include <array>

#include "BindingEngine.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    glass::LayoutDocument OneControlDocument()
    {
        glass::LayoutDocument document{};

        document.PageWidth = 1280;
        document.PageHeight = 800;
        document.CanvasWidth = 1280;
        document.CanvasHeight = 800;

        glass::DeviceEntry desk{};
        desk.Name = L"Desk";
        document.Devices.push_back(desk);

        glass::Page page{};
        page.Id = L"p1";

        glass::Control fader{};
        fader.Id = L"f1";
        fader.Kind = glass::ControlKind::Fader;
        fader.Width = 40;
        fader.Height = 180;

        glass::ControlMessage cc{};
        cc.Trigger = glass::MessageTrigger::Changes;
        cc.Kind = glass::MessageKind::ControlChange;
        cc.DeviceName = L"Desk";
        cc.GroupIndex = 0;
        cc.ChannelIndex = 0;
        cc.Number = 7;

        fader.Messages.push_back(cc);
        page.Controls.push_back(fader);
        document.Pages.push_back(page);

        return document;
    }

    std::vector<glass::PreparedDestination> DeskOn(glass::DestinationProtocol protocol)
    {
        glass::PreparedDestination desk{};
        desk.Name = L"Desk";
        desk.Protocol = protocol;
        desk.IsAvailable = true;

        return { desk };
    }
}

void BindingEngineTests::ScalesToTheTopOfTheRangeNotOneShort()
{
    // A fader pushed all the way up has to send the maximum the wire can carry. Getting this
    // wrong sends 126 instead of 127 and every layout is quietly a little bit wrong at the top,
    // which is the kind of thing nobody reports and everybody notices.
    VERIFY_ARE_EQUAL(uint32_t{ 127 }, glass::ScaleToBits(1.0, 7));
    VERIFY_ARE_EQUAL(uint32_t{ 16383 }, glass::ScaleToBits(1.0, 14));
    VERIFY_ARE_EQUAL(uint32_t{ 65535 }, glass::ScaleToBits(1.0, 16));
    VERIFY_ARE_EQUAL(uint32_t{ 4294967295 }, glass::ScaleToBits(1.0, 32));

    VERIFY_ARE_EQUAL(uint32_t{ 0 }, glass::ScaleToBits(0.0, 7));
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, glass::ScaleToBits(0.0, 32));
}

void BindingEngineTests::ScalesTheMidpointWhereItBelongs()
{
    // 0.5 of 127 is 63.5, which rounds to 64.
    VERIFY_ARE_EQUAL(uint32_t{ 64 }, glass::ScaleToBits(0.5, 7));
    VERIFY_ARE_EQUAL(uint32_t{ 8192 }, glass::ScaleToBits(0.5, 14));

    // and a value near the bottom does not collapse to zero
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, glass::ScaleToBits(1.0 / 127.0, 7));
}

void BindingEngineTests::ClampsAndSurvivesNonsense()
{
    VERIFY_ARE_EQUAL(uint32_t{ 127 }, glass::ScaleToBits(5.0, 7));
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, glass::ScaleToBits(-5.0, 7));

    auto const nan = std::numeric_limits<double>::quiet_NaN();
    auto const infinity = std::numeric_limits<double>::infinity();

    VERIFY_ARE_EQUAL(uint32_t{ 0 }, glass::ScaleToBits(nan, 7));
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, glass::ScaleToBits(infinity, 7));
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, glass::ScaleToBits(0.5, 0));
}

void BindingEngineTests::AlwaysSendsMidi2ProtocolWhateverTheDeviceIs()
{
    // The service downscales, both from MIDI 2.0 protocol to MIDI 1.0 protocol and from UMP to
    // MIDI 1.0 byte format, for whichever of the client or the device needs it. So the app sends
    // at full resolution and does not fold anything itself. The destination's own protocol must
    // make no difference to what leaves here.
    auto const document = OneControlDocument();

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> onMidi1{};
    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> onMidi2{};

    glass::BindingEngine one{};
    one.Prepare(document, DeskOn(glass::DestinationProtocol::Midi1));

    glass::BindingEngine two{};
    two.Prepare(document, DeskOn(glass::DestinationProtocol::Midi2));

    for (auto const value : { 0.0, 0.25, 0.5, 0.75, 1.0 })
    {
        VERIFY_ARE_EQUAL(uint32_t{ 1 }, one.Evaluate(0, glass::MessageTrigger::Changes, value, onMidi1));
        VERIFY_ARE_EQUAL(uint32_t{ 1 }, two.Evaluate(0, glass::MessageTrigger::Changes, value, onMidi2));

        VERIFY_ARE_EQUAL(onMidi1[0].WordCount, onMidi2[0].WordCount);
        VERIFY_ARE_EQUAL(onMidi1[0].Words[0], onMidi2[0].Words[0]);
        VERIFY_ARE_EQUAL(onMidi1[0].Words[1], onMidi2[0].Words[1]);
    }
}

void BindingEngineTests::BuildsAMidi2ControlChangeAtFullResolution()
{
    glass::BindingEngine engine{};
    engine.Prepare(OneControlDocument(), DeskOn(glass::DestinationProtocol::Midi1));

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    VERIFY_ARE_EQUAL(uint32_t{ 1 }, engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends));
    VERIFY_ARE_EQUAL(uint32_t{ 2 }, sends[0].WordCount);

    // message type 4, group 0, control change on channel 0, index 7
    VERIFY_ARE_EQUAL(0x40B00700u, sends[0].Words[0]);

    // the whole 32 bit range, not a 7 bit value sitting in a 32 bit field
    VERIFY_ARE_EQUAL(0xFFFFFFFFu, sends[0].Words[1]);

    engine.Evaluate(0, glass::MessageTrigger::Changes, 0.5, sends);
    VERIFY_ARE_EQUAL(0x80000000u, sends[0].Words[1]);
}

void BindingEngineTests::BuildsANoteAtSixteenBitVelocity()
{
    auto document = OneControlDocument();

    document.Pages[0].Controls[0].Messages[0].Kind = glass::MessageKind::Note;
    document.Pages[0].Controls[0].Messages[0].Number = 60;

    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi1));

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends);

    VERIFY_ARE_EQUAL(uint32_t{ 2 }, sends[0].WordCount);
    VERIFY_ARE_EQUAL(0x40903C00u, sends[0].Words[0]);

    // A 7 bit velocity here would throw away the resolution on a velocity sensitive pad, and the
    // service is perfectly able to fold it for a device that cannot use it.
    VERIFY_ARE_EQUAL(0xFFFF0000u, sends[0].Words[1]);
}

void BindingEngineTests::SendsRegisteredControllersForTheServiceToExpand()
{
    auto document = OneControlDocument();
    document.Pages[0].Controls[0].Messages[0].Kind = glass::MessageKind::RegisteredController;
    document.Pages[0].Controls[0].Messages[0].Number = (5 << 7) | 9;

    // The destination is a MIDI 1.0 device. This still goes out: the service turns one registered
    // controller into the four control change messages that carry it on MIDI 1.0. Refusing to
    // send would deny the customer a feature the platform already provides.
    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi1));

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    VERIFY_ARE_EQUAL(uint32_t{ 1 }, engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends));
    VERIFY_ARE_EQUAL(uint32_t{ 2 }, sends[0].WordCount);

    // bank 5, index 9, which is the shape the scratch pad sends and Pocket MIDI receives as
    // CC 101 / CC 100 / CC 6 / CC 38.
    VERIFY_ARE_EQUAL(0x40200509u, sends[0].Words[0]);
}

void BindingEngineTests::NeverScalesAProgramNumber()
{
    auto document = OneControlDocument();
    document.Pages[0].Controls[0].Messages[0].Kind = glass::MessageKind::ProgramChange;
    document.Pages[0].Controls[0].Messages[0].Number = 42;

    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi2));

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    // A program number is an identity, not a position. Scaling it by the control's value would
    // recall a different patch depending on where a fader happened to be.
    for (auto const value : { 0.0, 0.5, 1.0 })
    {
        engine.Evaluate(0, glass::MessageTrigger::Changes, value, sends);
        VERIFY_ARE_EQUAL(0x40C00000u, sends[0].Words[0]);
        VERIFY_ARE_EQUAL(0x2A000000u, sends[0].Words[1]);
    }
}

void BindingEngineTests::SendsMidi1ProtocolWhenTheMessageAsksForIt()
{
    auto document = OneControlDocument();
    document.Pages[0].Controls[0].Messages[0].UseMidi1Protocol = true;

    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi2));

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    // Still UMP, but MIDI 1.0 protocol: one word, message type 2. Even though the device is a
    // MIDI 2.0 one, because the customer asked for an exact value.
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends));
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, sends[0].WordCount);
    VERIFY_ARE_EQUAL(0x20B0077Fu, sends[0].Words[0]);
}

void BindingEngineTests::APadColorVelocityLandsExactlyAsTyped()
{
    // Some pads set their color from the velocity of a note on: 127 discrete colors plus off.
    // "Nearly 37" is a different color, so this is a code rather than a position.
    auto document = OneControlDocument();

    auto& message = document.Pages[0].Controls[0].Messages[0];
    message.Kind = glass::MessageKind::Note;
    message.Number = 36;
    message.UseMidi1Protocol = true;
    message.Maximum = { 37, glass::ValueScaling::Absolute };

    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi2));

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends);

    VERIFY_ARE_EQUAL(uint32_t{ 1 }, sends[0].WordCount);

    auto const velocity = sends[0].Words[0] & 0x7F;
    Log::Comment(String().Format(L"asked for velocity 37, wire carries %u", velocity));

    VERIFY_ARE_EQUAL(uint32_t{ 37 }, velocity);
    VERIFY_ARE_EQUAL(0x20902425u, sends[0].Words[0]);
}

void BindingEngineTests::EverySevenBitValueSurvivesTheFractionExactly()
{
    // The model stores a fraction, not a byte. That is only safe for exact values if every one of
    // the 128 seven bit values round trips through it, so check all of them rather than a sample.
    for (uint32_t expected = 0; expected < 128; ++expected)
    {
        auto const fraction = expected / 127.0;
        auto const actual = glass::ScaleToBits(fraction, 7);

        if (actual != expected)
        {
            Log::Error(String().Format(L"value %u came back as %u", expected, actual));
        }

        VERIFY_ARE_EQUAL(expected, actual);
    }
}

void BindingEngineTests::AnAbsoluteValueIsWrittenNotScaled()
{
    auto document = OneControlDocument();

    auto& message = document.Pages[0].Controls[0].Messages[0];
    message.UseMidi1Protocol = true;
    message.Minimum = { 0, glass::ValueScaling::Absolute };
    message.Maximum = { 64, glass::ValueScaling::Absolute };

    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi2));

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    // 64 means 64, not 64 percent of 127.
    engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends);
    VERIFY_ARE_EQUAL(uint32_t{ 64 }, sends[0].Words[0] & 0x7F);

    engine.Evaluate(0, glass::MessageTrigger::Changes, 0.0, sends);
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, sends[0].Words[0] & 0x7F);
}

void BindingEngineTests::DrivesAnApc40ClipLedFromItsOwnDocumentation()
{
    // Straight out of the APC40 Mk2 communication protocol: a clip LED is a note on where the
    // note number is the LED, the channel is the display type and the velocity is the color.
    // Clip Launch 1 is note 0x00, channel 0 is a solid primary color, and velocity 5 is #FF0000.
    //
    // A customer reading that table types 0, 0 and 5. If the model could only hold percentages
    // they would have to work out that 5 is 3.9 percent, and the editor would show them 3.9
    // afterwards rather than the 5 the manual told them to use.
    auto document = OneControlDocument();

    auto& led = document.Pages[0].Controls[0].Messages[0];
    led.Kind = glass::MessageKind::Note;
    led.Trigger = glass::MessageTrigger::TurnsOn;
    led.Number = 0x00;
    led.ChannelIndex = 0;
    led.UseMidi1Protocol = true;
    led.Maximum = { 5, glass::ValueScaling::Absolute };

    // The same LED pulsing green: channel 9 is "Secondary Color - Pulsing 1/4", velocity 21 is
    // #00FF00.
    glass::ControlMessage pulsing{};
    pulsing.Kind = glass::MessageKind::Note;
    pulsing.Trigger = glass::MessageTrigger::Touched;
    pulsing.DeviceName = L"Desk";
    pulsing.Number = 0x00;
    pulsing.ChannelIndex = 9;
    pulsing.UseMidi1Protocol = true;
    pulsing.Maximum = { 21, glass::ValueScaling::Absolute };

    document.Pages[0].Controls[0].Messages.push_back(pulsing);

    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi2));

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    engine.Evaluate(0, glass::MessageTrigger::TurnsOn, 1.0, sends);
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, sends[0].WordCount);
    VERIFY_ARE_EQUAL(0x20900005u, sends[0].Words[0]);

    engine.Evaluate(0, glass::MessageTrigger::Touched, 1.0, sends);
    VERIFY_ARE_EQUAL(0x20990015u, sends[0].Words[0]);
}

void BindingEngineTests::AnAbsoluteValueWorksOnAWideMidi2Field()
{
    // An exact value has to mean the same thing in a 16 bit velocity as in a 7 bit one, so
    // absolute is not tied to MIDI 1.0.
    auto document = OneControlDocument();

    auto& message = document.Pages[0].Controls[0].Messages[0];
    message.Kind = glass::MessageKind::Note;
    message.Number = 60;
    message.Maximum = { 30000, glass::ValueScaling::Absolute };

    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi2));

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends);

    VERIFY_ARE_EQUAL(uint32_t{ 2 }, sends[0].WordCount);
    VERIFY_ARE_EQUAL(uint32_t{ 30000 }, sends[0].Words[1] >> 16);
}

void BindingEngineTests::ClampsAnAbsoluteValueToItsField()
{
    VERIFY_ARE_EQUAL(uint32_t{ 5 }, glass::ClampToBits(5, 7));
    VERIFY_ARE_EQUAL(uint32_t{ 127 }, glass::ClampToBits(5000, 7));
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, glass::ClampToBits(-10, 7));
    VERIFY_ARE_EQUAL(uint32_t{ 65535 }, glass::ClampToBits(70000, 16));
    VERIFY_ARE_EQUAL(uint32_t{ 4294967295 }, glass::ClampToBits(1e30, 32));

    auto const nan = std::numeric_limits<double>::quiet_NaN();
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, glass::ClampToBits(nan, 7));
}

void BindingEngineTests::AFaderLimitedToSevenBitsQuantizesOntoWholeNumbers()
{
    // A fader on a MIDI 2.0 endpoint whose range is stated in MIDI 1.0 terms. The ends are whole
    // numbers, so the rounding quantizes it onto the 128 values the customer asked for even
    // though the field is 32 bits wide.
    auto document = OneControlDocument();

    auto& message = document.Pages[0].Controls[0].Messages[0];
    message.UseMidi1Protocol = true;
    message.Minimum = { 0, glass::ValueScaling::Absolute };
    message.Maximum = { 127, glass::ValueScaling::Absolute };

    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi2));

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    struct { double Position; uint32_t Expected; } const cases[]
    {
        { 0.0, 0 }, { 0.25, 32 }, { 0.5, 64 }, { 0.75, 95 }, { 1.0, 127 },
    };

    for (auto const& item : cases)
    {
        engine.Evaluate(0, glass::MessageTrigger::Changes, item.Position, sends);

        auto const actual = sends[0].Words[0] & 0x7F;
        Log::Comment(String().Format(L"position %.2f -> %u", item.Position, actual));

        VERIFY_ARE_EQUAL(item.Expected, actual);
    }
}

void BindingEngineTests::AButtonIsJustTheTwoEndsOfARange()
{
    // A button only ever sits at one end or the other, so it needs no separate on and off pair:
    // the range already is one. Off is 17 and on is 13005, which is the kind of thing a MIDI 2.0
    // device's documentation will ask for.
    auto document = OneControlDocument();

    auto& message = document.Pages[0].Controls[0].Messages[0];
    message.Kind = glass::MessageKind::Note;
    message.Number = 60;
    message.Minimum = { 17, glass::ValueScaling::Absolute };
    message.Maximum = { 13005, glass::ValueScaling::Absolute };

    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi2));

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends);
    VERIFY_ARE_EQUAL(uint32_t{ 13005 }, sends[0].Words[1] >> 16);

    engine.Evaluate(0, glass::MessageTrigger::Changes, 0.0, sends);
    VERIFY_ARE_EQUAL(uint32_t{ 17 }, sends[0].Words[1] >> 16);

    // and the percentage form of the same idea
    message.Minimum = { 0.0, glass::ValueScaling::Fraction };
    message.Maximum = { 1.0, glass::ValueScaling::Fraction };
    document.Pages[0].Controls[0].Messages[0] = message;

    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi2));

    engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends);
    VERIFY_ARE_EQUAL(uint32_t{ 65535 }, sends[0].Words[1] >> 16);
}

void BindingEngineTests::TheTwoEndsCanUseDifferentUnits()
{
    // Nothing says both ends have to be stated the same way. "From zero to exactly 100" is a
    // reasonable thing to ask for and it should not force the bottom end into absolute units too.
    auto document = OneControlDocument();

    auto& message = document.Pages[0].Controls[0].Messages[0];
    message.UseMidi1Protocol = true;
    message.Minimum = { 0.0, glass::ValueScaling::Fraction };
    message.Maximum = { 100, glass::ValueScaling::Absolute };

    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi1));

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends);
    VERIFY_ARE_EQUAL(uint32_t{ 100 }, sends[0].Words[0] & 0x7F);

    engine.Evaluate(0, glass::MessageTrigger::Changes, 0.5, sends);
    VERIFY_ARE_EQUAL(uint32_t{ 50 }, sends[0].Words[0] & 0x7F);
}

void BindingEngineTests::AMinimumAboveAMaximumInvertsTheControl()
{
    // Falls out of the arithmetic rather than needing a switch, and it is what somebody wants for
    // a fader that reads top to bottom.
    auto document = OneControlDocument();

    auto& message = document.Pages[0].Controls[0].Messages[0];
    message.UseMidi1Protocol = true;
    message.Minimum = { 127, glass::ValueScaling::Absolute };
    message.Maximum = { 0, glass::ValueScaling::Absolute };

    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi1));

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    engine.Evaluate(0, glass::MessageTrigger::Changes, 0.0, sends);
    VERIFY_ARE_EQUAL(uint32_t{ 127 }, sends[0].Words[0] & 0x7F);

    engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends);
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, sends[0].Words[0] & 0x7F);
}

void BindingEngineTests::StopsAtEvenPercentages()
{
    // 0 to 100 percent in steps of 10.
    auto document = OneControlDocument();

    auto& message = document.Pages[0].Controls[0].Messages[0];
    message.UseMidi1Protocol = true;
    message.Detents.Mode = glass::DetentMode::EvenSteps;
    message.Detents.Scaling = glass::ValueScaling::Fraction;
    message.Detents.Step = 0.1;

    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi1));

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    std::vector<uint32_t> seen{};

    for (int32_t i = 0; i <= 100; ++i)
    {
        engine.Evaluate(0, glass::MessageTrigger::Changes, i / 100.0, sends);

        auto const value = sends[0].Words[0] & 0x7F;

        if (seen.empty() || seen.back() != value)
        {
            seen.push_back(value);
        }
    }

    Log::Comment(String().Format(L"a 10 percent detented fader stopped at %zu values", seen.size()));

    // eleven stops, 0 through 127 in tenths
    VERIFY_ARE_EQUAL(size_t{ 11 }, seen.size());
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, seen.front());
    VERIFY_ARE_EQUAL(uint32_t{ 127 }, seen.back());
}

void BindingEngineTests::StopsAtEvenAbsoluteSteps()
{
    // A knob that goes 27 to 127 in steps of 5.
    auto document = OneControlDocument();

    auto& message = document.Pages[0].Controls[0].Messages[0];
    message.UseMidi1Protocol = true;
    message.Minimum = { 27, glass::ValueScaling::Absolute };
    message.Maximum = { 127, glass::ValueScaling::Absolute };
    message.Detents.Mode = glass::DetentMode::EvenSteps;
    message.Detents.Scaling = glass::ValueScaling::Absolute;
    message.Detents.Step = 5;

    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi1));

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    std::vector<uint32_t> seen{};

    for (int32_t i = 0; i <= 200; ++i)
    {
        engine.Evaluate(0, glass::MessageTrigger::Changes, i / 200.0, sends);

        auto const value = sends[0].Words[0] & 0x7F;

        if (seen.empty() || seen.back() != value)
        {
            seen.push_back(value);
        }
    }

    VERIFY_ARE_EQUAL(size_t{ 21 }, seen.size());
    VERIFY_ARE_EQUAL(uint32_t{ 27 }, seen.front());
    VERIFY_ARE_EQUAL(uint32_t{ 127 }, seen.back());

    // every stop is a multiple of five above the bottom end, including the bottom end itself
    for (auto const value : seen)
    {
        VERIFY_ARE_EQUAL(uint32_t{ 0 }, (value - 27) % 5);
    }
}

void BindingEngineTests::StopsAtAnArbitraryListOfValues()
{
    auto document = OneControlDocument();

    auto& message = document.Pages[0].Controls[0].Messages[0];
    message.UseMidi1Protocol = true;
    message.Detents.Mode = glass::DetentMode::ExplicitValues;
    message.Detents.Scaling = glass::ValueScaling::Absolute;
    message.Detents.Stops = { 10, 17, 38, 39, 40, 57 };

    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi1));

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    // The ends land exactly on the first and last stop.
    engine.Evaluate(0, glass::MessageTrigger::Changes, 0.0, sends);
    VERIFY_ARE_EQUAL(uint32_t{ 10 }, sends[0].Words[0] & 0x7F);

    engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends);
    VERIFY_ARE_EQUAL(uint32_t{ 57 }, sends[0].Words[0] & 0x7F);

    // and nothing outside the list is ever sent
    std::vector<uint32_t> const allowed{ 10, 17, 38, 39, 40, 57 };

    for (int32_t i = 0; i <= 500; ++i)
    {
        engine.Evaluate(0, glass::MessageTrigger::Changes, i / 500.0, sends);

        auto const value = sends[0].Words[0] & 0x7F;

        VERIFY_IS_TRUE(std::find(allowed.begin(), allowed.end(), value) != allowed.end());
    }
}

void BindingEngineTests::CrowdedStopsAreStillEachReachable()
{
    // This is the whole reason a listed stop gets an equal share of the travel rather than
    // sitting where its value falls. 38, 39 and 40 are one apart in a range 47 wide; spaced by
    // value they would be a fortieth of the travel apart and nobody could pick one on purpose.
    auto document = OneControlDocument();

    auto& message = document.Pages[0].Controls[0].Messages[0];
    message.UseMidi1Protocol = true;
    message.Detents.Mode = glass::DetentMode::ExplicitValues;
    message.Detents.Scaling = glass::ValueScaling::Absolute;
    message.Detents.Stops = { 10, 17, 38, 39, 40, 57 };

    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi1));

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    auto const count = glass::DetentCount(glass::PreparedMessage{}, 7);
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, count);

    std::vector<uint32_t> reached{};

    for (uint32_t index = 0; index < 6; ++index)
    {
        engine.Evaluate(0, glass::MessageTrigger::Changes, glass::DetentPosition(index, 6), sends);
        reached.push_back(sends[0].Words[0] & 0x7F);
    }

    Log::Comment(String().Format(L"six stops reached: %u %u %u %u %u %u",
        reached[0], reached[1], reached[2], reached[3], reached[4], reached[5]));

    std::vector<uint32_t> const expected{ 10, 17, 38, 39, 40, 57 };
    VERIFY_IS_TRUE(reached == expected);
}

void BindingEngineTests::ReportsHowManyStopsThereAreForTheSurface()
{
    glass::PreparedMessage smooth{};
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, glass::DetentCount(smooth, 7));

    glass::PreparedMessage tenths{};
    tenths.Detents.Mode = glass::DetentMode::EvenSteps;
    tenths.Detents.Step = 0.1;
    VERIFY_ARE_EQUAL(uint32_t{ 11 }, glass::DetentCount(tenths, 7));

    glass::PreparedMessage fives{};
    fives.Minimum = { 27, glass::ValueScaling::Absolute };
    fives.Maximum = { 127, glass::ValueScaling::Absolute };
    fives.Detents.Mode = glass::DetentMode::EvenSteps;
    fives.Detents.Scaling = glass::ValueScaling::Absolute;
    fives.Detents.Step = 5;
    VERIFY_ARE_EQUAL(uint32_t{ 21 }, glass::DetentCount(fives, 7));

    glass::PreparedMessage listed{};
    listed.Detents.Mode = glass::DetentMode::ExplicitValues;
    listed.Detents.Stops = { 0.1, 0.2, 0.3 };
    VERIFY_ARE_EQUAL(uint32_t{ 3 }, glass::DetentCount(listed, 7));

    // stops are evenly spaced in travel whatever their values
    VERIFY_ARE_EQUAL(0.0, glass::DetentPosition(0, 3));
    VERIFY_ARE_EQUAL(0.5, glass::DetentPosition(1, 3));
    VERIFY_ARE_EQUAL(1.0, glass::DetentPosition(2, 3));
}

void BindingEngineTests::SurvivesNonsenseDetents()
{
    auto document = OneControlDocument();

    auto& message = document.Pages[0].Controls[0].Messages[0];
    message.UseMidi1Protocol = true;
    message.Detents.Mode = glass::DetentMode::EvenSteps;
    message.Detents.Step = 0.0;

    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi1));

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    // A zero step would divide by zero if it were believed. It falls back to smooth.
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends));
    VERIFY_ARE_EQUAL(uint32_t{ 127 }, sends[0].Words[0] & 0x7F);

    // an empty list is smooth too rather than sending nothing
    message.Detents.Mode = glass::DetentMode::ExplicitValues;
    message.Detents.Stops.clear();
    document.Pages[0].Controls[0].Messages[0] = message;

    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi1));

    VERIFY_ARE_EQUAL(uint32_t{ 1 }, engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends));
    VERIFY_ARE_EQUAL(uint32_t{ 127 }, sends[0].Words[0] & 0x7F);
}

void BindingEngineTests::PreviewsPitchBendWithTheLowByteFirst()
{
    glass::PreparedMessage message{};
    message.Kind = glass::MessageKind::PitchBend;

    uint32_t words[4]{};

    // Center is 8192, which is 0x2000: low seven bits 0, high seven bits 0x40.
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, glass::BuildMidi1ProtocolWords(message, 0.5, words));
    VERIFY_ARE_EQUAL(0x20E00040u, words[0]);

    // Full up is 16383: both bytes 0x7F.
    glass::BuildMidi1ProtocolWords(message, 1.0, words);
    VERIFY_ARE_EQUAL(0x20E07F7Fu, words[0]);
}

void BindingEngineTests::DoesNotPretendToPreviewAnRpnExpansion()
{
    glass::PreparedMessage message{};
    message.Kind = glass::MessageKind::RegisteredController;

    uint32_t words[4]{};

    // One registered controller becomes four control change messages on MIDI 1.0. The service
    // does that expansion; reproducing it here would be a second implementation of something the
    // platform already owns, and the two would drift.
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, glass::BuildMidi1ProtocolWords(message, 1.0, words));
}

void BindingEngineTests::ResolvesDeviceNamesToIndexesOnce()
{
    auto document = OneControlDocument();

    glass::DeviceEntry synth{};
    synth.Name = L"Synth";
    document.Devices.push_back(synth);

    glass::PreparedDestination desk{ L"Desk", glass::DestinationProtocol::Midi1, true };
    glass::PreparedDestination synthDest{ L"Synth", glass::DestinationProtocol::Midi2, true };

    glass::BindingEngine engine{};
    engine.Prepare(document, { desk, synthDest });

    VERIFY_ARE_EQUAL(size_t{ 1 }, engine.ControlCount());
    VERIFY_ARE_EQUAL(size_t{ 2 }, engine.Destinations().size());

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends);
    VERIFY_ARE_EQUAL(0, sends[0].DestinationIndex);
}

void BindingEngineTests::SkipsAMessageWhoseDeviceIsMissing()
{
    auto document = OneControlDocument();
    document.Pages[0].Controls[0].Messages[0].DeviceName = L"A Device Nobody Added";

    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi1));

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    VERIFY_ARE_EQUAL(uint32_t{ 0 }, engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends));
}

void BindingEngineTests::StopsSendingWhenADeviceGoesAwayAndResumesWhenItReturns()
{
    auto const document = OneControlDocument();

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi1));

    VERIFY_ARE_EQUAL(uint32_t{ 1 }, engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends));

    // The device is pulled mid performance. Controls bound to it stop sending rather than
    // throwing, and nothing is queued: stale MIDI arriving late is worse than nothing.
    glass::PreparedDestination gone{ L"Desk", glass::DestinationProtocol::Midi1, false };
    engine.Prepare(document, { gone });

    VERIFY_ARE_EQUAL(uint32_t{ 0 }, engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends));
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, engine.Evaluate(0, glass::MessageTrigger::Changes, 0.5, sends));

    // and it comes back on its own
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi1));
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends));
}

void BindingEngineTests::SendsOnlyTheMatchingTrigger()
{
    auto document = OneControlDocument();

    glass::ControlMessage onPress{};
    onPress.Trigger = glass::MessageTrigger::TurnsOn;
    onPress.Kind = glass::MessageKind::Note;
    onPress.DeviceName = L"Desk";
    onPress.Number = 60;

    document.Pages[0].Controls[0].Messages.push_back(onPress);

    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi1));

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    VERIFY_ARE_EQUAL(uint32_t{ 1 }, engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends));
    VERIFY_ARE_EQUAL(0x40B00700u, sends[0].Words[0]);

    VERIFY_ARE_EQUAL(uint32_t{ 1 }, engine.Evaluate(0, glass::MessageTrigger::TurnsOn, 1.0, sends));
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, engine.Evaluate(0, glass::MessageTrigger::Released, 1.0, sends));
}

void BindingEngineTests::SendsToSeveralDevicesFromOneControl()
{
    // The ordinary case, not an advanced one: a button sends a note to the synth and a control
    // change to the DAW.
    auto document = OneControlDocument();

    glass::DeviceEntry daw{};
    daw.Name = L"DAW";
    document.Devices.push_back(daw);

    glass::ControlMessage toDaw{};
    toDaw.Trigger = glass::MessageTrigger::Changes;
    toDaw.Kind = glass::MessageKind::ControlChange;
    toDaw.DeviceName = L"DAW";
    toDaw.ChannelIndex = 15;
    toDaw.Number = 20;

    document.Pages[0].Controls[0].Messages.push_back(toDaw);

    glass::PreparedDestination desk{ L"Desk", glass::DestinationProtocol::Midi1, true };
    glass::PreparedDestination dawDest{ L"DAW", glass::DestinationProtocol::Midi2, true };

    glass::BindingEngine engine{};
    engine.Prepare(document, { desk, dawDest });

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    VERIFY_ARE_EQUAL(uint32_t{ 2 }, engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, sends));

    VERIFY_ARE_EQUAL(0, sends[0].DestinationIndex);
    VERIFY_ARE_EQUAL(0x40B00700u, sends[0].Words[0]);
    VERIFY_ARE_EQUAL(0xFFFFFFFFu, sends[0].Words[1]);

    // the same movement to the other device, on channel 16
    VERIFY_ARE_EQUAL(1, sends[1].DestinationIndex);
    VERIFY_ARE_EQUAL(0x40BF1400u, sends[1].Words[0]);
    VERIFY_ARE_EQUAL(0xFFFFFFFFu, sends[1].Words[1]);
}

void BindingEngineTests::SendsStartupValuesInKeyboardOrder()
{
    auto document = OneControlDocument();

    document.Pages[0].Controls[0].KeyboardOrder = 2;
    document.Pages[0].Controls[0].DefaultValue = 1.0;
    document.Pages[0].Controls[0].SendsValueOnStart = true;

    auto second = document.Pages[0].Controls[0];
    second.Id = L"f2";
    second.KeyboardOrder = 1;
    second.DefaultValue = 0.0;
    second.Messages[0].Number = 8;

    document.Pages[0].Controls.push_back(second);

    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi1));

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    VERIFY_ARE_EQUAL(uint32_t{ 2 }, engine.EvaluateStartupValues(sends));

    // Keyboard order, not file order. It is the order the person building the layout could see
    // and fix, which matters when a synth has to be put into a known state in a set sequence.
    VERIFY_ARE_EQUAL(0x40B00800u, sends[0].Words[0]);
    VERIFY_ARE_EQUAL(0x00000000u, sends[0].Words[1]);
    VERIFY_ARE_EQUAL(0x40B00700u, sends[1].Words[0]);
    VERIFY_ARE_EQUAL(0xFFFFFFFFu, sends[1].Words[1]);
}

void BindingEngineTests::TheGlobalOverrideSuppressesEveryStartupValue()
{
    auto document = OneControlDocument();

    document.Pages[0].Controls[0].DefaultValue = 0.0;
    document.Pages[0].Controls[0].SendsValueOnStart = true;

    // A layout whose faders sit at zero, pushed to a live desk on load, mutes the show. This is
    // the one switch that stops it.
    document.SuppressAllStartupValues = true;

    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi1));

    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};

    VERIFY_ARE_EQUAL(uint32_t{ 0 }, engine.EvaluateStartupValues(sends));
}

void BindingEngineTests::FeedbackMovesAControlFromADevice()
{
    auto document = OneControlDocument();

    auto& fader = document.Pages[0].Controls[0];
    fader.Feedback.Enabled = true;
    fader.Feedback.Kind = glass::MessageKind::ControlChange;
    fader.Feedback.DeviceName = L"Desk";
    fader.Feedback.GroupIndex = 0;
    fader.Feedback.ChannelIndex = 0;
    fader.Feedback.Number = 7;

    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi1));

    // The DAW moves the fader: control change 7, value 64, on channel 1.
    uint32_t const incoming[] { 0x20B00740u };

    size_t controlIndex{ 999 };
    double value{ -1.0 };

    VERIFY_IS_TRUE(engine.TryResolveFeedback(incoming, 1, controlIndex, value));
    VERIFY_ARE_EQUAL(size_t{ 0 }, controlIndex);
    VERIFY_IS_LESS_THAN(std::fabs(value - 64.0 / 127.0), 0.001);

    // and the MIDI 2.0 form of the same thing
    uint32_t const incoming2[] { 0x40B00700u, 0x80000000u };

    VERIFY_IS_TRUE(engine.TryResolveFeedback(incoming2, 2, controlIndex, value));
    VERIFY_IS_LESS_THAN(std::fabs(value - 0.5), 0.001);
}

void BindingEngineTests::FeedbackIgnoresAMessageNobodyWants()
{
    auto document = OneControlDocument();

    auto& fader = document.Pages[0].Controls[0];
    fader.Feedback.Enabled = true;
    fader.Feedback.Kind = glass::MessageKind::ControlChange;
    fader.Feedback.DeviceName = L"Desk";
    fader.Feedback.Number = 7;

    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi1));

    size_t controlIndex{ 0 };
    double value{ 0.0 };

    // the right kind on the wrong controller
    uint32_t const wrongNumber[] { 0x20B00840u };
    VERIFY_IS_FALSE(engine.TryResolveFeedback(wrongNumber, 1, controlIndex, value));

    // the right controller on the wrong channel
    uint32_t const wrongChannel[] { 0x20B10740u };
    VERIFY_IS_FALSE(engine.TryResolveFeedback(wrongChannel, 1, controlIndex, value));

    // the right controller in the wrong group
    uint32_t const wrongGroup[] { 0x21B00740u };
    VERIFY_IS_FALSE(engine.TryResolveFeedback(wrongGroup, 1, controlIndex, value));

    // and something that is not a channel voice message at all
    uint32_t const clock[] { 0x10F80000u };
    VERIFY_IS_FALSE(engine.TryResolveFeedback(clock, 1, controlIndex, value));

    VERIFY_IS_FALSE(engine.TryResolveFeedback(nullptr, 0, controlIndex, value));
}

void BindingEngineTests::EvaluateWritesNoMoreThanTheCallerAllowed()
{
    auto document = OneControlDocument();

    // five messages, all on the same trigger
    for (int i = 0; i < 4; ++i)
    {
        auto extra = document.Pages[0].Controls[0].Messages[0];
        extra.Number = static_cast<uint32_t>(20 + i);
        document.Pages[0].Controls[0].Messages.push_back(extra);
    }

    glass::BindingEngine engine{};
    engine.Prepare(document, DeskOn(glass::DestinationProtocol::Midi1));

    std::array<glass::PreparedSend, 2> small{};

    // The hot path writes into a caller-owned buffer and must never write past it.
    VERIFY_ARE_EQUAL(uint32_t{ 2 }, engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, small));

    std::array<glass::PreparedSend, 0> none{};
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, engine.Evaluate(0, glass::MessageTrigger::Changes, 1.0, none));

    // an index nobody has
    std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> sends{};
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, engine.Evaluate(99, glass::MessageTrigger::Changes, 1.0, sends));
}
