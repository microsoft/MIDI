// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML. Which way a finger moves a control is arithmetic, and a
// fader that reads upside down is the kind of defect nobody finds until it is on stage.

#include <sal.h>

#include "LayoutModel.h"

namespace glass
{
    // Whether a finger on this kind of control sets a position outright or nudges it. A fader
    // jumps to where it was touched; a knob does not, because a knob has no travel under the
    // finger and jumping would make every touch a wild move.
    bool UsesAbsolutePosition(_In_ ControlKind kind) noexcept;

    bool IsMomentary(_In_ ControlKind kind) noexcept;
    bool IsToggling(_In_ ControlKind kind) noexcept;
    bool IsInteractive(_In_ ControlKind kind) noexcept;

    // How far a finger has to travel to take a knob from one end to the other. Chosen so a knob
    // can be set precisely without running out of screen, which is what every plug-in does.
    constexpr double KnobDragPixels = 200.0;

    // Where a point inside a control puts a continuous value.
    double PositionToValue(
        _In_ ControlKind kind,
        _In_ double width,
        _In_ double height,
        _In_ double x,
        _In_ double y) noexcept;
}
