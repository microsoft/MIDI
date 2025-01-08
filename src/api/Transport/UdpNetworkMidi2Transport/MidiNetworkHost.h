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

    bool HasStarted() { return m_started; }

private:
    bool m_started{ false };

    //HRESULT EstablishNewSession();
    std::wstring m_hostEndpointName{ };
    std::wstring m_hostProductInstanceId{ };

    std::atomic<bool> m_keepProcessing{ true };

    // todo: these probably aren't necessary. The only queues we need are for midi messages
//    std::queue<MidiNetworkOutOfBandIncomingCommandPacket> m_incomingOutOfBandCommands;
//    std::queue<MidiNetworkOutOfBandOutgoingCommandPacket> m_outgoingOutOfBandCommands;

    winrt::impl::consume_Windows_Networking_Sockets_IDatagramSocket<IDatagramSocket>::MessageReceived_revoker m_messageReceivedRevoker;
    void OnMessageReceived(
        _In_ DatagramSocket const& sender,
        _In_ DatagramSocketMessageReceivedEventArgs const& args);

    MidiNetworkHostDefinition m_hostDefinition{};

    std::shared_ptr<MidiNetworkAdvertiser> m_advertiser{ nullptr };


    DatagramSocket m_socket{ nullptr };

    // Map of MidiNetworkHostSessions and their related remote client addresses
    // the keys for these two maps are the same values created with CreateSessionMapKey
    std::map<std::string, std::shared_ptr<MidiNetworkConnection>> m_connections{ };

    inline std::string CreateConnectionMapKey(_In_ HostName const& hostName, _In_ winrt::hstring const& port)
    {
        return winrt::to_string(hostName.CanonicalName() + L":" + port);
    }

    inline bool ConnectionExists(_In_ HostName const& hostName, _In_ winrt::hstring const& port)
    {
        auto key = CreateConnectionMapKey(hostName, port);

        return m_connections.find(key) != m_connections.end();
    }


    inline void RemoveConnection(_In_ HostName const& hostName, _In_ winrt::hstring const& port)
    {
        if (ConnectionExists(hostName, port))
        {
            auto entry = m_connections.find(CreateConnectionMapKey(hostName, port));

            LOG_IF_FAILED(entry->second->Shutdown());

            m_connections.erase(CreateConnectionMapKey(hostName, port));
        }
    }

    inline std::shared_ptr<MidiNetworkConnection> GetOrCreateConnection(_In_ HostName const& hostName, _In_ winrt::hstring const& port)
    {
        auto key = CreateConnectionMapKey(hostName, port);

        if (!ConnectionExists(hostName, port))
        {
            // need to create it and then add one

            auto conn = std::make_shared<MidiNetworkConnection>();

            if (conn)
            {
                conn->Initialize(
                    MidiNetworkConnectionRole::ConnectionWindowsIsHost,
                    m_socket,
                    hostName,
                    port,
                    m_hostEndpointName,
                    m_hostProductInstanceId,
                    TransportState::Current().TransportSettings.RetransmitBufferMaxCommandPacketCount,
                    TransportState::Current().TransportSettings.ForwardErrorCorrectionMaxCommandPacketCount
                );

                m_connections.insert_or_assign(key, conn);
            }
            else
            {
                // could not create the connection object.
            }
        }

        return m_connections.find(key)->second;
    }


};
