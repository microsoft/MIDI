// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Pure: no precompiled header, so the unit tests compile this file exactly as it ships.

#include "HidControllers.h"

#include <algorithm>
#include <cmath>
#include <cwctype>

namespace midipatchbay
{
    namespace
    {
        struct AxisInfo
        {
            HidAxis Axis;
            wchar_t const* Key;
            HidUsage Usage;
            ControlRest Rest;
        };

        // Sticks and twist rest in the middle, pedals at the bottom; sliders stay where they're left.
        constexpr AxisInfo Axes[] =
        {
            { HidAxis::X, L"x", { HidPageGenericDesktop, 0x30 }, ControlRest::Middle },
            { HidAxis::Y, L"y", { HidPageGenericDesktop, 0x31 }, ControlRest::Middle },
            { HidAxis::Z, L"z", { HidPageGenericDesktop, 0x32 }, ControlRest::Middle },
            { HidAxis::Rx, L"rx", { HidPageGenericDesktop, 0x33 }, ControlRest::Middle },
            { HidAxis::Ry, L"ry", { HidPageGenericDesktop, 0x34 }, ControlRest::Middle },
            { HidAxis::Rz, L"rz", { HidPageGenericDesktop, 0x35 }, ControlRest::Middle },
            { HidAxis::Slider, L"slider", { HidPageGenericDesktop, 0x36 }, ControlRest::None },
            { HidAxis::Slider2, L"slider2", { HidPageGenericDesktop, 0x36 }, ControlRest::None },
            { HidAxis::Dial, L"dial", { HidPageGenericDesktop, 0x37 }, ControlRest::None },
            { HidAxis::Wheel, L"wheel", { HidPageGenericDesktop, 0x38 }, ControlRest::None },
            { HidAxis::Throttle, L"throttle", { HidPageSimulation, 0xBB }, ControlRest::None },
            { HidAxis::Rudder, L"rudder", { HidPageSimulation, 0xBA }, ControlRest::Middle },
            { HidAxis::Accelerator, L"accelerator", { HidPageSimulation, 0xC4 }, ControlRest::Bottom },
            { HidAxis::Brake, L"brake", { HidPageSimulation, 0xC5 }, ControlRest::Bottom },
            { HidAxis::Steering, L"steering", { HidPageSimulation, 0xC8 }, ControlRest::Middle },
        };

        static_assert(std::size(Axes) == HidAxisCount);

        constexpr bool AxesAreInOrder() noexcept
        {
            for (size_t i = 0; i < std::size(Axes); i++)
            {
                if (static_cast<size_t>(Axes[i].Axis) != i)
                {
                    return false;
                }
            }

            return true;
        }

        static_assert(AxesAreInOrder());

        constexpr wchar_t ButtonPrefix[] = L"button";
        constexpr wchar_t LowSuffix[] = L"Low";
        constexpr wchar_t HighSuffix[] = L"High";

        struct HatInfo
        {
            HidControlKind Kind;
            wchar_t const* Key;
        };

        constexpr HatInfo Hats[] =
        {
            { HidControlKind::HatUp, L"hatUp" },
            { HidControlKind::HatDown, L"hatDown" },
            { HidControlKind::HatLeft, L"hatLeft" },
            { HidControlKind::HatRight, L"hatRight" },
        };

        // Past half way from where it rests, the same as an Xbox stick direction.
        constexpr double LowDownAt = 0.25;
        constexpr double HighDownAt = 0.75;
        constexpr double PedalDownAt = 0.5;

        // Learn only takes a move that's clearly on purpose.
        constexpr double LearnRestingWithin = 0.2;
        constexpr double LearnCenterMovedBy = 0.35;
        constexpr double LearnPedalFrom = 0.25;
        constexpr double LearnLooseMovedBy = 0.25;

        AxisInfo const* FindAxis(_In_ uint16_t number) noexcept
        {
            return number < std::size(Axes) ? &Axes[number] : nullptr;
        }

        bool SameText(_In_ std::wstring_view a, _In_ std::wstring_view b) noexcept
        {
            return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(),
                [](wchar_t x, wchar_t y) { return std::towupper(x) == std::towupper(y); });
        }

        bool HatPointsAt(_In_ int8_t hat, _In_ HidControlKind kind) noexcept
        {
            switch (kind)
            {
            case HidControlKind::HatUp:     return hat == 7 || hat == 0 || hat == 1;
            case HidControlKind::HatRight:  return hat == 1 || hat == 2 || hat == 3;
            case HidControlKind::HatDown:   return hat == 3 || hat == 4 || hat == 5;
            case HidControlKind::HatLeft:   return hat == 5 || hat == 6 || hat == 7;
            default:                        return false;
            }
        }
    }

    _Use_decl_annotations_
    HidUsage HidAxisUsage(HidAxis axis) noexcept
    {
        auto const* info = FindAxis(static_cast<uint16_t>(axis));

        return info == nullptr ? HidUsage{} : info->Usage;
    }

    _Use_decl_annotations_
    std::optional<HidAxis> HidAxisFromUsage(uint16_t page, uint16_t usage, bool second) noexcept
    {
        for (auto const& info : Axes)
        {
            if (info.Usage.Page != page || info.Usage.Usage != usage)
            {
                continue;
            }

            if (info.Axis == HidAxis::Slider)
            {
                return second ? HidAxis::Slider2 : HidAxis::Slider;
            }

            return second ? std::nullopt : std::optional<HidAxis>{ info.Axis };
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    ControlRest HidAxisRest(HidAxis axis) noexcept
    {
        auto const* info = FindAxis(static_cast<uint16_t>(axis));

        return info == nullptr ? ControlRest::None : info->Rest;
    }

    _Use_decl_annotations_
    std::wstring HidControlKey(HidControl control)
    {
        switch (control.Kind)
        {
        case HidControlKind::Button:
            return control.Number >= 1 && control.Number <= HidMaximumButtons
                ? ButtonPrefix + std::to_wstring(control.Number)
                : std::wstring{};

        case HidControlKind::Axis:
        case HidControlKind::AxisLow:
        case HidControlKind::AxisHigh:
        {
            auto const* info = FindAxis(control.Number);

            if (info == nullptr)
            {
                return {};
            }

            return std::wstring{ info->Key } +
                (control.Kind == HidControlKind::AxisLow ? LowSuffix : control.Kind == HidControlKind::AxisHigh ? HighSuffix : L"");
        }

        default:
            for (auto const& hat : Hats)
            {
                if (hat.Kind == control.Kind)
                {
                    return hat.Key;
                }
            }

            return {};
        }
    }

    _Use_decl_annotations_
    std::optional<HidControl> HidControlFromKey(std::wstring_view key) noexcept
    {
        std::wstring_view const prefix{ ButtonPrefix };

        if (key.size() > prefix.size() && key.substr(0, prefix.size()) == prefix)
        {
            auto const digits = key.substr(prefix.size());

            // No leading zeros, so each button has one name.
            if (digits.size() > 3 || digits.front() == L'0' ||
                !std::all_of(digits.begin(), digits.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; }))
            {
                return std::nullopt;
            }

            uint32_t number{ 0 };

            for (auto const c : digits)
            {
                number = number * 10 + static_cast<uint32_t>(c - L'0');
            }

            if (number < 1 || number > HidMaximumButtons)
            {
                return std::nullopt;
            }

            return HidButtonControl(static_cast<uint16_t>(number));
        }

        for (auto const& hat : Hats)
        {
            if (key == hat.Key)
            {
                return HidControl{ hat.Kind, 0 };
            }
        }

        for (auto const& info : Axes)
        {
            std::wstring_view const name{ info.Key };
            auto const number = static_cast<uint16_t>(info.Axis);

            if (key == name)
            {
                return HidControl{ HidControlKind::Axis, number };
            }

            if (key.size() > name.size() && key.substr(0, name.size()) == name)
            {
                auto const rest = key.substr(name.size());

                if (rest == LowSuffix)
                {
                    return HidControl{ HidControlKind::AxisLow, number };
                }

                if (rest == HighSuffix)
                {
                    return HidControl{ HidControlKind::AxisHigh, number };
                }
            }
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    bool CanBePressed(HidControl control) noexcept
    {
        switch (control.Kind)
        {
        case HidControlKind::Button:
            return control.Number >= 1 && control.Number <= HidMaximumButtons;

        case HidControlKind::AxisLow:
        case HidControlKind::AxisHigh:
            return FindAxis(control.Number) != nullptr;

        case HidControlKind::HatUp:
        case HidControlKind::HatDown:
        case HidControlKind::HatLeft:
        case HidControlKind::HatRight:
            return true;

        default:
            return false;
        }
    }

    _Use_decl_annotations_
    bool CanSlide(HidControl control) noexcept
    {
        return control.Kind == HidControlKind::Axis && FindAxis(control.Number) != nullptr;
    }

    _Use_decl_annotations_
    std::optional<HidAxis> HidControlAxis(HidControl control) noexcept
    {
        if ((control.Kind == HidControlKind::Axis || control.Kind == HidControlKind::AxisLow ||
             control.Kind == HidControlKind::AxisHigh) && FindAxis(control.Number) != nullptr)
        {
            return static_cast<HidAxis>(control.Number);
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    ControlRest HidControlRest(HidControl control) noexcept
    {
        if (control.Kind == HidControlKind::Axis)
        {
            if (auto const axis = HidControlAxis(control))
            {
                return HidAxisRest(*axis);
            }
        }

        return ControlRest::Bottom;
    }

    _Use_decl_annotations_
    double HidControlValue(HidReading const& reading, HidControl control) noexcept
    {
        if (control.Kind == HidControlKind::Axis)
        {
            auto const axis = HidControlAxis(control);

            return reading.Connected && axis.has_value()
                ? reading.Axes[static_cast<size_t>(*axis)]
                : std::numeric_limits<double>::quiet_NaN();
        }

        return HidControlIsDown(reading, control) ? 1.0 : 0.0;
    }

    _Use_decl_annotations_
    bool HidControlIsDown(HidReading const& reading, HidControl control) noexcept
    {
        if (!reading.Connected)
        {
            return false;
        }

        switch (control.Kind)
        {
        case HidControlKind::Button:
            return control.Number >= 1 && control.Number <= HidMaximumButtons && reading.Buttons.test(control.Number - 1u);

        case HidControlKind::AxisLow:
        case HidControlKind::AxisHigh:
        {
            auto const axis = HidControlAxis(control);

            if (!axis.has_value())
            {
                return false;
            }

            auto const value = reading.Axes[static_cast<size_t>(*axis)];

            if (!std::isfinite(value))
            {
                return false;
            }

            if (control.Kind == HidControlKind::AxisLow)
            {
                return value <= LowDownAt;
            }

            return value >= (HidAxisRest(*axis) == ControlRest::Bottom ? PedalDownAt : HighDownAt);
        }

        case HidControlKind::HatUp:
        case HidControlKind::HatDown:
        case HidControlKind::HatLeft:
        case HidControlKind::HatRight:
            return HatPointsAt(reading.Hat, control.Kind);

        default:
            return false;
        }
    }

    _Use_decl_annotations_
    double NormalizeHidValue(uint32_t raw, int32_t logicalMinimum, int32_t logicalMaximum, uint16_t bitSize) noexcept
    {
        int64_t lowest = logicalMinimum;
        int64_t highest = logicalMaximum;

        // A descriptor that writes an unsigned maximum in too few bytes reads as negative.
        if (highest < lowest && lowest >= 0 && bitSize >= 1 && bitSize <= 32)
        {
            highest = (int64_t{ 1 } << bitSize) - 1;
        }

        if (highest <= lowest)
        {
            return std::numeric_limits<double>::quiet_NaN();
        }

        int64_t value = raw;

        if (bitSize >= 1 && bitSize < 32)
        {
            auto const mask = (uint32_t{ 1 } << bitSize) - 1;

            value = raw & mask;

            if (lowest < 0 && (raw & (uint32_t{ 1 } << (bitSize - 1))) != 0)
            {
                value -= int64_t{ 1 } << bitSize;
            }
        }
        else if (lowest < 0)
        {
            value = static_cast<int32_t>(raw);
        }

        value = std::clamp(value, lowest, highest);

        return static_cast<double>(value - lowest) / static_cast<double>(highest - lowest);
    }

    _Use_decl_annotations_
    int8_t HatPosition(uint32_t raw, int32_t logicalMinimum, int32_t logicalMaximum) noexcept
    {
        int64_t const value = raw;
        int64_t const lowest = logicalMinimum;
        int64_t const highest = logicalMaximum;

        if (highest <= lowest || value < lowest || value > highest)
        {
            return -1;
        }

        auto const positions = highest - lowest + 1;
        auto const index = value - lowest;

        return static_cast<int8_t>(((index * 8 + positions / 2) / positions) % 8);
    }

    _Use_decl_annotations_
    std::optional<HidControl> FindMovedHidControl(HidReading const& before, HidReading const& now, bool forButton) noexcept
    {
        if (!now.Connected)
        {
            return std::nullopt;
        }

        if (forButton)
        {
            for (uint16_t i = 0; i < HidMaximumButtons; i++)
            {
                if (now.Buttons.test(i) && !before.Buttons.test(i))
                {
                    return HidButtonControl(static_cast<uint16_t>(i + 1));
                }
            }

            for (auto const kind : { HidControlKind::HatUp, HidControlKind::HatRight, HidControlKind::HatDown, HidControlKind::HatLeft })
            {
                if (HatPointsAt(now.Hat, kind) && !HatPointsAt(before.Hat, kind))
                {
                    return HidControl{ kind, 0 };
                }
            }
        }

        for (auto const& info : Axes)
        {
            auto const index = static_cast<size_t>(info.Axis);
            auto const was = before.Axes[index];
            auto const is = now.Axes[index];

            if (!std::isfinite(was) || !std::isfinite(is))
            {
                continue;
            }

            auto const number = static_cast<uint16_t>(info.Axis);

            switch (info.Rest)
            {
            case ControlRest::Middle:
            {
                if (std::abs(was - 0.5) >= LearnRestingWithin)
                {
                    break;
                }

                if (!forButton && std::abs(is - 0.5) >= LearnCenterMovedBy)
                {
                    return HidAxisControl(info.Axis);
                }

                if (forButton && is <= LowDownAt)
                {
                    return HidControl{ HidControlKind::AxisLow, number };
                }

                if (forButton && is >= HighDownAt)
                {
                    return HidControl{ HidControlKind::AxisHigh, number };
                }

                break;
            }

            case ControlRest::Bottom:
                if (was < LearnPedalFrom && is >= PedalDownAt)
                {
                    return forButton ? HidControl{ HidControlKind::AxisHigh, number } : HidAxisControl(info.Axis);
                }
                break;

            default:
                if (std::abs(is - was) < LearnLooseMovedBy)
                {
                    break;
                }

                if (!forButton)
                {
                    return HidAxisControl(info.Axis);
                }

                if (is <= LowDownAt)
                {
                    return HidControl{ HidControlKind::AxisLow, number };
                }

                if (is >= HighDownAt)
                {
                    return HidControl{ HidControlKind::AxisHigh, number };
                }
                break;
            }
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    void SettleHidLearnBaseline(HidReading& before, HidReading const& now) noexcept
    {
        if (!now.Connected)
        {
            return;
        }

        // A button let go of can be learned the next time it's pressed.
        before.Buttons &= now.Buttons;

        if (now.Hat < 0)
        {
            before.Hat = now.Hat;
        }

        for (auto const& info : Axes)
        {
            auto const index = static_cast<size_t>(info.Axis);
            auto& was = before.Axes[index];
            auto const is = now.Axes[index];

            if (!std::isfinite(is))
            {
                continue;
            }

            auto const resting =
                (info.Rest == ControlRest::Middle && std::abs(is - 0.5) < LearnRestingWithin) ||
                (info.Rest == ControlRest::Bottom && is < LearnPedalFrom);

            if (!std::isfinite(was) || resting)
            {
                was = is;
            }
        }
    }

    bool HidDeviceId::IsEmpty() const noexcept
    {
        return Name.empty() && VendorId == 0 && ProductId == 0 && Path.empty();
    }

    _Use_decl_annotations_
    int32_t ChooseHidDevice(std::vector<HidDeviceId> const& present, HidDeviceId const& wanted) noexcept
    {
        if (wanted.IsEmpty())
        {
            return -1;
        }

        auto const count = static_cast<int32_t>(present.size());

        if (!wanted.Path.empty())
        {
            for (int32_t i = 0; i < count; i++)
            {
                if (SameText(present[static_cast<size_t>(i)].Path, wanted.Path))
                {
                    return i;
                }
            }
        }

        if (wanted.VendorId != 0 || wanted.ProductId != 0)
        {
            int32_t first{ -1 };

            for (int32_t i = 0; i < count; i++)
            {
                auto const& device = present[static_cast<size_t>(i)];

                if (device.VendorId != wanted.VendorId || device.ProductId != wanted.ProductId)
                {
                    continue;
                }

                if (wanted.Name.empty() || SameText(device.Name, wanted.Name))
                {
                    return i;
                }

                if (first < 0)
                {
                    first = i;
                }
            }

            if (first >= 0)
            {
                return first;
            }
        }

        if (!wanted.Name.empty())
        {
            for (int32_t i = 0; i < count; i++)
            {
                if (SameText(present[static_cast<size_t>(i)].Name, wanted.Name))
                {
                    return i;
                }
            }
        }

        return -1;
    }

    _Use_decl_annotations_
    bool IsXInputDevicePath(std::wstring_view path) noexcept
    {
        for (size_t i = 0; i + 3 <= path.size(); i++)
        {
            if (std::towupper(path[i]) == L'I' && std::towupper(path[i + 1]) == L'G' && path[i + 2] == L'_')
            {
                return true;
            }
        }

        return false;
    }
}
