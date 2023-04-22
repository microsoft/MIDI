#pragma once
#include "NetworkMidiClientUmpEndpoint.g.h"


#include "Properties.h"


namespace winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::implementation
{
    struct NetworkMidiClientUmpEndpoint : NetworkMidiClientUmpEndpointT<NetworkMidiClientUmpEndpoint>
    {
        NetworkMidiClientUmpEndpoint() = default;

        winrt::Windows::Foundation::Collections::IVector<uint32_t> IncomingMessages();
        winrt::Windows::Foundation::Collections::IVector<uint32_t> OutgoingMessages();
        hstring Id();
        winrt::Windows::Foundation::Collections::IMapView<hstring, winrt::Windows::Foundation::IInspectable> Properties();
        winrt::Windows::Foundation::IAsyncAction Start(hstring hostName, hstring port, hstring midiEndpointName, hstring productInstanceId);
    };
}
namespace winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::factory_implementation
{
    struct NetworkMidiClientUmpEndpoint : NetworkMidiClientUmpEndpointT<NetworkMidiClientUmpEndpoint, implementation::NetworkMidiClientUmpEndpoint>
    {
    };
}
