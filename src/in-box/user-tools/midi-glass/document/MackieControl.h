// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h, XAML and the MIDI SDK.
//
// Mackie Control: the named functions a DAW control surface has, and the MIDI each one is.
// Everything is on channel 1 except the faders, which use pitch bend on channels 1 to 9.

#include "LayoutModel.h"

#include <string>
#include <string_view>
#include <vector>

namespace glass
{
    // A row's Number when its kind is MackieControl. A button is its own note number, 0 to 103,
    // so the whole button map is one range; faders, V-Pots and the jog wheel sit above it.
    constexpr uint32_t MackieFaderBase = 0x100;
    constexpr uint32_t MackieVPotBase = 0x200;
    constexpr uint32_t MackieJog = 0x300;
    constexpr uint32_t MackieNoFunction = 0xFFFF;

    constexpr uint32_t MackieStripCount = 8;

    // The master fader is the ninth, on channel 9.
    constexpr uint32_t MackieMasterStrip = 8;

    constexpr uint32_t MackieLastButtonNote = 103;
    constexpr uint32_t MackieFaderTouchNote = 104;
    constexpr uint32_t MackieVPotController = 16;
    constexpr uint32_t MackieJogController = 60;

    enum class MackieShape
    {
        None = 0,

        // A note: velocity 127 when pressed and 0 when let go. The DAW lights it the same way.
        Button = 1,

        // Pitch bend for the position, and a touch note while a finger is on it.
        Fader = 2,

        // A controller that says how far it turned and which way, not where it is.
        Encoder = 3,
    };

    MackieShape ShapeOfMackieFunction(_In_ uint32_t function) noexcept;

    // 0 to 7 for a strip, 8 for the master fader. Only meaningful for a fader or a V-Pot.
    uint32_t MackieStripOf(_In_ uint32_t function) noexcept;

    // The name the file uses: "play", "mute3", "fader8", "masterFader", "vpot2", "jog".
    std::wstring MackieFunctionFileName(_In_ uint32_t function);

    // MackieNoFunction for a name this build does not know.
    uint32_t MackieFunctionFromFileName(_In_ std::wstring_view name) noexcept;

    // What to call a function: a resource key, and the number to format into it when it has one.
    struct MackieFunctionText
    {
        wchar_t const* ResourceKey{ nullptr };

        // 1 to 8 for a strip or a function key, 0 when the name takes no number.
        uint32_t Number{ 0 };
    };

    MackieFunctionText DescribeMackieFunction(_In_ uint32_t function) noexcept;

    // One line of the list a control picks its function from. A heading has no function.
    struct MackiePickerEntry
    {
        uint32_t Function{ MackieNoFunction };
        wchar_t const* HeadingKey{ nullptr };
    };

    // What a control of this kind can do on a Mackie Control surface: buttons press, faders
    // slide, and knobs, platters and wheels turn. Empty for everything else.
    std::vector<MackiePickerEntry> MackiePickerFor(_In_ ControlKind kind);

    bool MackieFunctionFits(_In_ uint32_t function, _In_ ControlKind kind) noexcept;

    // A Mackie Control row for this function, to this device.
    ControlMessage MakeMackieRow(
        _In_ uint32_t function,
        _In_ std::wstring const& deviceName,
        _In_ int32_t groupIndex);

    // The function a plain MIDI row already is, such as note 94 on channel 1 for Play, or
    // MackieNoFunction.
    uint32_t MackieFunctionOf(_In_ ControlMessage const& message) noexcept;

    // The plain MIDI row a function is, for a device that stops speaking Mackie Control. One
    // row that changes with the control, which is a press and a release for a button.
    ControlMessage PlainRowFor(_In_ ControlMessage const& mackieRow);

    // A new control headed for a Mackie Control device gets one row waiting for a function, in
    // place of the plain rows it was made with, which the device would ignore.
    void WaitForMackieFunction(_Inout_ Control& control, _In_ LayoutDocument const& document);

    // A relative turn the way Mackie Control sends it: 1 to 63 clockwise, 65 to 127 the other
    // way, with how far in the low six bits.
    uint8_t MackieTurnValue(_In_ int32_t ticks) noexcept;

    // How many ticks a turn across the whole of a control's travel is worth.
    constexpr double MackieTicksPerTravel = 64.0;

    // What an LED is told to do by the velocity of a note: 127 is on, odd is blinking, and even
    // is off.
    enum class MackieLight
    {
        Off = 0,
        On = 1,
        Blinking = 2,
    };

    MackieLight MackieLightFromVelocity(_In_ uint32_t velocity) noexcept;
}
