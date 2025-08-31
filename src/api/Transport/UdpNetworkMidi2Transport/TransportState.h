// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

struct MidiTransportSettings
{
    uint32_t OutboundPingInterval{ MIDI_NETWORK_OUTBOUND_PING_INTERVAL_DEFAULT };
    uint16_t RetransmitBufferMaxCommandPacketCount{ MIDI_NETWORK_RETRANSMIT_BUFFER_PACKET_COUNT_DEFAULT };
    uint8_t ForwardErrorCorrectionMaxCommandPacketCount{ MIDI_NETWORK_FEC_PACKET_COUNT_DEFAULT };
    uint32_t DirectConnectionScanInterval{ MIDI_NETWORK_DIRECT_CONNECTION_SCAN_INTERVAL_DEFAULT };
};



// singleton
class TransportState
{

public:
    static TransportState& Current();

    // no copying
    TransportState(_In_ const TransportState&) = delete;
    TransportState& operator=(_In_ const TransportState&) = delete;

    MidiTransportSettings TransportSettings{ };


    wil::com_ptr<CMidi2NetworkMidiEndpointManager> GetEndpointManager()
    {
        return m_endpointManager;
    }

    wil::com_ptr<CMidi2NetworkMidiConfigurationManager> GetConfigurationManager()
    {
        return m_configurationManager;
    }

    //std::shared_ptr<MidiNetworkDeviceTable> GetEndpointTable()
    //{
    //    return m_endpointTable;
    //}


    HRESULT Shutdown()
    {
        m_endpointManager.reset();
        m_configurationManager.reset();

        // TODO: Iterate through hosts and clients and call Cleanup()

        return S_OK;
    }

    HRESULT ConstructEndpointManager();
    HRESULT ConstructConfigurationManager();

    HRESULT AddHost(
        _In_ std::shared_ptr<MidiNetworkHost>);
    std::vector<std::shared_ptr<MidiNetworkHost>> GetHosts() { return m_hosts; }

    HRESULT AddPendingHostDefinition(
        _In_ std::shared_ptr<MidiNetworkHostDefinition>);
    std::vector<std::shared_ptr<MidiNetworkHostDefinition>> GetPendingHostDefinitions() { return m_pendingHostDefinitions; }


    std::shared_ptr<MidiNetworkHost> GetHost(_In_ winrt::hstring hostEntryIdentifier);






    HRESULT AddClient(
        _In_ std::shared_ptr<MidiNetworkClient>);
    std::vector<std::shared_ptr<MidiNetworkClient>> GetClients() { return m_clients; }

    HRESULT AddPendingClientDefinition(
        _In_ std::shared_ptr<MidiNetworkClientDefinition>);
    std::vector<std::shared_ptr<MidiNetworkClientDefinition>> GetPendingClientDefinitions() { return m_pendingClientDefinitions; }


    // these two sets of functions, and their related maps, work with the same
    // connection objects, just in different states

    // these are for when the connection is associated with a UMP endpoint
    HRESULT AssociateMidiEndpointWithConnection(
        _In_ std::wstring endpointDeviceInterfaceId, 
        _In_ winrt::Windows::Networking::HostName const& remoteHostName,
        _In_ winrt::hstring const& remotePort);

    HRESULT DisassociateMidiEndpointFromConnection(
        _In_ std::wstring endpointDeviceInterfaceId);

    std::shared_ptr<MidiNetworkConnection> GetSessionConnection(
        _In_ std::wstring endpointDeviceInterfaceId);

    // these are for when the connection is first created. They also live through when they become UMP endpoints
    bool NetworkConnectionExists(
        _In_ winrt::Windows::Networking::HostName const& remoteHostName,
        _In_ winrt::hstring const& remotePort);

    std::shared_ptr<MidiNetworkConnection> GetNetworkConnection(
        _In_ winrt::Windows::Networking::HostName const& remoteHostName,
        _In_ winrt::hstring const& remotePort);

    std::vector<std::shared_ptr<MidiNetworkConnection>> GetAllNetworkConnectionsForHost(_In_ winrt::hstring const& hostEntryIdentifier);

    HRESULT RemoveAllNetworkConnectionsForHost(_In_ winrt::hstring const& hostEntryIdentifier);


    HRESULT AddNetworkConnection(
        _In_ winrt::Windows::Networking::HostName const& remoteHostName,
        _In_ winrt::hstring const& remotePort, 
        _In_ std::shared_ptr<MidiNetworkConnection> connection);

    HRESULT RemoveNetworkConnection(
        _In_ winrt::Windows::Networking::HostName const& remoteHostName,
        _In_ winrt::hstring const& remotePort);


private:
    TransportState();
    ~TransportState();


    wil::com_ptr<CMidi2NetworkMidiEndpointManager> m_endpointManager;
    wil::com_ptr<CMidi2NetworkMidiConfigurationManager> m_configurationManager;

    std::vector<std::shared_ptr<MidiNetworkHost>> m_hosts{ };
    std::vector<std::shared_ptr<MidiNetworkClient>> m_clients{ };

    std::vector<std::shared_ptr<MidiNetworkHostDefinition>> m_pendingHostDefinitions{ };
    std::vector<std::shared_ptr<MidiNetworkClientDefinition>> m_pendingClientDefinitions{ };

    std::map<std::wstring, std::shared_ptr<MidiNetworkConnection>> m_sessionConnections{ };

    // Map of MidiNetworkConnections and their related remote client addresses
    // the keys for these two maps are the values created with CreateConnectionMapKey
    std::map<std::string, std::shared_ptr<MidiNetworkConnection>> m_networkConnections{ };

    inline std::string CreateNetworkConnectionMapKey(_In_ winrt::Windows::Networking::HostName const& remoteHostName, _In_ winrt::hstring const& remotePort)
    {
        return winrt::to_string(remoteHostName.CanonicalName() + L":" + remotePort);
    }

};