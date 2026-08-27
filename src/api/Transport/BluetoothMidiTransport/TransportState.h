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


    wil::com_ptr<CMidi2BluetoothMidiEndpointManager> GetEndpointManager()
    {
        return m_endpointManager;
    }

    wil::com_ptr<CMidi2BluetoothMidiConfigurationManager> GetConfigurationManager()
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

    // Probed once at start up. A machine with no usable radio still loads the transport, so this
    // is what lets every command explain itself rather than just doing nothing.
    void SetRadioCapabilities(_In_ MidiBleProtocol::RadioCapabilities const& capabilities);
    MidiBleProtocol::RadioCapabilities GetRadioCapabilities();

    // Approval of a Central which subscribes to this PC's peripheral. WinRT cannot refuse a GATT
    // subscription, so an unapproved Central is left subscribed with no endpoint and no data path.
    void SetPeripheralClientPolicy(_In_ MidiBleProtocol::PeripheralClientPolicy const policy);
    MidiBleProtocol::PeripheralClientPolicy GetPeripheralClientPolicy();

    void SetRememberedPeripheralClients(
        _In_ std::vector<MidiBleProtocol::PeripheralClientIdentity> const& allowed,
        _In_ std::vector<MidiBleProtocol::PeripheralClientIdentity> const& denied);

    std::vector<MidiBleProtocol::PeripheralClientIdentity> GetRememberedPeripheralClients(_In_ bool const allowed);

    // Returns what should happen to a Central which has just subscribed, and records it as the
    // waiting client when the answer is Pending.
    MidiBleProtocol::PeripheralClientDecision EvaluatePeripheralClient(
        _In_ MidiBleProtocol::PendingPeripheralClient const& client);

    bool TryGetPendingPeripheralClient(_Out_ MidiBleProtocol::PendingPeripheralClient& client);
    void ClearPendingPeripheralClient();

    // Drops the decision covering the current link only. Called when the Central goes away, so an
    // "allow once" does not carry over to whoever connects next.
    void ClearPeripheralClientLinkDecision();

    // Applies a user decision to the waiting client. Returns 0 on success, or the error code
    // saying why not: nothing waiting, a different device waiting, or a permanent decision asked
    // for about an address which rotates.
    uint32_t ApplyPeripheralClientDecision(
        _In_ std::wstring const& address,
        _In_ bool const approve,
        _In_ MidiBleProtocol::ApprovalScope const scope,
        _Out_ bool& shouldPersist,
        _Out_ MidiBleProtocol::PeripheralClientIdentity& identity);

    bool ForgetPeripheralClient(_In_ std::wstring const& address);

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


    wil::com_ptr<CMidi2BluetoothMidiEndpointManager> m_endpointManager;
    wil::com_ptr<CMidi2BluetoothMidiConfigurationManager> m_configurationManager;

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

    MidiBleProtocol::RadioCapabilities m_radioCapabilities{};
    std::mutex m_radioCapabilitiesLock;

    // BLE MIDI allows a single active link, so there is at most one Central waiting at a time.
    std::atomic<MidiBleProtocol::PeripheralClientPolicy> m_peripheralClientPolicy{
        MidiBleProtocol::PeripheralClientPolicy::RequireApproval };

    MidiBleProtocol::PendingPeripheralClient m_pendingPeripheralClient{};
    bool m_hasPendingPeripheralClient{ false };

    // The decision covering the Central which is connected right now, keyed on its WinRT device
    // id. This is what makes "once" mean once: it is dropped when the link goes away.
    std::wstring m_linkDecisionClientDeviceId{ };
    bool m_linkDecisionApproved{ false };

    // keyed by the normalized address so a hand-edited configuration entry still matches
    std::map<std::wstring, MidiBleProtocol::PeripheralClientIdentity> m_allowedPeripheralClients{ };
    std::map<std::wstring, MidiBleProtocol::PeripheralClientIdentity> m_deniedPeripheralClients{ };

    // decisions scoped to this run of the service, which are never written to the config file
    std::map<std::wstring, bool> m_sessionPeripheralClientDecisions{ };

    std::mutex m_peripheralClientLock;

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