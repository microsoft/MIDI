// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
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
    auto mt = internal::GetUmpMessageTypeFromFirstWord(word0);

    // check to see if we support this message type
    if (m_includedMessageTypes[mt])
    {
        if (internal::MessageHasGroupField(word0))
        {
            // if group is not in our list, return
            auto groupIndex = internal::GetGroupIndexFromFirstWord(word0);

            if (!m_includedGroupIndexes[groupIndex])
            {
                return S_OK;
            }
        }

        RETURN_IF_FAILED(m_router->Callback(data, length, position, context));
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualPatchBayRoutingSource::SetIncludedGroups(
    std::vector<uint8_t> groupIndexes
)
{
    if (groupIndexes.size() > 0)
    {
        m_includedGroupIndexes = { false };

        for (auto const messageType : groupIndexes)
        {
            if (messageType < 16)
            {
                m_includedGroupIndexes[messageType] = true;
            }
        }
    }
    else
    {
        // if an empty set is provided, all groups are included
        m_includedGroupIndexes = { true };
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualPatchBayRoutingSource::SetIncludedMessageTypes(
    std::vector<uint8_t> messageTypes
)
{
    if (messageTypes.size() > 0)
    {
        m_includedMessageTypes = { false };

        for (auto const messageType : messageTypes)
        {
            if (messageType < 16)
            {
                m_includedMessageTypes[messageType] = true;
            }
        }
    }
    else
    {
        // if an empty set is provided, all message types are included
        m_includedMessageTypes = { true };
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
CMidi2VirtualPatchBayRoutingSource::Shutdown()
{
    m_router = nullptr;

    return S_OK;
}