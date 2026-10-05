// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "Sf2Types.h"

#include <array>

namespace SoundFontSynth
{
    // What a modulator can read for one voice. Every value is already normalized to 0..1, with a
    // centered controller at exactly 0.5 so a bipolar source reads zero there.
    struct ModulationInputs
    {
        double const* Controllers{ nullptr };   // 128 entries
        double Velocity{ 0.0 };
        double Key{ 0.0 };
        double PolyPressure{ 0.0 };
        double ChannelPressure{ 0.0 };
        double PitchWheel{ 0.5 };
        double PitchWheelSensitivity{ 0.0 };
    };

    // 7 bit, 14 bit and 32 bit controller values all map 0 to 0.0, the center to exactly 0.5 and
    // the maximum to 1.0, so the same modulator gives the same answer for a MIDI 1.0 and a MIDI 2.0
    // controller.
    double NormalizeCentered7(_In_ uint8_t value) noexcept;
    double NormalizeCentered14(_In_ uint16_t value) noexcept;
    double NormalizeCentered32(_In_ uint32_t value) noexcept;

    double EvaluateSource(_In_ uint16_t source, _In_ ModulationInputs const& inputs) noexcept;

    // The modulator's contribution to its destination, in the destination generator's units.
    double EvaluateModulator(_In_ Sf2Modulator const& modulator, _In_ ModulationInputs const& inputs) noexcept;

    // SoundFont 2.04 section 8.4. Every voice starts with these, and a zone can replace one by
    // declaring a modulator with the same identity.
    constexpr size_t DefaultModulatorCount = 10;
    std::array<Sf2Modulator, DefaultModulatorCount> const& DefaultModulators() noexcept;
}
