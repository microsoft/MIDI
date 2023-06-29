#pragma once
#include "MidiEndpointConnectionList.g.h"
#include "MidiListBase.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiEndpointConnectionList : MidiEndpointConnectionListT<MidiEndpointConnectionList, Microsoft::Devices::Midi2::implementation::MidiListBase>
    {
        MidiEndpointConnectionList() = default;

    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiEndpointConnectionList : MidiEndpointConnectionListT<MidiEndpointConnectionList, implementation::MidiEndpointConnectionList>
    {
    };
}
