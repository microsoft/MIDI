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
CMidi2VirtualPatchBayRoutingEntry::Initialize(
    LPCWSTR associationId, 
    LPCWSTR name, 
    LPCWSTR description
)
{
    m_associationId = associationId;
    m_name = name;
    m_description = description;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualPatchBayRoutingEntry::AddSource(
    CMidi2VirtualPatchBayRoutingSource* source
)
{
    UNREFERENCED_PARAMETER(source);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualPatchBayRoutingEntry::AddDestination(
    CMidi2VirtualPatchBayRoutingDestination* destination
)
{
    UNREFERENCED_PARAMETER(destination);

    return S_OK;
}

// all sources call this. This then replicates the messages to all destinations.
// destinations are responsible for any group mapping etc.
_Use_decl_annotations_
HRESULT
CMidi2VirtualPatchBayRoutingEntry::Callback(
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

HRESULT
CMidi2VirtualPatchBayRoutingEntry::Cleanup()
{


    return S_OK;
}
