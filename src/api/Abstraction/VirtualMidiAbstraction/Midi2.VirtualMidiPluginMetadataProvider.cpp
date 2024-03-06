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
CMidi2VirtualMidiPluginMetadataProvider::Initialize()
{
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiPluginMetadataProvider::GetMetadata(
    PABSTRACTIONMETADATA metadata)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, metadata);

    metadata->Id = ABSTRACTION_LAYER_GUID;
    metadata->Mnemonic = TRANSPORT_MNEMONIC;

    internal::ResourceCopyToBSTR(IDS_PLUGIN_METADATA_NAME, &metadata->Name);
    internal::ResourceCopyToBSTR(IDS_PLUGIN_METADATA_DESCRIPTION, &metadata->Description);
    internal::ResourceCopyToBSTR(IDS_PLUGIN_METADATA_AUTHOR, &metadata->Author);
    internal::ResourceCopyToBSTR(IDS_PLUGIN_METADATA_VERSION, &metadata->Version);

    metadata->SmallImagePath = NULL;                        // TODO
    metadata->ClientConfigurationAssemblyName = NULL;       // TODO

    metadata->IsRuntimeCreatableByApps = true;
    metadata->IsRuntimeCreatableBySettings = false;

    metadata->IsSystemManaged = false;
    metadata->IsClientConfigurable = false;

    return S_OK;
}

HRESULT
CMidi2VirtualMidiPluginMetadataProvider::Cleanup()
{
    return S_OK;
}