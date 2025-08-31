// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

using namespace winrt::Windows::Networking;
using namespace winrt::Windows::Networking::Sockets;
using namespace winrt::Windows::Networking::ServiceDiscovery::Dnssd;

#include <queue>

enum MidiNetworkHostConnectionPolicy
{
    PolicyAllowAllConnections = 0,
    PolicyAllowFromIpList,
    PolicyAllowFromIpRange,
};

enum MidiNetworkHostAuthentication
{
    NoAuthentication = 0,
    PasswordAuthentication,
    UserAuthentication,
};

enum MidiNetworkHostProtocol
{
    ProtocolDefault = 0,
    ProtocolUdp,
};

struct MidiNetworkHostDefinition
{
    bool Created{ false };

    winrt::hstring EntryIdentifier;         // internal 

    bool UseAutomaticPortAllocation{ true };
    winrt::hstring Port;

    winrt::hstring UmpEndpointName;
    winrt::hstring ProductInstanceId;

    //bool UmpOnly{ true };
    bool IsEnabled{ true };
    bool Advertise{ true };

    bool CreateMidi1Ports{ MIDI_NETWORK_MIDI_CREATE_MIDI1_PORTS_DEFAULT };

    // connection rules
    MidiNetworkHostConnectionPolicy ConnectionPolicy{ MidiNetworkHostConnectionPolicy::PolicyAllowAllConnections };

    // protocol
    MidiNetworkHostProtocol NetworkProtocol{ MidiNetworkHostProtocol::ProtocolDefault };

    // ip information
    //std::vector<winrt::Windows::Networking::HostName> IpAddresses{};

    // authentication
    MidiNetworkHostAuthentication Authentication{ MidiNetworkHostAuthentication::NoAuthentication };

    // auth lookup key


    // generated properties
    winrt::hstring ServiceInstanceName;     // instance name for the PTR record
    winrt::hstring HostName;                // must include the .local domain

};



class MidiNetworkHost
{
public:
    HRESULT Initialize(_In_ MidiNetworkHostDefinition& hostDefinition);
    
    HRESULT Start();
    HRESULT Stop();

    HRESULT Shutdown();

    bool HasStarted() { return m_started; }

    bool IsEnabled() { return m_enabled; }

    MidiNetworkHostDefinition GetDefinition() { return m_hostDefinition; }

    winrt::hstring ActualPort() { return m_socket != nullptr ? m_socket.Information().LocalPort() : L""; }
    winrt::hstring ActualAddress() { return m_socket != nullptr ? m_socket.Information().LocalAddress().DisplayName() : L""; }

private:
//    winrt::hstring m_configIdentifier{};

    bool m_enabled{ true };
    bool m_started{ false };
    bool m_createUmpEndpointsOnly{ true };

    std::wstring m_hostEndpointName{ };
    std::wstring m_hostProductInstanceId{ };

    std::wstring m_parentDeviceInstanceId{ };

    std::atomic<bool> m_keepProcessing{ true };

    winrt::event_token m_messageReceivedEventToken;

    void OnMessageReceived(
        _In_ DatagramSocket const& sender,
        _In_ DatagramSocketMessageReceivedEventArgs const& args);

    MidiNetworkHostDefinition m_hostDefinition{};

    std::shared_ptr<MidiNetworkAdvertiser> m_advertiser{ nullptr };


    DatagramSocket m_socket{ nullptr };

    HRESULT CreateNetworkConnection(
        _In_ winrt::Windows::Networking::HostName const& remoteHostName, 
        _In_ winrt::hstring const& remotePort);

};
