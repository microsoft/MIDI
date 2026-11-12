// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// MIDI 2.0 values: note pitch, pitch bend, value scaling, UMP timing and manufacturer IDs.
// Plain C++ with no WinRT or XAML, so the unit tests compile it unchanged.
//
// Sources, all from the MIDI Association:
//  - M2-104-UM v1.1.2, UMP Format and MIDI 2.0 Protocol: Pitch 7.9 (Note On attribute type 3),
//    Pitch 7.25 (Registered Per-Note Controller #3), pitch bend centered at 0x80000000, RPN 0
//    in MIDI 2.0 (semitones in the top 7 bits, cents in the next 7), JR Timestamps in 1/31250 s,
//    Delta Clockstamps, and the manufacturer ID conversion code.
//  - M2-115-U v1.0.2, MIDI 2.0 Bit Scaling and Resolution: min-center-max scaling (section 3)
//    and zero-extension scaling with rounding (section 4).

#include <sal.h>

#include <cstdint>
#include <optional>

namespace midicalc
{
    // ---- value scaling (M2-115) ----

    // Bits run 1 to 32 and src must be narrower than dst for up, wider for down. Anything else
    // gives the value back masked to dstBits.
    uint32_t ScaleUpMinCenterMax(_In_ uint32_t value, _In_ uint8_t srcBits, _In_ uint8_t dstBits) noexcept;
    uint32_t ScaleDownMinCenterMax(_In_ uint32_t value, _In_ uint8_t srcBits, _In_ uint8_t dstBits) noexcept;
    uint32_t ScaleUpZeroExtension(_In_ uint32_t value, _In_ uint8_t srcBits, _In_ uint8_t dstBits) noexcept;
    uint32_t ScaleDownZeroExtension(_In_ uint32_t value, _In_ uint8_t srcBits, _In_ uint8_t dstBits) noexcept;

    // The largest value bits can hold.
    uint32_t MaximumForBits(_In_ uint8_t bits) noexcept;

    // ---- note pitch ----

    inline constexpr uint32_t Pitch79FractionSteps = 1u << 9;
    inline constexpr uint32_t Pitch725FractionSteps = 1u << 25;

    // A note with a fraction (69.389 is A3 plus 38.9 cents), 0 up to just under 128. Values
    // outside that are clamped.
    uint16_t Pitch79FromNote(_In_ double note) noexcept;
    double NoteFromPitch79(_In_ uint16_t value) noexcept;

    uint32_t Pitch725FromNote(_In_ double note) noexcept;
    double NoteFromPitch725(_In_ uint32_t value) noexcept;

    // ---- pitch bend ----

    inline constexpr uint32_t PitchBend32Center = 0x80000000u;
    inline constexpr uint16_t PitchBend14Center = 0x2000;

    // A bend of cents with the receiver's range set to rangeCents either way. Clamped to the
    // range. Up uses the steps above the center, down the steps below it, so both ends are
    // reachable.
    uint32_t PitchBend32FromCents(_In_ double cents, _In_ double rangeCents) noexcept;
    uint16_t PitchBend14FromCents(_In_ double cents, _In_ double rangeCents) noexcept;
    double CentsFromPitchBend32(_In_ uint32_t value, _In_ double rangeCents) noexcept;
    double CentsFromPitchBend14(_In_ uint16_t value, _In_ double rangeCents) noexcept;

    // The data of a MIDI 2.0 RPN 0 (pitch bend range) message. Semitones and cents are 0 to 127.
    uint32_t PitchBendRangeRpnData(_In_ uint8_t semitones, _In_ uint8_t cents) noexcept;

    // ---- UMP timing ----

    inline constexpr double JrTicksPerSecond = 31250.0;

    double JrTicksToMilliseconds(_In_ uint32_t ticks) noexcept;

    // How long ticks of a Delta Clockstamp last, with ticksPerQuarterNote from the DCTPQ
    // message. Empty when either rate is zero.
    std::optional<double> DeltaClockstampMilliseconds(
        _In_ uint32_t ticks,
        _In_ uint32_t ticksPerQuarterNote,
        _In_ double beatsPerMinute) noexcept;

    // ---- manufacturer IDs ----

    struct Midi1ManufacturerId
    {
        // a one-byte ID is in Byte1 with the others zero; a three-byte ID starts with 0x00
        uint8_t Byte1{ 0 };
        uint8_t Byte2{ 0 };
        uint8_t Byte3{ 0 };
        bool IsThreeBytes{ false };
    };

    uint16_t ManufacturerIdFromMidi1(_In_ Midi1ManufacturerId const& id) noexcept;
    Midi1ManufacturerId Midi1FromManufacturerId(_In_ uint16_t id) noexcept;
}
