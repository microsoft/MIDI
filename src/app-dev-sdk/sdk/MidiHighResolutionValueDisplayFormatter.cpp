// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiHighResolutionValueDisplayFormatter.h"
#include "MidiHighResolutionValueDisplayFormatter.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    hstring MidiHighResolutionValueDisplayFormatter::FormatUnipolarPositiveValue(uint32_t rawValue, uint32_t maxRawValue, uint32_t scaleMaxValue, uint8_t scaleNumberDecimalPlaces)
    {
        throw hresult_not_implemented();
    }
    hstring MidiHighResolutionValueDisplayFormatter::FormatUnipolarNegativeValue(int32_t rawValue, int32_t minRawValue, int32_t scaleMinValue, uint8_t scaleNumberDecimalPlaces)
    {
        throw hresult_not_implemented();
    }
    hstring MidiHighResolutionValueDisplayFormatter::FormatBipolarValue(int32_t rawValue, int32_t maxRawValue, int32_t minRawValue, int32_t scaleMaxValue, int32_t scaleMinValue, uint8_t scaleNumberDecimalPlaces)
    {
        throw hresult_not_implemented();
    }
}
