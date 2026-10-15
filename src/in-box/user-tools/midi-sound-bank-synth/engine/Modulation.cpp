// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "Modulation.h"

namespace SoundFontSynth
{
    namespace
    {
        constexpr uint16_t SourceIndexMask = 0x007F;
        constexpr uint16_t SourceIsController = 0x0080;
        constexpr uint16_t SourceIsNegative = 0x0100;
        constexpr uint16_t SourceIsBipolar = 0x0200;

        constexpr uint16_t CurveLinear = 0;
        constexpr uint16_t CurveConcave = 1;
        constexpr uint16_t CurveConvex = 2;

        constexpr uint16_t SourceNoController = 0;
        constexpr uint16_t SourceVelocity = 2;
        constexpr uint16_t SourceKey = 3;
        constexpr uint16_t SourcePolyPressure = 10;
        constexpr uint16_t SourceChannelPressure = 13;
        constexpr uint16_t SourcePitchWheel = 14;
        constexpr uint16_t SourcePitchWheelSensitivity = 16;

        constexpr uint16_t TransformAbsolute = 2;

        // The specification's concave curve is 40 log10 in amplitude terms, the same law CC7 uses,
        // scaled so that 960 centibels covers the whole range.
        double Concave(_In_ double x) noexcept
        {
            if (x <= 0.0)
            {
                return 0.0;
            }

            if (x >= 1.0)
            {
                return 1.0;
            }

            return (std::clamp)(-(40.0 / 96.0) * std::log10(1.0 - x), 0.0, 1.0);
        }

        double Convex(_In_ double x) noexcept
        {
            return 1.0 - Concave(1.0 - x);
        }

        double ApplyCurve(_In_ uint16_t curve, _In_ double x) noexcept
        {
            switch (curve)
            {
            case CurveLinear: return x;
            case CurveConcave: return Concave(x);
            case CurveConvex: return Convex(x);
            default: return (x >= 0.5) ? 1.0 : 0.0;
            }
        }

        double RawSourceValue(_In_ uint16_t source, _In_ ModulationInputs const& inputs) noexcept
        {
            auto const index = static_cast<uint16_t>(source & SourceIndexMask);

            if ((source & SourceIsController) != 0)
            {
                return (inputs.Controllers != nullptr) ? inputs.Controllers[index] : 0.0;
            }

            switch (index)
            {
            case SourceVelocity: return inputs.Velocity;
            case SourceKey: return inputs.Key;
            case SourcePolyPressure: return inputs.PolyPressure;
            case SourceChannelPressure: return inputs.ChannelPressure;
            case SourcePitchWheel: return inputs.PitchWheel;
            case SourcePitchWheelSensitivity: return inputs.PitchWheelSensitivity;
            default: return 0.0;
            }
        }

        // SoundFont 2.04 section 8.4. Pan uses 500 so the controller's full travel spans exactly the
        // generator's -500 to +500 range.
        constexpr std::array<Sf2Modulator, DefaultModulatorCount> DefaultModulatorTable
        {
            Sf2Modulator{ 0x0502, Gen::InitialAttenuation, 960, 0, 0 },
            Sf2Modulator{ 0x0102, Gen::InitialFilterFc, -2400, 0, 0 },
            Sf2Modulator{ 0x000D, Gen::VibLfoToPitch, 50, 0, 0 },
            Sf2Modulator{ 0x0081, Gen::VibLfoToPitch, 50, 0, 0 },
            Sf2Modulator{ 0x0587, Gen::InitialAttenuation, 960, 0, 0 },
            Sf2Modulator{ 0x028A, Gen::Pan, 500, 0, 0 },
            Sf2Modulator{ 0x058B, Gen::InitialAttenuation, 960, 0, 0 },
            Sf2Modulator{ 0x00DB, Gen::ReverbEffectsSend, 200, 0, 0 },
            Sf2Modulator{ 0x00DD, Gen::ChorusEffectsSend, 200, 0, 0 },
            Sf2Modulator{ 0x020E, Gen::InitialPitch, 12700, 0x0010, 0 },
        };
    }

    _Use_decl_annotations_
    double NormalizeCentered7(uint8_t value) noexcept
    {
        value &= 0x7F;

        return (value <= 64)
            ? static_cast<double>(value) / 128.0
            : 0.5 + static_cast<double>(value - 64) / 126.0;
    }

    _Use_decl_annotations_
    double NormalizeCentered14(uint16_t value) noexcept
    {
        value &= 0x3FFF;

        return (value <= 8192)
            ? static_cast<double>(value) / 16384.0
            : 0.5 + static_cast<double>(value - 8192) / 16382.0;
    }

    _Use_decl_annotations_
    double NormalizeCentered32(uint32_t value) noexcept
    {
        constexpr double center = 2147483648.0;

        return (value <= 0x80000000u)
            ? static_cast<double>(value) / (2.0 * center)
            : 0.5 + (static_cast<double>(value) - center) / (2.0 * (4294967295.0 - center));
    }

    _Use_decl_annotations_
    uint32_t DenormalizeCentered32(double normalized) noexcept
    {
        constexpr double center = 2147483648.0;

        if (!(normalized > 0.0))
        {
            return 0;
        }

        if (normalized >= 1.0)
        {
            return 0xFFFFFFFFu;
        }

        auto const value = (normalized <= 0.5)
            ? normalized * 2.0 * center
            : center + (normalized - 0.5) * 2.0 * (4294967295.0 - center);

        return static_cast<uint32_t>((std::min)(std::llround(value), 0xFFFFFFFFll));
    }

    _Use_decl_annotations_
    uint32_t ScaleUpValue(uint32_t value, uint8_t sourceBits, uint8_t destinationBits) noexcept
    {
        if (sourceBits == 0 || destinationBits <= sourceBits || destinationBits > 32)
        {
            return value;
        }

        auto const scaleBits = static_cast<uint8_t>(destinationBits - sourceBits);
        auto shifted = value << scaleBits;
        auto const center = 1u << (sourceBits - 1);

        if (value <= center)
        {
            return shifted;
        }

        // Above the center, the bits below the top one are repeated into the new low bits so that
        // the maximum maps to the maximum.
        auto const repeatBits = static_cast<uint8_t>(sourceBits - 1);
        auto const repeatMask = (1u << repeatBits) - 1;
        auto repeat = value & repeatMask;

        if (scaleBits > repeatBits)
        {
            repeat <<= (scaleBits - repeatBits);
        }
        else
        {
            repeat >>= (repeatBits - scaleBits);
        }

        while (repeat != 0)
        {
            shifted |= repeat;
            repeat >>= repeatBits;
        }

        return shifted;
    }

    _Use_decl_annotations_
    double EvaluateSource(uint16_t source, ModulationInputs const& inputs) noexcept
    {
        // "No controller" is defined as a constant 1, not as nothing.
        if ((source & SourceIsController) == 0 && (source & SourceIndexMask) == SourceNoController)
        {
            return 1.0;
        }

        double x = (std::clamp)(RawSourceValue(source, inputs), 0.0, 1.0);

        if ((source & SourceIsNegative) != 0)
        {
            x = 1.0 - x;
        }

        auto const curve = static_cast<uint16_t>(source >> 10);

        if ((source & SourceIsBipolar) == 0)
        {
            return ApplyCurve(curve, x);
        }

        // A bipolar curve is the unipolar curve mirrored about the center.
        auto const centered = 2.0 * x - 1.0;

        if (curve == CurveLinear)
        {
            return centered;
        }

        if (curve != CurveConcave && curve != CurveConvex)
        {
            return (centered >= 0.0) ? 1.0 : -1.0;
        }

        auto const magnitude = ApplyCurve(curve, std::abs(centered));

        return (centered < 0.0) ? -magnitude : magnitude;
    }

    _Use_decl_annotations_
    double EvaluateModulator(Sf2Modulator const& modulator, ModulationInputs const& inputs) noexcept
    {
        auto const value =
            EvaluateSource(modulator.Source, inputs) *
            EvaluateSource(modulator.AmountSource, inputs) *
            static_cast<double>(modulator.Amount);

        return (modulator.Transform == TransformAbsolute) ? std::abs(value) : value;
    }

    std::array<Sf2Modulator, DefaultModulatorCount> const& DefaultModulators() noexcept
    {
        return DefaultModulatorTable;
    }
}
