#pragma once
#include "MidiGroupEndpointListener.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiGroupEndpointListener : MidiGroupEndpointListenerT<MidiGroupEndpointListener>
    {
        MidiGroupEndpointListener() = default;

        winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::MidiGroup> IncludeGroups();
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiGroupEndpointListener : MidiGroupEndpointListenerT<MidiGroupEndpointListener, implementation::MidiGroupEndpointListener>
    {
    };
}
