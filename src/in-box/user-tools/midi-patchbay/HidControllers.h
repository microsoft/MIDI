// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Joysticks, wheels and gamepads that Windows reads as HID devices. Pure, for the unit tests.

#include <sal.h>

#include <array>
#include <bitset>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace midipatchbay
{
    enum class HidAxis : int32_t
    {
        X = 0,
        Y = 1,
        Z = 2,
        Rx = 3,
        Ry = 4,
        Rz = 5,
        Slider = 6,
        Slider2 = 7,
        Dial = 8,
        Wheel = 9,
        Throttle = 10,
        Rudder = 11,
        Accelerator = 12,
        Brake = 13,
        Steering = 14,
    };

    constexpr size_t HidAxisCount = 15;

    constexpr uint16_t HidPageGenericDesktop = 0x01;
    constexpr uint16_t HidPageSimulation = 0x02;
    constexpr uint16_t HidPageButton = 0x09;
    constexpr uint16_t HidUsageJoystick = 0x04;
    constexpr uint16_t HidUsageGamepad = 0x05;
    constexpr uint16_t HidUsageMultiAxis = 0x08;
    constexpr uint16_t HidUsageHatSwitch = 0x39;

    struct HidUsage
    {
        uint16_t Page{ 0 };
        uint16_t Usage{ 0 };
    };

    HidUsage HidAxisUsage(_In_ HidAxis axis) noexcept;

    // Only a slider can be found twice; second asks for the second one.
    std::optional<HidAxis> HidAxisFromUsage(_In_ uint16_t page, _In_ uint16_t usage, _In_ bool second) noexcept;

    // Where a control goes back to when it's let go of. A slider stays where it's left.
    enum class ControlRest : int32_t
    {
        Middle = 0,
        Bottom = 1,
        None = 2,
    };

    ControlRest HidAxisRest(_In_ HidAxis axis) noexcept;

    constexpr uint16_t HidMaximumButtons = 128;

    enum class HidControlKind : int32_t
    {
        Button = 0,
        Axis = 1,
        AxisLow = 2,
        AxisHigh = 3,
        HatUp = 4,
        HatDown = 5,
        HatLeft = 6,
        HatRight = 7,
    };

    struct HidControl
    {
        HidControlKind Kind{ HidControlKind::Button };

        // The button, counted from 1, or the HidAxis.
        uint16_t Number{ 1 };

        bool operator==(HidControl const&) const = default;
    };

    constexpr HidControl HidButtonControl(_In_ uint16_t button) noexcept { return { HidControlKind::Button, button }; }
    constexpr HidControl HidAxisControl(_In_ HidAxis axis) noexcept { return { HidControlKind::Axis, static_cast<uint16_t>(axis) }; }

    // "button3", "x", "xLow", "rzHigh", "hatUp".
    std::wstring HidControlKey(_In_ HidControl control);
    std::optional<HidControl> HidControlFromKey(_In_ std::wstring_view key) noexcept;

    bool CanBePressed(_In_ HidControl control) noexcept;
    bool CanSlide(_In_ HidControl control) noexcept;

    std::optional<HidAxis> HidControlAxis(_In_ HidControl control) noexcept;
    ControlRest HidControlRest(_In_ HidControl control) noexcept;

    constexpr std::array<double, HidAxisCount> NoHidAxes() noexcept
    {
        std::array<double, HidAxisCount> axes{};

        for (auto& axis : axes)
        {
            axis = std::numeric_limits<double>::quiet_NaN();
        }

        return axes;
    }

    struct HidReading
    {
        bool Connected{ false };

        std::bitset<HidMaximumButtons> Buttons{};

        // 0 to 1, low end to high end. NaN for an axis the device doesn't have.
        std::array<double, HidAxisCount> Axes{ NoHidAxes() };

        // -1 centered or none, otherwise 0 to 7 clockwise from up.
        int8_t Hat{ -1 };
    };

    // NaN when the control isn't there. A button or a direction is 0 or 1.
    double HidControlValue(_In_ HidReading const& reading, _In_ HidControl control) noexcept;

    bool HidControlIsDown(_In_ HidReading const& reading, _In_ HidControl control) noexcept;

    // A negative logical minimum means the value is signed, in bitSize bits.
    double NormalizeHidValue(_In_ uint32_t raw, _In_ int32_t logicalMinimum, _In_ int32_t logicalMaximum, _In_ uint16_t bitSize) noexcept;

    // One step of a value scaled 0 to 1. NaN when the range is empty.
    double HidValueStep(_In_ int32_t logicalMinimum, _In_ int32_t logicalMaximum, _In_ uint16_t bitSize) noexcept;

    // A control resting between two steps flickers between them, and each flicker would be a
    // message. A value keeps going the way it moved a step at a time; turning back takes two.
    double SteadyHidValue(_In_ double last, _In_ double now, _In_ double step, _Inout_ int8_t& direction) noexcept;

    // -1 for anything outside the logical range, the hat's centered position.
    int8_t HatPosition(_In_ uint32_t raw, _In_ int32_t logicalMinimum, _In_ int32_t logicalMaximum) noexcept;

    std::optional<HidControl> FindMovedHidControl(
        _In_ HidReading const& before,
        _In_ HidReading const& now,
        _In_ bool forButton) noexcept;

    // Moves Learn's starting point for each control to where it rests now. An axis has no value
    // until the controller's first report, and that report can come from the very control being
    // moved, so without this a stick first read halfway over could never be learned.
    void SettleHidLearnBaseline(_Inout_ HidReading& before, _In_ HidReading const& now) noexcept;

    struct HidDeviceId
    {
        std::wstring Name{};
        uint16_t VendorId{ 0 };
        uint16_t ProductId{ 0 };

        // Includes the USB port, so it tells two identical controllers apart.
        std::wstring Path{};

        bool IsEmpty() const noexcept;
        bool operator==(HidDeviceId const&) const = default;
    };

    struct HidDeviceInfo
    {
        HidDeviceId Id{};
        uint16_t ButtonCount{ 0 };
        std::bitset<HidAxisCount> Axes{};
        bool HasHat{ false };
    };

    // The same path, else the same vendor and product (same name first), else the same name. -1 for none.
    int32_t ChooseHidDevice(_In_ std::vector<HidDeviceId> const& present, _In_ HidDeviceId const& wanted) noexcept;

    // An Xbox controller's HID collection, which puts both triggers on one axis. XInput reads those.
    bool IsXInputDevicePath(_In_ std::wstring_view path) noexcept;
}
