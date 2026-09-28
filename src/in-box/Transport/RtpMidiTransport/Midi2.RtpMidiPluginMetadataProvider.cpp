// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Describes the transport to the service and the settings app.
// ============================================================================

#include "pch.h"

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
        RETURN_IF_FAILED(internal::ResourceCopyToCoString(IDS_PLUGIN_METADATA_AUTHOR, &metadata->Author));
        RETURN_IF_FAILED(internal::ResourceCopyToCoString(IDS_PLUGIN_METADATA_VERSION, &metadata->Version));

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
