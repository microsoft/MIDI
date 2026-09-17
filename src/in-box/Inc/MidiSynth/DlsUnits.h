// Unit conversions for the DLS Level 1 device architecture, per the MMA specification.
// These are the conversions the whole engine depends on being right, so each one names the
// formula it implements.

#pragma once

#include <cmath>
#include <cstdint>

namespace MidiSynth
{
    // The specification reserves 0x80000000 to mean absolute zero time / infinite attenuation.
    // Applied to a destination, that destination can no longer be modified by any source.
    constexpr int32_t DlsAbsoluteZero = INT32_MIN;

    constexpr double DlsSilenceDb = -96.0;

    // TimeCents = log2(seconds) * 1200 * 65536
    inline double TimeCentsToSeconds(_In_ int32_t timeCents) noexcept
    {
        if (timeCents == DlsAbsoluteZero)
        {
            return 0.0;
        }

        return std::pow(2.0, static_cast<double>(timeCents) / (1200.0 * 65536.0));
    }

    // Pitch is 1/65536 of a cent.
    inline double PitchUnitsToCents(_In_ int32_t pitchUnits) noexcept
    {
        return static_cast<double>(pitchUnits) / 65536.0;
    }

    // Absolute pitch: Pitch = (1200 * log2(f / 440) + 6900) * 65536
    inline double AbsolutePitchUnitsToHertz(_In_ int32_t pitchUnits) noexcept
    {
        const double cents = static_cast<double>(pitchUnits) / 65536.0;
        return 440.0 * std::pow(2.0, (cents - 6900.0) / 1200.0);
    }

    // Attenuation is 1/655360 dB per unit: attenuation_dB = 200 * 65536 * log10(V / v).
    // Values in gm.dls are negative, so this is a gain in dB and must not be negated again.
    inline double RelativeGainToDb(_In_ int32_t gainUnits) noexcept
    {
        if (gainUnits == DlsAbsoluteZero)
        {
            return DlsSilenceDb;
        }

        return static_cast<double>(gainUnits) / 655360.0;
    }

    // Sustain level and pan both use 0.1 percent units, so 1000 is 100 percent.
    inline double PercentUnitsToFraction(_In_ int32_t percentUnits) noexcept
    {
        return static_cast<double>(percentUnits) / 1000.0;
    }

    inline double DecibelsToLinear(_In_ double decibels) noexcept
    {
        if (decibels <= DlsSilenceDb)
        {
            return 0.0;
        }

        return std::pow(10.0, decibels / 20.0);
    }

    // Concave transform: atten_dB = 20 * log10((value / 127)^2), which is 40 * log10(value / 127).
    // Returns a gain in dB, zero at full scale and falling to silence at zero.
    inline double ConcaveTransformDb(_In_ double normalizedValue) noexcept
    {
        if (normalizedValue <= 0.0)
        {
            return DlsSilenceDb;
        }

        if (normalizedValue >= 1.0)
        {
            return 0.0;
        }

        const double decibels = 40.0 * std::log10(normalizedValue);
        return decibels < DlsSilenceDb ? DlsSilenceDb : decibels;
    }

    inline double CentsToPitchRatio(_In_ double cents) noexcept
    {
        return std::pow(2.0, cents / 1200.0);
    }
}
