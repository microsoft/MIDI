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

    for (uint8_t i = 0; i < 16; i++)
    {
        m_groupTransformMap[i] = i;
    }

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
    UNREFERENCED_PARAMETER(context);

    RETURN_HR_IF_NULL(E_INVALIDARG, data);
    RETURN_HR_IF_NULL(E_FAIL, m_router);

    auto word0 = internal::MidiWord0FromVoidMessageDataPointer(data);

    byte* newData{ nullptr };

    // if this is a groupless message
    if (internal::MessageHasGroupField(word0))
    {
        // TODO: Perform any group transformations and create new message data








    }
    else
    {
        // nothing changes, so just send original data
        newData = (byte*)data;
    }

    RETURN_IF_FAILED(m_endpointBidi->SendMidiMessage(newData, length, position));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualPatchBayRoutingDestination::SetGroupTransforms(
    std::map<uint8_t, uint8_t> groupTransformMap
)
{
    // reset all groups before applying any transform
    for (uint8_t i = 0; i < 16; i++)
    {
        m_groupTransformMap[i] = i;
    }

    // apply only the transforms presented
    for (auto transform : groupTransformMap)
    {
        if (transform.first < 16)
        {
            m_groupTransformMap[transform.first] = transform.second;
        }
    }

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2VirtualPatchBayRoutingDestination::SetEndpointBidi(
    LPCWSTR resolvedEndpointDeviceInterfaceId, 
    IMidiBidirectional* destinationEndpointBidi)
{
    UNREFERENCED_PARAMETER(resolvedEndpointDeviceInterfaceId);
    UNREFERENCED_PARAMETER(destinationEndpointBidi);

    return S_OK;
}

HRESULT
CMidi2VirtualPatchBayRoutingDestination::Cleanup()
{
    m_router = nullptr;

    return S_OK;
}