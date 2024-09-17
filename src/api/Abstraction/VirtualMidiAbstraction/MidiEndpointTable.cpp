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
    entry.MidiClientBiDi = nullptr;
    entry.MidiDeviceBiDi = nullptr;


    if (auto endpoint = m_endpoints.find(cleanId); endpoint != m_endpoints.end())
    {
        // we already have an endpoint using this association id, so we need to fail

        RETURN_IF_FAILED(E_INVALIDARG);
    }
    else
    {
        m_endpoints[cleanId] = entry;

        return S_OK;
    }

}


_Use_decl_annotations_
HRESULT 
MidiEndpointTable::OnClientConnected(
    std::wstring const clientEndpointInterfaceId,
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
            auto lock = m_entriesLock.lock();

            if (m_endpoints.find(associationId) != m_endpoints.end())
            {
                // find this id in the table
                auto entry = m_endpoints[associationId];

                RETURN_HR_IF_NULL(E_UNEXPECTED, entry.MidiDeviceBiDi);

                // add the client
                //entry.MidiClientConnections.push_back(clientBiDi);
                entry.MidiClientBiDi = clientBiDi;
                entry.MidiDeviceBiDi->LinkAssociatedCallback(entry.MidiClientBiDi);
                entry.MidiClientBiDi->LinkAssociatedCallback(entry.MidiDeviceBiDi);

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


// This is called from the BiDi Cleanup when it's the client-side BiDi
_Use_decl_annotations_
HRESULT
MidiEndpointTable::OnClientDisconnected(
    std::wstring const clientEndpointInterfaceId) noexcept
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
            auto lock = m_entriesLock.lock();

            if (m_endpoints.find(associationId) != m_endpoints.end())
            {
                // find this id in the table and then remove it
                auto entry = m_endpoints[associationId];

                // we unlink the two, but only remove the client
                entry.MidiDeviceBiDi->UnlinkAssociatedCallback();
                entry.MidiClientBiDi->UnlinkAssociatedCallback();
                entry.MidiClientBiDi.reset();

                m_endpoints[associationId] = entry;

                return S_OK;
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

                RETURN_IF_FAILED(E_INVALIDARG);
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

            RETURN_IF_FAILED(E_INVALIDARG);
        }
    }
    CATCH_RETURN();


    
}



_Use_decl_annotations_
HRESULT MidiEndpointTable::OnDeviceConnected(
    std::wstring const deviceEndpointInterfaceId,
    CMidi2VirtualMidiBiDi* deviceBiDi) noexcept
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
            auto lock = m_entriesLock.lock();

            if (m_endpoints.find(associationId) != m_endpoints.end())
            {
                // find this id in the table
                auto entry = m_endpoints[associationId];

                if (entry.MidiDeviceBiDi == nullptr)
                {
                    entry.MidiDeviceBiDi = deviceBiDi;
                //    m_endpoints[associationId] = entry;
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
                        TraceLoggingWideString(L"Device-side was already connected", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(deviceEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                    );

                    return E_FAIL;
                }


                // if we have an endpoint manager, go ahead and create the client endpoint
                if (AbstractionState::Current().GetEndpointManager())
                {
                    entry.CreatedClientEndpointId = L"";
                    entry.CreatedShortClientInstanceId = L"";
                    entry.MidiClientBiDi = nullptr;

                    RETURN_IF_FAILED(AbstractionState::Current().GetEndpointManager()->CreateClientVisibleEndpoint(entry));

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

// This is called from the BiDi Cleanup when it's the device side BiDi
_Use_decl_annotations_
HRESULT MidiEndpointTable::OnDeviceDisconnected(
    std::wstring const deviceEndpointInterfaceId) noexcept
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
                auto lock = m_entriesLock.lock();

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
                        entry.MidiDeviceBiDi->UnlinkAssociatedCallback();
                        entry.MidiDeviceBiDi.reset();

                        entry.MidiClientBiDi->UnlinkAssociatedCallback();
                        entry.MidiClientBiDi.reset();

                        RETURN_IF_FAILED(AbstractionState::Current().GetEndpointManager()->DeleteClientEndpoint(entry.CreatedShortClientInstanceId));

                        //entry.CreatedClientEndpointId = L"";
                        //entry.CreatedShortClientInstanceId = L"";

                        //m_endpoints[entry.VirtualEndpointAssociationId] = entry;

                        // deactivate the device
                        // TODO: Should this really be done on device disconnect vs an explicit delete command?
                        RETURN_IF_FAILED(AbstractionState::Current().GetEndpointManager()->DeleteDeviceEndpoint(entry.CreatedShortDeviceInstanceId));
                        m_endpoints.erase(entry.VirtualEndpointAssociationId);

                        return S_OK;
                    }
                }
                else
                {
                    TraceLoggingWrite(
                        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Unexpected. There's no entry for associationId", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(deviceEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                    );

                    RETURN_IF_FAILED(E_INVALIDARG);  // fallback error


                }
            }
            else
            {
                TraceLoggingWrite(
                    MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Unable to get association id for this endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(deviceEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                );

                RETURN_IF_FAILED(E_INVALIDARG);  // fallback error
            }
        }
        else
        {
            TraceLoggingWrite(
                MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Endpoint Manager is null", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(deviceEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
            );

            RETURN_IF_FAILED(E_POINTER);  // fallback error

        }
    }
    CATCH_LOG();

    return S_OK;
}


_Use_decl_annotations_
bool MidiEndpointTable::IsUniqueIdInUse(std::wstring const uniqueId) noexcept
{
    auto cleanId = internal::ToLowerTrimmedWStringCopy(uniqueId);

    for (auto const& it : m_endpoints)
    {
        if (it.second.ShortUniqueId == cleanId)
        {
            return true;
        }
    }

    return false;
}


