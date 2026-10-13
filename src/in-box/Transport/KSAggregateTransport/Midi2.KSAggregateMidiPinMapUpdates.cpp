// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

// Opening and closing an aggregate endpoint so that it also picks up pins the device adds or
// removes while the endpoint is open (Feature_Servicing_MIDI2KSAPinChangesWhileOpen). Initialize
// and Shutdown in Midi2.KSAggregateMidi.cpp are left as they shipped.

#include "pch.h"


_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidi::InitializeFollowingPinMapChanges(
    LPCWSTR endpointDeviceInterfaceId,
    MidiFlow flow,
    PTRANSPORTCREATIONPARAMS /* creationParams */,
    DWORD* mmCssTaskId,
    IMidiCallback* callback,
    LONGLONG context
)
try
{
    RETURN_HR_IF(E_INVALIDARG, callback == nullptr);
    RETURN_HR_IF(E_INVALIDARG, mmCssTaskId == nullptr);
    RETURN_HR_IF(E_INVALIDARG, endpointDeviceInterfaceId == nullptr);
    RETURN_HR_IF(E_INVALIDARG, flow != MidiFlow::MidiFlowBidirectional);

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Initializing, following pin map changes", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    m_callback = callback;
    m_proxyContext = context;
    m_followedEndpointDeviceId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);

    // we don't actually use a buffer, but this is needed to initialize
    // the KS code which will handle the ioctls
    m_proxyBufferSize = PAGE_SIZE;
    RETURN_IF_FAILED(GetRequiredBufferSize(m_proxyBufferSize));

    RETURN_IF_FAILED(m_pinMapSubscription.Begin(
        TransportState::Current().OpenEndpointPinMaps(),
        m_followedEndpointDeviceId,
        [this](std::vector<KsaPinMap::Entry> const& entries)
        {
            return ApplyPinMap(entries, false, &m_mmCssTaskIdForAddedPins);
        },
        [&]() -> HRESULT
        {
            std::vector<KsaPinMap::Entry> entries{ };
            RETURN_IF_FAILED(ReadPinMap(endpointDeviceInterfaceId, entries));

            auto const opened = ApplyPinMap(entries, true, mmCssTaskId);

            if (FAILED(opened))
            {
                // an endpoint which failed to open is never used, so close what did open
                LOG_IF_FAILED(ApplyPinMap({ }, false, mmCssTaskId));
                RETURN_IF_FAILED(opened);
            }

            m_mmCssTaskIdForAddedPins = *mmCssTaskId;

            return S_OK;
        }));

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Completed all initialization", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingUInt32(static_cast<uint32_t>(m_openPins.size()), "open pin count")
    );

    return S_OK;
}
CATCH_RETURN();


HRESULT
CMidi2KSAggregateMidi::ShutdownFollowingPinMapChanges()
{
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_followedEndpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    // waits for a change being applied, and stops any more, before the proxies go away
    m_pinMapSubscription.End();

    m_openPins.clear();

    return Shutdown();
}


_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidi::ReadPinMap(
    LPCWSTR endpointDeviceInterfaceId,
    std::vector<KsaPinMap::Entry>& entries
)
try
{
    entries.clear();

    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
    additionalProperties.Append(STRING_DEVPKEY_KsAggMidiGroupPinMap);

    auto deviceInfo = winrt::Windows::Devices::Enumeration::DeviceInformation::CreateFromIdAsync(
        endpointDeviceInterfaceId,
        additionalProperties,
        winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

    auto pinMap = internal::SafeGetSwdBinaryPropertyFromDeviceInformation(STRING_DEVPKEY_KsAggMidiGroupPinMap, deviceInfo);

    if (pinMap == nullptr)
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Endpoint device properties are missing the pin map", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        return E_FAIL;
    }

    auto const data = pinMap.Value();
    auto const parsed = KsaPinMap::Parse(data.data(), data.size(), entries);

    if (FAILED(parsed))
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Pin map is malformed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
            TraceLoggingUInt32(static_cast<uint32_t>(data.size()), "Pin map byte count"),
            TraceLoggingHResult(parsed, MIDI_TRACE_EVENT_HRESULT_FIELD)
        );

        RETURN_IF_FAILED(parsed);
    }

    return S_OK;
}
CATCH_RETURN();


// Called only while the subscription holds back deliveries, so never twice at once.
_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidi::ApplyPinMap(
    std::vector<KsaPinMap::Entry> const& entries,
    bool stopAtFirstFailure,
    DWORD* mmCssTaskId
)
try
{
    auto const changes = KsaPinMap::PlanChanges(m_openPins, entries);

    if (changes.ToClose.empty() && changes.ToOpen.empty())
    {
        return S_OK;
    }

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Applying pin map", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_followedEndpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingUInt32(static_cast<uint32_t>(changes.ToClose.size()), "pins to close"),
        TraceLoggingUInt32(static_cast<uint32_t>(changes.ToOpen.size()), "pins to open")
    );

    for (auto const& entry : changes.ToClose)
    {
        CloseProxyForPin(entry);
    }

    HRESULT result{ S_OK };

    for (auto const& entry : changes.ToOpen)
    {
        auto const opened = OpenProxyForPin(entry, mmCssTaskId);

        if (FAILED(opened))
        {
            TraceLoggingWrite(
                MidiKSAggregateTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Unable to open pin", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_followedEndpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                TraceLoggingWideString(entry.FilterId.c_str(), "Filter Id"),
                TraceLoggingUInt32(entry.PinId, "Pin"),
                TraceLoggingUInt8(entry.GroupIndex, "Group Index"),
                TraceLoggingHResult(opened, MIDI_TRACE_EVENT_HRESULT_FIELD)
            );

            if (stopAtFirstFailure)
            {
                RETURN_IF_FAILED(opened);
            }

            // keep going, so one pin that won't open doesn't take the others with it
            if (SUCCEEDED(result))
            {
                result = opened;
            }
        }
    }

    CloseFiltersNoLongerUsed();

    return result;
}
CATCH_RETURN();


_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidi::OpenProxyForPin(
    KsaPinMap::Entry const& entry,
    DWORD* mmCssTaskId
)
{
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Opening pin", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_followedEndpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingWideString(entry.FilterId.c_str(), "Filter Id"),
        TraceLoggingUInt32(entry.PinId, "Pin"),
        TraceLoggingUInt8(entry.GroupIndex, "Group Index"),
        TraceLoggingWideString(entry.PinDataFlow == MidiFlow::MidiFlowOut ? L"Source" : L"Destination", "Flow Type")
    );

    // so recording the pin below cannot fail after its proxy is running
    m_openPins.reserve(m_openPins.size() + 1);

    KsHandleWrapper* filterWrapper{ nullptr };

    auto const openFilter = m_openKsFilters.find(entry.FilterId);

    if (openFilter != m_openKsFilters.end())
    {
        filterWrapper = openFilter->second.get();
    }
    else
    {
        auto newWrapper = std::make_unique<KsHandleWrapper>(entry.FilterId);
        RETURN_IF_FAILED(newWrapper->Open());

        auto [inserted, _] = m_openKsFilters.insert_or_assign(entry.FilterId, std::move(newWrapper));
        filterWrapper = inserted->second.get();
    }

    // a duplicate this function owns, or null once the device is gone
    wil::unique_handle filterHandle(filterWrapper->GetHandle());
    RETURN_HR_IF_NULL(E_HANDLE, filterHandle);

    if (entry.PinDataFlow == MidiFlow::MidiFlowOut)
    {
        // pin is a message source, so this is a MIDI In

        wil::com_ptr_nothrow<CMidi2KSAggregateMidiInProxy> proxy;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2KSAggregateMidiInProxy>(&proxy));

        auto const initialized = proxy->Initialize(
            entry.FilterId.c_str(),
            filterHandle.get(),
            entry.PinId,
            m_proxyBufferSize,
            mmCssTaskId,
            m_callback,
            m_proxyContext,
            entry.GroupIndex);

        if (FAILED(initialized))
        {
            LOG_IF_FAILED(proxy->Shutdown());
            RETURN_IF_FAILED(initialized);
        }

        try
        {
            m_midiInDeviceGroupMap.insert_or_assign(entry.GroupIndex, proxy);
        }
        catch (...)
        {
            LOG_IF_FAILED(proxy->Shutdown());
            throw;
        }
    }
    else
    {
        // pin is a message destination, so this is a MIDI Out

        wil::com_ptr_nothrow<CMidi2KSAggregateMidiOutProxy> proxy;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2KSAggregateMidiOutProxy>(&proxy));

        auto const initialized = proxy->Initialize(
            entry.FilterId.c_str(),
            filterHandle.get(),
            entry.PinId,
            m_proxyBufferSize,
            mmCssTaskId,
            m_proxyContext,
            entry.GroupIndex);

        if (FAILED(initialized))
        {
            LOG_IF_FAILED(proxy->Shutdown());
            RETURN_IF_FAILED(initialized);
        }

        try
        {
            // SendMidiMessage reads this map under the send lock
            auto sendLock = m_SendLock.lock();
            m_midiOutDeviceGroupMap.insert_or_assign(entry.GroupIndex, proxy);
        }
        catch (...)
        {
            LOG_IF_FAILED(proxy->Shutdown());
            throw;
        }
    }

    m_openPins.push_back(entry);

    return S_OK;
}


_Use_decl_annotations_
void
CMidi2KSAggregateMidi::CloseProxyForPin(
    KsaPinMap::Entry const& entry
)
{
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Closing pin", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_followedEndpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingWideString(entry.FilterId.c_str(), "Filter Id"),
        TraceLoggingUInt32(entry.PinId, "Pin"),
        TraceLoggingUInt8(entry.GroupIndex, "Group Index"),
        TraceLoggingWideString(entry.PinDataFlow == MidiFlow::MidiFlowOut ? L"Source" : L"Destination", "Flow Type")
    );

    if (entry.PinDataFlow == MidiFlow::MidiFlowOut)
    {
        auto const open = m_midiInDeviceGroupMap.find(entry.GroupIndex);

        if (open != m_midiInDeviceGroupMap.end())
        {
            auto proxy = open->second;
            m_midiInDeviceGroupMap.erase(open);

            LOG_IF_FAILED(proxy->Shutdown());
        }
    }
    else
    {
        wil::com_ptr_nothrow<CMidi2KSAggregateMidiOutProxy> proxy;

        {
            // out of the map under the send lock first, so no send is using it when it shuts down
            auto sendLock = m_SendLock.lock();

            auto const open = m_midiOutDeviceGroupMap.find(entry.GroupIndex);

            if (open != m_midiOutDeviceGroupMap.end())
            {
                proxy = open->second;
                m_midiOutDeviceGroupMap.erase(open);
            }
        }

        if (proxy)
        {
            LOG_IF_FAILED(proxy->Shutdown());
        }
    }

    m_openPins.erase(
        std::remove_if(m_openPins.begin(), m_openPins.end(), [&entry](KsaPinMap::Entry const& open) { return KsaPinMap::SameSlot(open, entry); }),
        m_openPins.end());
}


void
CMidi2KSAggregateMidi::CloseFiltersNoLongerUsed()
{
    for (auto filter = m_openKsFilters.begin(); filter != m_openKsFilters.end();)
    {
        auto const& filterId = filter->first;

        bool const inUse = std::any_of(m_openPins.begin(), m_openPins.end(), [&filterId](KsaPinMap::Entry const& open)
            {
                return CompareStringOrdinal(open.FilterId.c_str(), -1, filterId.c_str(), -1, TRUE) == CSTR_EQUAL;
            });

        if (inUse)
        {
            ++filter;
        }
        else
        {
            filter = m_openKsFilters.erase(filter);
        }
    }
}
