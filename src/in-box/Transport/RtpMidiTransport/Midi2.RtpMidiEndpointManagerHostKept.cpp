// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Endpoints a host keeps for its remotes while they are away.
//
// Some customers want Windows to be the host for a remote device, so the device invites this PC
// rather than the other way around. A remote which has its own settings on the host can keep its
// endpoint, the same way a client entry can: the endpoint stays when the remote leaves, drops what
// apps send to it while the remote is away, and is handed the remote's next connection.
//
// A remote that connects to a host is known only by the name it sends, so the customer opts in for
// each remote by name. Only the endpoint whose id comes from that name is kept. A second remote
// sending a name already in use gets an endpoint of its own, which is never kept.
// ============================================================================

#include "pch.h"

namespace
{
    std::wstring LowercaseCopy(_In_ std::wstring const& text)
    {
        std::wstring lower{ text };
        if (!lower.empty()) CharLowerBuffW(lower.data(), static_cast<DWORD>(lower.size()));

        return lower;
    }

    bool SameRemoteName(_In_ std::wstring const& first, _In_ std::wstring const& second)
    {
        return CompareStringOrdinal(
            first.c_str(), static_cast<int>(first.size()),
            second.c_str(), static_cast<int>(second.size()),
            TRUE) == CSTR_EQUAL;
    }
}


// The same identity a connection's endpoint has always had, so an endpoint made for a connection
// and one made before the remote connects are interchangeable
_Use_decl_annotations_
std::wstring
CMidi2RtpMidiEndpointManager::HostRemoteIdentity(GUID const& hostId, std::wstring const& remoteName)
{
    return internal::GuidToString(hostId) + L"|" + remoteName;
}

// An enabled host, and a remote with its own settings there which keep the endpoint
_Use_decl_annotations_
bool
CMidi2RtpMidiEndpointManager::HostKeepsEndpointFor(GUID const& hostId, std::wstring const& remoteName)
{
    if (remoteName.empty()) return false;

    RtpMidiHostDefinition host{};
    if (!TransportState::Current().TryGetHostDefinition(hostId, host) || !host.Enabled) return false;

    auto const settings = TransportState::Current().FindRemoteClientSettings(hostId, remoteName);

    return settings.has_value() && settings->KeepsEndpoint();
}

// The one decision for a host's remote endpoint, wherever it is made. The customer ending the
// connection ends a timed endpoint with it. "Always" stays: it would only be made again at once.
_Use_decl_annotations_
bool
CMidi2RtpMidiEndpointManager::ShouldKeepHostEndpoint(std::shared_ptr<RtpMidiEndpoint> const& endpoint)
{
    if (endpoint == nullptr || !endpoint->IsHostRemoteEndpoint()) return false;
    if (!HostKeepsEndpointFor(endpoint->EntryId(), endpoint->HostRemoteName())) return false;
    if (!endpoint->ShouldReleaseWhenConnectionEnds()) return true;

    auto const settings = TransportState::Current().FindRemoteClientSettings(endpoint->EntryId(), endpoint->HostRemoteName());

    return settings.has_value() && settings->OfflineRetentionSeconds == MIDI_RTP_OFFLINE_RETENTION_KEEP_ALWAYS;
}

_Use_decl_annotations_
std::shared_ptr<RtpMidiEndpoint>
CMidi2RtpMidiEndpointManager::FindHostRemoteEndpoint(GUID const& hostId, std::wstring const& remoteName)
{
    auto lock = m_createdEndpointsLock.lock_exclusive();

    for (auto const& endpoint : m_createdEndpoints)
    {
        if (endpoint->IsHostRemoteEndpoint() && endpoint->EntryId() == hostId && SameRemoteName(endpoint->HostRemoteName(), remoteName))
        {
            return endpoint;
        }
    }

    return nullptr;
}


// Under the list's lock, which attaching takes too, so a remote coming back at this moment is
// never left marked away
_Use_decl_annotations_
void
CMidi2RtpMidiEndpointManager::MarkHostRemoteEndpointAway(std::shared_ptr<RtpMidiEndpoint> const& endpoint)
{
    if (endpoint == nullptr || !endpoint->IsHostRemoteEndpoint()) return;

    auto lock = m_createdEndpointsLock.lock_exclusive();

    if (endpoint->Connection() == nullptr) TransportState::Current().Approvals().RemoteEndpointAway(endpoint->EntryId(), endpoint->HostRemoteName());
}


// Unlisted and removed only while nothing is attached to it. Attaching happens under the same
// lock, so a remote coming back at this moment either gets the endpoint or a new one, never one
// being removed.
_Use_decl_annotations_
bool
CMidi2RtpMidiEndpointManager::TryRemoveDetachedEndpoint(std::shared_ptr<RtpMidiEndpoint> const& endpoint)
{
    if (endpoint == nullptr) return false;

    {
        auto lock = m_createdEndpointsLock.lock_exclusive();

        if (endpoint->Connection() != nullptr) return false;

        std::erase(m_createdEndpoints, endpoint);
    }

    LOG_IF_FAILED(RemoveRtpEndpoint(endpoint));

    return true;
}


_Use_decl_annotations_
HRESULT
CMidi2RtpMidiEndpointManager::CreateHostKeptEndpoint(GUID const& hostId, std::wstring const& remoteName)
{
    RETURN_HR_IF(E_INVALIDARG, remoteName.empty());

    auto const identity = HostRemoteIdentity(hostId, remoteName);

    // named after the remote, as its connection's endpoint is
    auto const endpoint = std::make_shared<RtpMidiEndpoint>(
        hostId,
        BuildInstanceId(remoteName, identity),
        remoteName,
        remoteName);

    endpoint->SetKept(true);

    auto const hr = ActivateRtpEndpoint(
        endpoint,
        RtpMidiText::StableHash(identity),
        internal::ResourceGetWString(IDS_RTP_ENDPOINT_DESCRIPTION_HOST));

    // kept for a remote which has not connected yet, so the first one to use the name gets it
    if (SUCCEEDED(hr)) MarkHostRemoteEndpointAway(endpoint);

    return hr;
}


// The node's delivery thread. Attached before anything else is delivered, so an app which kept the
// endpoint open hears the remote's first message. Only a remote at an address the endpoint was used
// from, or one the customer approved, gets it. Any other remote using the name gets one of its own.
_Use_decl_annotations_
bool
CMidi2RtpMidiEndpointManager::AttachToKeptHostEndpoint(std::shared_ptr<RtpMidiConnection> const& connection)
{
    if (connection == nullptr || !connection->ThisPcIsHost() || connection->RemoteName().empty()) return false;

    auto& approvals = TransportState::Current().Approvals();
    auto const remoteAddress = RtpMidiNet::AddressToString(connection->RemoteControl());

    std::shared_ptr<RtpMidiEndpoint> attached{ nullptr };

    {
        auto lock = m_createdEndpointsLock.lock_exclusive();

        for (auto const& endpoint : m_createdEndpoints)
        {
            if (!endpoint->IsHostRemoteEndpoint() || !endpoint->IsKept() || endpoint->EntryId() != connection->EntryId()) continue;
            if (!SameRemoteName(endpoint->HostRemoteName(), connection->RemoteName())) continue;

            // another remote sending the same name has it. This one gets an endpoint of its own.
            if (endpoint->Connection() != nullptr) return false;

            if (!approvals.MayTakeRemoteEndpoint(connection->EntryId(), endpoint->HostRemoteName(), remoteAddress)) return false;

            // a connection which ended while this waited needs nothing more
            if (!endpoint->Attach(connection)) return true;

            approvals.RemoteEndpointConnected(connection->EntryId(), endpoint->HostRemoteName(), remoteAddress);

            attached = endpoint;
            break;
        }
    }

    if (attached == nullptr) return false;

    TraceLoggingWrite(
        MidiRtpMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"A remote came back to the endpoint its host kept for it", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(attached->NormalizedInstanceId().c_str(), "instance id")
    );

    return true;
}


// The node's delivery thread. The settings are read now rather than when the remote connected, so
// a change the customer made while it was connected applies. The remote's held notes were already
// ended and any open SysEx closed on the way here.
_Use_decl_annotations_
bool
CMidi2RtpMidiEndpointManager::DetachFromKeptHostEndpoint(std::shared_ptr<RtpMidiConnection> const& connection)
{
    if (connection == nullptr || !connection->ThisPcIsHost()) return false;

    auto const endpoint = FindEndpointForConnection(connection.get());
    if (endpoint == nullptr || !endpoint->IsHostRemoteEndpoint()) return false;

    bool const keep = ShouldKeepHostEndpoint(endpoint);

    // before the connection goes, so nothing sent meanwhile fails and stalls the app
    endpoint->SetKept(keep);

    if (!keep) return false;

    // From now on a remote of this name from an address it never used is asked about first
    if (endpoint->DetachIfCurrent(connection.get())) MarkHostRemoteEndpointAway(endpoint);

    TraceLoggingWrite(
        MidiRtpMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Keeping the endpoint of a remote which left its host", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpoint->NormalizedInstanceId().c_str(), "instance id")
    );

    return true;
}


// While a remote is connected its endpoint follows the settings, so a send which meets the
// connection ending is dropped rather than failed when the endpoint is about to be kept
_Use_decl_annotations_
void
CMidi2RtpMidiEndpointManager::RefreshHostEndpointsKept(GUID const& hostId)
{
    std::vector<std::shared_ptr<RtpMidiEndpoint>> connected;

    {
        auto lock = m_createdEndpointsLock.lock_exclusive();

        for (auto const& endpoint : m_createdEndpoints)
        {
            if (endpoint->IsHostRemoteEndpoint() && endpoint->EntryId() == hostId && endpoint->Connection() != nullptr)
            {
                connected.push_back(endpoint);
            }
        }
    }

    // outside the lock: the definitions lock is taken for each
    for (auto const& endpoint : connected)
    {
        endpoint->SetKept(ShouldKeepHostEndpoint(endpoint));
    }
}


// The customer ended this connection, so its endpoint goes with it like any other
_Use_decl_annotations_
void
CMidi2RtpMidiEndpointManager::ReleaseHostEndpointWhenConnectionEnds(GUID const& hostId, uint32_t const connectionId)
{
    std::shared_ptr<RtpMidiNode> node{ nullptr };

    {
        auto lock = m_runtimeLock.lock_exclusive();
        if (auto const host = m_hosts.find(hostId); host != m_hosts.end()) node = host->second.Node;
    }

    if (node == nullptr) return;

    for (auto const& connection : node->Connections())
    {
        if (connection == nullptr || connection->ParticipantId() != connectionId) continue;

        if (auto const endpoint = FindEndpointForConnection(connection.get())) endpoint->ReleaseWhenConnectionEnds();
    }
}


// The worker thread, and Initialize before the worker starts
_Use_decl_annotations_
void
CMidi2RtpMidiEndpointManager::ReconcileHostKeptEndpoints(uint64_t const now)
{
    try
    {
        auto const hosts = TransportState::Current().GetHostDefinitions();

        std::vector<std::shared_ptr<RtpMidiEndpoint>> hostEndpoints;

        {
            auto lock = m_createdEndpointsLock.lock_exclusive();

            for (auto const& endpoint : m_createdEndpoints)
            {
                if (endpoint->IsHostRemoteEndpoint()) hostEndpoints.push_back(endpoint);
            }
        }

        for (auto const& endpoint : hostEndpoints)
        {
            if (endpoint->Connection() != nullptr)
            {
                endpoint->SetKept(ShouldKeepHostEndpoint(endpoint));
                continue;
            }

            // Detached, so it is either kept or left over. Another thread can change the kept flag
            // around a remote leaving, so it is decided again here rather than trusted: one the
            // settings keep is marked kept, and anything else is removed rather than left behind.
            // Gone with its host, with its settings ("Use the host's settings"), when they no
            // longer keep it, and once it has been away for as long as they say. The time counts
            // from when the remote left, so a shorter time set while it is away can end it at once.
            auto const settings = TransportState::Current().FindRemoteClientSettings(endpoint->EntryId(), endpoint->HostRemoteName());

            bool const keeps = ShouldKeepHostEndpoint(endpoint);

            // read now: the remote can leave after this pass began, and an earlier tick would wrap
            auto const disconnectedSince = endpoint->DisconnectedSinceTick();
            auto const currentTick = GetTickCount64();

            bool const expired =
                keeps &&
                settings.has_value() &&
                settings->OfflineRetentionSeconds > 0 &&
                currentTick > disconnectedSince &&
                currentTick - disconnectedSince >= static_cast<uint64_t>(settings->OfflineRetentionSeconds) * 1000;

            if (keeps && !expired)
            {
                endpoint->SetKept(true);
                MarkHostRemoteEndpointAway(endpoint);
                continue;
            }

            if (TryRemoveDetachedEndpoint(endpoint))
            {
                TraceLoggingWrite(
                    MidiRtpMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(expired ?
                        L"Removing a host's kept endpoint whose remote has been away past its offline retention" :
                        L"Removing a host's kept endpoint which its host or the remote's settings no longer keep", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(endpoint->NormalizedInstanceId().c_str(), "instance id")
                );
            }
        }

        // "always" is there from the start, named after the remote as the customer saved it. A
        // number of seconds counts from a connection ending, so it needs a connection first.
        for (auto const& host : hosts)
        {
            if (!host.Enabled) continue;

            for (auto const& settings : TransportState::Current().GetRemoteClientSettings(host.EntryId))
            {
                if (settings.OfflineRetentionSeconds != MIDI_RTP_OFFLINE_RETENTION_KEEP_ALWAYS) continue;
                if (FindHostRemoteEndpoint(host.EntryId, settings.RemoteName) != nullptr) continue;

                auto const retryKey = internal::GuidToString(host.EntryId) + L"|" + LowercaseCopy(settings.RemoteName);

                if (auto const retry = m_keptHostEndpointRetryTicks.find(retryKey); retry != m_keptHostEndpointRetryTicks.end() && now < retry->second) continue;

                if (SUCCEEDED(LOG_IF_FAILED(CreateHostKeptEndpoint(host.EntryId, settings.RemoteName))))
                {
                    m_keptHostEndpointRetryTicks.erase(retryKey);
                }
                else
                {
                    m_keptHostEndpointRetryTicks[retryKey] = now + MIDI_RTP_KEPT_ENDPOINT_RETRY_INTERVAL_MS;
                }
            }
        }

        // the ones for remotes which no longer need a retry
        std::erase_if(m_keptHostEndpointRetryTicks, [now](auto const& entry) { return now >= entry.second; });
    }
    CATCH_LOG();
}


// One remote's own settings in the host's status. The endpoint id is only there while the remote is
// away and the host is keeping its endpoint.
_Use_decl_annotations_
json::JsonObject
CMidi2RtpMidiEndpointManager::BuildRemoteClientSettingsJson(GUID const& hostId, RtpMidiRemoteClientSettings const& settings)
{
    json::JsonObject item;

    item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_NAME_KEY, json::JsonValue::CreateStringValue(settings.RemoteName));
    item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_SEND_SPEED_LIMIT_KEY, json::JsonValue::CreateNumberValue(static_cast<double>(settings.SendSpeedLimit)));
    item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_SKIP_IN_BETWEEN_VALUES_TO_CATCH_UP_KEY, json::JsonValue::CreateBooleanValue(settings.SkipInBetweenValuesToCatchUp));
    item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_OFFLINE_RETENTION_KEY, json::JsonValue::CreateStringValue(RtpMidiJson::OfflineRetentionToString(settings.OfflineRetentionSeconds)));

    std::wstring keptEndpointDeviceId{};

    if (auto const endpoint = FindHostRemoteEndpoint(hostId, settings.RemoteName); endpoint != nullptr && endpoint->IsKept() && endpoint->Connection() == nullptr)
    {
        keptEndpointDeviceId = endpoint->InterfaceId();
    }

    item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ENDPOINT_DEVICE_ID_KEY, json::JsonValue::CreateStringValue(keptEndpointDeviceId));

    return item;
}
