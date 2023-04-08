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


        // WinRT can't expose constants, so the SDK will have strongly-typed
        // classes that handle all this cruft. We use json here so we can extend
        // this in the future, and add new transports, without breaking the API contract
        hstring connectionType = jsonProps.GetObject().GetNamedString(PKEY_ConnectionType);
        hstring midiEndpointName = jsonProps.GetObject().GetNamedString(PKEY_MidiEndpointName);
        hstring midiProductInstanceId = jsonProps.GetObject().GetNamedString(PKEY_MidiProductInstanceId);

        bool shouldAdvertise = jsonProps.GetObject().GetNamedBoolean(PKEY_Server_Advertise);
        hstring serviceInstanceName = jsonProps.GetObject().GetNamedString(PKEY_Server_ServiceInstanceName);
        hstring serverPort = jsonProps.GetObject().GetNamedString(PKEY_Server_Port);
        hstring serverHostName = jsonProps.GetObject().GetNamedString(PKEY_Server_HostName);


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