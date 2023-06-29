#pragma once
#include "MidiEndpointDisplayFormatter.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiEndpointDisplayFormatter : MidiEndpointDisplayFormatterT<MidiEndpointDisplayFormatter>
    {
        MidiEndpointDisplayFormatter() = default;

        static hstring FormatGroupWithFunctionBlocks(winrt::Microsoft::Devices::Midi2::MidiGroup const& group, winrt::Microsoft::Devices::Midi2::MidiFunctionBlockList const& blocks, bool abbreviatedVersion);
        static hstring FormatAllGroupOptionWithFunctionBlocks(winrt::Microsoft::Devices::Midi2::MidiFunctionBlockList const& blocks, bool abbreviatedVersion);
        static hstring FormatChannel(winrt::Microsoft::Devices::Midi2::MidiChannel const& channel, bool abbreviatedVersion);
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiEndpointDisplayFormatter : MidiEndpointDisplayFormatterT<MidiEndpointDisplayFormatter, implementation::MidiEndpointDisplayFormatter>
    {
    };
}
