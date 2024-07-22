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
CMidi2VirtualPatchBayRoutingDestination::Initialize(
    LPCWSTR endpointMatchJson, 
    CMidi2VirtualPatchBayRoutingEntry* router
)
{
    m_matchJson = endpointMatchJson;
    m_router = router;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualPatchBayRoutingDestination::Callback(
    PVOID data, 
    UINT length, 
    LONGLONG position, 
    LONGLONG context)
{
    UNREFERENCED_PARAMETER(data);
    UNREFERENCED_PARAMETER(length);
    UNREFERENCED_PARAMETER(position);
    UNREFERENCED_PARAMETER(context);

    // TODO: Perform any group transformations

    // TODO: Call the BiDi for the actual destination


    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualPatchBayRoutingDestination::SetGroupTransforms(
    std::map<uint8_t, uint8_t> groupTransformMap
)
{
    UNREFERENCED_PARAMETER(groupTransformMap);

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2VirtualPatchBayRoutingDestination::SetEndpointBiDi(
    LPCWSTR resolvedEndpointDeviceInterfaceId, 
    IMidiBiDi* destinationEndpointBiDi)
{
    UNREFERENCED_PARAMETER(resolvedEndpointDeviceInterfaceId);
    UNREFERENCED_PARAMETER(destinationEndpointBiDi);

    return S_OK;
}

HRESULT
CMidi2VirtualPatchBayRoutingDestination::Cleanup()
{

    return S_OK;
}