// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include "pch.h"


HRESULT
CMidi2NetworkMidiPluginMetadataProvider::Initialize()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );



    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiPluginMetadataProvider::GetMetadata(
    PTRANSPORTMETADATA metadata)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, metadata);

    metadata->TransportId = TRANSPORT_LAYER_GUID;

    wil::unique_cotaskmem_string tempString;
    tempString = wil::make_cotaskmem_string_nothrow(TRANSPORT_CODE);
    RETURN_IF_NULL_ALLOC(tempString.get());
    metadata->TransportCode = tempString.release();

    internal::ResourceCopyToCoString(IDS_PLUGIN_METADATA_NAME, &metadata->Name);
    internal::ResourceCopyToCoString(IDS_PLUGIN_METADATA_DESCRIPTION, &metadata->Description);
    internal::ResourceCopyToCoString(IDS_PLUGIN_METADATA_AUTHOR, &metadata->Author);
    internal::ResourceCopyToCoString(IDS_PLUGIN_METADATA_VERSION, &metadata->Version);

    metadata->SmallImagePath = NULL;                        // TODO
    //metadata->ClientConfigurationAssemblyName = NULL;       // TODO

    metadata->Flags = (MetadataFlags)(
        MetadataFlags::MetadataFlags_IsRuntimeCreatableBySettings | 
        MetadataFlags::MetadataFlags_IsClientConfigurable
        );


    return S_OK;
}

HRESULT
CMidi2NetworkMidiPluginMetadataProvider::Shutdown()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    return S_OK;
}