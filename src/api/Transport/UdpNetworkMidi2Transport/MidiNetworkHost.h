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
    winrt::hstring EntryIdentifier;         // internal 

    bool UseAutomaticPortAllocation{ true };
    winrt::hstring Port;

    winrt::hstring UmpEndpointName;
    winrt::hstring ProductInstanceId;

    bool UmpOnly{ true };
    bool Enabled{ true };
    bool Advertise{ true };

    // connection rules
    MidiNetworkHostConnectionPolicy ConnectionPolicy{ MidiNetworkHostConnectionPolicy::PolicyAllowAllConnections };

    // network adapter (the id is a guid)


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

    HRESULT Shutdown();

private:
    // this will need to take the incoming packet and then route it to the 
    // correct session based on the client IP/Port sending the message, or 
    // start up a new session if appropriate
    HRESULT ProcessIncomingPackets();
    HRESULT ProcessOutgoingPackets();

    HRESULT EstablishNewSession();

    std::queue<MidiNetworkOutOfBandIncomingCommandPacket> m_incomingOutOfBandCommands;
    std::queue<MidiNetworkOutOfBandOutgoingCommandPacket> m_outgoingOutOfBandCommands;

    void OnUdpPacketReceived(
        _In_ DatagramSocket const& sender,
        _In_ DatagramSocketMessageReceivedEventArgs const& args);

    MidiNetworkHostDefinition m_hostDefinition{};

    std::shared_ptr<MidiNetworkAdvertiser> m_advertiser;

    DatagramSocket m_socket;

    std::atomic<bool> m_keepProcessing{ true };

    // Map of MidiNetworkHostSessions and their related remote client addresses
    std::map<std::string, std::shared_ptr<MidiNetworkHostSession>> m_sessions{ };

    inline std::string CreateSessionMapKey(_In_ HostName const& hostName, _In_ winrt::hstring const& port)
    {
        return winrt::to_string(hostName.ToString() + L":" + port);
    }

    inline bool SessionExists(_In_ HostName const& hostName, _In_ winrt::hstring const& port)
    {
        auto key = CreateSessionMapKey(hostName, port);

        return m_sessions.find(key) != m_sessions.end();
    }

};
