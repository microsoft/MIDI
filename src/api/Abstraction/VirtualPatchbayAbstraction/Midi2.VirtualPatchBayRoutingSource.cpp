// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

_Use_decl_annotations_ 
HRESULT 
CMidi2VirtualPatchBayRoutingSource::Initialize(
    LPCWSTR endpointMatchJson, 
    CMidi2VirtualPatchBayRoutingEntry* router
)
{
    UNREFERENCED_PARAMETER(endpointMatchJson);
    UNREFERENCED_PARAMETER(router);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualPatchBayRoutingSource::Callback(
    PVOID data, 
    UINT length, 
    LONGLONG position, 
    LONGLONG context
)
{
    UNREFERENCED_PARAMETER(data);
    UNREFERENCED_PARAMETER(length);
    UNREFERENCED_PARAMETER(position);
    UNREFERENCED_PARAMETER(context);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualPatchBayRoutingSource::AddIncludedGroups(
    std::vector<uint8_t> groupIndexes
)
{
    UNREFERENCED_PARAMETER(groupIndexes);

    
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualPatchBayRoutingSource::SetEndpointCallback(
    LPCWSTR resolvedEndpointDeviceInterfaceId, 
    IMidiCallback* sourceEndpointCallback
)
{
    UNREFERENCED_PARAMETER(resolvedEndpointDeviceInterfaceId);
    UNREFERENCED_PARAMETER(sourceEndpointCallback);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualPatchBayRoutingSource::Cleanup()
{

    return S_OK;
}