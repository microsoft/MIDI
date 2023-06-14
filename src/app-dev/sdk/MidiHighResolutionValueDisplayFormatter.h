// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiHighResolutionValueDisplayFormatter.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiHighResolutionValueDisplayFormatter : MidiHighResolutionValueDisplayFormatterT<MidiHighResolutionValueDisplayFormatter>
    {
        MidiHighResolutionValueDisplayFormatter() = default;

        static hstring FormatUnipolarPositiveValue(uint32_t rawValue, uint32_t maxRawValue, uint32_t scaleMaxValue, uint8_t scaleNumberDecimalPlaces);
        static hstring FormatUnipolarNegativeValue(int32_t rawValue, int32_t minRawValue, int32_t scaleMinValue, uint8_t scaleNumberDecimalPlaces);
        static hstring FormatBipolarValue(int32_t rawValue, int32_t maxRawValue, int32_t minRawValue, int32_t scaleMaxValue, int32_t scaleMinValue, uint8_t scaleNumberDecimalPlaces);
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiHighResolutionValueDisplayFormatter : MidiHighResolutionValueDisplayFormatterT<MidiHighResolutionValueDisplayFormatter, implementation::MidiHighResolutionValueDisplayFormatter>
    {
    };
}
