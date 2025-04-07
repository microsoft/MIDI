// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiUniversalSystemExclusiveChannel.h"
#include "MidiUniversalSystemExclusiveChannel.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    winrt::hstring MidiUniversalSystemExclusiveChannel::ToString()
    {
        return winrt::to_hstring(std::format("{} {}", winrt::to_string(midi2::MidiUniversalSystemExclusiveChannel::LongLabel()), DisplayValue()));
    }
}
