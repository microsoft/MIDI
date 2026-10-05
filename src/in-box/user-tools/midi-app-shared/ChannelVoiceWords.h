// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Channel voice messages built from a value, word by word. Deliberately free of pch.h, WinRT and
// XAML, so the unit tests compile it unchanged. MIDI Glass sends every control through these, and
// MIDI Patchbay's LFO step does too, so a sweep lands on the wire the same way in both.

#include <sal.h>
#include <cstdint>
#include <optional>
#include <string_view>

namespace midiapp
{
    // A fraction of full scale to an unsigned value of the given width. 0.0 is the bottom of the
    // range and 1.0 is the top, so a fader at the top of its travel sends the maximum the wire can
    // carry rather than one short of it.
    uint32_t ScaleToBits(_In_ double fraction, _In_ uint32_t bits) noexcept;

    // An exact value into the same field, clamped rather than scaled. This is what a customer
    // copying a device's documentation gets.
    uint32_t ClampToBits(_In_ double value, _In_ uint32_t bits) noexcept;

    // MIDI 1.0 channel voice, message type 2. One word.
    uint32_t BuildMidi1ChannelVoice(
        _In_ uint8_t group,
        _In_ uint8_t status,
        _In_ uint8_t channel,
        _In_ uint8_t data1,
        _In_ uint8_t data2) noexcept;

    // MIDI 2.0 channel voice, message type 4. Two words.
    void BuildMidi2ChannelVoice(
        _In_ uint8_t group,
        _In_ uint8_t status,
        _In_ uint8_t channel,
        _In_ uint8_t index1,
        _In_ uint8_t index2,
        _In_ uint32_t data,
        _Out_writes_(2) uint32_t* words) noexcept;

    // What a value that moves on its own, such as an LFO, is sent as.
    enum class ValueMessageKind : int32_t
    {
        ControlChange = 0,
        PitchBend = 1,
        ChannelPressure = 2,

        // Pressure on one note. Number is the note.
        PolyPressure = 3,

        // RPN. Number is the bank times 128 plus the index.
        RegisteredController = 4,

        // NRPN. Number is the bank times 128 plus the index.
        AssignableController = 5,
    };

    constexpr ValueMessageKind AllValueMessageKinds[]
    {
        ValueMessageKind::ControlChange,
        ValueMessageKind::PitchBend,
        ValueMessageKind::ChannelPressure,
        ValueMessageKind::PolyPressure,
        ValueMessageKind::RegisteredController,
        ValueMessageKind::AssignableController,
    };

    // RPN and NRPN numbers: a bank and an index, 0 to 127 each, held as bank * 128 + index.
    constexpr uint32_t MaximumValueMessageNumber = 16383;

    struct ValueMessageTarget
    {
        ValueMessageKind Kind{ ValueMessageKind::ControlChange };

        // Both counted from 0.
        uint8_t Group{ 0 };
        uint8_t Channel{ 0 };

        // The controller for a control change, the note for poly pressure, and bank * 128 + index
        // for an RPN or NRPN. Ignored for pitch bend and channel pressure.
        uint32_t Number{ 1 };

        // MIDI 1.0 protocol words, with 7 bit values and a 14 bit pitch bend, rather than the
        // 32 bit values of MIDI 2.0. An RPN or an NRPN always goes as MIDI 2.0: in MIDI 1.0 it is
        // four control changes, and Windows makes those for a MIDI 1.0 device.
        bool Midi1Protocol{ false };
    };

    // Whether the kind uses Number, and what its largest value is: 127, or 16383 for an RPN or an
    // NRPN. Zero for a kind with no number.
    uint32_t ValueMessageNumberMaximum(_In_ ValueMessageKind kind) noexcept;

    // The words for a value from 0 (the bottom of the field) to 1 (the top). Returns how many
    // were written, 1 or 2.
    uint32_t BuildValueMessage(
        _In_ ValueMessageTarget const& target,
        _In_ double fraction,
        _Out_writes_(2) uint32_t* words) noexcept;

    // The names files use: "controlChange", "pitchBend", "channelPressure", "polyPressure",
    // "rpn" and "nrpn", the same as the MIDI Glass assistant tools.
    std::wstring_view ValueMessageKindKey(_In_ ValueMessageKind kind) noexcept;
    std::optional<ValueMessageKind> ValueMessageKindFromKey(_In_ std::wstring_view key) noexcept;
}
