// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "InputRules.h"

#include <algorithm>

namespace glass
{
    _Use_decl_annotations_
    bool UsesAbsolutePosition(ControlKind kind) noexcept
    {
        switch (kind)
        {
        case ControlKind::Fader:
        case ControlKind::XYPad:
        case ControlKind::Joystick:
        case ControlKind::Ribbon:
            return true;

        default:
            return false;
        }
    }

    _Use_decl_annotations_
    bool UsesTwoAxes(ControlKind kind) noexcept
    {
        return kind == ControlKind::XYPad || kind == ControlKind::Joystick;
    }

    _Use_decl_annotations_
    bool PlaysKeys(ControlKind kind) noexcept
    {
        return kind == ControlKind::PianoKeyboard;
    }

    _Use_decl_annotations_
    bool IsMomentary(ControlKind kind) noexcept
    {
        switch (kind)
        {
        case ControlKind::Pad:
        case ControlKind::Button:
        case ControlKind::PageTab:
            return true;

        default:
            return false;
        }
    }

    _Use_decl_annotations_
    bool IsToggling(ControlKind kind) noexcept
    {
        // A clock is running or it is not, and a press is what changes which. That is a toggle,
        // whatever else the control is doing while it runs.
        return kind == ControlKind::Toggle || kind == ControlKind::BeatClock;
    }

    _Use_decl_annotations_
    bool IsInteractive(ControlKind kind) noexcept
    {
        switch (kind)
        {
        case ControlKind::Meter:
        case ControlKind::Lamp:
        case ControlKind::Readout:
        case ControlKind::Label:
        case ControlKind::Image:
        case ControlKind::Panel:
            return false;

        default:
            return true;
        }
    }
    _Use_decl_annotations_
    double PositionToValue(
        ControlKind kind,
        double width,
        double height,
        double x,
        double y) noexcept
    {
        if (width <= 0.0 || height <= 0.0)
        {
            return 0.0;
        }

        if (kind == ControlKind::XYPad || kind == ControlKind::Joystick)
        {
            return std::clamp(x / width, 0.0, 1.0);
        }

        // A ribbon reads the long way, like a fader, but the light follows the finger rather
        // than a cap riding a slot.
        if (kind == ControlKind::Ribbon)
        {
            return height > width
                ? std::clamp(1.0 - (y / height), 0.0, 1.0)
                : std::clamp(x / width, 0.0, 1.0);
        }

        // A fader laid out wider than it is tall is a horizontal fader, so the axis follows the
        // rectangle rather than the name of the control.
        if (height >= width)
        {
            // Screen coordinates run downward and a fader does not, which is the one place this
            // is easy to get backwards.
            return std::clamp(1.0 - (y / height), 0.0, 1.0);
        }

        return std::clamp(x / width, 0.0, 1.0);
    }

    _Use_decl_annotations_
    double PositionToValueY(double height, double y) noexcept
    {
        if (height <= 0.0)
        {
            return 0.0;
        }

        return std::clamp(1.0 - (y / height), 0.0, 1.0);
    }

    namespace
    {
        constexpr bool BlackInOctave[12]
        {
            false, true, false, true, false, false, true, false, true, false, true, false
        };

        bool IsBlack(_In_ int32_t note) noexcept
        {
            return BlackInOctave[((note % 12) + 12) % 12];
        }

        int32_t WhitesBelow(_In_ int32_t lowest, _In_ int32_t note) noexcept
        {
            int32_t count{ 0 };

            for (auto walk = lowest; walk < note; ++walk)
            {
                if (!IsBlack(walk))
                {
                    count++;
                }
            }

            return count;
        }

        // Same proportions the surface draws with. Kept in step by hand rather than shared,
        // because this file is deliberately free of everything the renderer needs.
        constexpr double BlackKeyLength = 0.62;
        constexpr double BlackKeyWidth = 0.60;
    }

    _Use_decl_annotations_
    int32_t KeyAtPosition(
        KeyboardSpec const& keyboard,
        double width,
        double height,
        double x,
        double y) noexcept
    {
        if (width <= 0.0 || height <= 0.0 || x < 0.0 || y < 0.0 || x > width || y > height)
        {
            return -1;
        }

        auto const keys = std::clamp(keyboard.KeyCount, MinimumKeyboardKeys, MaximumKeyboardKeys);
        auto const lowest = std::clamp(keyboard.LowestNote, 0, 127);

        auto const whiteCount = WhitesBelow(lowest, lowest + keys);

        if (whiteCount <= 0)
        {
            return -1;
        }

        auto const whiteWidth = width / static_cast<double>(whiteCount);
        auto const blackWidth = whiteWidth * BlackKeyWidth;

        if (y <= height * BlackKeyLength)
        {
            for (int32_t index = 0; index < keys; ++index)
            {
                auto const note = lowest + index;

                if (!IsBlack(note))
                {
                    continue;
                }

                auto const left = WhitesBelow(lowest, note) * whiteWidth - blackWidth * 0.5;

                if (x >= left && x < left + blackWidth)
                {
                    return index;
                }
            }
        }

        auto const white = static_cast<int32_t>(x / whiteWidth);

        int32_t seen{ 0 };

        for (int32_t index = 0; index < keys; ++index)
        {
            if (IsBlack(lowest + index))
            {
                continue;
            }

            if (seen == white)
            {
                return index;
            }

            seen++;
        }

        return -1;
    }

    _Use_decl_annotations_
    double KeyVelocityFromPosition(
        KeyboardSpec const& keyboard,
        int32_t key,
        double height,
        double y) noexcept
    {
        if (height <= 0.0 || key < 0)
        {
            return 1.0;
        }

        auto const lowest = std::clamp(keyboard.LowestNote, 0, 127);

        // A black key is shorter, so the same touch has to mean the same thing on both. The
        // travel is measured against the key that was actually hit.
        auto const travel = IsBlack(lowest + key) ? height * BlackKeyLength : height;

        if (travel <= 0.0)
        {
            return 1.0;
        }

        // A floor rather than zero: a key that plays nothing reads as a control that missed.
        return std::clamp(0.2 + 0.8 * (y / travel), 0.0, 1.0);
    }
}
