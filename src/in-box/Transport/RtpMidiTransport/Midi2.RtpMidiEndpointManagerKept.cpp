// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Making and removing endpoints, including the ones a client entry keeps while its remote is away.
//
// A WinMM or WinRT MIDI 1.0 app cannot ask whether a device is there. Whether its port exists is
// all it sees, and many DAWs drop a control surface's setup when they start without its port. So a
// client entry can keep its endpoint: made when the transport starts, handed each new connection,
// and dropping what apps send while the remote is away, the way a switched off DIN device would.
// A host's remotes can keep theirs too, each one only when the customer has given it its own
// settings by name. See Midi2.RtpMidiEndpointManagerHostKept.cpp.
// ============================================================================

#include "pch.h"

_Use_decl_annotations_
std::wstring
CMidi2RtpMidiEndpointManager::BuildInstanceId(std::wstring const& readableName, std::wstring const& identity)
{
    auto readable = internal::RemoveInvalidSWDUniqueIdCharacters(readableName);
    if (readable.size() > MIDI_RTP_ENDPOINT_INSTANCE_ID_NAME_MAX_CHARS) readable.resize(MIDI_RTP_ENDPOINT_INSTANCE_ID_NAME_MAX_CHARS);

    return std::wstring{ MIDI_RTP_ENDPOINT_INSTANCE_ID_PREFIX } + readable + L"_" + RtpMidiText::StableHash(identity);
}

// The same id the entry's connections have always had, so a kept endpoint and the one made for a
// connection are interchangeable
_Use_decl_annotations_
std::wstring
CMidi2RtpMidiEndpointManager::KeptEndpointInstanceId(RtpMidiClientDefinition const& definition)
{
    return BuildInstanceId(
        definition.IsDirect() ? definition.RemoteAddress : definition.RemoteServiceInstanceName,
        internal::GuidToString(definition.EntryId));
}

// From the entry, never from the remote, so the port's name is the same before and after it connects
_Use_decl_annotations_
std::wstring
CMidi2RtpMidiEndpointManager::KeptEndpointName(RtpMidiClientDefinition const& definition)
{
    if (!definition.CustomEndpointName.empty()) return definition.CustomEndpointName;

    return definition.IsDirect() ? definition.RemoteAddress : definition.RemoteServiceInstanceName;
}


_Use_decl_annotations_
HRESULT
CMidi2RtpMidiEndpointManager::ActivateRtpEndpoint(
    std::shared_ptr<RtpMidiEndpoint> const& endpoint,
    std::wstring const& uniqueIdentifier,
    std::wstring const& endpointDescription)
{
    try
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, endpoint);
        RETURN_HR_IF_NULL(E_UNEXPECTED, m_midiDeviceManager);
        RETURN_HR_IF(E_UNEXPECTED, m_parentDeviceId.empty());

        auto const& instanceId = endpoint->InstanceId();
        auto const& endpointName = endpoint->EndpointName();

        std::wstring transportCode{ TRANSPORT_CODE };
        std::vector<DEVPROPERTY> interfaceDevProperties;

        // looked up before the device node is created, so an endpoint is never published under the
        // wrong name and renamed a moment later
        WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria matchCriteria{};
        matchCriteria.DeviceInstanceId = winrt::hstring{ instanceId };
        matchCriteria.TransportSuppliedEndpointName = winrt::hstring{ endpointName };

        std::wstring customName{};
        std::wstring customDescription{};
        std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties> customProperties{ nullptr };

        if (auto configurationManager = TransportState::Current().GetConfigurationManager())
        {
            customProperties = configurationManager->CustomPropertiesCache()->GetProperties(matchCriteria);

            if (customProperties != nullptr)
            {
                if (!customProperties->Name.empty()) customName = customProperties->Name;
                if (!customProperties->Description.empty()) customDescription = customProperties->Description;

                customProperties->WriteNonCommonProperties(interfaceDevProperties);
            }
        }

        std::wstring const friendlyName = customName.empty() ? endpointName : customName;

        DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;

        // own the memory the group terminal block and name table properties point at
        std::vector<std::byte> groupTerminalBlockData{};
        WindowsMidiServicesNamingLib::MidiEndpointNameTable nameTable{};

        // A byte stream endpoint has no endpoint discovery to run, so the service is told it is done
        // and creates the MIDI 1.0 ports straight away
        interfaceDevProperties.push_back({ { PKEY_MIDI_EndpointDiscoveryProcessComplete, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), (PVOID)&devPropTrue });

        LOG_IF_FAILED(RtpMidiEndpointProperties::BuildMidi1PortProperties(
            friendlyName,
            customProperties,
            groupTerminalBlockData,
            nameTable,
            interfaceDevProperties));

        MIDIENDPOINTCOMMONPROPERTIES commonProperties{};
        commonProperties.TransportId = TRANSPORT_LAYER_GUID;
        commonProperties.EndpointDeviceType = MidiEndpointDeviceType::MidiEndpointDeviceType_Normal;
        commonProperties.FriendlyName = friendlyName.c_str();
        commonProperties.TransportCode = transportCode.c_str();
        commonProperties.EndpointName = endpointName.c_str();
        commonProperties.EndpointDescription = endpointDescription.c_str();
        commonProperties.CustomEndpointName = customName.empty() ? nullptr : customName.c_str();
        commonProperties.CustomEndpointDescription = customDescription.empty() ? nullptr : customDescription.c_str();
        commonProperties.UniqueIdentifier = uniqueIdentifier.c_str();
        commonProperties.SupportedDataFormats = MidiDataFormats::MidiDataFormats_UMP;
        commonProperties.NativeDataFormat = MidiDataFormats::MidiDataFormats_ByteStream;

        UINT32 capabilities{ 0 };
        capabilities |= MidiEndpointCapabilities_SupportsMidi1Protocol;
        capabilities |= MidiEndpointCapabilities_SupportsMultiClient;
        capabilities |= MidiEndpointCapabilities_GenerateIncomingTimestamps;
        commonProperties.Capabilities = (MidiEndpointCapabilities)capabilities;

        SW_DEVICE_CREATE_INFO createInfo{};
        createInfo.cbSize = sizeof(createInfo);
        createInfo.pszInstanceId = instanceId.c_str();
        createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
        createInfo.pszDeviceDescription = friendlyName.c_str();

        wil::unique_cotaskmem_string newDeviceInterfaceId;

        // The service makes the endpoint visible, and builds its MIDI 1.0 ports, before activation
        // returns. An app can open it in that time, and finds it here by its instance id.
        {
            auto lock = m_createdEndpointsLock.lock_exclusive();
            m_createdEndpoints.push_back(endpoint);
        }

        auto unlistOnFailure = wil::scope_exit([&]()
            {
                {
                    auto lock = m_createdEndpointsLock.lock_exclusive();
                    std::erase(m_createdEndpoints, endpoint);
                }

                if (auto const connection = endpoint->Connection()) endpoint->DetachIfCurrent(connection.get());
            });

        auto const activate = [&]()
            {
                newDeviceInterfaceId.reset();

                return m_midiDeviceManager->ActivateEndpoint(
                    m_parentDeviceId.c_str(),
                    false,                                          // when false, WinMM MIDI 1.0 ports are created as well
                    MidiFlow::MidiFlowBidirectional,
                    &commonProperties,
                    static_cast<ULONG>(interfaceDevProperties.size()),
                    static_cast<ULONG>(0),
                    interfaceDevProperties.size() > 0 ? interfaceDevProperties.data() : nullptr,
                    nullptr,
                    &createInfo,
                    &newDeviceInterfaceId);
            };

        auto activateHR = activate();

        // S_FALSE means the instance id is already active. If no other endpoint listed here has
        // it, it was left in the service by a removal which failed, and nothing uses it.
        if (activateHR == S_FALSE && !IsInstanceIdListedElsewhere(endpoint))
        {
            if (SUCCEEDED(LOG_IF_FAILED(m_midiDeviceManager->RemoveEndpoint(endpoint->NormalizedInstanceId().c_str()))))
            {
                {
                    auto lock = m_createdEndpointsLock.lock_exclusive();
                    std::erase_if(m_endpointsToRemove, [&](FailedRemoval const& failed) { return failed.NormalizedInstanceId == endpoint->NormalizedInstanceId(); });
                }

                activateHR = activate();
            }
        }

        RETURN_IF_FAILED(activateHR);

        // S_FALSE means the instance id is already active and no interface id came back
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_DEVICE_ALREADY_ATTACHED), activateHR == S_FALSE || newDeviceInterfaceId.get() == nullptr);

        endpoint->SetInterfaceId(internal::NormalizeEndpointInterfaceIdWStringCopy(newDeviceInterfaceId.get()));

        unlistOnFailure.release();

        TraceLoggingWrite(
            MidiRtpMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"rtpMIDI endpoint activated", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(instanceId.c_str(), "instance id"),
            TraceLoggingWideString(friendlyName.c_str(), "friendly name"),
            TraceLoggingWideString(newDeviceInterfaceId.get(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
            TraceLoggingBool(endpoint->IsKept(), "kept while the remote is away"),
            TraceLoggingBool(endpoint->Connection() != nullptr, "connected")
        );

        return S_OK;
    }
    CATCH_RETURN();
}


_Use_decl_annotations_
HRESULT
CMidi2RtpMidiEndpointManager::RemoveRtpEndpoint(std::shared_ptr<RtpMidiEndpoint> const& endpoint)
{
    try
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, endpoint);

        {
            auto lock = m_createdEndpointsLock.lock_exclusive();
            std::erase(m_createdEndpoints, endpoint);
        }

        if (auto const connection = endpoint->Connection()) endpoint->DetachIfCurrent(connection.get());

        if (endpoint->IsHostRemoteEndpoint()) TransportState::Current().Approvals().RemoteEndpointRemoved(endpoint->EntryId(), endpoint->HostRemoteName());

        auto const interfaceId = endpoint->InterfaceId();
        if (!interfaceId.empty()) m_lastWrittenLatencyTicks.erase(interfaceId);

        RETURN_HR_IF_NULL(E_UNEXPECTED, m_midiDeviceManager);

        // Any client with the endpoint open is shut down inside this call, which re-enters the
        // bidi's Shutdown. No lock of this class is held here.
        auto const removeHr = m_midiDeviceManager->RemoveEndpoint(endpoint->NormalizedInstanceId().c_str());

        if (FAILED(removeHr))
        {
            // still in the service, so the worker tries again rather than leave it there
            auto lock = m_createdEndpointsLock.lock_exclusive();

            if (m_endpointsToRemove.size() < MIDI_RTP_MAX_FAILED_ENDPOINT_REMOVALS &&
                std::none_of(m_endpointsToRemove.begin(), m_endpointsToRemove.end(),
                    [&](FailedRemoval const& failed) { return failed.NormalizedInstanceId == endpoint->NormalizedInstanceId(); }))
            {
                m_endpointsToRemove.push_back(FailedRemoval{ endpoint->NormalizedInstanceId(), 1 });
            }
        }

        RETURN_IF_FAILED(removeHr);

        TraceLoggingWrite(
            MidiRtpMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"rtpMIDI endpoint removed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(endpoint->NormalizedInstanceId().c_str(), "instance id")
        );

        return S_OK;
    }
    CATCH_RETURN();
}


// The worker thread
void
CMidi2RtpMidiEndpointManager::RetryFailedRemovals()
{
    std::vector<FailedRemoval> failed;

    {
        auto lock = m_createdEndpointsLock.lock_exclusive();
        if (m_endpointsToRemove.empty()) return;

        failed = m_endpointsToRemove;
    }

    if (m_midiDeviceManager == nullptr) return;

    for (auto const& removal : failed)
    {
        // made again since, and so in use: its next activation took care of the old one
        bool const listed = IsInstanceIdInUse(removal.NormalizedInstanceId);

        bool done = listed;

        if (!listed)
        {
            done = SUCCEEDED(LOG_IF_FAILED(m_midiDeviceManager->RemoveEndpoint(removal.NormalizedInstanceId.c_str()))) ||
                removal.Attempts + 1 >= MIDI_RTP_MAX_FAILED_REMOVAL_ATTEMPTS;
        }

        auto lock = m_createdEndpointsLock.lock_exclusive();

        auto const it = std::find_if(m_endpointsToRemove.begin(), m_endpointsToRemove.end(),
            [&](FailedRemoval const& candidate) { return candidate.NormalizedInstanceId == removal.NormalizedInstanceId; });

        if (it == m_endpointsToRemove.end()) continue;

        if (done) m_endpointsToRemove.erase(it);
        else it->Attempts++;
    }
}


_Use_decl_annotations_
bool
CMidi2RtpMidiEndpointManager::IsInstanceIdListedElsewhere(std::shared_ptr<RtpMidiEndpoint> const& endpoint)
{
    auto lock = m_createdEndpointsLock.lock_shared();

    return std::any_of(m_createdEndpoints.begin(), m_createdEndpoints.end(),
        [&](std::shared_ptr<RtpMidiEndpoint> const& listed) { return listed != endpoint && listed->NormalizedInstanceId() == endpoint->NormalizedInstanceId(); });
}


_Use_decl_annotations_
std::shared_ptr<RtpMidiEndpoint>
CMidi2RtpMidiEndpointManager::FindEndpointForConnection(RtpMidiConnection const* connection)
{
    if (connection == nullptr) return nullptr;

    auto lock = m_createdEndpointsLock.lock_exclusive();

    for (auto const& endpoint : m_createdEndpoints)
    {
        if (endpoint->Connection().get() == connection) return endpoint;
    }

    return nullptr;
}

// A client entry has one endpoint at most. A host has one for each connection, so this is for clients.
_Use_decl_annotations_
std::shared_ptr<RtpMidiEndpoint>
CMidi2RtpMidiEndpointManager::FindEndpointForEntry(GUID const& entryId)
{
    auto lock = m_createdEndpointsLock.lock_exclusive();

    for (auto const& endpoint : m_createdEndpoints)
    {
        if (endpoint->EntryId() == entryId) return endpoint;
    }

    return nullptr;
}


_Use_decl_annotations_
HRESULT
CMidi2RtpMidiEndpointManager::CreateKeptEndpoint(
    RtpMidiClientDefinition const& definition,
    std::shared_ptr<RtpMidiConnection> const& connection)
{
    // the connection's queued creation and the worker's own pass can both get here
    if (auto const existing = FindEndpointForEntry(definition.EntryId); existing != nullptr && existing->IsKept())
    {
        if (connection != nullptr) existing->Attach(connection);
        return S_OK;
    }

    auto const endpoint = std::make_shared<RtpMidiEndpoint>(
        definition.EntryId,
        KeptEndpointInstanceId(definition),
        KeptEndpointName(definition));

    endpoint->SetKept(true);

    if (connection != nullptr) endpoint->Attach(connection);

    return ActivateRtpEndpoint(
        endpoint,
        RtpMidiText::StableHash(internal::GuidToString(definition.EntryId)),
        internal::ResourceGetWString(IDS_RTP_ENDPOINT_DESCRIPTION_CLIENT));
}


// The node's delivery thread. Attached before anything else is delivered, so an app which kept the
// endpoint open hears the remote's first message. Under the list's lock, so the worker can't remove
// the endpoint between finding it and handing it the connection.
_Use_decl_annotations_
bool
CMidi2RtpMidiEndpointManager::AttachToKeptEndpoint(std::shared_ptr<RtpMidiConnection> const& connection)
{
    if (connection == nullptr) return false;
    if (connection->ThisPcIsHost()) return AttachToKeptHostEndpoint(connection);

    auto lock = m_createdEndpointsLock.lock_exclusive();

    for (auto const& endpoint : m_createdEndpoints)
    {
        if (endpoint->EntryId() != connection->EntryId()) continue;
        if (!endpoint->IsKept()) return false;

        // a connection which already ended needs nothing more
        (void)endpoint->Attach(connection);

        return true;
    }

    return false;
}

// The node's delivery thread. The remote's held notes were already ended and any open SysEx closed
// on the way here, so an app keeping the endpoint open is left with nothing sounding.
_Use_decl_annotations_
bool
CMidi2RtpMidiEndpointManager::DetachFromKeptEndpoint(std::shared_ptr<RtpMidiConnection> const& connection)
{
    if (connection == nullptr) return false;
    if (connection->ThisPcIsHost()) return DetachFromKeptHostEndpoint(connection);

    auto const endpoint = FindEndpointForConnection(connection.get());
    if (endpoint == nullptr || !endpoint->IsKept()) return false;

    endpoint->DetachIfCurrent(connection.get());

    TraceLoggingWrite(
        MidiRtpMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Keeping the endpoint of a client whose connection ended", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpoint->NormalizedInstanceId().c_str(), "instance id")
    );

    return true;
}


// The worker thread, and Initialize before the worker starts
void
CMidi2RtpMidiEndpointManager::ReconcileKeptEndpoints()
{
    try
    {
        if (m_midiDeviceManager == nullptr || m_parentDeviceId.empty()) return;

        auto const definitions = TransportState::Current().GetClientDefinitions();
        auto const now = GetTickCount64();

        struct EntryView
        {
            ClientEntryState State{ ClientEntryState::Pending };
            std::shared_ptr<RtpMidiNode> Node;
            std::shared_ptr<RtpMidiConnection> Connection;
        };

        std::map<GUID, EntryView, GuidLess> entries;

        {
            auto lock = m_runtimeLock.lock_exclusive();
            for (auto const& [entryId, runtime] : m_clients) entries.emplace(entryId, EntryView{ runtime.State, runtime.Node, nullptr });
        }

        // outside the runtime lock, which is never held across a call into a node
        for (auto& [entryId, view] : entries)
        {
            if (view.Node == nullptr) continue;

            // a client invites one remote, so it has one connection at most
            auto const connections = view.Node->Connections();
            if (!connections.empty()) view.Connection = connections.front();
        }

        auto const findDefinition = [&](GUID const& entryId) -> RtpMidiClientDefinition const*
            {
                auto const found = std::find_if(definitions.begin(), definitions.end(),
                    [&](RtpMidiClientDefinition const& candidate) { return candidate.EntryId == entryId; });

                return found == definitions.end() ? nullptr : &*found;
            };

        // Disconnected by the customer stays disconnected, and so loses its endpoint, until reconnected
        auto const keeps = [&](RtpMidiClientDefinition const* definition)
            {
                if (definition == nullptr || !definition->KeepsEndpoint()) return false;

                auto const view = entries.find(definition->EntryId);
                return view == entries.end() || view->second.State != ClientEntryState::Unavailable;
            };

        auto const matches = [](std::shared_ptr<RtpMidiEndpoint> const& endpoint, RtpMidiClientDefinition const& definition)
            {
                return endpoint->NormalizedInstanceId() == internal::NormalizeDeviceInstanceIdWStringCopy(KeptEndpointInstanceId(definition)) &&
                    endpoint->EndpointName() == KeptEndpointName(definition);
            };

        std::vector<std::shared_ptr<RtpMidiEndpoint>> kept;

        {
            auto lock = m_createdEndpointsLock.lock_exclusive();

            for (auto const& endpoint : m_createdEndpoints)
            {
                // a host's remotes are reconciled below, against the host's settings
                if (endpoint->IsKept() && !endpoint->IsHostRemoteEndpoint()) kept.push_back(endpoint);
            }
        }

        for (auto const& endpoint : kept)
        {
            auto const definition = findDefinition(endpoint->EntryId());
            bool const connected = endpoint->Connection() != nullptr;

            if (!keeps(definition))
            {
                // One with a connection now ends with it, like any other. Removed only while
                // nothing is attached, in case its remote came back since this pass looked.
                endpoint->SetKept(false);

                if (!connected) (void)TryRemoveDetachedEndpoint(endpoint);

                continue;
            }

            // read now: the remote can leave after this pass began, and an earlier tick would wrap
            auto const disconnectedSince = endpoint->DisconnectedSinceTick();
            auto const currentTick = GetTickCount64();

            bool const expired =
                !connected &&
                definition->OfflineRetentionSeconds > 0 &&
                currentTick > disconnectedSince &&
                currentTick - disconnectedSince >= static_cast<uint64_t>(definition->OfflineRetentionSeconds) * 1000;

            // A new name or a new remote is a new endpoint, made again below
            if (expired || !matches(endpoint, *definition))
            {
                TraceLoggingWrite(
                    MidiRtpMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(expired ?
                        L"Removing a kept endpoint whose remote has been away past its offline retention" :
                        L"Replacing a kept endpoint whose client entry changed its name or remote", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(endpoint->NormalizedInstanceId().c_str(), "instance id"),
                    TraceLoggingInt32(definition->OfflineRetentionSeconds, "offline retention seconds")
                );

                // a remote back since this pass looked keeps its endpoint until it leaves again
                if (matches(endpoint, *definition)) (void)TryRemoveDetachedEndpoint(endpoint);
                else LOG_IF_FAILED(RemoveRtpEndpoint(endpoint));
            }
        }

        for (auto const& definition : definitions)
        {
            if (!keeps(&definition)) continue;

            auto const view = entries.find(definition.EntryId);
            auto const connection = view == entries.end() ? nullptr : view->second.Connection;

            auto const existing = FindEndpointForEntry(definition.EntryId);

            if (existing != nullptr && existing->IsKept())
            {
                // connected before its endpoint was there to take the connection
                if (connection != nullptr && existing->Connection() == nullptr) existing->Attach(connection);

                continue;
            }

            if (existing != nullptr)
            {
                // Made before the entry kept its endpoint. Apps never see it go when nothing would change.
                if (matches(existing, definition))
                {
                    existing->SetKept(true);
                    continue;
                }

                LOG_IF_FAILED(RemoveRtpEndpoint(existing));
            }

            // "always" is there from the start. A number of seconds counts from a connection ending.
            if (connection == nullptr && definition.OfflineRetentionSeconds != MIDI_RTP_OFFLINE_RETENTION_KEEP_ALWAYS) continue;

            if (auto const retry = m_keptEndpointRetryTicks.find(definition.EntryId); retry != m_keptEndpointRetryTicks.end() && now < retry->second) continue;

            if (SUCCEEDED(LOG_IF_FAILED(CreateKeptEndpoint(definition, connection))))
            {
                m_keptEndpointRetryTicks.erase(definition.EntryId);
            }
            else
            {
                m_keptEndpointRetryTicks[definition.EntryId] = now + MIDI_RTP_KEPT_ENDPOINT_RETRY_INTERVAL_MS;
            }
        }

        ReconcileHostKeptEndpoints(now);
    }
    CATCH_LOG();
}
