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
    auto lock = std::scoped_lock{ m_managersLock };
    return m_endpointManager;
}

wil::com_ptr<CMidi2RtpMidiConfigurationManager>
TransportState::GetConfigurationManager()
{
    auto lock = std::scoped_lock{ m_managersLock };
    return m_configurationManager;
}

HRESULT
TransportState::ConstructEndpointManager()
{
    auto lock = std::scoped_lock{ m_managersLock };

    if (m_endpointManager == nullptr)
    {
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2RtpMidiEndpointManager>(&m_endpointManager));
    }

    return S_OK;
}

HRESULT
TransportState::ConstructConfigurationManager()
{
    auto lock = std::scoped_lock{ m_managersLock };

    if (m_configurationManager == nullptr)
    {
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2RtpMidiConfigurationManager>(&m_configurationManager));
    }

    return S_OK;
}


_Use_decl_annotations_
void
TransportState::SetHostDefinition(RtpMidiHostDefinition const& definition)
{
    {
        auto lock = std::scoped_lock{ m_definitionsLock };
        m_hosts.insert_or_assign(definition.EntryId, definition);
    }

    // before the worker can start the host, so no invitation meets a host without a policy
    m_approvals.SetPolicy(definition.EntryId, definition.RemoteClientPolicy);
}

_Use_decl_annotations_
bool
TransportState::RemoveHostDefinition(GUID const& entryId)
{
    bool removed{ false };

    {
        auto lock = std::scoped_lock{ m_definitionsLock };
        removed = m_hosts.erase(entryId) > 0;
    }

    if (removed) m_approvals.RemoveHost(entryId);

    return removed;
}

_Use_decl_annotations_
bool
TransportState::TryGetHostDefinition(GUID const& entryId, RtpMidiHostDefinition& definition)
{
    auto lock = std::scoped_lock{ m_definitionsLock };

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
    auto lock = std::scoped_lock{ m_definitionsLock };

    auto const it = m_hosts.find(entryId);
    if (it == m_hosts.end()) return false;

    it->second.Enabled = enabled;
    return true;
}

std::vector<RtpMidiHostDefinition>
TransportState::GetHostDefinitions()
{
    auto lock = std::scoped_lock{ m_definitionsLock };

    std::vector<RtpMidiHostDefinition> definitions;
    for (auto const& entry : m_hosts) definitions.push_back(entry.second);

    return definitions;
}


_Use_decl_annotations_
void
TransportState::SetClientDefinition(RtpMidiClientDefinition const& definition)
{
    auto lock = std::scoped_lock{ m_definitionsLock };
    m_clients.insert_or_assign(definition.EntryId, definition);
}

_Use_decl_annotations_
bool
TransportState::RemoveClientDefinition(GUID const& entryId)
{
    auto lock = std::scoped_lock{ m_definitionsLock };
    return m_clients.erase(entryId) > 0;
}

_Use_decl_annotations_
bool
TransportState::TryGetClientDefinition(GUID const& entryId, RtpMidiClientDefinition& definition)
{
    auto lock = std::scoped_lock{ m_definitionsLock };

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
    auto lock = std::scoped_lock{ m_definitionsLock };

    std::vector<RtpMidiClientDefinition> definitions;
    for (auto const& entry : m_clients) definitions.push_back(entry.second);

    return definitions;
}
