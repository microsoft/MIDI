// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

struct MidiNetworkClientDefinition
{
    bool Created{ false };

    winrt::hstring EntryIdentifier;         // internal 
    bool Enabled{ true };

    winrt::hstring Name;                    // the name of the endpoint before we do discovery

    winrt::hstring LocalEndpointName;       // this is required by protocol
    winrt::hstring LocalProductInstanceId;  // also required by protocol


    bool CreateMidi1Ports{ MIDI_NETWORK_MIDI_CREATE_MIDI1_PORTS_DEFAULT };


    // protocol
//    MidiNetworkHostProtocol NetworkProtocol{ MidiNetworkHostProtocol::ProtocolDefault };


    // all match criteria follows

    winrt::hstring MatchId{};

    // these are direct connections. HostName or IP are required, plus the port
    winrt::hstring MatchDirectHostNameOrIPAddress{};
    winrt::hstring MatchDirectPort{};
};



class MidiNetworkClient
{
public:
    // will need some different versions of Initialize for the different ways of connecting
    HRESULT Initialize(
        _In_ MidiNetworkClientDefinition& clientDefinition
    );

    HRESULT Start(
        _In_ winrt::Windows::Networking::HostName const& remoteHostName,
        _In_ winrt::hstring const& remotePort
    );

    HRESULT Shutdown();

private:
    winrt::hstring m_configIdentifier{};

    bool m_createUmpEndpointsOnly{ true };

    std::shared_ptr<MidiNetworkConnection> m_networkConnection{ nullptr };
    winrt::Windows::Networking::Sockets::DatagramSocket m_socket{ nullptr };

    std::wstring m_thisEndpointName{ };
    std::wstring m_thisProductInstanceId{ };


    winrt::event_token m_messageReceivedEventToken;

    void OnMessageReceived(
        _In_ winrt::Windows::Networking::Sockets::DatagramSocket const& sender,
        _In_ winrt::Windows::Networking::Sockets::DatagramSocketMessageReceivedEventArgs const& args);


};