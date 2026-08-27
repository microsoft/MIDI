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
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2BluetoothMidiEndpointManager>(&m_endpointManager));

    return S_OK;
}

HRESULT
TransportState::ConstructConfigurationManager()
{
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2BluetoothMidiConfigurationManager>(&m_configurationManager));

    return S_OK;
}


_Use_decl_annotations_
HRESULT
TransportState::AddConnection(std::shared_ptr<MidiBleConnection> connection)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, connection);
    RETURN_HR_IF(E_INVALIDARG, connection->DeviceId().empty());

    auto lock = std::scoped_lock{ m_connectionsLock };

    m_connections.insert_or_assign(connection->DeviceId(), connection);

    return S_OK;
}


std::shared_ptr<MidiBlePeripheral>
TransportState::GetPeripheral()
{
    auto lock = std::scoped_lock{ m_peripheralLock };

    return m_peripheral;
}


_Use_decl_annotations_
HRESULT
TransportState::StartPeripheral(MidiBleProtocol::Protocol const protocol)
{
    auto lock = std::scoped_lock{ m_peripheralLock };

    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED), m_peripheral != nullptr && m_peripheral->IsRunning());

    auto peripheral = std::make_shared<MidiBlePeripheral>();
    RETURN_IF_NULL_ALLOC(peripheral);

    RETURN_IF_FAILED(peripheral->Start(protocol));

    m_peripheral = peripheral;

    return S_OK;
}


HRESULT
TransportState::StopPeripheral()
{
    std::shared_ptr<MidiBlePeripheral> peripheral{ nullptr };

    {
        auto lock = std::scoped_lock{ m_peripheralLock };

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
        auto lock = std::scoped_lock{ m_connectionsLock };

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
        auto lock = std::scoped_lock{ m_connectionsLock };

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
    auto lock = std::scoped_lock{ m_connectionsLock };

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

    auto lock = std::scoped_lock{ m_connectionsLock };

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
    auto lock = std::scoped_lock{ m_connectionsLock };

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

    auto lock = std::scoped_lock{ m_configuredDeviceIdsLock };

    if (std::find(m_configuredDeviceIds.begin(), m_configuredDeviceIds.end(), deviceId) == m_configuredDeviceIds.end())
    {
        m_configuredDeviceIds.push_back(deviceId);
    }
}


std::vector<winrt::hstring>
TransportState::TakeConfiguredDeviceIds()
{
    auto lock = std::scoped_lock{ m_configuredDeviceIdsLock };

    auto ids = std::move(m_configuredDeviceIds);
    m_configuredDeviceIds.clear();

    return ids;
}


_Use_decl_annotations_
void
TransportState::SetConfiguredPeripheralProtocol(MidiBleProtocol::Protocol const protocol)
{
    auto lock = std::scoped_lock{ m_configuredDeviceIdsLock };

    m_configuredPeripheralProtocol = protocol;
}


MidiBleProtocol::Protocol
TransportState::TakeConfiguredPeripheralProtocol()
{
    auto lock = std::scoped_lock{ m_configuredDeviceIdsLock };

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
TransportState::SetRadioCapabilities(MidiBleProtocol::RadioCapabilities const& capabilities)
{
    auto lock = std::scoped_lock{ m_radioCapabilitiesLock };

    m_radioCapabilities = capabilities;
}


MidiBleProtocol::RadioCapabilities
TransportState::GetRadioCapabilities()
{
    auto lock = std::scoped_lock{ m_radioCapabilitiesLock };

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
    auto lock = std::scoped_lock{ m_peripheralClientLock };

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
    auto lock = std::scoped_lock{ m_peripheralClientLock };

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

    auto lock = std::scoped_lock{ m_peripheralClientLock };

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
    auto lock = std::scoped_lock{ m_peripheralClientLock };

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
    auto lock = std::scoped_lock{ m_peripheralClientLock };

    m_pendingPeripheralClient = {};
    m_hasPendingPeripheralClient = false;
}


void
TransportState::ClearPeripheralClientLinkDecision()
{
    auto lock = std::scoped_lock{ m_peripheralClientLock };

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

    auto lock = std::scoped_lock{ m_peripheralClientLock };

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

    auto lock = std::scoped_lock{ m_peripheralClientLock };

    auto const removed =
        m_allowedPeripheralClients.erase(key) + m_deniedPeripheralClients.erase(key);

    m_sessionPeripheralClientDecisions.erase(key);

    return removed > 0;
}




//_Use_decl_annotations_
//HRESULT 
//TransportState::AddHost(
//    std::shared_ptr<MidiNetworkHost> host)
//{
//    RETURN_HR_IF_NULL(E_INVALIDARG, host);
//
//    m_hosts.push_back(host);
//
//    return S_OK;
//}

//_Use_decl_annotations_
//HRESULT
//TransportState::AddPendingHostDefinition(
//    std::shared_ptr<MidiNetworkHostDefinition> hostDefinition)
//{
//    RETURN_HR_IF_NULL(E_INVALIDARG, hostDefinition);
//
//    m_pendingHostDefinitions.push_back(hostDefinition);
//
//    return S_OK;
//}

//_Use_decl_annotations_
//HRESULT
//TransportState::AddClient(
//    std::shared_ptr<MidiNetworkClient> client)
//{
//    RETURN_HR_IF_NULL(E_INVALIDARG, client);
//
//    m_clients.push_back(client);
//
//    return S_OK;
//}


//_Use_decl_annotations_
//HRESULT
//TransportState::AddPendingClientDefinition(
//    std::shared_ptr<MidiNetworkClientDefinition> clientDefinition)
//{
//    RETURN_HR_IF_NULL(E_INVALIDARG, clientDefinition);
//
//    m_pendingClientDefinitions.push_back(clientDefinition);
//
//    return S_OK;
//}



//_Use_decl_annotations_
//HRESULT
//TransportState::AssociateMidiEndpointWithConnection(
//    _In_ std::wstring endpointDeviceInterfaceId,
//    _In_ winrt::Windows::Networking::HostName const& remoteHostName,
//    _In_ winrt::hstring const& remotePort)
//{
//    auto cleanId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);
//
//    // find the connection and then associate it here
//
//    auto connection = GetNetworkConnection(remoteHostName, remotePort);
//
//    if (connection != nullptr)
//    {
//        m_sessionConnections.insert_or_assign(cleanId, connection);
//
//        return S_OK;
//    }
//    else
//    {
//        return E_FAIL;
//    }
//
//}

//_Use_decl_annotations_
//HRESULT
//TransportState::DisassociateMidiEndpointFromConnection(
//    std::wstring endpointDeviceInterfaceId)
//{
//    RETURN_HR_IF(E_INVALIDARG, endpointDeviceInterfaceId.empty());
//    auto cleanId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);
//    RETURN_HR_IF(E_INVALIDARG, cleanId.empty());
//
//    if (auto session = m_sessionConnections.find(cleanId); session != m_sessionConnections.end())
//    {
//        //m_sessionConnections.find(cleanId)->second.reset();
//        m_sessionConnections.erase(cleanId);
//    }
//    else
//    {
//        RETURN_IF_FAILED(E_NOTFOUND);
//    }
//
//    return S_OK;
//}

//_Use_decl_annotations_
//std::shared_ptr<MidiNetworkConnection> 
//TransportState::GetSessionConnection(_In_ std::wstring endpointDeviceInterfaceId)
//{
//    auto cleanId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);
//
//    if (auto entry = m_sessionConnections.find(cleanId); entry != m_sessionConnections.end())
//    {
//        return entry->second;
//    }
//
//    return nullptr;
//}





//_Use_decl_annotations_
//bool 
//TransportState::NetworkConnectionExists(
//    winrt::Windows::Networking::HostName const& remoteHostName, 
//    winrt::hstring const& port)
//{
//    auto key = CreateNetworkConnectionMapKey(remoteHostName, port);
//
//    return m_networkConnections.find(key) != m_networkConnections.end();
//}


//_Use_decl_annotations_
//HRESULT 
//TransportState::RemoveNetworkConnection(
//    winrt::Windows::Networking::HostName const& remoteHostName, 
//    winrt::hstring const& remotePort)
//{
//    if (NetworkConnectionExists(remoteHostName, remotePort))
//    {
//        auto entry = m_networkConnections.find(CreateNetworkConnectionMapKey(remoteHostName, remotePort));
//
//        LOG_IF_FAILED(entry->second->Shutdown());
//
//        m_networkConnections.erase(CreateNetworkConnectionMapKey(remoteHostName, remotePort));
//    }
//
//    return S_OK;
//}

//_Use_decl_annotations_
//std::shared_ptr<MidiNetworkConnection> 
//TransportState::GetNetworkConnection(
//    winrt::Windows::Networking::HostName const& remoteHostName, 
//    winrt::hstring const& remotePort)
//{
//    auto key = CreateNetworkConnectionMapKey(remoteHostName, remotePort);
//
//    if (NetworkConnectionExists(remoteHostName, remotePort))
//    {
//        return m_networkConnections.find(key)->second;
//    }
//
//    return nullptr;
//}


//_Use_decl_annotations_
//HRESULT
//TransportState::AddNetworkConnection(
//    winrt::Windows::Networking::HostName const& remoteHostName,
//    winrt::hstring const& remotePort, 
//    std::shared_ptr<MidiNetworkConnection> connection
//)
//{
//    auto key = CreateNetworkConnectionMapKey(remoteHostName, remotePort);
//
//    m_networkConnections.insert_or_assign(key, connection);
//
//    return S_OK;
//}
