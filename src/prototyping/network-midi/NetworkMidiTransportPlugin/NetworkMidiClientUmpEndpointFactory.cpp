#include "pch.h"
#include "NetworkMidiClientUmpEndpointFactory.h"
#include "NetworkMidiClientUmpEndpointFactory.g.cpp"


namespace winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::implementation
{
    winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::NetworkMidiClientUmpEndpoint NetworkMidiClientUmpEndpointFactory::GetExistingUmpEndpoint(hstring const& id)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::NetworkMidiClientUmpEndpoint> NetworkMidiClientUmpEndpointFactory::CreateNewUmpEndpoint(hstring configurationJson)
    {
        throw hresult_not_implemented();
    }
}
