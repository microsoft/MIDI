// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, WinRT and XAML, so the unit tests compile it unchanged.

#include "CalcMidi2.h"

#include <algorithm>
#include <cmath>

namespace midicalc
{
    namespace
    {
        bool BitsOk(_In_ uint8_t bits) noexcept
        {
            return bits >= 1 && bits <= 32;
        }

        uint32_t Mask(_In_ uint32_t value, _In_ uint8_t bits) noexcept
        {
            return BitsOk(bits) ? (value & MaximumForBits(bits)) : value;
        }

        // A note with a fraction as a fixed-point 7.N value, rounding to the nearest step and
        // never past the largest value.
        uint64_t FixedFromNote(_In_ double note, _In_ uint32_t fractionSteps) noexcept
        {
            auto const largest = 128.0 * fractionSteps - 1.0;

            if (!std::isfinite(note) || note <= 0.0)
            {
                return 0;
            }

            return static_cast<uint64_t>((std::min)(std::round(note * fractionSteps), largest));
        }
    }

    _Use_decl_annotations_
    uint32_t MaximumForBits(uint8_t bits) noexcept
    {
        if (bits >= 32)
        {
            return 0xFFFFFFFFu;
        }

        if (bits == 0)
        {
            return 0;
        }

        return (1u << bits) - 1u;
    }

    _Use_decl_annotations_
    uint32_t ScaleUpMinCenterMax(uint32_t value, uint8_t srcBits, uint8_t dstBits) noexcept
    {
        if (!BitsOk(srcBits) || !BitsOk(dstBits) || srcBits >= dstBits)
        {
            return Mask(value, dstBits);
        }

        value &= MaximumForBits(srcBits);

        // M2-115 3.3: a one-bit value goes to the ends
        if (srcBits == 1)
        {
            return value == 0 ? 0 : MaximumForBits(dstBits);
        }

        // M2-115 3.3.1, in 64 bits so a shift of 31 can't overflow
        uint8_t const scaleBits = dstBits - srcBits;
        uint64_t const srcCenter = 1ull << (srcBits - 1);
        uint64_t result = static_cast<uint64_t>(value) << scaleBits;

        if (value <= srcCenter)
        {
            return static_cast<uint32_t>(result);
        }

        uint8_t const repeatBits = srcBits - 1;
        uint64_t const repeatMask = (1ull << repeatBits) - 1;
        uint64_t repeatValue = value & repeatMask;

        if (scaleBits > repeatBits)
        {
            repeatValue <<= (scaleBits - repeatBits);
        }
        else
        {
            repeatValue >>= (repeatBits - scaleBits);
        }

        while (repeatValue != 0)
        {
            result |= repeatValue;
            repeatValue >>= repeatBits;
        }

        return static_cast<uint32_t>(result);
    }

    _Use_decl_annotations_
    uint32_t ScaleDownMinCenterMax(uint32_t value, uint8_t srcBits, uint8_t dstBits) noexcept
    {
        if (!BitsOk(srcBits) || !BitsOk(dstBits) || srcBits <= dstBits)
        {
            return Mask(value, dstBits);
        }

        // M2-115 3.4.1: cutting off the lower bits
        return static_cast<uint32_t>((static_cast<uint64_t>(value) & MaximumForBits(srcBits)) >> (srcBits - dstBits));
    }

    _Use_decl_annotations_
    uint32_t ScaleUpZeroExtension(uint32_t value, uint8_t srcBits, uint8_t dstBits) noexcept
    {
        if (!BitsOk(srcBits) || !BitsOk(dstBits) || srcBits >= dstBits)
        {
            return Mask(value, dstBits);
        }

        // M2-115 4.3.1
        return static_cast<uint32_t>((static_cast<uint64_t>(value) & MaximumForBits(srcBits)) << (dstBits - srcBits));
    }

    _Use_decl_annotations_
    uint32_t ScaleDownZeroExtension(uint32_t value, uint8_t srcBits, uint8_t dstBits) noexcept
    {
        if (!BitsOk(srcBits) || !BitsOk(dstBits) || srcBits <= dstBits)
        {
            return Mask(value, dstBits);
        }

        // M2-115 4.4.1, in 64 bits so adding half the step to a 32-bit value can't wrap
        uint8_t const scaleBits = srcBits - dstBits;
        uint64_t const half = 1ull << (scaleBits - 1);
        uint64_t const shifted = ((static_cast<uint64_t>(value) & MaximumForBits(srcBits)) + half) >> scaleBits;

        return static_cast<uint32_t>((std::min)(shifted, static_cast<uint64_t>(MaximumForBits(dstBits))));
    }

    _Use_decl_annotations_
    uint16_t Pitch79FromNote(double note) noexcept
    {
        return static_cast<uint16_t>(FixedFromNote(note, Pitch79FractionSteps));
    }

    _Use_decl_annotations_
    double NoteFromPitch79(uint16_t value) noexcept
    {
        return static_cast<double>(value) / Pitch79FractionSteps;
    }

    _Use_decl_annotations_
    uint32_t Pitch725FromNote(double note) noexcept
    {
        return static_cast<uint32_t>(FixedFromNote(note, Pitch725FractionSteps));
    }

    _Use_decl_annotations_
    double NoteFromPitch725(uint32_t value) noexcept
    {
        return static_cast<double>(value) / Pitch725FractionSteps;
    }

    _Use_decl_annotations_
    uint32_t PitchBend32FromCents(double cents, double rangeCents) noexcept
    {
        if (!(rangeCents > 0.0) || !std::isfinite(cents))
        {
            return PitchBend32Center;
        }

        auto const part = std::clamp(cents / rangeCents, -1.0, 1.0);

        if (part >= 0.0)
        {
            return PitchBend32Center + static_cast<uint32_t>(std::llround(part * 0x7FFFFFFF));
        }

        return PitchBend32Center - static_cast<uint32_t>(std::llround(-part * 0x80000000));
    }

    _Use_decl_annotations_
    uint16_t PitchBend14FromCents(double cents, double rangeCents) noexcept
    {
        if (!(rangeCents > 0.0) || !std::isfinite(cents))
        {
            return PitchBend14Center;
        }

        auto const part = std::clamp(cents / rangeCents, -1.0, 1.0);

        if (part >= 0.0)
        {
            return static_cast<uint16_t>(PitchBend14Center + std::lround(part * 0x1FFF));
        }

        return static_cast<uint16_t>(PitchBend14Center - std::lround(-part * 0x2000));
    }

    _Use_decl_annotations_
    double CentsFromPitchBend32(uint32_t value, double rangeCents) noexcept
    {
        if (value >= PitchBend32Center)
        {
            return static_cast<double>(value - PitchBend32Center) / 0x7FFFFFFF * rangeCents;
        }

        return -static_cast<double>(PitchBend32Center - value) / 0x80000000 * rangeCents;
    }

    _Use_decl_annotations_
    double CentsFromPitchBend14(uint16_t value, double rangeCents) noexcept
    {
        value &= 0x3FFF;

        if (value >= PitchBend14Center)
        {
            return static_cast<double>(value - PitchBend14Center) / 0x1FFF * rangeCents;
        }

        return -static_cast<double>(PitchBend14Center - value) / 0x2000 * rangeCents;
    }

    _Use_decl_annotations_
    uint32_t PitchBendRangeRpnData(uint8_t semitones, uint8_t cents) noexcept
    {
        return (static_cast<uint32_t>(semitones & 0x7F) << 25) | (static_cast<uint32_t>(cents & 0x7F) << 18);
    }

    _Use_decl_annotations_
    double JrTicksToMilliseconds(uint32_t ticks) noexcept
    {
        return static_cast<double>(ticks) * 1000.0 / JrTicksPerSecond;
    }

    _Use_decl_annotations_
    std::optional<double> DeltaClockstampMilliseconds(uint32_t ticks, uint32_t ticksPerQuarterNote, double beatsPerMinute) noexcept
    {
        if (ticksPerQuarterNote == 0 || !(beatsPerMinute > 0.0))
        {
            return std::nullopt;
        }

        return static_cast<double>(ticks) / ticksPerQuarterNote * 60000.0 / beatsPerMinute;
    }

    _Use_decl_annotations_
    uint16_t ManufacturerIdFromMidi1(Midi1ManufacturerId const& id) noexcept
    {
        // M2-104-UM, the example conversion code beside Table 21
        if (id.IsThreeBytes || id.Byte1 == 0x00)
        {
            return static_cast<uint16_t>(0x8000 | ((id.Byte2 & 0x7F) << 8) | (id.Byte3 & 0x7F));
        }

        return static_cast<uint16_t>(id.Byte1 & 0x7F);
    }

    _Use_decl_annotations_
    Midi1ManufacturerId Midi1FromManufacturerId(uint16_t id) noexcept
    {
        Midi1ManufacturerId result{};

        if ((id & 0x8000) == 0)
        {
            result.Byte1 = static_cast<uint8_t>(id & 0x7F);
            return result;
        }

        result.IsThreeBytes = true;
        result.Byte2 = static_cast<uint8_t>((id & 0x7F00) >> 8);
        result.Byte3 = static_cast<uint8_t>(id & 0x7F);
        return result;
    }
}
