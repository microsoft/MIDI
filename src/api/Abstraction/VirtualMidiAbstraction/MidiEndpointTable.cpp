// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

HRESULT MidiEndpointTable::Cleanup()
{
    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    m_endpoints.clear();

    return S_OK;
}

_Use_decl_annotations_
HRESULT MidiEndpointTable::AddCreatedEndpointDevice(MidiVirtualDeviceEndpointEntry& entry) noexcept
{
    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(entry.VirtualEndpointAssociationId.c_str(), "entry.VirtualEndpointAssociationId"),
        TraceLoggingWideString(entry.BaseEndpointName.c_str(), "entry.BaseEndpointName"),
        TraceLoggingWideString(entry.ShortUniqueId.c_str(), "entry.ShortUniqueId"),
        TraceLoggingWideString(entry.CreatedClientEndpointId.c_str(), "entry.CreatedClientEndpointId"),
        TraceLoggingWideString(entry.CreatedDeviceEndpointId.c_str(), "entry.CreatedDeviceEndpointId"),
        TraceLoggingWideString(entry.CreatedShortClientInstanceId.c_str(), "entry.CreatedShortClientInstanceId")
        );

    // index by the association Id
    std::wstring cleanId = internal::ToUpperTrimmedWStringCopy(entry.VirtualEndpointAssociationId);
    entry.VirtualEndpointAssociationId = cleanId;

    m_endpoints[cleanId] = entry;

    return S_OK;
}


_Use_decl_annotations_
HRESULT 
MidiEndpointTable::OnClientConnected(
    std::wstring clientEndpointInterfaceId, 
    CMidi2VirtualMidiBiDi* clientBiDi) noexcept
{
    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(clientEndpointInterfaceId.c_str())
    );

    // get the device BiDi, and then wire them together

    try
    {
        RETURN_HR_IF_NULL(E_UNEXPECTED, clientBiDi);

        std::wstring cleanId = internal::NormalizeEndpointInterfaceIdWStringCopy(clientEndpointInterfaceId);

        // look up the association ID in SWD properties

        auto associationId = GetSwdPropertyVirtualEndpointAssociationId(clientEndpointInterfaceId);

        if (associationId != L"")
        {
            if (m_endpoints.find(associationId) != m_endpoints.end())
            {
                // find this id in the table
                auto entry = m_endpoints[associationId];

                RETURN_HR_IF_NULL(E_UNEXPECTED, entry.MidiDeviceBiDi);

                // add the client
                //entry.MidiClientConnections.push_back(clientBiDi);
                entry.MidiDeviceBiDi->LinkAssociatedBiDi(clientBiDi);

                clientBiDi->LinkAssociatedBiDi(entry.MidiDeviceBiDi);

                m_endpoints[associationId] = entry;
            }
            else
            {
                // couldn't find the entry
                TraceLoggingWrite(
                    MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                    __FUNCTION__,
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Unable to find device table entry", "message")
                );
            }
        }
    }
    CATCH_RETURN();

    return S_OK;
}


_Use_decl_annotations_
HRESULT
MidiEndpointTable::OnClientDisconnected(
    std::wstring clientEndpointInterfaceId, 
    CMidi2VirtualMidiBiDi* clientBiDi) noexcept
{
    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(clientEndpointInterfaceId.c_str())
    );

    try
    {
        std::wstring associationId = GetSwdPropertyVirtualEndpointAssociationId(clientEndpointInterfaceId);

        if (associationId != L"")
        {
            if (m_endpoints.find(associationId) != m_endpoints.end())
            {
                // find this id in the table and then remove it
                auto entry = m_endpoints[associationId];

                entry.MidiDeviceBiDi->UnlinkAssociatedBiDi(clientBiDi);
            }
            else
            {
                // association id isn't present. That's not right.

                TraceLoggingWrite(
                    MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                    __FUNCTION__,
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Association id property was not present in device table", "message")
                );
            }
        }
        else
        {
            // association id is blank, which is also not right

            TraceLoggingWrite(
                MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Association id property was blank", "message")
            );
        }
    }
    CATCH_RETURN();


    return S_OK;
}



_Use_decl_annotations_
HRESULT MidiEndpointTable::OnDeviceConnected(std::wstring deviceEndpointInterfaceId, CMidi2VirtualMidiBiDi* deviceBiDi) noexcept
{
    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(deviceEndpointInterfaceId.c_str(), "deviceEndpointInterfaceId")
    );

    try
    {
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
                    entry.MidiDeviceBiDi = deviceBiDi;
                    m_endpoints[associationId] = entry;
                }
                else
                {
                    // already created. Ignore. This happens during protocol negotiation.
                }

                // if we have an endpoint manager, go ahead and create the client endpoint
                if (AbstractionState::Current().GetEndpointManager())
                {
                    entry.CreatedClientEndpointId = L"";
                    entry.CreatedShortClientInstanceId = L"";

                    RETURN_IF_FAILED(AbstractionState::Current().GetEndpointManager()->CreateClientVisibleEndpoint(entry));

                    TraceLoggingWrite(
                        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                        __FUNCTION__,
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"After creating client visible endpoint", "message"),
                        TraceLoggingWideString(deviceEndpointInterfaceId.c_str(), "Endpoint interface id"),
                        TraceLoggingWideString(entry.BaseEndpointName.c_str(), "Base endpoint name"),
                        TraceLoggingWideString(entry.CreatedClientEndpointId.c_str(), "Created Client Endpoint Id"),
                        TraceLoggingWideString(entry.CreatedShortClientInstanceId.c_str(), "Created Short Client Instance Id"),
                        TraceLoggingWideString(entry.CreatedDeviceEndpointId.c_str(), "Created Device Endpoint Id"),
                        TraceLoggingWideString(entry.CreatedShortDeviceInstanceId.c_str(), "Created Short Device Instance Id"),
                        TraceLoggingWideString(entry.ShortUniqueId.c_str(), "Short Unique Id"),
                        TraceLoggingWideString(entry.VirtualEndpointAssociationId.c_str(), "Association Id")
                    );

                    m_endpoints[associationId] = entry;

                }
                else
                {
                    TraceLoggingWrite(
                        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                        __FUNCTION__,
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Endpoint Manager is null", "message")
                    );

                    return E_FAIL;
                }
            }
            else
            {
                // association id isn't present. That's not right.

                TraceLoggingWrite(
                    MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                    __FUNCTION__,
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Association id property was not present in device table", "message")
                );
            }
        }
        else
        {
            // association id is blank, which is also not right

            TraceLoggingWrite(
                MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Association id property was blank", "message")
            );
        }
    }
    CATCH_RETURN();

    return S_OK;
}

_Use_decl_annotations_
HRESULT MidiEndpointTable::OnDeviceDisconnected(std::wstring deviceEndpointInterfaceId) noexcept
{
    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"), 
        TraceLoggingWideString(deviceEndpointInterfaceId.c_str(), "deviceEndpointInterfaceId")
    );

    try
    {
        if (AbstractionState::Current().GetEndpointManager() != nullptr)
        {
            std::wstring associationId = GetSwdPropertyVirtualEndpointAssociationId(deviceEndpointInterfaceId);

            if (associationId != L"")
            {
                if (m_endpoints.find(associationId) != m_endpoints.end())
                {
                    auto entry = m_endpoints[associationId];

                    if (entry.MidiDeviceBiDi != nullptr)
                    {
                        TraceLoggingWrite(
                            MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                            __FUNCTION__,
                            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(deviceEndpointInterfaceId.c_str(), "deviceEndpointInterfaceId"),
                            TraceLoggingWideString(L"Unlinking from MidiDeviceBiDi", "message")
                        );

                        // unlink the bidi devices
                        //entry.MidiDeviceBiDi->UnlinkAssociatedBiDi();
                        entry.MidiDeviceBiDi->UnlinkAllAssociatedBiDi();
                        entry.MidiDeviceBiDi = nullptr;

                        // deactivate the client
                        LOG_IF_FAILED(AbstractionState::Current().GetEndpointManager()->DeleteClientEndpoint(entry.CreatedShortClientInstanceId));

                        // deactivate the device
                        LOG_IF_FAILED(AbstractionState::Current().GetEndpointManager()->DeleteDeviceEndpoint(entry.CreatedShortDeviceInstanceId));

                        // when we disconnect the device, we remove the whole entry
                        m_endpoints.erase(entry.VirtualEndpointAssociationId);
                    }
                }
                else
                {
                    TraceLoggingWrite(
                        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                        __FUNCTION__,
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Unexpected. There's no entry for associationId", "message"),
                        TraceLoggingWideString(associationId.c_str())
                    );
                }
            }
            else
            {
                TraceLoggingWrite(
                    MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                    __FUNCTION__,
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"unable to get association id for this endpoint", "message")
                );
            }
        }
        else
        {
            TraceLoggingWrite(
                MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"endpoint manager is null", "message")
            );
        }
    }
    CATCH_LOG();

    return S_OK;
}


