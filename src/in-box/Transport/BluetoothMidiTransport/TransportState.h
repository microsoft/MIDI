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

    wil::com_ptr<CMidi2BluetoothMidiEndpointManager> GetEndpointManager()
    {
        auto lock = m_managersLock.lock_exclusive();

        return m_endpointManager;
    }

    wil::com_ptr<CMidi2BluetoothMidiConfigurationManager> GetConfigurationManager()
    {
        auto lock = m_managersLock.lock_exclusive();

        return m_configurationManager;
    }

    // Stops the peripheral and every connection, which are the only things here with threads, and
    // drops the references to both managers. Anything still held here is released at DLL unload,
    // after the service has destroyed the objects the managers hold. Called from both managers'
    // Shutdown, so it is safe to call twice. A manager calling this must hold its own reference
    // for the rest of its Shutdown.
    void Shutdown() noexcept;

    // This class is a function-local static, so its destructor runs after main returns. Anything
    // holding a thread or a WinRT object has to be released by Shutdown above instead.

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
    void SetConfiguredDeviceAutoConnectDisabled(_In_ winrt::hstring const& deviceId, _In_ bool const disabled);
    bool IsConfiguredDeviceAutoConnectDisabled(_In_ winrt::hstring const& deviceId);

    void SetConfiguredPeripheralProtocol(_In_ MidiBleProtocol::Protocol const protocol);
    MidiBleProtocol::Protocol TakeConfiguredPeripheralProtocol();

    // Read at connect time, so changing it and reconnecting is enough to compare presets.
    void SetConnectionParameterPreference(_In_ MidiBleProtocol::ConnectionParameterPreference const preference);
    MidiBleProtocol::ConnectionParameterPreference GetConnectionParameterPreference();

    // How long an endpoint outlives its device going offline. The per-device value wins unless it
    // is OfflineRetentionUseTransportDefault, so a customer can set one policy and override it for
    // the odd device.
    void SetDefaultOfflineRetentionSeconds(_In_ int32_t const seconds);
    int32_t GetDefaultOfflineRetentionSeconds();

    void SetDeviceOfflineRetentionSeconds(_In_ winrt::hstring const& deviceId, _In_ int32_t const seconds);
    int32_t GetDeviceOfflineRetentionSeconds(_In_ winrt::hstring const& deviceId);
    int32_t GetEffectiveOfflineRetentionSeconds(_In_ winrt::hstring const& deviceId);
    std::map<winrt::hstring, int32_t> GetDeviceOfflineRetentionOverrides();

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

private:
    TransportState();
    ~TransportState();


    wil::srwlock m_managersLock;
    wil::com_ptr<CMidi2BluetoothMidiEndpointManager> m_endpointManager;
    wil::com_ptr<CMidi2BluetoothMidiConfigurationManager> m_configurationManager;

    // keyed by the GATT service device interface id, which is what discovery and the config
    // commands both work with
    std::map<winrt::hstring, std::shared_ptr<MidiBleConnection>> m_connections{ };
    wil::srwlock m_connectionsLock;

    std::vector<winrt::hstring> m_configuredDeviceIds{ };
    std::set<winrt::hstring> m_disabledConfiguredDeviceIds{ };
    wil::srwlock m_configuredDeviceIdsLock;

    std::shared_ptr<MidiBlePeripheral> m_peripheral{ nullptr };
    wil::srwlock m_peripheralLock;

    MidiBleProtocol::Protocol m_configuredPeripheralProtocol{ MidiBleProtocol::Protocol::Unknown };

    std::atomic<MidiBleProtocol::ConnectionParameterPreference> m_connectionParameterPreference{
        MidiBleProtocol::ConnectionParameterPreference::ThroughputOptimized };

    // Defaults to keeping the endpoint, which is the behavior before this was configurable.
    std::atomic<int32_t> m_defaultOfflineRetentionSeconds{ MidiBleProtocol::OfflineRetentionKeepAlways };

    wil::srwlock m_offlineRetentionLock;
    std::map<winrt::hstring, int32_t> m_deviceOfflineRetentionSeconds;

    MidiBleProtocol::RadioCapabilities m_radioCapabilities{};
    wil::srwlock m_radioCapabilitiesLock;

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

    wil::srwlock m_peripheralClientLock;
};