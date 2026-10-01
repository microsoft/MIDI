// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Describes the transport to the service and the settings app.
// ============================================================================

#include "pch.h"

#include "midi_service_plugin_version.h"

HRESULT
CMidi2RtpMidiPluginMetadataProvider::Initialize()
{
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2RtpMidiPluginMetadataProvider::GetMetadata(PTRANSPORTMETADATA metadata)
{
    try
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, metadata);

        metadata->TransportId = TRANSPORT_LAYER_GUID;

        auto transportCode = wil::make_cotaskmem_string_nothrow(TRANSPORT_CODE);
        RETURN_IF_NULL_ALLOC(transportCode.get());
        metadata->TransportCode = transportCode.release();

        RETURN_IF_FAILED(internal::ResourceCopyToCoString(IDS_PLUGIN_METADATA_NAME, &metadata->Name));
        RETURN_IF_FAILED(internal::ResourceCopyToCoString(IDS_PLUGIN_METADATA_DESCRIPTION, &metadata->Description));

        auto author = wil::make_cotaskmem_string_nothrow(internal::GetCurrentModuleVersionCompanyName().c_str());
        RETURN_IF_NULL_ALLOC(author.get());
        metadata->Author = author.release();

        auto version = wil::make_cotaskmem_string_nothrow(internal::GetCurrentModuleVersion().c_str());
        RETURN_IF_NULL_ALLOC(version.get());
        metadata->Version = version.release();

        metadata->SmallImagePath = nullptr;

        metadata->Flags = (MetadataFlags)(
            MetadataFlags::MetadataFlags_IsRuntimeCreatableBySettings |
            MetadataFlags::MetadataFlags_IsClientConfigurable);

        return S_OK;
    }
    CATCH_RETURN();
}

HRESULT
CMidi2RtpMidiPluginMetadataProvider::Shutdown()
{
    return S_OK;
}
