// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "pch.h"

#include "midi_service_plugin_version.h"

HRESULT
CMidi2KSMidiPluginMetadataProvider::Initialize()
{
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSMidiPluginMetadataProvider::GetMetadata(
    PTRANSPORTMETADATA metadata)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, metadata);


    //ATL:CComBSTR smallImagePath();
    //smallImagePath.CopyTo(&metadata->SmallImagePath);

    metadata->TransportId = TRANSPORT_LAYER_GUID;

    wil::unique_cotaskmem_string tempString;
    tempString = wil::make_cotaskmem_string_nothrow(TRANSPORT_CODE);
    RETURN_IF_NULL_ALLOC(tempString.get());
    metadata->TransportCode = tempString.release();

    internal::ResourceCopyToCoString(IDS_PLUGIN_METADATA_NAME, &metadata->Name);
    internal::ResourceCopyToCoString(IDS_PLUGIN_METADATA_DESCRIPTION, &metadata->Description);

    wil::unique_cotaskmem_string tempAuthorString;
    tempAuthorString = wil::make_cotaskmem_string_nothrow(internal::GetCurrentModuleVersionCompanyName().c_str());
    RETURN_IF_NULL_ALLOC(tempAuthorString.get());
    metadata->Author = tempAuthorString.release();

    wil::unique_cotaskmem_string tempVersionString;
    tempVersionString = wil::make_cotaskmem_string_nothrow(internal::GetCurrentModuleVersion().c_str());
    RETURN_IF_NULL_ALLOC(tempVersionString.get());
    metadata->Version = tempVersionString.release();

    metadata->SmallImagePath = NULL;   // TODO
//    metadata->ClientConfigurationAssemblyName = NULL;    // No assembly


    metadata->Flags = MetadataFlags_None;

    return S_OK;
}

HRESULT
CMidi2KSMidiPluginMetadataProvider::Shutdown()
{
    return S_OK;
}