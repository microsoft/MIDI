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


enum MidiNetworkUdpHostConnectionPolicy
{
    AllowAllConnections = 0,
    AllowFromIpList,
    AllowFromIpRange,
};

enum MidiNetworkUdpHostAuthentication
{
    NoAuthentication = 0,
    PasswordAuthentication,
    UserAuthentication,
};

struct MidiNetworkUdpHostDefinition
{
    winrt::hstring EntryIdentifier;         // internal 

    uint16_t Port;

    winrt::hstring UmpEndpointName;
    winrt::hstring ProductInstanceId;

    bool UmpOnly{ true };
    bool Enabled{ true };

    // connection rules
    MidiNetworkUdpHostConnectionPolicy ConnectionPolicy{ MidiNetworkUdpHostConnectionPolicy::AllowAllConnections };

    // network adapter (the id is a guid)


    // ip information
    std::vector<winrt::Windows::Networking::HostName> IpAddresses{};

    // authentication
    MidiNetworkUdpHostAuthentication Authentication{ MidiNetworkUdpHostAuthentication::NoAuthentication };

    // auth lookup key


    // generated properties
    winrt::hstring ServiceInstanceName;     // instance name for the PTR record
    winrt::hstring HostName;                // must include the .local domain


};



class MidiNetworkHost
{
public:
    HRESULT Initialize(_In_ MidiNetworkUdpHostDefinition& hostDefinition);

    HRESULT Cleanup();

private:
    // this will need to take the incoming packet and then route it to the 
    // correct session based on the client IP/Port sending the message, or 
    // start up a new session if appropriate
    HRESULT ProcessIncomingPacket();

    HRESULT EstablishNewSession();


    DatagramSocket m_socket;

    // TODO: Map of client connections and their sessions



};
