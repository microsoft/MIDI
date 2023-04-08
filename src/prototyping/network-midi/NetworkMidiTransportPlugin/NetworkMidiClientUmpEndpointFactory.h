#pragma once
#include "NetworkMidiClientUmpEndpointFactory.g.h"


namespace winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::implementation
{
    struct NetworkMidiClientUmpEndpointFactory : NetworkMidiClientUmpEndpointFactoryT<NetworkMidiClientUmpEndpointFactory>
    {
        NetworkMidiClientUmpEndpointFactory() = default;

        winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::NetworkMidiClientUmpEndpoint GetExistingUmpEndpoint(hstring const& id);
        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::NetworkMidiClientUmpEndpoint> CreateNewUmpEndpoint(hstring configurationJson);
    };
}
namespace winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::factory_implementation
{
    struct NetworkMidiClientUmpEndpointFactory : NetworkMidiClientUmpEndpointFactoryT<NetworkMidiClientUmpEndpointFactory, implementation::NetworkMidiClientUmpEndpointFactory>
    {
    };
}
