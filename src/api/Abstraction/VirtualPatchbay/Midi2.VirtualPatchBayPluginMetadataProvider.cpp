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
CMidi2VirtualPatchBayPluginMetadataProvider::Initialize()
{
    TraceLoggingWrite(
        MidiVirtualPatchBayAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualPatchBayPluginMetadataProvider::GetMetadata(
    PABSTRACTIONMETADATA metadata)
{
    TraceLoggingWrite(
        MidiVirtualPatchBayAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, metadata);

    metadata->Id = ABSTRACTION_LAYER_GUID;
    metadata->TransportCode = TRANSPORT_CODE;

    internal::ResourceCopyToBSTR(IDS_PLUGIN_METADATA_NAME, &metadata->Name);
    internal::ResourceCopyToBSTR(IDS_PLUGIN_METADATA_DESCRIPTION, &metadata->Description);
    internal::ResourceCopyToBSTR(IDS_PLUGIN_METADATA_AUTHOR, &metadata->Author);
    internal::ResourceCopyToBSTR(IDS_PLUGIN_METADATA_VERSION, &metadata->Version);

    metadata->SmallImagePath = NULL;                        // TODO
    //metadata->ClientConfigurationAssemblyName = NULL;

    metadata->IsRuntimeCreatableByApps = false;
    metadata->IsRuntimeCreatableBySettings = true;

    metadata->IsSystemManaged = false;
    metadata->IsClientConfigurable = true;

    return S_OK;
}


HRESULT
CMidi2VirtualPatchBayPluginMetadataProvider::Cleanup()
{
    TraceLoggingWrite(
        MidiVirtualPatchBayAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    return S_OK;
}