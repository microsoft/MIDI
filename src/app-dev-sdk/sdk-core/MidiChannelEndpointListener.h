#pragma once
#include "MidiChannelEndpointListener.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiChannelEndpointListener : MidiChannelEndpointListenerT<MidiChannelEndpointListener>
    {
        MidiChannelEndpointListener() = default;

        winrt::Microsoft::Devices::Midi2::MidiGroup IncludeGroup();
        void IncludeGroup(winrt::Microsoft::Devices::Midi2::MidiGroup const& value);
        winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::MidiChannel> IncludeChannels();
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiChannelEndpointListener : MidiChannelEndpointListenerT<MidiChannelEndpointListener, implementation::MidiChannelEndpointListener>
    {
    };
}
