// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include "pch.h"


_Use_decl_annotations_
std::shared_ptr<MidiBasicLoopbackDevice> MidiBasicLoopbackDeviceTable::GetDevice(
    winrt::guid const& associationId)
{
    auto lock = m_devicesLock.lock_shared();

    // single lookup; avoid operator[] which would default-insert a null
    // entry if the key were missing.
    if (auto it = m_devices.find(associationId); it != m_devices.end())
    {
        return it->second;
    }

    return nullptr;
}


_Use_decl_annotations_
std::shared_ptr<MidiBasicLoopbackDevice> MidiBasicLoopbackDeviceTable::GetDeviceById(
    std::wstring const& endpointDeviceId)
{
    auto cleanId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceId);

    auto lock = m_devicesLock.lock_shared();

    for (auto const& [key, device] : m_devices)
    {
        // device may be shut down (Definition reset) but not yet removed,
        // so guard both the device and its definition.
        if (device && device->Definition)
        {
            if (cleanId == internal::NormalizeEndpointInterfaceIdWStringCopy(device->Definition->CreatedEndpointInterfaceId))
            {
                return device;
            }
        }
    }

    return nullptr;
}


_Use_decl_annotations_
void MidiBasicLoopbackDeviceTable::SetDevice(
    winrt::guid const& associationId,
    std::shared_ptr<MidiBasicLoopbackDevice> device)
{
    TraceLoggingWrite(
        MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingGuid(associationId, "association id")
    );

    auto lock = m_devicesLock.lock_exclusive();

    m_devices[associationId] = device;
}

_Use_decl_annotations_
void MidiBasicLoopbackDeviceTable::RemoveDevice(
    winrt::guid const& associationId)
{
    // Detach the device from the map under the lock, then shut it down
    // outside the lock so we don't hold the table lock across a callback
    // teardown (which could otherwise invert lock order against the data path).
    std::shared_ptr<MidiBasicLoopbackDevice> device;
    {
        auto lock = m_devicesLock.lock_exclusive();

        if (auto it = m_devices.find(associationId); it != m_devices.end())
        {
            device = it->second;
            m_devices.erase(it);
        }
    }

    if (device)
    {
        device->Shutdown();
    }
}

_Use_decl_annotations_
bool MidiBasicLoopbackDeviceTable::IsUniqueIdentifierInUseForLoopback(
    std::wstring const& uniqueIdentifier)
{
    auto cleanId = internal::ToLowerTrimmedWStringCopy(uniqueIdentifier);

    auto lock = m_devicesLock.lock_shared();

    for (auto const& [key, device] : m_devices)
    {
        if (device && device->Definition)
        {
            if (cleanId == internal::ToLowerTrimmedWStringCopy(device->Definition->EndpointUniqueIdentifier))
            {
                return true;
            }
        }
    }

    return false;
}


HRESULT MidiBasicLoopbackDeviceTable::Shutdown()
{
    // Move the map out under the lock, then shut down the devices outside
    // the lock (same rationale as RemoveDevice).
    std::map<winrt::guid, std::shared_ptr<MidiBasicLoopbackDevice>> devices;
    {
        auto lock = m_devicesLock.lock_exclusive();
        devices = std::move(m_devices);
        m_devices.clear();
    }

    for (auto& [key, device] : devices)
    {
        if (device)
        {
            device->Shutdown();
        }
    }

    return S_OK;
}