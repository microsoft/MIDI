#include "pch.h"
#include "NetworkMidiClientUmpEndpoint.h"
#include "NetworkMidiClientUmpEndpoint.g.cpp"


namespace winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::implementation
{
    winrt::Windows::Foundation::Collections::IVector<uint32_t> NetworkMidiClientUmpEndpoint::IncomingMessages()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Collections::IVector<uint32_t> NetworkMidiClientUmpEndpoint::OutgoingMessages()
    {
        throw hresult_not_implemented();
    }
    hstring NetworkMidiClientUmpEndpoint::Id()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Collections::IMapView<hstring, winrt::Windows::Foundation::IInspectable> NetworkMidiClientUmpEndpoint::Properties()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::IAsyncAction NetworkMidiClientUmpEndpoint::Start(hstring hostName, hstring port, hstring midiEndpointName, hstring productInstanceId)
    {
        throw hresult_not_implemented();
    }
}
