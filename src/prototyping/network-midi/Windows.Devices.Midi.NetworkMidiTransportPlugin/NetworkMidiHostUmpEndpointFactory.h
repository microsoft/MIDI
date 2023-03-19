#pragma once

#include "NetworkMidiHostUmpEndpoint.h"

#include "NetworkMidiHostUmpEndpointFactory.g.h"

namespace winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::implementation
{
    struct NetworkMidiHostUmpEndpointFactory : NetworkMidiHostUmpEndpointFactoryT<NetworkMidiHostUmpEndpointFactory>
    {
        // TODO: List of endpoints
    private:


    public:

        NetworkMidiHostUmpEndpointFactory() = default;

        winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::NetworkMidiHostUmpEndpoint GetExistingUmpEndpoint(hstring const& id);
        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::NetworkMidiHostUmpEndpoint> CreateNewUmpEndpoint(hstring const& configurationJson);
    };
}
namespace winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::factory_implementation
{
    struct NetworkMidiHostUmpEndpointFactory : NetworkMidiHostUmpEndpointFactoryT<NetworkMidiHostUmpEndpointFactory, implementation::NetworkMidiHostUmpEndpointFactory>
    {
    };
}