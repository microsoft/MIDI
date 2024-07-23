// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

HRESULT MidiEndpointTable::Cleanup()
{
    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
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
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
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
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(clientEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    // get the device BiDi, and then wire them together

    try
    {
        RETURN_HR_IF_NULL(E_UNEXPECTED, clientBiDi);

        std::wstring cleanId = internal::NormalizeEndpointInterfaceIdWStringCopy(clientEndpointInterfaceId);

        // look up the association ID in SWD properties

        auto associationId = internal::GetSwdPropertyVirtualEndpointAssociationId(clientEndpointInterfaceId);

        if (!associationId.empty())
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

                LOG_IF_FAILED(E_FAIL);  // cause fallback error to be logged

                TraceLoggingWrite(
                    MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Unable to find device table entry", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(clientEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
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
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(clientEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    try
    {
        std::wstring associationId = internal::GetSwdPropertyVirtualEndpointAssociationId(clientEndpointInterfaceId);

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
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Association id property was not present in device table", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(clientEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                );

            }
        }
        else
        {
            // association id is blank, which is also not right

            TraceLoggingWrite(
                MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Association id property was blank", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(clientEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
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
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(deviceEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );


    try
    {
        // look up the association ID in SWD properties
        auto associationId = internal::GetSwdPropertyVirtualEndpointAssociationId(deviceEndpointInterfaceId);

        if (!associationId.empty())
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

                    //TraceLoggingWrite(
                    //    MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                    //    __FUNCTION__,
                    //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    //    TraceLoggingPointer(this, "this"),
                    //    TraceLoggingWideString(L"After creating client visible endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    //    TraceLoggingWideString(deviceEndpointInterfaceId.c_str(), "Endpoint interface id"),
                    //    TraceLoggingWideString(entry.BaseEndpointName.c_str(), "Base endpoint name"),
                    //    TraceLoggingWideString(entry.CreatedClientEndpointId.c_str(), "Created Client Endpoint Id"),
                    //    TraceLoggingWideString(entry.CreatedShortClientInstanceId.c_str(), "Created Short Client Instance Id"),
                    //    TraceLoggingWideString(entry.CreatedDeviceEndpointId.c_str(), "Created Device Endpoint Id"),
                    //    TraceLoggingWideString(entry.CreatedShortDeviceInstanceId.c_str(), "Created Short Device Instance Id"),
                    //    TraceLoggingWideString(entry.ShortUniqueId.c_str(), "Short Unique Id"),
                    //    TraceLoggingWideString(entry.VirtualEndpointAssociationId.c_str(), "Association Id")
                    //);

                    m_endpoints[associationId] = entry;

                }
                else
                {
                    LOG_IF_FAILED(E_FAIL);  // cause fallback error to be logged

                    TraceLoggingWrite(
                        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Endpoint Manager is null", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(deviceEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                    );

                    return E_FAIL;
                }
            }
            else
            {
                // association id isn't present. That's not right.
                LOG_IF_FAILED(E_FAIL);  // cause fallback error to be logged

                TraceLoggingWrite(
                    MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Association id property was not present in device table", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(deviceEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                );

                return E_FAIL;
            }
        }
        else
        {
            // association id is blank, which is also not right
            LOG_IF_FAILED(E_FAIL);  // cause fallback error to be logged

            TraceLoggingWrite(
                MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Association id property was blank", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(deviceEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
            );

            return E_FAIL;
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
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(deviceEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );


    try
    {
        if (AbstractionState::Current().GetEndpointManager() != nullptr)
        {
            std::wstring associationId = internal::GetSwdPropertyVirtualEndpointAssociationId(deviceEndpointInterfaceId);

            if (associationId != L"")
            {
                if (m_endpoints.find(associationId) != m_endpoints.end())
                {
                    auto entry = m_endpoints[associationId];

                    if (entry.MidiDeviceBiDi != nullptr)
                    {
                        TraceLoggingWrite(
                            MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                            MIDI_TRACE_EVENT_VERBOSE,
                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(L"Unlinking from MidiDeviceBiDi", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                            TraceLoggingWideString(deviceEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
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
                    LOG_IF_FAILED(E_FAIL);  // fallback error

                    TraceLoggingWrite(
                        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Unexpected. There's no entry for associationId", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(deviceEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                    );

                }
            }
            else
            {
                LOG_IF_FAILED(E_FAIL);  // fallback error

                TraceLoggingWrite(
                    MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Unable to get association id for this endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(deviceEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                );
            }
        }
        else
        {
            LOG_IF_FAILED(E_FAIL);  // fallback error

            TraceLoggingWrite(
                MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Endpoint Manager is null", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(deviceEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
            );

        }
    }
    CATCH_LOG();

    return S_OK;
}


