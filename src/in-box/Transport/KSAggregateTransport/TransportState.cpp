// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "Feature_Servicing_MIDI2KSAWatcherHardening.h"


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
    if (Feature_Servicing_MIDI2KSAWatcherHardening::IsEnabled())
    {
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2KSAggregateMidiEndpointManager3>(&m_endpointManager3));
    }
    else
    {
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2KSAggregateMidiEndpointManager2>(&m_endpointManager2));
    }

    return S_OK;
}


HRESULT
TransportState::ConstructConfigurationManager()
{
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2KSAggregateMidiConfigurationManager>(&m_configurationManager));

    return S_OK;
}


_Use_decl_annotations_
HRESULT
TransportState::PublishPinMapToOpenEndpoint(
    std::wstring const& endpointDeviceId,
    std::vector<std::byte> const& pinMapPropertyData
)
try
{
    std::vector<KsaPinMap::Entry> entries{ };

    RETURN_IF_FAILED(KsaPinMap::Parse(
        reinterpret_cast<uint8_t const*>(pinMapPropertyData.data()),
        pinMapPropertyData.size(),
        entries));

    auto const normalizedEndpointDeviceId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceId);

    size_t appliedCount{ 0 };
    auto const published = m_openEndpointPinMaps.Publish(normalizedEndpointDeviceId, entries, &appliedCount);

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Pin map published to the open instances of the endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(normalizedEndpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingUInt32(static_cast<uint32_t>(entries.size()), "pin count"),
        TraceLoggingUInt32(static_cast<uint32_t>(appliedCount), "open instances updated"),
        TraceLoggingHResult(published, MIDI_TRACE_EVENT_HRESULT_FIELD)
    );

    RETURN_IF_FAILED(published);

    return S_OK;
}
CATCH_RETURN();
