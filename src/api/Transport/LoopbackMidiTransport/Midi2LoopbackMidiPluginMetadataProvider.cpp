// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "pch.h"

HRESULT
CMidi2LoopbackMidiPluginMetadataProvider::Initialize()
{
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2LoopbackMidiPluginMetadataProvider::GetMetadata(
    PTRANSPORTMETADATA metadata)
{
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

    UINT32 flags {0};
    flags |= MetadataFlags_IsRuntimeCreatableByApps;
    flags |= MetadataFlags_IsRuntimeCreatableBySettings;
    flags |= MetadataFlags_IsClientConfigurable;
    metadata->Flags = (MetadataFlags) flags;

    return S_OK;
}

HRESULT
CMidi2LoopbackMidiPluginMetadataProvider::Shutdown()
{
    return S_OK;
}