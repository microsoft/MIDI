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
    PABSTRACTIONMETADATA metadata)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, metadata);

    metadata->Id = ABSTRACTION_LAYER_GUID;
    metadata->TransportCode = TRANSPORT_CODE;

    internal::ResourceCopyToBSTR(IDS_PLUGIN_METADATA_NAME, &metadata->Name);
    internal::ResourceCopyToBSTR(IDS_PLUGIN_METADATA_DESCRIPTION, &metadata->Description);
    internal::ResourceCopyToBSTR(IDS_PLUGIN_METADATA_AUTHOR, &metadata->Author);
    internal::ResourceCopyToBSTR(IDS_PLUGIN_METADATA_VERSION, &metadata->Version);

    metadata->SmallImagePath = NULL;                        // TODO
    //metadata->ClientConfigurationAssemblyName = NULL;       // TODO

    metadata->IsRuntimeCreatableByApps = true;
    metadata->IsRuntimeCreatableBySettings = true;

    metadata->IsSystemManaged = false;
    metadata->IsClientConfigurable = true;

    return S_OK;
}

HRESULT
CMidi2LoopbackMidiPluginMetadataProvider::Cleanup()
{
    return S_OK;
}