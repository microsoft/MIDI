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

    // Two values rather than one. Worth asking about separately from "does a touch set it
    // outright", because a joystick answers yes to both and a ribbon only to the second.
    bool UsesTwoAxes(_In_ ControlKind kind) noexcept;

    // Keys, where a touch means a note rather than a position.
    bool PlaysKeys(_In_ ControlKind kind) noexcept;

    // Where a point inside a control puts a continuous value.
    double PositionToValue(
        _In_ ControlKind kind,
        _In_ double width,
        _In_ double height,
        _In_ double x,
        _In_ double y) noexcept;

    // The other axis, for the controls that have one. Bottom is zero: the screen counts the
    // other way and every joystick, pad and plug-in says up is more.
    double PositionToValueY(
        _In_ double height,
        _In_ double y) noexcept;

    // Which key a point lands on, counted from the leftmost, or -1 for none. White and black
    // keys overlap, so the black ones are tested first: a finger in the top part of a white
    // key where a black one sits is on the black one, the way a real keyboard behaves.
    int32_t KeyAtPosition(
        _In_ KeyboardSpec const& keyboard,
        _In_ double width,
        _In_ double height,
        _In_ double x,
        _In_ double y) noexcept;

    // How hard that key was hit, when the control takes velocity from where it was touched.
    // Near the back of a key is quiet and near the front is loud, which is the way a hammer
    // action behaves and the only cue a flat screen can offer.
    double KeyVelocityFromPosition(
        _In_ KeyboardSpec const& keyboard,
        _In_ int32_t key,
        _In_ double height,
        _In_ double y) noexcept;
}
