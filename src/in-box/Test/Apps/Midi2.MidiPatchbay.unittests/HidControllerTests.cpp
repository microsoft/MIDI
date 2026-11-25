// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "HidControllerTests.h"
#include "TestMessages.h"

#include "ControllerSteps.h"
#include "HidControllers.h"
#include "PatchSerializer.h"
#include "ProcessingBlock.h"

#include <cmath>
#include <set>
#include <string>
#include <vector>

using namespace midipatchbay;
using namespace patchbaytests;

namespace
{
    HidReading Connected()
    {
        HidReading reading{};
        reading.Connected = true;
        return reading;
    }

    HidReading WithAxis(HidAxis axis, double value)
    {
        auto reading = Connected();
        reading.Axes[static_cast<size_t>(axis)] = value;
        return reading;
    }

    HidControl Low(HidAxis axis) { return { HidControlKind::AxisLow, static_cast<uint16_t>(axis) }; }
    HidControl High(HidAxis axis) { return { HidControlKind::AxisHigh, static_cast<uint16_t>(axis) }; }

    HidDeviceId Joystick(wchar_t const* path)
    {
        return HidDeviceId{ L"Extreme 3D pro", 0x046D, 0xC215, path };
    }

    bool Near(double a, double b) { return std::abs(a - b) < 0.000001; }
}

void HidControllerTests::EveryHidControlHasOneKey()
{
    std::set<std::wstring> keys{};
    std::vector<HidControl> controls{};

    for (uint16_t button = 1; button <= HidMaximumButtons; button++)
    {
        controls.push_back(HidButtonControl(button));
    }

    for (size_t axis = 0; axis < HidAxisCount; axis++)
    {
        controls.push_back(HidAxisControl(static_cast<HidAxis>(axis)));
        controls.push_back(Low(static_cast<HidAxis>(axis)));
        controls.push_back(High(static_cast<HidAxis>(axis)));
    }

    for (auto const kind : { HidControlKind::HatUp, HidControlKind::HatDown, HidControlKind::HatLeft, HidControlKind::HatRight })
    {
        controls.push_back(HidControl{ kind, 0 });
    }

    for (auto const& control : controls)
    {
        auto const key = HidControlKey(control);

        VERIFY_IS_FALSE(key.empty());
        VERIFY_IS_TRUE(keys.insert(key).second);
        VERIFY_IS_TRUE(HidControlFromKey(key) == control);
        VERIFY_ARE_NOT_EQUAL(CanBePressed(control), CanSlide(control));
    }

    VERIFY_IS_TRUE(HidControlKey(HidButtonControl(3)) == L"button3");
    VERIFY_IS_TRUE(HidControlKey(High(HidAxis::Rz)) == L"rzHigh");
    VERIFY_IS_TRUE(HidControlFromKey(L"slider2Low") == Low(HidAxis::Slider2));
    VERIFY_IS_TRUE(CanSlide(HidAxisControl(HidAxis::Slider)));
    VERIFY_IS_TRUE(CanBePressed(HidControl{ HidControlKind::HatLeft, 0 }));

    for (auto const bad : { L"button0", L"button129", L"button01", L"Button1", L"xlow", L"hat", L"", L"slider3", L"x Low", L"a" })
    {
        VERIFY_IS_FALSE(HidControlFromKey(bad).has_value());
    }
}

void HidControllerTests::ValuesSpanTheLogicalRange()
{
    // The Extreme 3D Pro's stick: 10 bits.
    VERIFY_ARE_EQUAL(0.0, NormalizeHidValue(0, 0, 1023, 10));
    VERIFY_ARE_EQUAL(1.0, NormalizeHidValue(1023, 0, 1023, 10));
    VERIFY_IS_TRUE(Near(512.0 / 1023.0, NormalizeHidValue(512, 0, 1023, 10)));

    // Signed values arrive as their raw bits.
    VERIFY_ARE_EQUAL(0.0, NormalizeHidValue(0x8000, -32768, 32767, 16));
    VERIFY_ARE_EQUAL(1.0, NormalizeHidValue(0x7FFF, -32768, 32767, 16));
    VERIFY_IS_TRUE(Near(32768.0 / 65535.0, NormalizeHidValue(0, -32768, 32767, 16)));
    VERIFY_ARE_EQUAL(0.0, NormalizeHidValue(0x81, -127, 127, 8));
    VERIFY_ARE_EQUAL(1.0, NormalizeHidValue(0x7F, -127, 127, 8));

    // A maximum of 65535 written in two bytes reads as -1.
    VERIFY_ARE_EQUAL(1.0, NormalizeHidValue(65535, 0, -1, 16));
    VERIFY_ARE_EQUAL(0.0, NormalizeHidValue(0, 0, -1, 16));

    // Past the end is the end, and a range with no width is nothing.
    VERIFY_ARE_EQUAL(1.0, NormalizeHidValue(1023, 0, 1000, 10));
    VERIFY_IS_TRUE(std::isnan(NormalizeHidValue(5, 5, 5, 8)));
}

void HidControllerTests::OneStepFlickerIsIgnored()
{
    // An untouched Extreme 3D Pro's twist flickered between 123 and 124 of 255, 20 times a second.
    auto const step = HidValueStep(0, 255, 8);
    VERIFY_IS_TRUE(Near(1.0 / 255.0, step));
    VERIFY_IS_TRUE(Near(1.0 / 65535.0, HidValueStep(0, -1, 16)));
    VERIFY_IS_TRUE(std::isnan(HidValueStep(5, 5, 8)));

    auto const at = [](uint32_t raw) { return NormalizeHidValue(raw, 0, 255, 8); };

    int8_t direction{ 0 };
    auto value = SteadyHidValue(std::numeric_limits<double>::quiet_NaN(), at(124), step, direction);
    VERIFY_ARE_EQUAL(at(124), value);

    // The first change counts; after that, back and forth by one step doesn't.
    value = SteadyHidValue(value, at(123), step, direction);
    VERIFY_ARE_EQUAL(at(123), value);

    for (int i = 0; i < 10; i++)
    {
        value = SteadyHidValue(value, at(124), step, direction);
        VERIFY_ARE_EQUAL(at(123), value);
        value = SteadyHidValue(value, at(123), step, direction);
        VERIFY_ARE_EQUAL(at(123), value);
    }

    // Going on the same way counts a step at a time; turning back takes two.
    value = SteadyHidValue(value, at(122), step, direction);
    VERIFY_ARE_EQUAL(at(122), value);
    value = SteadyHidValue(value, at(123), step, direction);
    VERIFY_ARE_EQUAL(at(122), value);
    value = SteadyHidValue(value, at(124), step, direction);
    VERIFY_ARE_EQUAL(at(124), value);
    value = SteadyHidValue(value, at(125), step, direction);
    VERIFY_ARE_EQUAL(at(125), value);

    // The ends always count, and a step back from one is flicker.
    value = SteadyHidValue(value, at(255), step, direction);
    VERIFY_ARE_EQUAL(1.0, value);
    value = SteadyHidValue(value, at(254), step, direction);
    VERIFY_ARE_EQUAL(1.0, value);
    value = SteadyHidValue(value, at(253), step, direction);
    VERIFY_ARE_EQUAL(at(253), value);
    value = SteadyHidValue(value, at(254), step, direction);
    VERIFY_ARE_EQUAL(at(253), value);
    value = SteadyHidValue(value, at(255), step, direction);
    VERIFY_ARE_EQUAL(1.0, value);

    value = SteadyHidValue(value, at(0), step, direction);
    VERIFY_ARE_EQUAL(0.0, value);
    value = SteadyHidValue(value, at(1), step, direction);
    VERIFY_ARE_EQUAL(0.0, value);

    // Without a step size, every value counts.
    VERIFY_ARE_EQUAL(0.6, SteadyHidValue(0.5, 0.6, std::numeric_limits<double>::quiet_NaN(), direction));
}

void HidControllerTests::TheHatPointsEightWays()
{
    for (uint32_t raw = 0; raw < 8; raw++)
    {
        VERIFY_ARE_EQUAL(static_cast<int8_t>(raw), HatPosition(raw, 0, 7));
    }

    // Centered is outside the range.
    VERIFY_ARE_EQUAL(int8_t{ -1 }, HatPosition(8, 0, 7));
    VERIFY_ARE_EQUAL(int8_t{ -1 }, HatPosition(15, 0, 7));

    // Counted from 1, as an Xbox controller's HID collection does.
    VERIFY_ARE_EQUAL(int8_t{ 0 }, HatPosition(1, 1, 8));
    VERIFY_ARE_EQUAL(int8_t{ 2 }, HatPosition(3, 1, 8));
    VERIFY_ARE_EQUAL(int8_t{ -1 }, HatPosition(0, 1, 8));

    // Four ways, and degrees.
    VERIFY_ARE_EQUAL(int8_t{ 2 }, HatPosition(1, 0, 3));
    VERIFY_ARE_EQUAL(int8_t{ 6 }, HatPosition(3, 0, 3));
    VERIFY_ARE_EQUAL(int8_t{ 2 }, HatPosition(90, 0, 359));
    VERIFY_ARE_EQUAL(int8_t{ 4 }, HatPosition(180, 0, 359));

    // A diagonal is two directions at once.
    auto reading = Connected();
    reading.Hat = 1;

    VERIFY_IS_TRUE(HidControlIsDown(reading, HidControl{ HidControlKind::HatUp, 0 }));
    VERIFY_IS_TRUE(HidControlIsDown(reading, HidControl{ HidControlKind::HatRight, 0 }));
    VERIFY_IS_FALSE(HidControlIsDown(reading, HidControl{ HidControlKind::HatDown, 0 }));
    VERIFY_IS_FALSE(HidControlIsDown(reading, HidControl{ HidControlKind::HatLeft, 0 }));

    reading.Hat = -1;
    VERIFY_IS_FALSE(HidControlIsDown(reading, HidControl{ HidControlKind::HatUp, 0 }));
}

void HidControllerTests::ButtonsHatsAndAxisEndsPress()
{
    auto reading = Connected();
    reading.Buttons.set(2);

    VERIFY_IS_TRUE(HidControlIsDown(reading, HidButtonControl(3)));
    VERIFY_IS_FALSE(HidControlIsDown(reading, HidButtonControl(2)));
    VERIFY_ARE_EQUAL(1.0, HidControlValue(reading, HidButtonControl(3)));

    auto pushed = WithAxis(HidAxis::X, 0.1);

    VERIFY_IS_TRUE(HidControlIsDown(pushed, Low(HidAxis::X)));
    VERIFY_IS_FALSE(HidControlIsDown(pushed, High(HidAxis::X)));
    VERIFY_IS_TRUE(HidControlIsDown(WithAxis(HidAxis::X, 0.9), High(HidAxis::X)));
    VERIFY_IS_FALSE(HidControlIsDown(WithAxis(HidAxis::X, 0.6), High(HidAxis::X)));

    // A pedal is down past half way, the same as a trigger.
    VERIFY_IS_TRUE(HidControlIsDown(WithAxis(HidAxis::Brake, 0.6), High(HidAxis::Brake)));

    // An axis the device doesn't have is never down, and has no value.
    VERIFY_IS_FALSE(HidControlIsDown(Connected(), Low(HidAxis::Y)));
    VERIFY_IS_TRUE(std::isnan(HidControlValue(Connected(), HidAxisControl(HidAxis::Y))));

    // Nothing is down on a controller that went away.
    reading.Connected = false;
    VERIFY_IS_FALSE(HidControlIsDown(reading, HidButtonControl(3)));

    auto gone = WithAxis(HidAxis::X, 0.5);
    gone.Connected = false;
    VERIFY_IS_TRUE(std::isnan(HidControlValue(gone, HidAxisControl(HidAxis::X))));

    VERIFY_IS_TRUE(HidAxisRest(HidAxis::X) == ControlRest::Middle);
    VERIFY_IS_TRUE(HidAxisRest(HidAxis::Slider) == ControlRest::None);
    VERIFY_IS_TRUE(HidAxisRest(HidAxis::Brake) == ControlRest::Bottom);
}

void HidControllerTests::ASliderHasNoDeadZoneAndNoRest()
{
    ControllerAxisSettings throttle{};
    throttle.Device = Joystick(L"\\\\?\\HID#VID_046D&PID_C215#1");
    throttle.DeviceControl = HidAxisControl(HidAxis::Slider);
    throttle.Target = { midiapp::ValueMessageKind::ControlChange, 0, 0, 7, false };
    throttle.IntervalMilliseconds = 0;

    VERIFY_IS_TRUE(AxisRest(throttle) == ControlRest::None);
    VERIFY_IS_TRUE(Near(0.05, ShapedAxisValue(throttle, 0.05)));

    ControllerAxisMachine machine{};
    std::vector<uint32_t> sent{};
    ControllerSink const sink = [&sent](uint32_t const* words, uint8_t count) { sent.insert(sent.end(), words, words + count); };

    machine.Update(throttle, 0, true, 0.3, sink);
    machine.Update(throttle, 10, true, 0.8, sink);
    VERIFY_ARE_EQUAL(size_t{ 2 }, sent.size());

    // Routing stops, or the joystick goes away: a throttle stays where it was left.
    sent.clear();
    machine.Update(throttle, 20, false, 0.8, sink);
    machine.Release(throttle, sink);
    VERIFY_ARE_EQUAL(size_t{ 0 }, sent.size());

    // The stick itself springs back, so it goes back to the middle.
    auto stick = throttle;
    stick.DeviceControl = HidAxisControl(HidAxis::X);
    stick.Target = { midiapp::ValueMessageKind::PitchBend, 0, 0, 0, false };

    VERIFY_ARE_EQUAL(0.5, ShapedAxisValue(stick, 0.52));

    ControllerAxisMachine stickMachine{};
    stickMachine.Update(stick, 0, true, 0.5, sink);
    stickMachine.Update(stick, 10, true, 1.0, sink);

    // No reading yet for an axis is the same as the stick let go.
    sent.clear();
    stickMachine.Update(stick, 20, true, std::numeric_limits<double>::quiet_NaN(), sink);

    VERIFY_ARE_EQUAL(size_t{ 2 }, sent.size());
    VERIFY_ARE_EQUAL(uint32_t{ 0x80000000 }, sent[1]);
}

void HidControllerTests::LearnFindsAMovedHidControl()
{
    auto const rest = [] 
        {
            auto reading = Connected();
            reading.Axes[static_cast<size_t>(HidAxis::X)] = 0.5;
            reading.Axes[static_cast<size_t>(HidAxis::Slider)] = 0.3;
            reading.Axes[static_cast<size_t>(HidAxis::Brake)] = 0.0;
            return reading;
        }();

    auto pressed = rest;
    pressed.Buttons.set(4);
    VERIFY_IS_TRUE(FindMovedHidControl(rest, pressed, true) == HidButtonControl(5));
    VERIFY_IS_FALSE(FindMovedHidControl(pressed, pressed, true).has_value());

    auto hat = rest;
    hat.Hat = 6;
    VERIFY_IS_TRUE(FindMovedHidControl(rest, hat, true) == (HidControl{ HidControlKind::HatLeft, 0 }));

    auto stick = rest;
    stick.Axes[static_cast<size_t>(HidAxis::X)] = 0.05;
    VERIFY_IS_TRUE(FindMovedHidControl(rest, stick, false) == HidAxisControl(HidAxis::X));
    VERIFY_IS_TRUE(FindMovedHidControl(rest, stick, true) == Low(HidAxis::X));

    // A slider counts once it has moved a quarter of the way, from wherever it was.
    auto slid = rest;
    slid.Axes[static_cast<size_t>(HidAxis::Slider)] = 0.6;
    VERIFY_IS_TRUE(FindMovedHidControl(rest, slid, false) == HidAxisControl(HidAxis::Slider));

    auto braked = rest;
    braked.Axes[static_cast<size_t>(HidAxis::Brake)] = 0.7;
    VERIFY_IS_TRUE(FindMovedHidControl(rest, braked, true) == High(HidAxis::Brake));
    VERIFY_IS_TRUE(FindMovedHidControl(rest, braked, false) == HidAxisControl(HidAxis::Brake));

    // A little drift is nothing, and neither is a button to a stick step.
    auto drift = rest;
    drift.Axes[static_cast<size_t>(HidAxis::X)] = 0.6;
    drift.Axes[static_cast<size_t>(HidAxis::Slider)] = 0.4;
    VERIFY_IS_FALSE(FindMovedHidControl(rest, drift, false).has_value());
    VERIFY_IS_FALSE(FindMovedHidControl(rest, pressed, false).has_value());

    // An axis with no reading before can't have moved.
    VERIFY_IS_FALSE(FindMovedHidControl(Connected(), stick, false).has_value());
}

void HidControllerTests::LearnStartsFromWhereControlsRest()
{
    // The first report came while the stick was pushed most of the way and button 3 was held.
    auto before = Connected();
    before.Axes[static_cast<size_t>(HidAxis::X)] = 0.9;
    before.Buttons.set(2);
    before.Hat = 2;

    auto pushed = before;
    pushed.Axes[static_cast<size_t>(HidAxis::X)] = 0.95;
    SettleHidLearnBaseline(before, pushed);
    VERIFY_IS_FALSE(FindMovedHidControl(before, pushed, false).has_value());

    // Let go of, everything settles where it rests...
    auto resting = Connected();
    resting.Axes[static_cast<size_t>(HidAxis::X)] = 0.52;
    SettleHidLearnBaseline(before, resting);
    VERIFY_ARE_EQUAL(0.52, before.Axes[static_cast<size_t>(HidAxis::X)]);
    VERIFY_IS_FALSE(before.Buttons.test(2));
    VERIFY_ARE_EQUAL(static_cast<int8_t>(-1), before.Hat);
    VERIFY_IS_FALSE(FindMovedHidControl(before, resting, false).has_value());

    // ...so the next push, press, or hat is learned.
    SettleHidLearnBaseline(before, pushed);
    VERIFY_IS_TRUE(FindMovedHidControl(before, pushed, false) == HidAxisControl(HidAxis::X));
    VERIFY_IS_TRUE(FindMovedHidControl(before, pushed, true) == HidButtonControl(3));

    auto hat = resting;
    hat.Hat = 2;
    SettleHidLearnBaseline(before, hat);
    VERIFY_IS_TRUE(FindMovedHidControl(before, hat, true) == (HidControl{ HidControlKind::HatRight, 0 }));

    // A slider has no rest: its first value is where it starts, and moving from there counts.
    auto slider = Connected();
    slider.Axes[static_cast<size_t>(HidAxis::Slider)] = 0.3;
    auto sliderBefore = Connected();
    SettleHidLearnBaseline(sliderBefore, slider);
    VERIFY_ARE_EQUAL(0.3, sliderBefore.Axes[static_cast<size_t>(HidAxis::Slider)]);

    slider.Axes[static_cast<size_t>(HidAxis::Slider)] = 0.8;
    SettleHidLearnBaseline(sliderBefore, slider);
    VERIFY_ARE_EQUAL(0.3, sliderBefore.Axes[static_cast<size_t>(HidAxis::Slider)]);
    VERIFY_IS_TRUE(FindMovedHidControl(sliderBefore, slider, false) == HidAxisControl(HidAxis::Slider));

    // A controller that goes away changes nothing.
    auto gone = HidReading{};
    SettleHidLearnBaseline(sliderBefore, gone);
    VERIFY_ARE_EQUAL(0.3, sliderBefore.Axes[static_cast<size_t>(HidAxis::Slider)]);
}

void HidControllerTests::DevicesMatchByPathThenModelThenName()
{
    std::vector<HidDeviceId> const present
    {
        Joystick(L"\\\\?\\HID#VID_046D&PID_C215#PORT1"),
        Joystick(L"\\\\?\\HID#VID_046D&PID_C215#PORT2"),
        HidDeviceId{ L"T.16000M", 0x044F, 0xB10A, L"\\\\?\\HID#VID_044F&PID_B10A#PORT3" },
    };

    // The same port, whatever the letter case.
    VERIFY_ARE_EQUAL(1, ChooseHidDevice(present, Joystick(L"\\\\?\\hid#vid_046d&pid_c215#port2")));

    // Another port: the same model.
    VERIFY_ARE_EQUAL(0, ChooseHidDevice(present, Joystick(L"\\\\?\\HID#VID_046D&PID_C215#PORT9")));
    VERIFY_ARE_EQUAL(2, ChooseHidDevice(present, HidDeviceId{ L"", 0x044F, 0xB10A, L"" }));

    // A name alone, as an AI agent might write it.
    VERIFY_ARE_EQUAL(2, ChooseHidDevice(present, HidDeviceId{ L"t.16000m", 0, 0, L"" }));

    VERIFY_ARE_EQUAL(-1, ChooseHidDevice(present, HidDeviceId{ L"Nothing", 0x1234, 0x5678, L"" }));
    VERIFY_ARE_EQUAL(-1, ChooseHidDevice(present, HidDeviceId{}));

    // The same model with another name still counts.
    std::vector<HidDeviceId> const renamed{ HidDeviceId{ L"Logitech Extreme 3D", 0x046D, 0xC215, L"x" } };
    VERIFY_ARE_EQUAL(0, ChooseHidDevice(renamed, Joystick(L"y")));
}

void HidControllerTests::XInputCollectionsAreLeftToXInput()
{
    // Paths read from an Xbox Adaptive Controller, a Logitech F310 and an Extreme 3D Pro on a test PC.
    VERIFY_IS_TRUE(IsXInputDevicePath(L"\\\\?\\HID#VID_045E&PID_02FF&IG_00#9&2dce9272&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}"));
    VERIFY_IS_TRUE(IsXInputDevicePath(L"\\\\?\\hid#vid_046d&pid_c21d&ig_00#a&34ff1454&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}"));
    VERIFY_IS_FALSE(IsXInputDevicePath(L"\\\\?\\HID#VID_046D&PID_C215#9&39e2db6a&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}"));
    VERIFY_IS_FALSE(IsXInputDevicePath(L""));
}

void HidControllerTests::DeviceStepsSurviveTheFile()
{
    auto button = DefaultBlockSettings(BlockKind::ControllerButton);
    button.ControllerButton.Player = 3;
    button.ControllerButton.Device = Joystick(L"\\\\?\\HID#VID_046D&PID_C215#PORT1");
    button.ControllerButton.DeviceControl = HidControl{ HidControlKind::HatLeft, 0 };
    button.ControllerButton.Pressed.Number = 48;

    auto const object = BlockSettingsToJson(BlockKind::ControllerButton, button);

    VERIFY_IS_TRUE(object.HasKey(L"device"));
    VERIFY_IS_FALSE(object.HasKey(L"player"));
    VERIFY_IS_TRUE(object.GetNamedString(L"control") == L"hatLeft");
    VERIFY_ARE_EQUAL(double{ 0xC215 }, object.GetNamedObject(L"device").GetNamedNumber(L"productId"));

    auto const read = BlockSettingsFromJson(BlockKind::ControllerButton, object);

    VERIFY_IS_TRUE(BlockSettingsSignature(BlockKind::ControllerButton, read) == BlockSettingsSignature(BlockKind::ControllerButton, button));
    VERIFY_IS_TRUE(read.ControllerButton.Device == button.ControllerButton.Device);

    auto axis = DefaultBlockSettings(BlockKind::ControllerAxis);
    axis.ControllerAxis.Device = HidDeviceId{ L"Wheel", 0x046D, 0xC24F, L"" };
    axis.ControllerAxis.DeviceControl = HidAxisControl(HidAxis::Accelerator);

    auto const readAxis = BlockSettingsFromJson(BlockKind::ControllerAxis, BlockSettingsToJson(BlockKind::ControllerAxis, axis));

    VERIFY_IS_TRUE(BlockSettingsSignature(BlockKind::ControllerAxis, readAxis) == BlockSettingsSignature(BlockKind::ControllerAxis, axis));
    VERIFY_IS_TRUE(readAxis.ControllerAxis.DeviceControl == HidAxisControl(HidAxis::Accelerator));

    // A control that isn't on a HID device, or a button for a stick step, is the first of its kind.
    auto const xboxName = ControllerButtonFromJson(json::JsonObject::Parse(LR"({"device":{"name":"Pad"},"control":"a"})"));
    VERIFY_IS_TRUE(xboxName.ReadsDevice());
    VERIFY_IS_TRUE(xboxName.DeviceControl == HidButtonControl(1));

    auto const buttonOnAxis = ControllerAxisFromJson(json::JsonObject::Parse(LR"({"device":{"vendorId":1133,"productId":49685},"control":"button3"})"));
    VERIFY_IS_TRUE(buttonOnAxis.DeviceControl == HidAxisControl(HidAxis::X));

    // An empty device is an Xbox controller.
    auto const empty = ControllerButtonFromJson(json::JsonObject::Parse(LR"({"device":{},"player":1,"control":"b"})"));
    VERIFY_IS_FALSE(empty.ReadsDevice());
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, empty.Player);
    VERIFY_IS_TRUE(empty.Control == ControllerControl::B);

    // An Xbox step's file has no device in it.
    VERIFY_IS_FALSE(BlockSettingsToJson(BlockKind::ControllerButton, DefaultBlockSettings(BlockKind::ControllerButton)).HasKey(L"device"));
}

void HidControllerTests::ChoosingAnotherDeviceRestartsTheStep()
{
    auto settings = DefaultBlockSettings(BlockKind::ControllerButton);
    auto const xbox = GeneratorRestartSignature(BlockKind::ControllerButton, settings);

    settings.ControllerButton.Device = Joystick(L"\\\\?\\HID#VID_046D&PID_C215#PORT1");
    auto const joystick = GeneratorRestartSignature(BlockKind::ControllerButton, settings);
    VERIFY_IS_FALSE(joystick == xbox);

    // The message changes on the fly.
    settings.ControllerButton.Pressed.Number = 72;
    VERIFY_IS_TRUE(GeneratorRestartSignature(BlockKind::ControllerButton, settings) == joystick);

    settings.ControllerButton.DeviceControl = HidButtonControl(2);
    VERIFY_IS_FALSE(GeneratorRestartSignature(BlockKind::ControllerButton, settings) == joystick);

    settings.ControllerButton.DeviceControl = HidButtonControl(1);
    settings.ControllerButton.Device.Path = L"\\\\?\\HID#VID_046D&PID_C215#PORT2";
    VERIFY_IS_FALSE(GeneratorRestartSignature(BlockKind::ControllerButton, settings) == joystick);

    // An Xbox player number means nothing to a step that reads a device.
    settings.ControllerButton.Device.Path = L"\\\\?\\HID#VID_046D&PID_C215#PORT1";
    settings.ControllerButton.Player = 2;
    VERIFY_IS_TRUE(GeneratorRestartSignature(BlockKind::ControllerButton, settings) == joystick);
}

void HidControllerTests::TheGuidesJoystickExamplesRead()
{
    auto const patch = ReadPatchJson(LR"({
        "fileVersion": 2,
        "endpoints": [ { "id": "synth", "displayName": "Synth" } ],
        "blocks": [
            { "id": "trigger-note", "type": "controllerButton", "settings": { "device": { "name": "Extreme 3D pro", "vendorId": 1133, "productId": 49685 }, "control": "button1", "pressed": { "message": "note", "number": 60 } } },
            { "id": "throttle", "type": "controllerAxis", "settings": { "device": { "name": "Extreme 3D pro", "vendorId": 1133, "productId": 49685 }, "control": "slider", "message": "controlChange", "number": 7, "shape": { "invert": true } } }
        ],
        "connections": [
            { "id": "c1", "source": "trigger-note", "destination": "synth" },
            { "id": "c2", "source": "throttle", "destination": "synth" }
        ] })", L"Guide");

    VERIFY_IS_TRUE(patch.has_value());

    auto const* trigger = patch->FindBlock(L"trigger-note");
    VERIFY_IS_NOT_NULL(trigger);
    VERIFY_IS_TRUE(trigger->Settings.ControllerButton.Device == (HidDeviceId{ L"Extreme 3D pro", 0x046D, 0xC215, L"" }));
    VERIFY_IS_TRUE(trigger->Settings.ControllerButton.DeviceControl == HidButtonControl(1));

    auto const* throttle = patch->FindBlock(L"throttle");
    VERIFY_IS_NOT_NULL(throttle);
    VERIFY_IS_TRUE(throttle->Settings.ControllerAxis.DeviceControl == HidAxisControl(HidAxis::Slider));
    VERIFY_IS_TRUE(throttle->Settings.ControllerAxis.Shape.Invert);
    VERIFY_IS_TRUE(AxisRest(throttle->Settings.ControllerAxis) == ControlRest::None);
}
