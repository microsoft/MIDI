// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

HRESULT MidiEndpointTable::Shutdown()
{
    TraceLoggingWrite(
        MidiVirtualMidiTransportTelemetryProvider::Provider(),
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
        MidiVirtualMidiTransportTelemetryProvider::Provider(),
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
    entry.MidiClientBidi = nullptr;
    entry.MidiDeviceBidi = nullptr;


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
    CMidi2VirtualMidiBidi* clientBidi) noexcept
{
    TraceLoggingWrite(
        MidiVirtualMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(clientEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    // get the device Bidi, and then wire them together

    try
    {
        RETURN_HR_IF_NULL(E_UNEXPECTED, clientBidi);

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

                RETURN_HR_IF_NULL(E_UNEXPECTED, entry.MidiDeviceBidi);

                // add the client
                //entry.MidiClientConnections.push_back(clientBidi);
                entry.MidiClientBidi = clientBidi;
                RETURN_IF_FAILED(entry.MidiDeviceBidi->LinkAssociatedCallback(entry.MidiClientBidi));
                RETURN_IF_FAILED(entry.MidiClientBidi->LinkAssociatedCallback(entry.MidiDeviceBidi));

                m_endpoints[associationId] = entry;
            }
            else
            {
                // couldn't find the entry

                LOG_IF_FAILED(E_NOTFOUND);  // cause fallback error to be logged

                TraceLoggingWrite(
                    MidiVirtualMidiTransportTelemetryProvider::Provider(),
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


// This is called from the Bidi Cleanup when it's the client-side Bidi
_Use_decl_annotations_
HRESULT
MidiEndpointTable::OnClientDisconnected(
    std::wstring const clientEndpointInterfaceId) noexcept
{
    TraceLoggingWrite(
        MidiVirtualMidiTransportTelemetryProvider::Provider(),
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

        if (!associationId.empty())
        {
            auto lock = m_entriesLock.lock();

            if (m_endpoints.find(associationId) != m_endpoints.end())
            {
                // find this id in the table and then remove it
                auto entry = m_endpoints[associationId];

                // we unlink the two, but only remove the client
                if (entry.MidiDeviceBidi != nullptr)
                {
                    LOG_IF_FAILED(entry.MidiDeviceBidi->UnlinkAssociatedCallback());
                    entry.MidiDeviceBidi.reset();
                }

                if (entry.MidiClientBidi != nullptr)
                {
                    LOG_IF_FAILED(entry.MidiClientBidi->UnlinkAssociatedCallback());
                    entry.MidiClientBidi.reset();
                }

                m_endpoints[associationId] = entry;

                return S_OK;
            }
            else
            {
                // association id isn't present. That's not right.
                TraceLoggingWrite(
                    MidiVirtualMidiTransportTelemetryProvider::Provider(),
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
                MidiVirtualMidiTransportTelemetryProvider::Provider(),
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
    CMidi2VirtualMidiBidi* deviceBidi) noexcept
{
    TraceLoggingWrite(
        MidiVirtualMidiTransportTelemetryProvider::Provider(),
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

                if (entry.MidiDeviceBidi == nullptr)
                {
                    entry.MidiDeviceBidi = deviceBidi;
                    m_endpoints[associationId] = entry;
                }
                else
                {
                    LOG_IF_FAILED(E_FAIL);  // cause fallback error to be logged

                    TraceLoggingWrite(
                        MidiVirtualMidiTransportTelemetryProvider::Provider(),
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
                if (TransportState::Current().GetEndpointManager())
                {
                    entry.CreatedClientEndpointId = L"";
                    entry.CreatedShortClientInstanceId = L"";
                    entry.MidiClientBidi = nullptr;

                    RETURN_IF_FAILED(TransportState::Current().GetEndpointManager()->CreateClientVisibleEndpoint(entry));

                    m_endpoints[associationId] = entry;
                }
                else
                {
                    LOG_IF_FAILED(E_FAIL);  // cause fallback error to be logged

                    TraceLoggingWrite(
                        MidiVirtualMidiTransportTelemetryProvider::Provider(),
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
                    MidiVirtualMidiTransportTelemetryProvider::Provider(),
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
                MidiVirtualMidiTransportTelemetryProvider::Provider(),
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

// This is called from the Bidi Cleanup when it's the device side Bidi
_Use_decl_annotations_
HRESULT MidiEndpointTable::OnDeviceDisconnected(
    std::wstring const deviceEndpointInterfaceId) noexcept
{
    TraceLoggingWrite(
        MidiVirtualMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(deviceEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    try
    {
        if (TransportState::Current().GetEndpointManager() != nullptr)
        {
            std::wstring associationId = internal::GetSwdPropertyVirtualEndpointAssociationId(deviceEndpointInterfaceId);

            if (associationId != L"")
            {
                auto lock = m_entriesLock.lock();

                if (m_endpoints.find(associationId) != m_endpoints.end())
                {
                    auto entry = m_endpoints[associationId];

                    if (entry.MidiDeviceBidi != nullptr)
                    {
                        TraceLoggingWrite(
                            MidiVirtualMidiTransportTelemetryProvider::Provider(),
                            MIDI_TRACE_EVENT_VERBOSE,
                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(L"Unlinking from MidiDeviceBidi", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                            TraceLoggingWideString(deviceEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                        );


                        TraceLoggingWrite(
                            MidiVirtualMidiTransportTelemetryProvider::Provider(),
                            MIDI_TRACE_EVENT_VERBOSE,
                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(L"Unlinking MIDI Client BiDi", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                            TraceLoggingWideString(deviceEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                        );

                        if (entry.MidiClientBidi != nullptr)
                        {
                            LOG_IF_FAILED(entry.MidiClientBidi->UnlinkAssociatedCallback());
                            entry.MidiClientBidi.reset();
                        }

                        LOG_IF_FAILED(entry.MidiDeviceBidi->UnlinkAssociatedCallback());
                        entry.MidiDeviceBidi.reset();

                        TraceLoggingWrite(
                            MidiVirtualMidiTransportTelemetryProvider::Provider(),
                            MIDI_TRACE_EVENT_VERBOSE,
                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(L"Removing client visibile endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                            TraceLoggingWideString(deviceEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                        );

                        // remove the client endpoint. This will also trigger removal of MIDI 1.0 endpoints
                        LOG_IF_FAILED(TransportState::Current().GetEndpointManager()->DeleteClientEndpoint(entry.CreatedShortClientInstanceId));

                        TraceLoggingWrite(
                            MidiVirtualMidiTransportTelemetryProvider::Provider(),
                            MIDI_TRACE_EVENT_VERBOSE,
                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(L"Removing app host endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                            TraceLoggingWideString(deviceEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                        );

                        // deactivate the device
                        RETURN_IF_FAILED(TransportState::Current().GetEndpointManager()->DeleteDeviceEndpoint(entry.CreatedShortDeviceInstanceId));
                        
                        m_endpoints.erase(entry.VirtualEndpointAssociationId);

                        TraceLoggingWrite(
                            MidiVirtualMidiTransportTelemetryProvider::Provider(),
                            MIDI_TRACE_EVENT_VERBOSE,
                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(L"Endpoint teardown complete", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                            TraceLoggingWideString(deviceEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                        );

                        return S_OK;
                    }
                }
                else
                {
                    TraceLoggingWrite(
                        MidiVirtualMidiTransportTelemetryProvider::Provider(),
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
                    MidiVirtualMidiTransportTelemetryProvider::Provider(),
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
                MidiVirtualMidiTransportTelemetryProvider::Provider(),
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


