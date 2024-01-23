// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"


MidiEndpointTable::MidiEndpointTable() = default;
MidiEndpointTable::~MidiEndpointTable() = default;

MidiEndpointTable& MidiEndpointTable::Current()
{
    // explanation: http://www.modernescpp.com/index.php/thread-safe-initialization-of-data/

    static MidiEndpointTable current;

    return current;
}



HRESULT MidiEndpointTable::Cleanup()
{
    OutputDebugString(__FUNCTION__ L"");

    m_endpointManager = nullptr;
    m_endpoints.clear();

    return S_OK;
}

_Use_decl_annotations_
HRESULT MidiEndpointTable::AddCreatedEndpointDevice(MidiVirtualDeviceEndpointEntry& entry) noexcept
{
    OutputDebugString(__FUNCTION__ L"");

    // index by the association Id
    std::wstring cleanId = internal::ToUpperTrimmedWStringCopy(entry.VirtualEndpointAssociationId);

    m_endpoints[cleanId] = entry;

    return S_OK;
}


_Use_decl_annotations_
HRESULT 
MidiEndpointTable::OnClientConnected(std::wstring clientEndpointInterfaceId, CMidi2VirtualMidiBiDi* clientBiDi)
{
    // get the device BiDi, and then wire them together

    try
    {
        OutputDebugString(__FUNCTION__ L"");

        std::wstring cleanId = internal::NormalizeEndpointInterfaceIdCopy(clientEndpointInterfaceId);

        // look up the association ID in SWD properties

        auto associationId = GetSwdPropertyVirtualEndpointAssociationId(clientEndpointInterfaceId);

        if (associationId != L"")
        {
            if (m_endpoints.find(associationId) != m_endpoints.end())
            {
                // find this id in the table
                auto entry = m_endpoints[associationId];

                if (entry.MidiClientBiDi == nullptr)
                {
                    entry.MidiClientBiDi = clientBiDi;
                    entry.CreatedClientEndpointId = cleanId;

                    entry.MidiDeviceBiDi->LinkAssociatedBiDi(clientBiDi);
                    clientBiDi->LinkAssociatedBiDi(entry.MidiDeviceBiDi);

                    m_endpoints[associationId] = entry;
                }
            }
            else
            {
                // couldn't find the entry
                OutputDebugString(__FUNCTION__ L" - unable to find device table entry");
            }
        }
    }
    CATCH_RETURN();

    return S_OK;
}



_Use_decl_annotations_
HRESULT MidiEndpointTable::OnDeviceConnected(std::wstring deviceEndpointInterfaceId, CMidi2VirtualMidiBiDi* deviceBiDi)
{
    try
    {
        OutputDebugString(__FUNCTION__ L"");

        // look up the association ID in SWD properties
        auto associationId = GetSwdPropertyVirtualEndpointAssociationId(deviceEndpointInterfaceId);

        if (associationId != L"")
        {
            if (m_endpoints.find(associationId) != m_endpoints.end())
            {
                // find this id in the table
                auto entry = m_endpoints[associationId];

                if (entry.MidiDeviceBiDi == nullptr)
                {
                    OutputDebugString(__FUNCTION__ L" - no registered device bidi yet\n");

                    entry.MidiDeviceBiDi = deviceBiDi;
                    m_endpoints[associationId] = entry;

                }
                else
                {
                    // already created. Exit. This happens during protocol negotiation.
                }


                // if we have an endpoint manager, go ahead and create the client endpoint
                if (m_endpointManager && entry.CreatedClientEndpointId == L"")
                {
                    entry.MidiClientBiDi = nullptr;
                    entry.CreatedClientEndpointId = L"";

                    OutputDebugString(__FUNCTION__ L" - Creating client endpoint");

                    // create the client endpoint
                    RETURN_IF_FAILED(m_endpointManager->CreateClientVisibleEndpoint(entry));

                    OutputDebugString(__FUNCTION__ L" - Client endpoint created");

                    m_endpoints[associationId] = entry;


                    // LOG_IF_FAILED(m_endpointManager->NegotiateAndRequestMetadata(entry.CreatedClientEndpointId));


                }
                else
                {
                    OutputDebugString(__FUNCTION__ L" - Endpoint Manager is null or created client endpoint ID is not empty");

                    return E_FAIL;
                }


            }
        }
    }
    CATCH_RETURN();

    return S_OK;
}

_Use_decl_annotations_
HRESULT MidiEndpointTable::OnDeviceDisconnected(std::wstring deviceEndpointInterfaceId)
{
    try
    {
        OutputDebugString(__FUNCTION__ L" - enter\n");

        if (m_endpointManager != nullptr)
        {
            std::wstring associationId = GetSwdPropertyVirtualEndpointAssociationId(deviceEndpointInterfaceId);

            if (associationId != L"")
            {
                OutputDebugString(__FUNCTION__ L" - Getting virtual endpoints table entry\n");

                if (m_endpoints.find(associationId) != m_endpoints.end())
                {
                    auto entry = m_endpoints[associationId];

                    if (entry.MidiClientBiDi != nullptr)
                    {
                        // unlink the bidi devices
                        entry.MidiClientBiDi->UnlinkAssociatedBiDi();
                        entry.MidiClientBiDi = nullptr;
                    }

                    if (entry.MidiDeviceBiDi != nullptr)
                    {
                        // unlink the bidi devices
                        entry.MidiDeviceBiDi->UnlinkAssociatedBiDi();
                        entry.MidiDeviceBiDi = nullptr;

                        // deactivate the client
                        m_endpointManager->DeleteClientEndpoint(entry.CreatedShortClientInstanceId);

                        entry.CreatedShortClientInstanceId = L"";
                        entry.CreatedClientEndpointId = L"";
                    }

                    // update with changes
                    m_endpoints[associationId] = entry;

                }
                else
                {
                    OutputDebugString(__FUNCTION__ L" - no entry for associationId\n");
                }
            }
            else
            {
                OutputDebugString(__FUNCTION__ L" - unable to get association Id\n");
            }
        }
        else
        {
            OutputDebugString(__FUNCTION__ L" - endpoint manager is null\n");
        }

        OutputDebugString(__FUNCTION__ L" - exit\n");
    }
    CATCH_LOG();

    return S_OK;
}


