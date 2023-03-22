#pragma once
#include "MidiUmpEndpoint.g.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiUmpEndpoint : MidiUmpEndpointT<MidiUmpEndpoint>
    {
        MidiUmpEndpoint() = default;

        winrt::Windows::Devices::Midi2::MidiMessageReadBuffer IncomingMessages();
        winrt::Windows::Devices::Midi2::MidiMessageWriteBuffer OutcomingMessages();
    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiUmpEndpoint : MidiUmpEndpointT<MidiUmpEndpoint, implementation::MidiUmpEndpoint>
    {
    };
}
