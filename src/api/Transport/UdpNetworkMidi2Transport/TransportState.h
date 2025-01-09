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


    HRESULT Cleanup()
    {
        m_endpointManager.reset();
        m_configurationManager.reset();

        // TODO: Iterate through hosts and clients and call Cleanup()

        return S_OK;
    }

    HRESULT ConstructEndpointManager();
    HRESULT ConstructConfigurationManager();

    HRESULT AddHost(_In_ std::shared_ptr<MidiNetworkHost>);
    std::vector<std::shared_ptr<MidiNetworkHost>> GetHosts() { return m_hosts; }

    HRESULT AddPendingHostDefinition(_In_ std::shared_ptr<MidiNetworkHostDefinition>);
    std::vector<std::shared_ptr<MidiNetworkHostDefinition>> GetPendingHostDefinitions() { return m_pendingHostDefinitions; }


    HRESULT AddPendingClientDefinition(_In_ std::shared_ptr<MidiNetworkClientDefinition>);
    std::vector<std::shared_ptr<MidiNetworkClientDefinition>> GetPendingClientDefinitions() { return m_pendingClientDefinitions; }


    HRESULT AddSessionConnection(_In_ std::wstring endpointDeviceInterfaceId, _In_ std::shared_ptr<MidiNetworkConnection> connection);
    HRESULT RemoveSessionConnection(_In_ std::wstring endpointDeviceInterfaceId);
    std::shared_ptr<MidiNetworkConnection> GetSessionConnection(_In_ std::wstring endpointDeviceInterfaceId);


private:
    TransportState();
    ~TransportState();


    wil::com_ptr<CMidi2NetworkMidiEndpointManager> m_endpointManager;
    wil::com_ptr<CMidi2NetworkMidiConfigurationManager> m_configurationManager;

    // key is the host identifier
    std::vector<std::shared_ptr<MidiNetworkHost>> m_hosts{ };

    std::vector<std::shared_ptr<MidiNetworkHostDefinition>> m_pendingHostDefinitions{ };
    std::vector<std::shared_ptr<MidiNetworkClientDefinition>> m_pendingClientDefinitions{ };

    std::map<std::wstring, std::shared_ptr<MidiNetworkConnection>> m_sessionConnections{ };

};