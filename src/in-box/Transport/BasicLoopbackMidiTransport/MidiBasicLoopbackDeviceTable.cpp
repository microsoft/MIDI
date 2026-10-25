// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


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
        if (!device) continue;

        auto definition = device->GetDefinition();
        if (!definition) continue;

        if (cleanId == internal::NormalizeEndpointInterfaceIdWStringCopy(definition->CreatedEndpointInterfaceId))
        {
            return device;
        }
    }

    return nullptr;
}


_Use_decl_annotations_
bool MidiBasicLoopbackDeviceTable::IsNameTakenLocked(
    std::wstring const& cleanName,
    MidiBasicLoopbackDevice const* ignoredDevice)
{
    for (auto const& [key, device] : m_devices)
    {
        if (!device || device.get() == ignoredDevice) continue;

        auto definition = device->GetDefinition();
        if (!definition) continue;

        if (cleanName == internal::ToLowerTrimmedWStringCopy(definition->EndpointName))
        {
            return true;
        }
    }

    for (auto const& [key, reservation] : m_reservations)
    {
        if (!reservation.CleanName.empty() && cleanName == reservation.CleanName)
        {
            return true;
        }
    }

    return false;
}


_Use_decl_annotations_
MidiBasicLoopbackReservation MidiBasicLoopbackDeviceTable::Reserve(
    winrt::guid const& associationId,
    std::wstring const& uniqueIdentifier,
    std::wstring const& endpointName,
    bool const checkOverlap)
{
    Reservation reservation{};
    reservation.CleanUniqueIdentifier = internal::ToLowerTrimmedWStringCopy(uniqueIdentifier);
    reservation.CleanName = internal::ToLowerTrimmedWStringCopy(endpointName);

    auto const uniqueIdRefusal = [&](std::wstring const& otherId) -> std::optional<MidiBasicLoopbackReservation>
    {
        if (otherId == reservation.CleanUniqueIdentifier)
        {
            return MidiBasicLoopbackReservation::UniqueIdInUse;
        }

        if (checkOverlap &&
            (otherId.starts_with(reservation.CleanUniqueIdentifier) || reservation.CleanUniqueIdentifier.starts_with(otherId)))
        {
            return MidiBasicLoopbackReservation::UniqueIdOverlaps;
        }

        return std::nullopt;
    };

    auto lock = m_devicesLock.lock_exclusive();

    // Two loopbacks under one association id would leave the first one in the service with
    // nothing here to remove it by.
    if (m_devices.find(associationId) != m_devices.end() ||
        m_reservations.find(associationId) != m_reservations.end())
    {
        return MidiBasicLoopbackReservation::AssociationIdInUse;
    }

    for (auto const& [key, device] : m_devices)
    {
        if (!device) continue;

        auto definition = device->GetDefinition();
        if (!definition) continue;

        if (auto refusal = uniqueIdRefusal(internal::ToLowerTrimmedWStringCopy(definition->EndpointUniqueIdentifier)))
        {
            return *refusal;
        }
    }

    for (auto const& [key, other] : m_reservations)
    {
        if (auto refusal = uniqueIdRefusal(other.CleanUniqueIdentifier))
        {
            return *refusal;
        }
    }

    if (!reservation.CleanName.empty() && IsNameTakenLocked(reservation.CleanName, nullptr))
    {
        return MidiBasicLoopbackReservation::NameInUse;
    }

    m_reservations.emplace(associationId, std::move(reservation));

    return MidiBasicLoopbackReservation::Reserved;
}


_Use_decl_annotations_
void MidiBasicLoopbackDeviceTable::ReleaseReservation(
    winrt::guid const& associationId)
{
    auto lock = m_devicesLock.lock_exclusive();

    m_reservations.erase(associationId);
}


_Use_decl_annotations_
HRESULT MidiBasicLoopbackDeviceTable::AddDevice(
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

    RETURN_HR_IF_NULL(E_INVALIDARG, device);

    auto lock = m_devicesLock.lock_exclusive();

    // Replacing a device here would orphan the one already in the service
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS), m_devices.find(associationId) != m_devices.end());

    m_devices.emplace(associationId, std::move(device));
    m_reservations.erase(associationId);

    return S_OK;
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
HRESULT MidiBasicLoopbackDeviceTable::RenameDevice(
    std::shared_ptr<MidiBasicLoopbackDevice> const& device,
    std::wstring const& endpointName,
    std::wstring const& endpointDescription,
    std::wstring const& imageFileName,
    std::shared_ptr<MidiBasicLoopbackDeviceDefinition const>& previous)
{
    previous = nullptr;

    RETURN_HR_IF_NULL(E_INVALIDARG, device);

    auto const cleanName = internal::ToLowerTrimmedWStringCopy(endpointName);

    auto lock = m_devicesLock.lock_exclusive();

    // The endpoint being renamed is left out, so re-saving a description without touching the
    // name does not collide with the name the endpoint already has.
    if (IsNameTakenLocked(cleanName, device.get()))
    {
        return HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);
    }

    std::shared_ptr<MidiBasicLoopbackDeviceDefinition const> replaced{};

    auto const renamed = device->ChangeDefinition([&](MidiBasicLoopbackDeviceDefinition& definition)
        {
            replaced = std::make_shared<MidiBasicLoopbackDeviceDefinition const>(definition);

            definition.EndpointName = endpointName;
            definition.EndpointDescription = endpointDescription;
            definition.ImageFileName = imageFileName;
        });

    RETURN_HR_IF_NULL(E_NOTFOUND, renamed);

    previous = std::move(replaced);

    return S_OK;
}



std::vector<MidiBasicLoopbackDeviceSnapshot> MidiBasicLoopbackDeviceTable::GetDeviceListSnapshot()
{
    std::vector<MidiBasicLoopbackDeviceSnapshot> results;

    // lock so no adds/removes happen while building the list
    auto lock = m_devicesLock.lock_shared();

    for (auto const& [key, device] : m_devices)
    {
        if (!device) continue;

        // device may be shut down (definition reset) but not yet removed
        auto definition = device->GetDefinition();
        if (!definition) continue;

        // snapshot so no pointer issues if removed from table after this point
        MidiBasicLoopbackDeviceSnapshot snapshot{ *definition, device->MessageCount() };

        if (device->Feedback != nullptr)
        {
            snapshot.FeedbackProtectionEnabled = device->Feedback->IsEnabled();
            snapshot.FeedbackStatus = device->Feedback->GetStatus();
        }

        results.push_back(std::move(snapshot));
    }

    return results;
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
        m_reservations.clear();
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
