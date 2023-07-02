// Copyright(c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiEndpointDisplayFormatter.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiEndpointDisplayFormatter : MidiEndpointDisplayFormatterT<MidiEndpointDisplayFormatter>
    {
        MidiEndpointDisplayFormatter() = default;

        static hstring FormatGroupWithFunctionBlocks(winrt::Microsoft::Devices::Midi2::MidiGroup const& group, winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::MidiFunctionBlock> const&, bool abbreviatedVersion);
        static hstring FormatAllGroupOptionWithFunctionBlocks(winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::MidiFunctionBlock> const& blocks, bool abbreviatedVersion);
        static hstring FormatChannel(winrt::Microsoft::Devices::Midi2::MidiChannel const& channel, bool abbreviatedVersion);
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiEndpointDisplayFormatter : MidiEndpointDisplayFormatterT<MidiEndpointDisplayFormatter, implementation::MidiEndpointDisplayFormatter>
    {
    };
}
