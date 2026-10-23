// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"


TransportState::TransportState() = default;
TransportState::~TransportState() = default;

TransportState& TransportState::Current()
{
    // explanation: http://www.modernescpp.com/index.php/thread-safe-initialization-of-data/

    static TransportState current;

    return current;
}


HRESULT
TransportState::ConstructEndpointManager()
{
    wil::com_ptr<CMidi2BluetoothMidiEndpointManager> endpointManager;
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2BluetoothMidiEndpointManager>(&endpointManager));

    auto lock = m_managersLock.lock_exclusive();

    // another Activate call may have won the race. Keep the one already published.
    if (m_endpointManager == nullptr)
    {
        m_endpointManager = endpointManager;
    }

    return S_OK;
}

HRESULT
TransportState::ConstructConfigurationManager()
{
    wil::com_ptr<CMidi2BluetoothMidiConfigurationManager> configurationManager;
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2BluetoothMidiConfigurationManager>(&configurationManager));

    auto lock = m_managersLock.lock_exclusive();

    if (m_configurationManager == nullptr)
    {
        m_configurationManager = configurationManager;
    }

    return S_OK;
}


void
TransportState::Shutdown() noexcept
{
    // Both own threads: every connection has a writer thread, and so does the connection attached
    // to the peripheral. The endpoint manager has normally stopped them already, in the order its
    // own teardown needs. Doing it again here is harmless, and means nothing with a thread is left
    // for the static destructor at DLL unload.
    try
    {
        LOG_IF_FAILED(StopPeripheral());
    }
    CATCH_LOG();

    try
    {
        ShutdownAllConnections();
    }
    CATCH_LOG();

    wil::com_ptr<CMidi2BluetoothMidiEndpointManager> endpointManager;
    wil::com_ptr<CMidi2BluetoothMidiConfigurationManager> configurationManager;

    {
        auto lock = m_managersLock.lock_exclusive();

        endpointManager.swap(m_endpointManager);
        configurationManager.swap(m_configurationManager);
    }

    // Released outside the lock, in case this is the last reference and a destructor runs
    endpointManager.reset();
    configurationManager.reset();
}


_Use_decl_annotations_
HRESULT
TransportState::AddConnection(std::shared_ptr<MidiBleConnection> connection)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, connection);
    RETURN_HR_IF(E_INVALIDARG, connection->DeviceId().empty());

    std::shared_ptr<MidiBleConnection> replaced{ nullptr };

    try
    {
        auto lock = m_connectionsLock.lock_exclusive();

        auto [entry, inserted] = m_connections.try_emplace(connection->DeviceId(), connection);

        if (!inserted && entry->second != connection)
        {
            replaced = std::exchange(entry->second, connection);
        }
    }
    CATCH_RETURN();

    // A connection replaced without being shut down keeps its writer thread for the life of the
    // service. Shut down outside the lock, because it joins that thread.
    if (replaced != nullptr)
    {
        LOG_IF_FAILED(replaced->Shutdown());
    }

    return S_OK;
}


std::shared_ptr<MidiBlePeripheral>
TransportState::GetPeripheral()
{
    auto lock = m_peripheralLock.lock_exclusive();

    return m_peripheral;
}


_Use_decl_annotations_
HRESULT
TransportState::StartPeripheral(MidiBleProtocol::Protocol const protocol)
{
    // Declared HRESULT, so it must not throw: callers use RETURN_IF_FAILED and an
    // escaping WinRT exception would unwind past them into a worker thread.
    try
    {
        {
            auto lock = m_peripheralLock.lock_exclusive();

            RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED), m_peripheral != nullptr && m_peripheral->IsRunning());
        }

        auto peripheral = std::make_shared<MidiBlePeripheral>();
        RETURN_IF_NULL_ALLOC(peripheral);

        // starting blocks on Bluetooth for as long as the GATT timeouts allow, so like Stop it
        // never runs under the lock: GetPeripheral is on the data path and would stall with it
        RETURN_IF_FAILED(peripheral->Start(protocol));

        std::shared_ptr<MidiBlePeripheral> alreadyRunning{ nullptr };

        {
            auto lock = m_peripheralLock.lock_exclusive();

            if (m_peripheral != nullptr && m_peripheral->IsRunning())
            {
                alreadyRunning = std::move(peripheral);
            }
            else
            {
                m_peripheral = peripheral;
            }
        }

        if (alreadyRunning != nullptr)
        {
            // another caller won the race while this one was starting
            LOG_IF_FAILED(alreadyRunning->Stop());

            return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
        }

        return S_OK;
    }
    CATCH_RETURN()
}


HRESULT
TransportState::StopPeripheral()
{
    std::shared_ptr<MidiBlePeripheral> peripheral{ nullptr };

    {
        auto lock = m_peripheralLock.lock_exclusive();

        peripheral = std::move(m_peripheral);
        m_peripheral = nullptr;
    }

    // stopping blocks on the writer thread and on Bluetooth, so it never runs under the lock
    if (peripheral != nullptr)
    {
        LOG_IF_FAILED(peripheral->Stop());
    }

    return S_OK;
}


_Use_decl_annotations_
HRESULT
TransportState::RemoveConnection(winrt::hstring const& deviceId)
{
    std::shared_ptr<MidiBleConnection> connection{ nullptr };

    {
        auto lock = m_connectionsLock.lock_exclusive();

        if (auto entry = m_connections.find(deviceId); entry != m_connections.end())
        {
            connection = entry->second;
            m_connections.erase(entry);
        }
    }

    // shutdown blocks on the writer thread and on Bluetooth, so it never runs under the lock
    if (connection != nullptr)
    {
        LOG_IF_FAILED(connection->Shutdown());
    }

    return S_OK;
}


void
TransportState::ShutdownAllConnections()
{
    std::vector<std::shared_ptr<MidiBleConnection>> connections;

    {
        auto lock = m_connectionsLock.lock_exclusive();

        for (auto const& entry : m_connections)
        {
            connections.push_back(entry.second);
        }

        m_connections.clear();
    }

    for (auto const& connection : connections)
    {
        LOG_IF_FAILED(connection->Shutdown());
    }
}


_Use_decl_annotations_
std::shared_ptr<MidiBleConnection>
TransportState::GetConnectionByDeviceId(winrt::hstring const& deviceId)
{
    auto lock = m_connectionsLock.lock_exclusive();

    if (auto entry = m_connections.find(deviceId); entry != m_connections.end())
    {
        return entry->second;
    }

    return nullptr;
}


_Use_decl_annotations_
std::shared_ptr<MidiBleConnection>
TransportState::GetConnectionByEndpointDeviceInterfaceId(std::wstring const& endpointDeviceInterfaceId)
{
    auto const normalizedId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);

    auto lock = m_connectionsLock.lock_exclusive();

    for (auto const& entry : m_connections)
    {
        if (entry.second != nullptr && entry.second->EndpointDeviceInterfaceId() == normalizedId)
        {
            return entry.second;
        }
    }

    return nullptr;
}


std::vector<std::shared_ptr<MidiBleConnection>>
TransportState::GetConnections()
{
    auto lock = m_connectionsLock.lock_exclusive();

    std::vector<std::shared_ptr<MidiBleConnection>> connections;
    connections.reserve(m_connections.size());

    for (auto const& entry : m_connections)
    {
        connections.push_back(entry.second);
    }

    return connections;
}


_Use_decl_annotations_
void
TransportState::AddConfiguredDeviceId(winrt::hstring const& deviceId)
{
    if (deviceId.empty())
    {
        return;
    }

    auto lock = m_configuredDeviceIdsLock.lock_exclusive();

    if (std::find(m_configuredDeviceIds.begin(), m_configuredDeviceIds.end(), deviceId) == m_configuredDeviceIds.end())
    {
        m_configuredDeviceIds.push_back(deviceId);
    }
}


std::vector<winrt::hstring>
TransportState::TakeConfiguredDeviceIds()
{
    auto lock = m_configuredDeviceIdsLock.lock_exclusive();

    auto ids = std::move(m_configuredDeviceIds);
    m_configuredDeviceIds.clear();

    return ids;
}


_Use_decl_annotations_
void
TransportState::SetConfiguredDeviceAutoConnectDisabled(winrt::hstring const& deviceId, bool const disabled)
{
    if (deviceId.empty())
    {
        return;
    }

    auto lock = m_configuredDeviceIdsLock.lock_exclusive();

    if (disabled)
    {
        m_disabledConfiguredDeviceIds.insert(deviceId);
    }
    else
    {
        m_disabledConfiguredDeviceIds.erase(deviceId);
    }
}


_Use_decl_annotations_
bool
TransportState::IsConfiguredDeviceAutoConnectDisabled(winrt::hstring const& deviceId)
{
    auto lock = m_configuredDeviceIdsLock.lock_exclusive();

    return m_disabledConfiguredDeviceIds.find(deviceId) != m_disabledConfiguredDeviceIds.end();
}


_Use_decl_annotations_
void
TransportState::SetConfiguredPeripheralProtocol(MidiBleProtocol::Protocol const protocol)
{
    auto lock = m_configuredDeviceIdsLock.lock_exclusive();

    m_configuredPeripheralProtocol = protocol;
}


MidiBleProtocol::Protocol
TransportState::TakeConfiguredPeripheralProtocol()
{
    auto lock = m_configuredDeviceIdsLock.lock_exclusive();

    auto const protocol = m_configuredPeripheralProtocol;
    m_configuredPeripheralProtocol = MidiBleProtocol::Protocol::Unknown;

    return protocol;
}


_Use_decl_annotations_
void
TransportState::SetConnectionParameterPreference(MidiBleProtocol::ConnectionParameterPreference const preference)
{
    m_connectionParameterPreference.store(preference);
}


MidiBleProtocol::ConnectionParameterPreference
TransportState::GetConnectionParameterPreference()
{
    return m_connectionParameterPreference.load();
}


_Use_decl_annotations_
void
TransportState::SetDefaultOfflineRetentionSeconds(int32_t const seconds)
{
    m_defaultOfflineRetentionSeconds.store(seconds);
}

int32_t
TransportState::GetDefaultOfflineRetentionSeconds()
{
    return m_defaultOfflineRetentionSeconds.load();
}


_Use_decl_annotations_
void
TransportState::SetDeviceOfflineRetentionSeconds(winrt::hstring const& deviceId, int32_t const seconds)
{
    if (deviceId.empty())
    {
        return;
    }

    auto lock = m_offlineRetentionLock.lock_exclusive();

    if (seconds == MidiBleProtocol::OfflineRetentionUseTransportDefault)
    {
        // Stored as an absence rather than as a value, so the configuration file and the responses
        // only ever mention devices which actually override the transport setting.
        m_deviceOfflineRetentionSeconds.erase(deviceId);

        return;
    }

    m_deviceOfflineRetentionSeconds.insert_or_assign(deviceId, seconds);
}

_Use_decl_annotations_
int32_t
TransportState::GetDeviceOfflineRetentionSeconds(winrt::hstring const& deviceId)
{
    auto lock = m_offlineRetentionLock.lock_exclusive();

    if (auto entry = m_deviceOfflineRetentionSeconds.find(deviceId); entry != m_deviceOfflineRetentionSeconds.end())
    {
        return entry->second;
    }

    return MidiBleProtocol::OfflineRetentionUseTransportDefault;
}

_Use_decl_annotations_
int32_t
TransportState::GetEffectiveOfflineRetentionSeconds(winrt::hstring const& deviceId)
{
    auto const configured = GetDeviceOfflineRetentionSeconds(deviceId);

    if (configured != MidiBleProtocol::OfflineRetentionUseTransportDefault)
    {
        return configured;
    }

    return GetDefaultOfflineRetentionSeconds();
}

std::map<winrt::hstring, int32_t>
TransportState::GetDeviceOfflineRetentionOverrides()
{
    auto lock = m_offlineRetentionLock.lock_exclusive();

    return m_deviceOfflineRetentionSeconds;
}


_Use_decl_annotations_
void
TransportState::SetRadioCapabilities(MidiBleProtocol::RadioCapabilities const& capabilities)
{
    auto lock = m_radioCapabilitiesLock.lock_exclusive();

    m_radioCapabilities = capabilities;
}


MidiBleProtocol::RadioCapabilities
TransportState::GetRadioCapabilities()
{
    auto lock = m_radioCapabilitiesLock.lock_exclusive();

    return m_radioCapabilities;
}


_Use_decl_annotations_
void
TransportState::SetPeripheralClientPolicy(MidiBleProtocol::PeripheralClientPolicy const policy)
{
    m_peripheralClientPolicy.store(policy);
}


MidiBleProtocol::PeripheralClientPolicy
TransportState::GetPeripheralClientPolicy()
{
    return m_peripheralClientPolicy.load();
}


_Use_decl_annotations_
void
TransportState::SetRememberedPeripheralClients(
    std::vector<MidiBleProtocol::PeripheralClientIdentity> const& allowed,
    std::vector<MidiBleProtocol::PeripheralClientIdentity> const& denied)
{
    auto lock = m_peripheralClientLock.lock_exclusive();

    m_allowedPeripheralClients.clear();
    m_deniedPeripheralClients.clear();

    for (auto const& entry : allowed)
    {
        auto const key = MidiBleUtilities::NormalizeClientMatchKey(entry.Address);

        if (!key.empty())
        {
            m_allowedPeripheralClients[key] = entry;
        }
    }

    // A device named in both lists is denied, because the safe reading of a contradictory
    // configuration file is the more restrictive one.
    for (auto const& entry : denied)
    {
        auto const key = MidiBleUtilities::NormalizeClientMatchKey(entry.Address);

        if (!key.empty())
        {
            m_deniedPeripheralClients[key] = entry;
            m_allowedPeripheralClients.erase(key);
        }
    }
}


_Use_decl_annotations_
std::vector<MidiBleProtocol::PeripheralClientIdentity>
TransportState::GetRememberedPeripheralClients(bool const allowed)
{
    auto lock = m_peripheralClientLock.lock_exclusive();

    std::vector<MidiBleProtocol::PeripheralClientIdentity> results{};

    for (auto const& entry : allowed ? m_allowedPeripheralClients : m_deniedPeripheralClients)
    {
        results.push_back(entry.second);
    }

    return results;
}


_Use_decl_annotations_
MidiBleProtocol::PeripheralClientDecision
TransportState::EvaluatePeripheralClient(MidiBleProtocol::PendingPeripheralClient const& client)
{
    if (m_peripheralClientPolicy.load() == MidiBleProtocol::PeripheralClientPolicy::AllowAny)
    {
        return MidiBleProtocol::PeripheralClientDecision::Allowed;
    }

    auto const key = MidiBleUtilities::NormalizeClientMatchKey(client.Address);

    auto lock = m_peripheralClientLock.lock_exclusive();

    // The decision already made about the Central on the current link wins over everything else,
    // because it is the most recent thing a person said and it may have been "once".
    if (!client.BluetoothDeviceId.empty() && client.BluetoothDeviceId == m_linkDecisionClientDeviceId)
    {
        return m_linkDecisionApproved ?
            MidiBleProtocol::PeripheralClientDecision::Allowed :
            MidiBleProtocol::PeripheralClientDecision::Denied;
    }

    // A Central with no resolvable address cannot be identified, let alone remembered, so it is
    // always put in front of a person.
    if (!key.empty())
    {
        if (m_deniedPeripheralClients.find(key) != m_deniedPeripheralClients.end())
        {
            return MidiBleProtocol::PeripheralClientDecision::Denied;
        }

        if (m_allowedPeripheralClients.find(key) != m_allowedPeripheralClients.end())
        {
            return MidiBleProtocol::PeripheralClientDecision::Allowed;
        }

        if (auto session = m_sessionPeripheralClientDecisions.find(key);
            session != m_sessionPeripheralClientDecisions.end())
        {
            return session->second ?
                MidiBleProtocol::PeripheralClientDecision::Allowed :
                MidiBleProtocol::PeripheralClientDecision::Denied;
        }
    }

    m_pendingPeripheralClient = client;
    m_hasPendingPeripheralClient = true;

    return MidiBleProtocol::PeripheralClientDecision::Pending;
}


_Use_decl_annotations_
bool
TransportState::TryGetPendingPeripheralClient(MidiBleProtocol::PendingPeripheralClient& client)
{
    auto lock = m_peripheralClientLock.lock_exclusive();

    if (!m_hasPendingPeripheralClient)
    {
        return false;
    }

    client = m_pendingPeripheralClient;

    return true;
}


void
TransportState::ClearPendingPeripheralClient()
{
    auto lock = m_peripheralClientLock.lock_exclusive();

    m_pendingPeripheralClient = {};
    m_hasPendingPeripheralClient = false;
}


void
TransportState::ClearPeripheralClientLinkDecision()
{
    auto lock = m_peripheralClientLock.lock_exclusive();

    m_linkDecisionClientDeviceId.clear();
    m_linkDecisionApproved = false;
}


_Use_decl_annotations_
uint32_t
TransportState::ApplyPeripheralClientDecision(
    std::wstring const& address,
    bool const approve,
    MidiBleProtocol::ApprovalScope const scope,
    bool& shouldPersist,
    MidiBleProtocol::PeripheralClientIdentity& identity)
{
    shouldPersist = false;
    identity = {};

    auto const key = MidiBleUtilities::NormalizeClientMatchKey(address);

    auto lock = m_peripheralClientLock.lock_exclusive();

    if (!m_hasPendingPeripheralClient)
    {
        return BLUETOOTH_MIDI_ERROR_CODE_CLIENT_NOT_PENDING;
    }

    // The decision has to name the Central which is actually waiting. Without this an approval
    // raced against a device swapping out would land on whoever connected in the meantime.
    if (key.empty() || MidiBleUtilities::NormalizeClientMatchKey(m_pendingPeripheralClient.Address) != key)
    {
        return BLUETOOTH_MIDI_ERROR_CODE_CLIENT_IDENTITY_MISMATCH;
    }

    identity.Address = m_pendingPeripheralClient.Address;
    identity.Name = m_pendingPeripheralClient.Name;

    // A rotating address cannot identify this device again, so recording a permanent decision
    // about it would be a permission which quietly stops matching. Refused rather than accepted
    // and ignored, so a caller which offers "always" anyway is told why it cannot have it.
    if (scope == MidiBleProtocol::ApprovalScope::Always && !m_pendingPeripheralClient.IsRememberable)
    {
        return BLUETOOTH_MIDI_ERROR_CODE_ADDRESS_NOT_REMEMBERABLE;
    }

    switch (scope)
    {
    case MidiBleProtocol::ApprovalScope::Always:
        if (approve)
        {
            m_allowedPeripheralClients[key] = identity;
            m_deniedPeripheralClients.erase(key);
        }
        else
        {
            m_deniedPeripheralClients[key] = identity;
            m_allowedPeripheralClients.erase(key);
        }

        shouldPersist = true;
        break;

    case MidiBleProtocol::ApprovalScope::UntilRestart:
        m_sessionPeripheralClientDecisions[key] = approve;
        break;

    default:
        // "once" is recorded against this link only, so the same device connecting again asks
        // again.
        break;
    }

    m_linkDecisionClientDeviceId = m_pendingPeripheralClient.BluetoothDeviceId;
    m_linkDecisionApproved = approve;

    m_pendingPeripheralClient = {};
    m_hasPendingPeripheralClient = false;

    return 0;
}


_Use_decl_annotations_
bool
TransportState::ForgetPeripheralClient(std::wstring const& address)
{
    auto const key = MidiBleUtilities::NormalizeClientMatchKey(address);

    if (key.empty())
    {
        return false;
    }

    auto lock = m_peripheralClientLock.lock_exclusive();

    auto const removed =
        m_allowedPeripheralClients.erase(key) + m_deniedPeripheralClients.erase(key);

    m_sessionPeripheralClientDecisions.erase(key);

    return removed > 0;
}
