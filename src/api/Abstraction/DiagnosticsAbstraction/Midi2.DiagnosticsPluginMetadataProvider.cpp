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
CMidi2DiagnosticsPluginMetadataProvider::Initialize()
{
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2DiagnosticsPluginMetadataProvider::GetMetadata(
    PTRANSPORTMETADATA metadata)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, metadata);

    metadata->TransportId = ABSTRACTION_LAYER_GUID;

    wil::unique_cotaskmem_string tempString;
    tempString = wil::make_cotaskmem_string_nothrow(TRANSPORT_CODE);
    RETURN_IF_NULL_ALLOC(tempString.get());
    metadata->TransportCode = tempString.release();

    internal::ResourceCopyToCoString(IDS_PLUGIN_METADATA_NAME, &metadata->Name);
    internal::ResourceCopyToCoString(IDS_PLUGIN_METADATA_DESCRIPTION, &metadata->Description);
    internal::ResourceCopyToCoString(IDS_PLUGIN_METADATA_AUTHOR, &metadata->Author);
    internal::ResourceCopyToCoString(IDS_PLUGIN_METADATA_VERSION, &metadata->Version);

    metadata->SmallImagePath = NULL;                        // TODO

    metadata->Flags = MetadataFlags_IsSystemManaged;

    return S_OK;
}

HRESULT
CMidi2DiagnosticsPluginMetadataProvider::Shutdown()
{
    return S_OK;
}