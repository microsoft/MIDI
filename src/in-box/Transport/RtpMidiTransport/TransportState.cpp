// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Transport-wide state.
// ============================================================================

#include "pch.h"

TransportState&
TransportState::Current()
{
    static TransportState current;
    return current;
}

wil::com_ptr<CMidi2RtpMidiEndpointManager>
TransportState::GetEndpointManager()
{
    auto lock = m_managersLock.lock_exclusive();
    return m_endpointManager;
}

wil::com_ptr<CMidi2RtpMidiConfigurationManager>
TransportState::GetConfigurationManager()
{
    auto lock = m_managersLock.lock_exclusive();
    return m_configurationManager;
}

HRESULT
TransportState::ConstructEndpointManager()
{
    auto lock = m_managersLock.lock_exclusive();

    if (m_endpointManager == nullptr)
    {
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2RtpMidiEndpointManager>(&m_endpointManager));
    }

    return S_OK;
}

HRESULT
TransportState::ConstructConfigurationManager()
{
    auto lock = m_managersLock.lock_exclusive();

    if (m_configurationManager == nullptr)
    {
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2RtpMidiConfigurationManager>(&m_configurationManager));
    }

    return S_OK;
}

void
TransportState::Shutdown() noexcept
{
    wil::com_ptr<CMidi2RtpMidiEndpointManager> endpointManager;
    wil::com_ptr<CMidi2RtpMidiConfigurationManager> configurationManager;

    {
        auto lock = m_managersLock.lock_exclusive();

        endpointManager.swap(m_endpointManager);
        configurationManager.swap(m_configurationManager);
    }

    // Released outside the lock, in case this is the last reference and a destructor runs
    endpointManager.reset();
    configurationManager.reset();

    // its deferred registry write would otherwise run during DLL unload
    m_approvals.Shutdown();
}


// Both steps under the definitions lock, policy first, so the worker never starts a host which has
// no policy yet, and a removal racing a definition can't leave a host whose approvals are gone. The
// approvals lock is only ever taken after this one.
_Use_decl_annotations_
void
TransportState::SetHostDefinition(RtpMidiHostDefinition const& definition)
{
    auto lock = m_definitionsLock.lock_exclusive();

    m_approvals.SetPolicy(definition.EntryId, definition.RemoteClientPolicy);
    m_hosts.insert_or_assign(definition.EntryId, definition);
}

_Use_decl_annotations_
bool
TransportState::RemoveHostDefinition(GUID const& entryId)
{
    auto lock = m_definitionsLock.lock_exclusive();

    bool const removed = m_hosts.erase(entryId) > 0;

    if (removed)
    {
        m_remoteClientSettings.erase(entryId);
        m_approvals.RemoveHost(entryId);
    }

    return removed;
}

_Use_decl_annotations_
void
TransportState::SetRemoteClientSettings(GUID const& hostId, std::vector<RtpMidiRemoteClientSettings> const& settings)
{
    auto lock = m_definitionsLock.lock_exclusive();

    // left over from a host which is gone
    if (m_hosts.find(hostId) == m_hosts.end()) return;

    m_remoteClientSettings.insert_or_assign(hostId, settings);
}

_Use_decl_annotations_
std::vector<RtpMidiRemoteClientSettings>
TransportState::GetRemoteClientSettings(GUID const& hostId)
{
    auto lock = m_definitionsLock.lock_exclusive();

    auto const it = m_remoteClientSettings.find(hostId);

    return it == m_remoteClientSettings.end() ? std::vector<RtpMidiRemoteClientSettings>{} : it->second;
}

_Use_decl_annotations_
std::optional<RtpMidiRemoteClientSettings>
TransportState::FindRemoteClientSettings(GUID const& hostId, std::wstring const& remoteName)
{
    auto lock = m_definitionsLock.lock_exclusive();

    auto const it = m_remoteClientSettings.find(hostId);
    if (it == m_remoteClientSettings.end()) return std::nullopt;

    for (auto const& settings : it->second)
    {
        if (CompareStringOrdinal(
                settings.RemoteName.c_str(), static_cast<int>(settings.RemoteName.size()),
                remoteName.c_str(), static_cast<int>(remoteName.size()),
                TRUE) == CSTR_EQUAL)
        {
            return settings;
        }
    }

    return std::nullopt;
}

_Use_decl_annotations_
bool
TransportState::TryGetHostDefinition(GUID const& entryId, RtpMidiHostDefinition& definition)
{
    auto lock = m_definitionsLock.lock_exclusive();

    auto const it = m_hosts.find(entryId);
    if (it == m_hosts.end())
    {
        definition = RtpMidiHostDefinition{};
        return false;
    }

    definition = it->second;
    return true;
}

_Use_decl_annotations_
bool
TransportState::SetHostEnabled(GUID const& entryId, bool const enabled)
{
    auto lock = m_definitionsLock.lock_exclusive();

    auto const it = m_hosts.find(entryId);
    if (it == m_hosts.end()) return false;

    it->second.Enabled = enabled;
    return true;
}

std::vector<RtpMidiHostDefinition>
TransportState::GetHostDefinitions()
{
    auto lock = m_definitionsLock.lock_exclusive();

    std::vector<RtpMidiHostDefinition> definitions;
    for (auto const& entry : m_hosts) definitions.push_back(entry.second);

    return definitions;
}


_Use_decl_annotations_
void
TransportState::SetClientDefinition(RtpMidiClientDefinition const& definition)
{
    auto lock = m_definitionsLock.lock_exclusive();
    m_clients.insert_or_assign(definition.EntryId, definition);
}

_Use_decl_annotations_
bool
TransportState::RemoveClientDefinition(GUID const& entryId)
{
    auto lock = m_definitionsLock.lock_exclusive();
    return m_clients.erase(entryId) > 0;
}

_Use_decl_annotations_
bool
TransportState::TryGetClientDefinition(GUID const& entryId, RtpMidiClientDefinition& definition)
{
    auto lock = m_definitionsLock.lock_exclusive();

    auto const it = m_clients.find(entryId);
    if (it == m_clients.end())
    {
        definition = RtpMidiClientDefinition{};
        return false;
    }

    definition = it->second;
    return true;
}

std::vector<RtpMidiClientDefinition>
TransportState::GetClientDefinitions()
{
    auto lock = m_definitionsLock.lock_exclusive();

    std::vector<RtpMidiClientDefinition> definitions;
    for (auto const& entry : m_clients) definitions.push_back(entry.second);

    return definitions;
}
