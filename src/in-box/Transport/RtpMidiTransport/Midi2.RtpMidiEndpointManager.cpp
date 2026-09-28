// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Runs the configured hosts and clients, and gives every connection an endpoint.
// ============================================================================

#include "pch.h"

namespace
{
    bool SameHostSettings(_In_ RtpMidiHostDefinition const& left, _In_ RtpMidiHostDefinition const& right)
    {
        return left.Name == right.Name &&
            left.ServiceInstanceName == right.ServiceInstanceName &&
            left.Port == right.Port &&
            left.AllowPortFallback == right.AllowPortFallback &&
            left.Advertise == right.Advertise &&
            left.SendRecoveryJournal == right.SendRecoveryJournal;
    }

    // The endpoint name and the reconnect choice apply without dropping the connection
    bool SameClientSettings(_In_ RtpMidiClientDefinition const& left, _In_ RtpMidiClientDefinition const& right)
    {
        return left.Name == right.Name &&
            left.RemoteServiceInstanceName == right.RemoteServiceInstanceName &&
            left.RemoteAddress == right.RemoteAddress &&
            left.RemotePort == right.RemotePort &&
            left.SendRecoveryJournal == right.SendRecoveryJournal;
    }

    // Session clock ticks. Averaged the same way Network MIDI 2.0 averages its pings.
    uint64_t AverageRoundTrip(_In_ RtpMidi::Participant const& participant)
    {
        auto const retained = (std::min)(static_cast<size_t>(participant.ClockSampleCount), participant.ClockSamples.size());
        if (retained == 0) return 0;

        uint64_t total{ 0 };
        for (size_t i = 0; i < retained; i++) total += participant.ClockSamples[i].RoundTrip;

        return total / retained;
    }

    // Machine-readable, not display text. The settings app maps these to its own strings.
    PCWSTR ConnectionStateToken(_In_ RtpMidi::ParticipantState const state)
    {
        switch (state)
        {
        case RtpMidi::ParticipantState::InvitingControl:
        case RtpMidi::ParticipantState::InvitingData:
            return L"inviting";
        case RtpMidi::ParticipantState::AwaitingDataInvitation:
            return L"accepting";
        case RtpMidi::ParticipantState::Synchronizing:
            return L"synchronizing";
        case RtpMidi::ParticipantState::Connected:
            return L"connected";
        default:
            return L"ended";
        }
    }

    HRESULT EndReasonToHresult(_In_ RtpMidi::EndReason const reason)
    {
        switch (reason)
        {
        case RtpMidi::EndReason::None:
        case RtpMidi::EndReason::LocalRequest:
            return S_OK;
        case RtpMidi::EndReason::Rejected:
            return E_ACCESSDENIED;
        case RtpMidi::EndReason::NoAnswer:
            return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
        case RtpMidi::EndReason::RemoteEndedSession:
            return HRESULT_FROM_WIN32(ERROR_GRACEFUL_DISCONNECT);
        case RtpMidi::EndReason::SyncTimeout:
            return HRESULT_FROM_WIN32(ERROR_CONNECTION_ABORTED);
        default:
            return E_FAIL;
        }
    }

    std::wstring BuildInstanceId(_In_ std::wstring const& readableName, _In_ std::wstring const& identity)
    {
        auto readable = internal::RemoveInvalidSWDUniqueIdCharacters(readableName);
        if (readable.size() > MIDI_RTP_ENDPOINT_INSTANCE_ID_NAME_MAX_CHARS) readable.resize(MIDI_RTP_ENDPOINT_INSTANCE_ID_NAME_MAX_CHARS);

        return std::wstring{ MIDI_RTP_ENDPOINT_INSTANCE_ID_PREFIX } + readable + L"_" + RtpMidiText::StableHash(identity);
    }

    json::IJsonValue JsonNumber(_In_ uint64_t const value)
    {
        return json::JsonValue::CreateNumberValue(static_cast<double>(value));
    }

    json::IJsonValue JsonString(_In_ std::wstring const& value)
    {
        return json::JsonValue::CreateStringValue(winrt::hstring{ value });
    }

    json::IJsonValue JsonBoolean(_In_ bool const value)
    {
        return json::JsonValue::CreateBooleanValue(value);
    }

    json::IJsonValue JsonHresult(_In_ HRESULT const value)
    {
        return json::JsonValue::CreateNumberValue(static_cast<double>(static_cast<uint32_t>(value)));
    }

    json::JsonArray JsonStrings(_In_ std::vector<std::wstring> const& values)
    {
        json::JsonArray array;
        for (auto const& value : values) array.Append(JsonString(value));
        return array;
    }
}


_Use_decl_annotations_
HRESULT
CMidi2RtpMidiEndpointManager::Initialize(
    IMidiDeviceManager* midiDeviceManager,
    IMidiEndpointProtocolManager* midiEndpointProtocolManager)
{
    try
    {
        TraceLoggingWrite(
            MidiRtpMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        RETURN_HR_IF_NULL(E_INVALIDARG, midiDeviceManager);
        RETURN_HR_IF_NULL(E_INVALIDARG, midiEndpointProtocolManager);
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED), m_initialized.load());

        RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManager), (void**)&m_midiDeviceManager));
        RETURN_IF_FAILED(midiEndpointProtocolManager->QueryInterface(__uuidof(IMidiEndpointProtocolManager), (void**)&m_midiProtocolManager));

        m_containerId = TRANSPORT_LAYER_GUID;

        WSADATA wsaData{};
        auto const wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        RETURN_HR_IF(HRESULT_FROM_WIN32(wsaResult), wsaResult != 0);
        m_winsockStarted = true;

        wchar_t computerName[256]{};
        DWORD size = ARRAYSIZE(computerName);
        if (GetComputerNameExW(ComputerNameDnsHostname, computerName, &size))
        {
            m_localDnsHostName = std::wstring{ computerName } + L".local";
        }

        RETURN_IF_FAILED(CreateParentDevice());

        LOG_IF_FAILED(m_announcer.Start(
            MIDI_RTP_DNSSD_SERVICE_TYPE,
            [this](size_t const hostCount, size_t const packetCount, WindowsMidiServicesInternal::MidiDnssdAnnouncementResult const& result)
            {
                TraceLoggingWrite(
                    MidiRtpMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Repeated the DNS-SD announcement of this PC's hosts", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingUInt64(hostCount, "hosts"),
                    TraceLoggingUInt64(packetCount, "packets"),
                    TraceLoggingUInt32(result.IPv4Interfaces, "IPv4 interfaces"),
                    TraceLoggingUInt32(result.IPv6Interfaces, "IPv6 interfaces"),
                    TraceLoggingInt32(result.LastError, "last error")
                );
            }));

        m_initialized = true;

        // Discovery only serves clients configured by name and the list of peers, so a failure here
        // leaves hosts and clients configured by address working.
        LOG_IF_FAILED(m_browser.Start(
            MIDI_RTP_DNSSD_SERVICE_TYPE,
            [this](WindowsMidiServicesInternal::MidiDnssdService const&) { WakeWorker(); },
            [this](WindowsMidiServicesInternal::MidiDnssdService const&, uint32_t) { WakeWorker(); },
            [](std::wstring const&, std::wstring const&) {}));

        m_worker = std::jthread([this](std::stop_token stopToken) { WorkerLoop(stopToken); });
        SetThreadDescription(m_worker.native_handle(), L"rtpMIDI endpoint worker");

        return S_OK;
    }
    CATCH_RETURN();
}


HRESULT
CMidi2RtpMidiEndpointManager::CreateParentDevice()
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_midiDeviceManager);

    auto const parentDeviceName = internal::ResourceGetWString(IDS_RTP_PARENT_DEVICE_NAME);
    auto const parentDeviceId = internal::NormalizeDeviceInstanceIdWStringCopy(TRANSPORT_PARENT_ID);

    SW_DEVICE_CREATE_INFO createInfo{};
    createInfo.cbSize = sizeof(createInfo);
    createInfo.pszInstanceId = parentDeviceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = parentDeviceName.c_str();
    createInfo.pContainerId = &m_containerId;

    wil::unique_cotaskmem_string newDeviceId;

    RETURN_IF_FAILED(m_midiDeviceManager->ActivateVirtualParentDevice(0, nullptr, &createInfo, &newDeviceId));
    RETURN_HR_IF_NULL(E_UNEXPECTED, newDeviceId.get());

    m_parentDeviceId = internal::NormalizeDeviceInstanceIdWStringCopy(newDeviceId.get());

    return S_OK;
}


HRESULT
CMidi2RtpMidiEndpointManager::Shutdown()
{
    try
    {
        TraceLoggingWrite(
            MidiRtpMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        if (!m_initialized.exchange(false)) return S_OK;

        m_browser.Stop();

        m_worker.request_stop();
        WakeWorker();
        if (m_worker.joinable()) m_worker.join();

        // before the hosts below withdraw their registrations, so no repeat can follow a goodbye
        m_announcer.Stop();

        std::vector<std::shared_ptr<RtpMidiNode>> nodes;

        {
            auto lock = std::scoped_lock{ m_runtimeLock };

            for (auto& entry : m_hosts) if (entry.second.Node != nullptr) nodes.push_back(std::move(entry.second.Node));
            for (auto& entry : m_clients) if (entry.second.Node != nullptr) nodes.push_back(std::move(entry.second.Node));

            m_hosts.clear();
            m_clients.clear();
        }

        // A goodbye to every remote, so none of them is left waiting out a timeout. The endpoints
        // themselves are the service's to tear down when it stops.
        for (auto const& node : nodes) node->Stop();
        nodes.clear();

        {
            auto lock = std::scoped_lock{ m_workLock };
            m_endpointWork.clear();
        }

        {
            auto lock = std::scoped_lock{ m_createdEndpointsLock };
            m_createdEndpoints.clear();
        }

        m_lastWrittenLatencyTicks.clear();

        m_midiDeviceManager.reset();
        m_midiProtocolManager.reset();

        if (m_winsockStarted)
        {
            WSACleanup();
            m_winsockStarted = false;
        }

        return S_OK;
    }
    CATCH_RETURN();
}


void
CMidi2RtpMidiEndpointManager::WakeWorker() noexcept
{
    {
        auto lock = std::scoped_lock{ m_workLock };
        m_wakeRequested = true;
    }

    m_workChanged.notify_one();
}


_Use_decl_annotations_
void
CMidi2RtpMidiEndpointManager::WorkerLoop(std::stop_token stopToken)
{
    while (!stopToken.stop_requested())
    {
        // one failed pass must not stop the next one
        try
        {
            ProcessEndpointWork();
            ReconcileHosts(stopToken);
            ReconcileClients(stopToken);
            ProcessEndpointWork();

            auto const now = GetTickCount64();

            if (now >= m_nextLatencyRefreshTick)
            {
                RefreshCalculatedLatency();
                m_nextLatencyRefreshTick = now + MIDI_RTP_LATENCY_PROPERTY_INTERVAL_MS;
            }
        }
        CATCH_LOG();

        auto lock = std::unique_lock{ m_workLock };

        m_workChanged.wait_for(lock, std::chrono::milliseconds(MIDI_RTP_WORKER_INTERVAL_MS),
            [&]() { return m_wakeRequested || stopToken.stop_requested(); });

        m_wakeRequested = false;
    }
}


void
CMidi2RtpMidiEndpointManager::ProcessEndpointWork()
{
    std::deque<EndpointWork> work;

    {
        auto lock = std::scoped_lock{ m_workLock };
        work.swap(m_endpointWork);
    }

    for (size_t i = 0; i < work.size(); i++)
    {
        auto const& item = work[i];
        if (item.Connection == nullptr) continue;

        if (item.Create)
        {
            // a connection which ended before the worker got to it never gets an endpoint
            bool const endsLater = std::any_of(work.begin() + i + 1, work.end(),
                [&](EndpointWork const& later) { return !later.Create && later.Connection == item.Connection; });

            if (!endsLater) LOG_IF_FAILED(CreateEndpoint(item.Connection));
        }
        else
        {
            LOG_IF_FAILED(RemoveEndpoint(item.Connection));
        }
    }
}


_Use_decl_annotations_
void
CMidi2RtpMidiEndpointManager::ReconcileHosts(std::stop_token const& stopToken)
{
    auto const definitions = TransportState::Current().GetHostDefinitions();
    auto const now = GetTickCount64();

    std::vector<std::shared_ptr<RtpMidiNode>> toStop;
    std::vector<RtpMidiHostDefinition> toStart;

    {
        auto lock = std::scoped_lock{ m_runtimeLock };

        for (auto it = m_hosts.begin(); it != m_hosts.end(); )
        {
            auto const definition = std::find_if(definitions.begin(), definitions.end(),
                [&](RtpMidiHostDefinition const& candidate) { return candidate.EntryId == it->first; });

            if (definition == definitions.end())
            {
                if (it->second.Node != nullptr) toStop.push_back(std::move(it->second.Node));
                it = m_hosts.erase(it);
                continue;
            }

            if (it->second.Node != nullptr && (!definition->Enabled || !SameHostSettings(*definition, it->second.Definition)))
            {
                toStop.push_back(std::move(it->second.Node));
                it->second.Node = nullptr;
                it->second.NextAttemptTick = 0;
                it->second.LastError = S_OK;
            }

            ++it;
        }

        for (auto const& definition : definitions)
        {
            auto& runtime = m_hosts[definition.EntryId];
            runtime.Definition = definition;

            if (!definition.Enabled || runtime.Node != nullptr || now < runtime.NextAttemptTick) continue;

            runtime.NextAttemptTick = now + MIDI_RTP_CLIENT_RETRY_INTERVAL_MS;
            toStart.push_back(definition);
        }
    }

    // withdrawn from the announcer first, so no repeat can follow the goodbye
    for (auto const& node : toStop)
    {
        if (node->IsAdvertised()) m_announcer.RemoveRegistration(node->AdvertisedLabel());
        node->Stop();
    }

    toStop.clear();

    for (auto const& definition : toStart)
    {
        // a stopping service should not wait for hosts it is about to stop again
        if (stopToken.stop_requested()) break;

        auto node = std::make_shared<RtpMidiNode>(
            RtpMidiNode::Role::Host, definition.EntryId, definition.Name, definition.SendRecoveryJournal, this);

        node->SetAdmissionCheck([hostId = definition.EntryId](std::wstring const& remoteName, std::wstring const& remoteAddress)
            {
                return TransportState::Current().Approvals().Admit(hostId, remoteName, remoteAddress);
            });

        // "auto" still tries the port other rtpMIDI software looks at first
        auto const preferredPort = definition.Port == 0 ? static_cast<uint16_t>(MIDI_RTP_DEFAULT_HOST_PORT) : definition.Port;

        std::vector<std::pair<uint16_t, uint16_t>> ranges{};

        if (definition.Port == 0 || definition.AllowPortFallback)
        {
            ranges.emplace_back(static_cast<uint16_t>(MIDI_RTP_HOST_FALLBACK_FIRST_PORT), static_cast<uint16_t>(MIDI_RTP_HOST_FALLBACK_LAST_PORT));
            ranges.emplace_back(static_cast<uint16_t>(MIDI_RTP_SECONDARY_FALLBACK_FIRST_PORT), static_cast<uint16_t>(MIDI_RTP_SECONDARY_FALLBACK_LAST_PORT));
        }

        auto const startHr = node->Start(preferredPort, ranges);

        // A host which cannot be advertised can still be reached by address, so it stays up
        HRESULT advertiseHr{ S_OK };
        if (SUCCEEDED(startHr) && definition.Advertise)
        {
            advertiseHr = node->Advertise(definition.EffectiveServiceInstanceName(), stopToken);
            LOG_IF_FAILED(advertiseHr);

            // added even if the host is not kept below, because its registration still flushed the others
            if (SUCCEEDED(advertiseHr)) m_announcer.AddRegistration(node->AdvertisedLabel());
        }

        bool keep{ false };

        {
            auto lock = std::scoped_lock{ m_runtimeLock };

            // the definition may have changed or gone while this one was starting
            auto const it = m_hosts.find(definition.EntryId);

            if (it != m_hosts.end() && it->second.Node == nullptr && it->second.Definition.Enabled && SameHostSettings(it->second.Definition, definition))
            {
                it->second.LastError = FAILED(startHr) ? startHr : advertiseHr;

                if (SUCCEEDED(startHr))
                {
                    it->second.Node = node;
                    keep = true;
                }
            }
        }

        TraceLoggingWrite(
            MidiRtpMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Host start attempted", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(definition.Name.c_str(), "name"),
            TraceLoggingHResult(startHr, "start result"),
            TraceLoggingHResult(advertiseHr, "advertise result"),
            TraceLoggingBool(keep, "kept")
        );

        if (!keep)
        {
            if (node->IsAdvertised()) m_announcer.RemoveRegistration(node->AdvertisedLabel());
            node->Stop();
        }
    }
}


_Use_decl_annotations_
void
CMidi2RtpMidiEndpointManager::ReconcileClients(std::stop_token const& stopToken)
{
    auto const definitions = TransportState::Current().GetClientDefinitions();
    auto const now = GetTickCount64();

    std::vector<std::shared_ptr<RtpMidiNode>> toStop;
    std::vector<RtpMidiClientDefinition> toAttempt;

    {
        auto lock = std::scoped_lock{ m_runtimeLock };

        for (auto it = m_clients.begin(); it != m_clients.end(); )
        {
            auto const definition = std::find_if(definitions.begin(), definitions.end(),
                [&](RtpMidiClientDefinition const& candidate) { return candidate.EntryId == it->first; });

            if (definition == definitions.end())
            {
                if (it->second.Node != nullptr) toStop.push_back(std::move(it->second.Node));
                it = m_clients.erase(it);
                continue;
            }

            if (!definition->Enabled || !SameClientSettings(*definition, it->second.Definition))
            {
                if (it->second.Node != nullptr) toStop.push_back(std::move(it->second.Node));

                it->second.Node = nullptr;
                it->second.State = ClientEntryState::Pending;
                it->second.InvitationOutstanding = false;
                it->second.NextAttemptTick = 0;
                it->second.LastError = S_OK;
            }

            ++it;
        }

        for (auto const& definition : definitions)
        {
            auto& runtime = m_clients[definition.EntryId];
            runtime.Definition = definition;

            if (!definition.Enabled) continue;
            if (runtime.State == ClientEntryState::Live || runtime.State == ClientEntryState::Unavailable) continue;
            if (runtime.InvitationOutstanding || now < runtime.NextAttemptTick) continue;

            toAttempt.push_back(definition);
        }
    }

    // Stopping ends the old node's invitation, and the report of that is ignored because the node
    // is no longer the entry's current one
    for (auto const& node : toStop) node->Stop();
    toStop.clear();

    for (auto const& definition : toAttempt)
    {
        if (stopToken.stop_requested()) break;

        RtpMidi::PeerAddress target{};

        if (!TryResolveClientTarget(definition, stopToken, target))
        {
            auto lock = std::scoped_lock{ m_runtimeLock };

            auto const it = m_clients.find(definition.EntryId);
            if (it != m_clients.end())
            {
                it->second.State = ClientEntryState::Pending;
                it->second.LastError = HRESULT_FROM_WIN32(ERROR_HOST_UNREACHABLE);

                // an advertised remote is looked for again whenever discovery reports a change
                it->second.NextAttemptTick = definition.IsDirect() ? now + MIDI_RTP_CLIENT_RETRY_INTERVAL_MS : 0;
            }

            continue;
        }

        std::shared_ptr<RtpMidiNode> node{ nullptr };

        {
            auto lock = std::scoped_lock{ m_runtimeLock };

            auto const it = m_clients.find(definition.EntryId);
            if (it == m_clients.end()) continue;

            node = it->second.Node;
        }

        if (node == nullptr)
        {
            node = std::make_shared<RtpMidiNode>(
                RtpMidiNode::Role::Client, definition.EntryId, definition.Name, definition.SendRecoveryJournal, this);

            std::vector<std::pair<uint16_t, uint16_t>> const ranges
            {
                { static_cast<uint16_t>(MIDI_RTP_CLIENT_FIRST_PORT), static_cast<uint16_t>(MIDI_RTP_CLIENT_LAST_PORT) }
            };

            auto const startHr = node->Start(0, ranges);

            bool keep{ false };

            {
                auto lock = std::scoped_lock{ m_runtimeLock };

                auto const it = m_clients.find(definition.EntryId);

                if (it != m_clients.end() && it->second.Node == nullptr && it->second.Definition.Enabled && SameClientSettings(it->second.Definition, definition))
                {
                    if (SUCCEEDED(startHr))
                    {
                        it->second.Node = node;
                        keep = true;
                    }
                    else
                    {
                        it->second.State = ClientEntryState::Failed;
                        it->second.LastError = startHr;
                        it->second.NextAttemptTick = now + MIDI_RTP_CLIENT_RETRY_INTERVAL_MS;
                    }
                }
            }

            if (!keep)
            {
                node->Stop();
                continue;
            }
        }

        {
            auto lock = std::scoped_lock{ m_runtimeLock };

            auto const it = m_clients.find(definition.EntryId);
            if (it == m_clients.end() || it->second.Node != node) continue;

            // set before inviting, because the answer can arrive before Invite returns
            it->second.InvitationOutstanding = true;
            it->second.State = ClientEntryState::Pending;
        }

        auto const inviteHr = node->Invite(target);

        TraceLoggingWrite(
            MidiRtpMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Inviting remote", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(definition.Name.c_str(), "name"),
            TraceLoggingWideString(RtpMidiNet::AddressToString(target).c_str(), "remote address"),
            TraceLoggingUInt16(target.Port, "remote port"),
            TraceLoggingHResult(inviteHr, "result")
        );

        if (FAILED(inviteHr))
        {
            auto lock = std::scoped_lock{ m_runtimeLock };

            auto const it = m_clients.find(definition.EntryId);
            if (it != m_clients.end() && it->second.Node == node)
            {
                it->second.InvitationOutstanding = false;
                it->second.State = ClientEntryState::Failed;
                it->second.LastError = inviteHr;
                it->second.NextAttemptTick = now + MIDI_RTP_CLIENT_RETRY_INTERVAL_MS;
            }
        }
    }
}


_Use_decl_annotations_
bool
CMidi2RtpMidiEndpointManager::TryResolveClientTarget(RtpMidiClientDefinition const& definition, std::stop_token const& stopToken, RtpMidi::PeerAddress& target)
{
    target = RtpMidi::PeerAddress{};

    try
    {
        if (definition.IsDirect())
        {
            if (RtpMidiNet::TryParseAddress(definition.RemoteAddress, definition.RemotePort, target)) return true;

            // A host name, which may be a .local name answered over multicast DNS. The lookup has
            // a time limit and a service stop cancels it, so a slow or missing DNS server cannot
            // hold up shutdown.
            wil::unique_event_nothrow resolved;
            wil::unique_event_nothrow stopped;

            if (!resolved.try_create(wil::EventOptions::ManualReset, nullptr) ||
                !stopped.try_create(wil::EventOptions::ManualReset, nullptr))
            {
                return false;
            }

            std::stop_callback const onStop{ stopToken, [&]() noexcept { stopped.SetEvent(); } };

            ADDRINFOEXW hints{};
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_DGRAM;
            hints.ai_protocol = IPPROTO_UDP;

            PADDRINFOEXW results{ nullptr };
            OVERLAPPED overlapped{};
            overlapped.hEvent = resolved.get();
            HANDLE cancel{ nullptr };
            timeval timeout{ MIDI_RTP_NAME_RESOLUTION_TIMEOUT_SECONDS, 0 };

            auto const service = std::to_wstring(definition.RemotePort);

            auto status = GetAddrInfoExW(definition.RemoteAddress.c_str(), service.c_str(), NS_ALL, nullptr, &hints, &results,
                &timeout, &overlapped, nullptr, &cancel);

            if (status == WSA_IO_PENDING)
            {
                HANDLE const handles[]{ resolved.get(), stopped.get() };

                if (WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, INFINITE) != WAIT_OBJECT_0)
                {
                    // canceled or not, the lookup has to finish before the buffers it writes go away
                    GetAddrInfoExCancel(&cancel);
                    WaitForSingleObject(resolved.get(), INFINITE);
                }

                status = GetAddrInfoExOverlappedResult(&overlapped);
            }

            auto freeResults = wil::scope_exit([&]() { if (results != nullptr) FreeAddrInfoExW(results); });

            if (status != NO_ERROR || results == nullptr) return false;

            RtpMidi::PeerAddress firstV6{};
            bool haveV6{ false };

            for (auto result = results; result != nullptr; result = result->ai_next)
            {
                if (result->ai_addr == nullptr || result->ai_addrlen > sizeof(sockaddr_storage)) continue;

                sockaddr_storage storage{};
                memcpy(&storage, result->ai_addr, result->ai_addrlen);

                auto const address = RtpMidiNet::FromSockaddr(storage);

                if (address.Family == 4)
                {
                    target = address;
                    return true;
                }

                if (address.Family == 6 && !haveV6)
                {
                    firstV6 = address;
                    haveV6 = true;
                }
            }

            if (haveV6) target = firstV6;
            return haveV6;
        }

        for (auto const& service : m_browser.EnumeratedServices())
        {
            if (!service.IsResolved()) continue;
            if (_wcsicmp(service.ServiceInstanceName.c_str(), definition.RemoteServiceInstanceName.c_str()) != 0) continue;

            return RtpMidiNet::ChooseServiceAddress(service, target);
        }

        return false;
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
        target = RtpMidi::PeerAddress{};
        return false;
    }
}


_Use_decl_annotations_
bool
CMidi2RtpMidiEndpointManager::IsInstanceIdInUse(std::wstring const& instanceId)
{
    auto const normalized = internal::NormalizeDeviceInstanceIdWStringCopy(instanceId);

    auto lock = std::scoped_lock{ m_createdEndpointsLock };

    return std::any_of(m_createdEndpoints.begin(), m_createdEndpoints.end(),
        [&](CreatedEndpoint const& record) { return std::wstring_view{ record.InstanceId } == normalized; });
}


_Use_decl_annotations_
HRESULT
CMidi2RtpMidiEndpointManager::CreateEndpoint(std::shared_ptr<RtpMidiConnection> const& connection)
{
    try
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, connection);
        RETURN_HR_IF_NULL(E_UNEXPECTED, m_midiDeviceManager);
        RETURN_HR_IF(E_UNEXPECTED, m_parentDeviceId.empty());

        std::wstring customEndpointName{};
        std::wstring readableName{ connection->RemoteName() };
        std::wstring identity{ internal::GuidToString(connection->EntryId()) };

        if (connection->ThisPcIsHost())
        {
            // A host serves many remotes. The name each one gives is the only identity that
            // survives a restart on both ends: its SSRC and ports change every time.
            identity += L"|" + connection->RemoteName();
        }
        else
        {
            RtpMidiClientDefinition definition{};

            if (TransportState::Current().TryGetClientDefinition(connection->EntryId(), definition))
            {
                customEndpointName = definition.CustomEndpointName;

                // configured by the customer, so it stays put when the remote renames itself
                readableName = definition.IsDirect() ? definition.RemoteAddress : definition.RemoteServiceInstanceName;
            }
        }

        std::wstring endpointName = customEndpointName.empty() ? connection->RemoteName() : customEndpointName;
        if (endpointName.empty()) endpointName = RtpMidiNet::AddressToString(connection->RemoteControl());

        auto instanceId = BuildInstanceId(readableName, identity);

        // two remotes giving the same name to one host
        if (IsInstanceIdInUse(instanceId))
        {
            identity += L"|" + RtpMidiNet::AddressToString(connection->RemoteControl()) + L"|" + std::to_wstring(connection->RemoteControl().Port);
            instanceId = BuildInstanceId(readableName, identity);
        }

        auto const uniqueIdentifier = RtpMidiText::StableHash(identity);
        auto const endpointDescription = internal::ResourceGetWString(
            connection->ThisPcIsHost() ? IDS_RTP_ENDPOINT_DESCRIPTION_HOST : IDS_RTP_ENDPOINT_DESCRIPTION_CLIENT);

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

        auto const activateHR = m_midiDeviceManager->ActivateEndpoint(
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

        RETURN_IF_FAILED(activateHR);

        // S_FALSE means the instance id is already active and no interface id came back
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_DEVICE_ALREADY_ATTACHED), activateHR == S_FALSE || newDeviceInterfaceId.get() == nullptr);

        auto const normalizedInstanceId = internal::NormalizeDeviceInstanceIdWStringCopy(instanceId);
        auto const normalizedInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(newDeviceInterfaceId.get());

        connection->SetEndpointIds(normalizedInstanceId, normalizedInterfaceId);

        {
            auto lock = std::scoped_lock{ m_createdEndpointsLock };

            m_createdEndpoints.push_back(CreatedEndpoint{
                connection,
                winrt::hstring{ normalizedInstanceId },
                winrt::hstring{ normalizedInterfaceId },
                winrt::hstring{ endpointName } });
        }

        TraceLoggingWrite(
            MidiRtpMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"rtpMIDI endpoint activated", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(instanceId.c_str(), "instance id"),
            TraceLoggingWideString(friendlyName.c_str(), "friendly name"),
            TraceLoggingWideString(newDeviceInterfaceId.get(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        return S_OK;
    }
    CATCH_RETURN();
}


_Use_decl_annotations_
HRESULT
CMidi2RtpMidiEndpointManager::RemoveEndpoint(std::shared_ptr<RtpMidiConnection> const& connection)
{
    try
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, connection);

        auto const instanceId = connection->EndpointDeviceInstanceId();
        auto const interfaceId = connection->EndpointDeviceInterfaceId();

        // never got one, because it ended first
        if (instanceId.empty()) return S_OK;

        connection->ClearEndpointIds();

        {
            auto lock = std::scoped_lock{ m_createdEndpointsLock };

            std::erase_if(m_createdEndpoints,
                [&](CreatedEndpoint const& record) { return std::wstring_view{ record.InstanceId } == instanceId; });
        }

        m_lastWrittenLatencyTicks.erase(interfaceId);

        RETURN_HR_IF_NULL(E_UNEXPECTED, m_midiDeviceManager);

        // Any client with the endpoint open is shut down inside this call, which re-enters the
        // bidi's Shutdown. No lock of this class is held here.
        RETURN_IF_FAILED(m_midiDeviceManager->RemoveEndpoint(instanceId.c_str()));

        TraceLoggingWrite(
            MidiRtpMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"rtpMIDI endpoint removed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(instanceId.c_str(), "instance id")
        );

        return S_OK;
    }
    CATCH_RETURN();
}


std::vector<std::shared_ptr<RtpMidiNode>>
CMidi2RtpMidiEndpointManager::RunningNodes()
{
    std::vector<std::shared_ptr<RtpMidiNode>> nodes;

    auto lock = std::scoped_lock{ m_runtimeLock };

    for (auto const& entry : m_hosts) if (entry.second.Node != nullptr) nodes.push_back(entry.second.Node);
    for (auto const& entry : m_clients) if (entry.second.Node != nullptr) nodes.push_back(entry.second.Node);

    return nodes;
}


// The clock sync round trip is the only measured latency figure rtpMIDI has. Half of it is the
// mean one-way delay the outbound scheduler compensates for, written the way Network MIDI 2.0
// writes its ping figure.
void
CMidi2RtpMidiEndpointManager::RefreshCalculatedLatency()
{
    if (!Feature_Servicing_MIDI2SchedulerV2::IsEnabled() || m_midiDeviceManager == nullptr) return;

    auto const thresholdTicks =
        (static_cast<uint64_t>(MIDI_RTP_LATENCY_PROPERTY_THRESHOLD_MICROSECONDS) * internal::GetMidiTimestampFrequency()) / 1000000;

    for (auto const& node : RunningNodes())
    {
        for (auto const& connection : node->Connections())
        {
            auto const interfaceId = connection->EndpointDeviceInterfaceId();
            if (interfaceId.empty()) continue;

            RtpMidi::Participant participant{};
            if (!node->TrySnapshot(connection->ParticipantId(), participant)) continue;

            auto const roundTrip = AverageRoundTrip(participant);
            if (roundTrip == 0) continue;

            uint64_t oneWayTicks = RtpMidiNode::SessionTicksToMidiTicks(roundTrip) / 2;

            auto const existing = m_lastWrittenLatencyTicks.find(interfaceId);

            if (existing != m_lastWrittenLatencyTicks.end())
            {
                auto const previous = existing->second;
                auto const difference = oneWayTicks > previous ? oneWayTicks - previous : previous - oneWayTicks;

                if (difference < thresholdTicks) continue;
            }

            DEVPROPERTY props[] =
            {
                { { PKEY_MIDI_MidiOutCalculatedLatencyTicks, DEVPROP_STORE_SYSTEM, nullptr },
                  DEVPROP_TYPE_UINT64, static_cast<ULONG>(sizeof(uint64_t)), (PVOID)&oneWayTicks },
            };

            if (SUCCEEDED(m_midiDeviceManager->UpdateEndpointProperties(interfaceId.c_str(), ARRAYSIZE(props), props)))
            {
                m_lastWrittenLatencyTicks[interfaceId] = oneWayTicks;
            }
        }
    }
}


_Use_decl_annotations_
void
CMidi2RtpMidiEndpointManager::OnConnectionUp(std::shared_ptr<RtpMidiConnection> const& connection)
{
    if (connection == nullptr) return;

    if (!connection->ThisPcIsHost())
    {
        auto const node = connection->Node();

        auto lock = std::scoped_lock{ m_runtimeLock };

        auto const it = m_clients.find(connection->EntryId());
        if (it != m_clients.end() && node != nullptr && it->second.Node == node)
        {
            it->second.State = ClientEntryState::Live;
            it->second.InvitationOutstanding = false;
            it->second.LastError = S_OK;
        }
    }

    {
        auto lock = std::scoped_lock{ m_workLock };
        m_endpointWork.push_back(EndpointWork{ true, connection });
        m_wakeRequested = true;
    }

    m_workChanged.notify_one();
}


_Use_decl_annotations_
void
CMidi2RtpMidiEndpointManager::OnConnectionDown(std::shared_ptr<RtpMidiConnection> const& connection, RtpMidi::EndReason const reason)
{
    if (connection == nullptr) return;

    TraceLoggingWrite(
        MidiRtpMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"rtpMIDI connection ended", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(connection->RemoteName().c_str(), "remote name"),
        TraceLoggingString(RtpMidi::EndReasonName(reason), "reason")
    );

    {
        auto lock = std::scoped_lock{ m_workLock };
        m_endpointWork.push_back(EndpointWork{ false, connection });
        m_wakeRequested = true;
    }

    m_workChanged.notify_one();
}


_Use_decl_annotations_
void
CMidi2RtpMidiEndpointManager::OnInvitationEnded(RtpMidiNode const* node, RtpMidi::EndReason const reason)
{
    if (node == nullptr) return;

    {
        auto lock = std::scoped_lock{ m_runtimeLock };

        auto const it = m_clients.find(node->EntryId());

        // a node which was replaced or stopped no longer speaks for the entry
        if (it == m_clients.end() || it->second.Node.get() != node) return;

        auto& runtime = it->second;
        runtime.InvitationOutstanding = false;
        runtime.LastError = EndReasonToHresult(reason);
        runtime.NextAttemptTick = GetTickCount64() + MIDI_RTP_CLIENT_RETRY_INTERVAL_MS;

        // A disconnect asked for here stays disconnected until a reconnect is asked for
        if (reason == RtpMidi::EndReason::LocalRequest || !runtime.Definition.AutoReconnect)
        {
            runtime.State = ClientEntryState::Unavailable;
        }
        else
        {
            runtime.State = ClientEntryState::Failed;
        }
    }

    WakeWorker();
}


_Use_decl_annotations_
HRESULT
CMidi2RtpMidiEndpointManager::ReconnectClient(GUID const& entryId)
{
    {
        auto lock = std::scoped_lock{ m_runtimeLock };

        auto const it = m_clients.find(entryId);

        if (it == m_clients.end())
        {
            RtpMidiClientDefinition definition{};
            RETURN_HR_IF(E_NOTFOUND, !TransportState::Current().TryGetClientDefinition(entryId, definition));

            // defined, and the worker has not reached it yet
            return S_FALSE;
        }

        if (it->second.State == ClientEntryState::Live || it->second.InvitationOutstanding) return S_FALSE;

        it->second.State = ClientEntryState::Pending;
        it->second.NextAttemptTick = 0;
        it->second.LastError = S_OK;
    }

    WakeWorker();

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2RtpMidiEndpointManager::DisconnectConnection(GUID const& entryId, uint32_t const connectionId)
{
    std::shared_ptr<RtpMidiNode> node{ nullptr };

    {
        auto lock = std::scoped_lock{ m_runtimeLock };

        if (auto const host = m_hosts.find(entryId); host != m_hosts.end()) node = host->second.Node;
        else if (auto const client = m_clients.find(entryId); client != m_clients.end()) node = client->second.Node;
    }

    RETURN_HR_IF_NULL(E_NOTFOUND, node);

    RtpMidi::Participant participant{};
    RETURN_HR_IF(E_NOTFOUND, !node->TrySnapshot(connectionId, participant));

    // an ended connection is kept briefly by the engine, but is gone as far as a caller can tell
    RETURN_HR_IF(E_NOTFOUND, participant.State == RtpMidi::ParticipantState::Ended);

    RETURN_IF_FAILED(node->EndConnection(connectionId));

    return S_OK;
}


_Use_decl_annotations_
size_t
CMidi2RtpMidiEndpointManager::EndConnectionsFromRemote(GUID const& hostId, std::wstring const& remoteName)
{
    std::shared_ptr<RtpMidiNode> node{ nullptr };

    {
        auto lock = std::scoped_lock{ m_runtimeLock };
        if (auto const host = m_hosts.find(hostId); host != m_hosts.end()) node = host->second.Node;
    }

    if (node == nullptr) return 0;

    size_t ended{ 0 };

    for (auto const& participant : node->Snapshot())
    {
        if (participant.State == RtpMidi::ParticipantState::Ended || participant.WeInitiated) continue;

        auto const name = RtpMidiText::Utf8ToWide(participant.RemoteName);

        if (CompareStringOrdinal(name.c_str(), static_cast<int>(name.size()), remoteName.c_str(), static_cast<int>(remoteName.size()), TRUE) != CSTR_EQUAL) continue;

        if (SUCCEEDED(node->EndConnection(participant.Id))) ended++;
    }

    return ended;
}


_Use_decl_annotations_
std::shared_ptr<RtpMidiConnection>
CMidi2RtpMidiEndpointManager::FindConnectionByEndpointDeviceInterfaceId(std::wstring const& endpointDeviceInterfaceId)
{
    auto const normalized = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);

    auto lock = std::scoped_lock{ m_createdEndpointsLock };

    for (auto const& record : m_createdEndpoints)
    {
        if (std::wstring_view{ record.InterfaceId } == normalized) return record.Connection.lock();
    }

    return nullptr;
}


_Use_decl_annotations_
winrt::hstring
CMidi2RtpMidiEndpointManager::FindMatchingInstantiatedEndpoint(WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria& criteria)
{
    criteria.Normalize();

    auto lock = std::scoped_lock{ m_createdEndpointsLock };

    for (auto const& record : m_createdEndpoints)
    {
        WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria available{};

        available.EndpointDeviceId = record.InterfaceId;
        available.DeviceInstanceId = record.InstanceId;
        available.TransportSuppliedEndpointName = record.EndpointName;

        if (available.Matches(criteria)) return available.EndpointDeviceId;
    }

    return L"";
}


_Use_decl_annotations_
json::JsonArray
CMidi2RtpMidiEndpointManager::BuildConnectionsJson(std::shared_ptr<RtpMidiNode> const& node)
{
    json::JsonArray connections;
    if (node == nullptr) return connections;

    auto const liveConnections = node->Connections();
    auto const advertised = m_browser.EnumeratedServices();

    for (auto const& participant : node->Snapshot())
    {
        if (participant.State == RtpMidi::ParticipantState::Ended) continue;

        auto const connection = std::find_if(liveConnections.begin(), liveConnections.end(),
            [&](std::shared_ptr<RtpMidiConnection> const& candidate) { return candidate->ParticipantId() == participant.Id; });

        bool const haveConnection = connection != liveConnections.end();

        json::JsonObject item;

        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_CONNECTION_ID_KEY, JsonNumber(participant.Id));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_NAME_KEY, JsonString(RtpMidiText::Utf8ToWide(participant.RemoteName)));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_ADDRESS_KEY, JsonString(RtpMidiNet::AddressToString(participant.RemoteControl)));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_PORT_KEY, JsonNumber(participant.RemoteControl.Port));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_HOST_NAME_KEY, JsonString(RtpMidiMdns::FindHostNameForAddress(advertised, participant.RemoteControl)));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_LOCAL_PORT_KEY, JsonNumber(node->ControlPort()));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_CONNECTION_STATE_KEY, JsonString(ConnectionStateToken(participant.State)));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_IS_CONNECTED_KEY, JsonBoolean(participant.State == RtpMidi::ParticipantState::Connected));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_WE_INITIATED_KEY, JsonBoolean(participant.WeInitiated));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ENDPOINT_DEVICE_ID_KEY, JsonString(haveConnection ? (*connection)->EndpointDeviceInterfaceId() : std::wstring{}));

        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_CURRENT_LATENCY_KEY, JsonNumber(RtpMidiNode::SessionTicksToMidiTicks(AverageRoundTrip(participant))));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_BEST_LATENCY_KEY, JsonNumber(RtpMidiNode::SessionTicksToMidiTicks(participant.Stats.BestRoundTripTicks)));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_PACKETS_SENT_KEY, JsonNumber(participant.Stats.PacketsSent));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_PACKETS_RECEIVED_KEY, JsonNumber(participant.Stats.PacketsReceived));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_PACKETS_LOST_KEY, JsonNumber(participant.Stats.PacketsLost));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_LOSSES_REPAIRED_KEY, JsonNumber(participant.Stats.LossEventsCovered));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_NOTES_ENDED_KEY, JsonNumber(participant.Stats.RecoveredNoteOffs));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_MESSAGES_SENT_KEY, JsonNumber(haveConnection ? (*connection)->MessagesSent() : 0));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_MESSAGES_RECEIVED_KEY, JsonNumber(participant.Stats.MessagesReceived));

        connections.Append(item);
    }

    return connections;
}


json::JsonArray
CMidi2RtpMidiEndpointManager::BuildHostsStatusJson()
{
    struct HostView
    {
        RtpMidiHostDefinition Definition{};
        std::shared_ptr<RtpMidiNode> Node;
        HRESULT LastError{ S_OK };
    };

    std::vector<HostView> views;

    {
        auto lock = std::scoped_lock{ m_runtimeLock };
        for (auto const& entry : m_hosts) views.push_back(HostView{ entry.second.Definition, entry.second.Node, entry.second.LastError });
    }

    // defined, and not reached by the worker yet
    for (auto const& definition : TransportState::Current().GetHostDefinitions())
    {
        bool const seen = std::any_of(views.begin(), views.end(), [&](HostView const& view) { return view.Definition.EntryId == definition.EntryId; });
        if (!seen) views.push_back(HostView{ definition, nullptr, S_OK });
    }

    json::JsonArray hosts;

    for (auto const& view : views)
    {
        auto const& definition = view.Definition;
        bool const running = view.Node != nullptr && view.Node->IsRunning();

        json::JsonObject item;

        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_IDENTIFIER_KEY, JsonString(internal::GuidToString(definition.EntryId)));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_NAME_KEY, JsonString(definition.Name));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_SERVICE_INSTANCE_NAME_KEY, JsonString(definition.EffectiveServiceInstanceName()));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ENABLED_KEY, JsonBoolean(definition.Enabled));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ADVERTISE_KEY, JsonBoolean(definition.Advertise));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ALLOW_PORT_FALLBACK_KEY, JsonBoolean(definition.AllowPortFallback));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_SEND_RECOVERY_JOURNAL_KEY, JsonBoolean(definition.SendRecoveryJournal));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_CONFIGURED_PORT_KEY,
            JsonString(definition.Port == 0 ? std::wstring{ MIDI_CONFIG_JSON_RTP_MIDI_PORT_VALUE_AUTO } : std::to_wstring(definition.Port)));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_HAS_STARTED_KEY, JsonBoolean(running));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_LAST_ERROR_KEY, JsonHresult(view.LastError));

        if (running)
        {
            item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ACTUAL_PORT_KEY, JsonNumber(view.Node->ControlPort()));
            item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_PORT_FALLBACK_USED_KEY, JsonBoolean(definition.Port != 0 && view.Node->UsedPortFallback()));

            if (view.Node->IsAdvertised())
            {
                item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ACTUAL_SERVICE_INSTANCE_NAME_KEY, JsonString(view.Node->AdvertisedLabel()));
                item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_SERVICE_INSTANCE_NAME_CHANGED_KEY, JsonBoolean(view.Node->AdvertisedLabelWasChanged()));
            }
        }

        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_CONNECTIONS_KEY, running ? BuildConnectionsJson(view.Node) : json::JsonArray{});

        auto& approvals = TransportState::Current().Approvals();

        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENT_POLICY_KEY, JsonString(
            approvals.Policy(definition.EntryId) == RtpMidiRemoteClientPolicy::RequireApproval ?
                MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENT_POLICY_VALUE_REQUIRE_APPROVAL :
                MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENT_POLICY_VALUE_ALLOW_ANY));

        json::JsonArray allowed;
        json::JsonArray denied;

        for (auto const& decision : approvals.Decisions(definition.EntryId))
        {
            json::JsonObject decisionItem;
            decisionItem.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_NAME_KEY, JsonString(decision.RemoteName));
            decisionItem.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_UNTIL_RESTART_KEY, JsonBoolean(decision.UntilRestart));

            (decision.Allowed ? allowed : denied).Append(decisionItem);
        }

        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ALLOWED_CLIENTS_KEY, allowed);
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_DENIED_CLIENTS_KEY, denied);

        hosts.Append(item);
    }

    return hosts;
}


json::JsonArray
CMidi2RtpMidiEndpointManager::BuildClientsStatusJson()
{
    struct ClientView
    {
        RtpMidiClientDefinition Definition{};
        std::shared_ptr<RtpMidiNode> Node;
        ClientEntryState State{ ClientEntryState::Pending };
        HRESULT LastError{ S_OK };
    };

    std::vector<ClientView> views;

    {
        auto lock = std::scoped_lock{ m_runtimeLock };
        for (auto const& entry : m_clients) views.push_back(ClientView{ entry.second.Definition, entry.second.Node, entry.second.State, entry.second.LastError });
    }

    for (auto const& definition : TransportState::Current().GetClientDefinitions())
    {
        bool const seen = std::any_of(views.begin(), views.end(), [&](ClientView const& view) { return view.Definition.EntryId == definition.EntryId; });
        if (!seen) views.push_back(ClientView{ definition, nullptr, ClientEntryState::Pending, S_OK });
    }

    json::JsonArray clients;

    for (auto const& view : views)
    {
        auto const& definition = view.Definition;
        bool const running = view.Node != nullptr && view.Node->IsRunning();

        PCWSTR state = MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_STATE_VALUE_PENDING;
        switch (view.State)
        {
        case ClientEntryState::Live: state = MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_STATE_VALUE_LIVE; break;
        case ClientEntryState::Failed: state = MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_STATE_VALUE_FAILED; break;
        case ClientEntryState::Unavailable: state = MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_STATE_VALUE_UNAVAILABLE; break;
        default: break;
        }

        json::JsonObject item;

        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_IDENTIFIER_KEY, JsonString(internal::GuidToString(definition.EntryId)));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_NAME_KEY, JsonString(definition.Name));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_IS_DIRECT_KEY, JsonBoolean(definition.IsDirect()));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_SERVICE_INSTANCE_NAME_KEY, JsonString(definition.RemoteServiceInstanceName));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_ADDRESS_KEY, JsonString(definition.RemoteAddress));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_PORT_KEY, JsonNumber(definition.RemotePort));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_CUSTOM_ENDPOINT_NAME_KEY, JsonString(definition.CustomEndpointName));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_AUTO_RECONNECT_KEY, JsonBoolean(definition.AutoReconnect));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ENABLED_KEY, JsonBoolean(definition.Enabled));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_SEND_RECOVERY_JOURNAL_KEY, JsonBoolean(definition.SendRecoveryJournal));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_STATE_KEY, JsonString(state));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_LAST_ERROR_KEY, JsonHresult(view.LastError));

        if (running) item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_LOCAL_PORT_KEY, JsonNumber(view.Node->ControlPort()));

        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_CONNECTIONS_KEY, running ? BuildConnectionsJson(view.Node) : json::JsonArray{});

        clients.Append(item);
    }

    return clients;
}


json::JsonArray
CMidi2RtpMidiEndpointManager::BuildAdvertisedHostsJson()
{
    json::JsonArray peers;

    for (auto const& service : m_browser.EnumeratedServices())
    {
        json::JsonObject item;

        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_SERVICE_INSTANCE_NAME_KEY, JsonString(service.ServiceInstanceName));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_HOST_NAME_KEY, JsonString(service.HostName));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_PORT_KEY, JsonNumber(service.Port));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_IPV4_ADDRESSES_KEY, JsonStrings(service.IPv4Addresses));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_IPV6_ADDRESSES_KEY, JsonStrings(service.IPv6Addresses));
        item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_IS_THIS_PC_KEY,
            JsonBoolean(!m_localDnsHostName.empty() && _wcsicmp(service.HostName.c_str(), m_localDnsHostName.c_str()) == 0));

        peers.Append(item);
    }

    return peers;
}
