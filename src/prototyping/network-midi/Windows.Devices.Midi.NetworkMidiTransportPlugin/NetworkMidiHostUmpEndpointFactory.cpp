#include "pch.h"
#include "NetworkMidiHostUmpEndpointFactory.h"
#include "NetworkMidiHostUmpEndpointFactory.g.cpp"

#include <iostream>

namespace winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::implementation
{
    winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::NetworkMidiHostUmpEndpoint NetworkMidiHostUmpEndpointFactory::GetExistingUmpEndpoint(hstring const& id)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::NetworkMidiHostUmpEndpoint> NetworkMidiHostUmpEndpointFactory::CreateNewUmpEndpoint(hstring const& configurationJson)
    {
        winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::NetworkMidiHostUmpEndpoint endpoint;


        // todo: set properties based on the json passed in.

        std::cout << "about to parse props" << std::endl;

        auto jsonProps = json::JsonValue::Parse(configurationJson);

        // WinRT can't expose constants, so maybe create a WinRT class with a bunch
        // of property getters that represent the different property keys. That becomes
        // part of the API contract, though, and so it can't be added to easily. So
        // it may be that documentation (ugh) or some language-specific header files
        // are the way to go.
        hstring connectionType = jsonProps.GetObject().GetNamedString(L"ConnectionType");
        hstring midiEndpointName = jsonProps.GetObject().GetNamedString(L"MidiEndpointName");
        hstring midiProductInstanceId = jsonProps.GetObject().GetNamedString(L"MidiProductInstanceId");

        bool shouldAdvertise = jsonProps.GetObject().GetNamedBoolean(L"Server_Advertise");
        hstring serviceInstanceName = jsonProps.GetObject().GetNamedString(L"Server_ServiceInstanceName");
        hstring serverPort = jsonProps.GetObject().GetNamedString(L"Server_Port");
        hstring serverHostName = jsonProps.GetObject().GetNamedString(L"Server_HostName");

        std::cout << "about to start server" << std::endl;

       // endpoint._id = GenerateEndpointDeviceId(connectionType, serverHostName, serverPort, midiEndpointName, midiProductInstanceId);

        endpoint.StartAsync(
            serverHostName,
            serverPort,
            midiEndpointName,
            midiProductInstanceId,
            shouldAdvertise,
            serviceInstanceName
        );

        // does this make a copy?
        co_return endpoint;
    }
}