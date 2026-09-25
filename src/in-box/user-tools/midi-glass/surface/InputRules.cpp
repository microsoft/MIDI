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
            return true;

        default:
            return false;
        }
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
        return kind == ControlKind::Toggle;
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

        if (kind == ControlKind::XYPad)
        {
            return std::clamp(x / width, 0.0, 1.0);
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
}
