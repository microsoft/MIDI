// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once


// singleton
class TransportState
{

public:
    static TransportState& Current();

    // no copying
    TransportState(_In_ const TransportState&) = delete;
    TransportState& operator=(_In_ const TransportState&) = delete;

//    MidiTransportSettings TransportSettings{ };


    wil::com_ptr<CMidi2Ble2MidiEndpointManager> GetEndpointManager()
    {
        return m_endpointManager;
    }

    wil::com_ptr<CMidi2Ble2MidiConfigurationManager> GetConfigurationManager()
    {
        return m_configurationManager;
    }

    //std::shared_ptr<MidiNetworkDeviceTable> GetEndpointTable()
    //{
    //    return m_endpointTable;
    //}


    // This class is a function-local static, so its destructor runs after main returns. Anything
    // holding a thread or a WinRT object has to be released from a transport Shutdown call
    // instead, which is what the endpoint manager does.

    // This PC published as a BLE MIDI Peripheral. There is at most one, because the radio can
    // advertise the MIDI service only once.
    std::shared_ptr<MidiBlePeripheral> GetPeripheral();
    HRESULT StartPeripheral(_In_ MidiBleProtocol::Protocol const protocol);
    HRESULT StopPeripheral();

    HRESULT ConstructEndpointManager();
    HRESULT ConstructConfigurationManager();

    HRESULT AddConnection(_In_ std::shared_ptr<MidiBleConnection> connection);
    HRESULT RemoveConnection(_In_ winrt::hstring const& deviceId);
    void ShutdownAllConnections();

    std::shared_ptr<MidiBleConnection> GetConnectionByDeviceId(_In_ winrt::hstring const& deviceId);
    std::shared_ptr<MidiBleConnection> GetConnectionByEndpointDeviceInterfaceId(_In_ std::wstring const& endpointDeviceInterfaceId);
    std::vector<std::shared_ptr<MidiBleConnection>> GetConnections();

    // The configuration file and the endpoint manager come up in an order this transport does
    // not control, so configured devices are parked here until someone can act on them.
    void AddConfiguredDeviceId(_In_ winrt::hstring const& deviceId);
    std::vector<winrt::hstring> TakeConfiguredDeviceIds();

    void SetConfiguredPeripheralProtocol(_In_ MidiBleProtocol::Protocol const protocol);
    MidiBleProtocol::Protocol TakeConfiguredPeripheralProtocol();

    // Read at connect time, so changing it and reconnecting is enough to compare presets.
    void SetConnectionParameterPreference(_In_ MidiBleProtocol::ConnectionParameterPreference const preference);
    MidiBleProtocol::ConnectionParameterPreference GetConnectionParameterPreference();

    //HRESULT AddHost(
    //    _In_ std::shared_ptr<MidiNetworkHost>);
    //std::vector<std::shared_ptr<MidiNetworkHost>> GetHosts() { return m_hosts; }

    //HRESULT AddPendingHostDefinition(
    //    _In_ std::shared_ptr<MidiNetworkHostDefinition>);
    //std::vector<std::shared_ptr<MidiNetworkHostDefinition>> GetPendingHostDefinitions() { return m_pendingHostDefinitions; }

    //HRESULT AddClient(
    //    _In_ std::shared_ptr<MidiNetworkClient>);
    //std::vector<std::shared_ptr<MidiNetworkClient>> GetClients() { return m_clients; }

    //HRESULT AddPendingClientDefinition(
    //    _In_ std::shared_ptr<MidiNetworkClientDefinition>);
    //std::vector<std::shared_ptr<MidiNetworkClientDefinition>> GetPendingClientDefinitions() { return m_pendingClientDefinitions; }


    // these two sets of functions, and their related maps, work with the same
    // connection objects, just in different states

    // these are for when the connection is associated with a UMP endpoint
    //HRESULT AssociateMidiEndpointWithConnection(
    //    _In_ std::wstring endpointDeviceInterfaceId, 
    //    _In_ winrt::Windows::Networking::HostName const& remoteHostName,
    //    _In_ winrt::hstring const& remotePort);

    //HRESULT DisassociateMidiEndpointFromConnection(
    //    _In_ std::wstring endpointDeviceInterfaceId);

    //std::shared_ptr<MidiNetworkConnection> GetSessionConnection(
    //    _In_ std::wstring endpointDeviceInterfaceId);

    // these are for when the connection is first created. They also live through when they become UMP endpoints
    //bool NetworkConnectionExists(
    //    _In_ winrt::Windows::Networking::HostName const& remoteHostName,
    //    _In_ winrt::hstring const& remotePort);

    //std::shared_ptr<MidiNetworkConnection> GetNetworkConnection(
    //    _In_ winrt::Windows::Networking::HostName const& remoteHostName,
    //    _In_ winrt::hstring const& remotePort);

    //HRESULT AddNetworkConnection(
    //    _In_ winrt::Windows::Networking::HostName const& remoteHostName,
    //    _In_ winrt::hstring const& remotePort, 
    //    _In_ std::shared_ptr<MidiNetworkConnection> connection);

    //HRESULT RemoveNetworkConnection(
    //    _In_ winrt::Windows::Networking::HostName const& remoteHostName,
    //    _In_ winrt::hstring const& remotePort);

private:
    TransportState();
    ~TransportState();


    wil::com_ptr<CMidi2Ble2MidiEndpointManager> m_endpointManager;
    wil::com_ptr<CMidi2Ble2MidiConfigurationManager> m_configurationManager;

    // keyed by the GATT service device interface id, which is what discovery and the config
    // commands both work with
    std::map<winrt::hstring, std::shared_ptr<MidiBleConnection>> m_connections{ };
    std::mutex m_connectionsLock;

    std::vector<winrt::hstring> m_configuredDeviceIds{ };
    std::mutex m_configuredDeviceIdsLock;

    std::shared_ptr<MidiBlePeripheral> m_peripheral{ nullptr };
    std::mutex m_peripheralLock;

    MidiBleProtocol::Protocol m_configuredPeripheralProtocol{ MidiBleProtocol::Protocol::Unknown };

    std::atomic<MidiBleProtocol::ConnectionParameterPreference> m_connectionParameterPreference{
        MidiBleProtocol::ConnectionParameterPreference::ThroughputOptimized };

    //std::vector<std::shared_ptr<MidiNetworkHost>> m_hosts{ };
    //std::vector<std::shared_ptr<MidiNetworkClient>> m_clients{ };

    //std::vector<std::shared_ptr<MidiNetworkHostDefinition>> m_pendingHostDefinitions{ };
    //std::vector<std::shared_ptr<MidiNetworkClientDefinition>> m_pendingClientDefinitions{ };

    //std::map<std::wstring, std::shared_ptr<MidiNetworkConnection>> m_sessionConnections{ };

    // Map of MidiNetworkConnections and their related remote client addresses
    // the keys for these two maps are the values created with CreateConnectionMapKey
    //std::map<std::string, std::shared_ptr<MidiNetworkConnection>> m_networkConnections{ };

    //inline std::string CreateNetworkConnectionMapKey(_In_ winrt::Windows::Networking::HostName const& remoteHostName, _In_ winrt::hstring const& remotePort)
    //{
    //    return winrt::to_string(remoteHostName.CanonicalName() + L":" + remotePort);
    //}

};