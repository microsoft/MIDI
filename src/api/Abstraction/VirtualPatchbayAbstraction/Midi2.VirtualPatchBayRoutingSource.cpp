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
    m_matchJson = endpointMatchJson;
    m_router = router;

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
    RETURN_HR_IF_NULL(E_INVALIDARG, data);
    RETURN_HR_IF_NULL(E_FAIL, m_router);

    // Verify message meets criteria.

    auto word0 = internal::MidiWord0FromVoidMessageDataPointer(data);

    // if this is a groupless message
    if (internal::MessageHasGroupField(word0))
    {
        // if group is not in our list, return
        auto groupIndex = internal::GetGroupIndexFromFirstWord(word0);

        if (!m_includedGroupIndexes[groupIndex])
        {
            return S_OK;
        }
    }
    else
    {
        // message has no group field
        if (!m_includeGrouplessMessages)
        {
            return S_OK;
        }
    }

    RETURN_IF_FAILED(m_router->Callback(data, length, position, context));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualPatchBayRoutingSource::AddIncludedGroups(
    std::vector<uint8_t> groupIndexes
)
{
    for (auto const index : groupIndexes)
    {
        if (index < 16)
        {
            m_includedGroupIndexes[index] = true;
        }
    }

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

HRESULT
CMidi2VirtualPatchBayRoutingSource::Cleanup()
{
    m_router = nullptr;

    return S_OK;
}