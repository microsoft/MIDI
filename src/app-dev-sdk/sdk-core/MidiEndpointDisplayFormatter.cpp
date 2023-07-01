// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointDisplayFormatter.h"
#include "MidiEndpointDisplayFormatter.g.cpp"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    hstring MidiEndpointDisplayFormatter::FormatGroupWithFunctionBlocks(winrt::Microsoft::Devices::Midi2::MidiGroup const& group, winrt::Microsoft::Devices::Midi2::MidiFunctionBlockList const& blocks, bool abbreviatedVersion)
    {
        throw hresult_not_implemented();
    }
    hstring MidiEndpointDisplayFormatter::FormatAllGroupOptionWithFunctionBlocks(winrt::Microsoft::Devices::Midi2::MidiFunctionBlockList const& blocks, bool abbreviatedVersion)
    {
        throw hresult_not_implemented();
    }
    hstring MidiEndpointDisplayFormatter::FormatChannel(winrt::Microsoft::Devices::Midi2::MidiChannel const& channel, bool abbreviatedVersion)
    {
        throw hresult_not_implemented();
    }
}
