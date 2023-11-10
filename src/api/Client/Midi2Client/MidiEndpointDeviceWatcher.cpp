// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointDeviceWatcher.h"
#include "MidiEndpointDeviceWatcher.g.cpp"

namespace winrt::Windows::Devices::Midi2::implementation
{
    MidiEndpointDeviceWatcher::~MidiEndpointDeviceWatcher()
    {
        if (m_watcher)
        {
            m_watcher.Added(m_deviceAddedEventRevokeToken);
            m_watcher.Updated(m_deviceUpdatedEventRevokeToken);
            m_watcher.Removed(m_deviceRemovedEventRevokeToken);
            m_watcher.EnumerationCompleted(m_enumerationCompletedEventRevokeToken);
            m_watcher.Stopped(m_stoppedEventRevokeToken);
        }
    }

    void MidiEndpointDeviceWatcher::OnDeviceAdded(
        _In_ winrt::Windows::Devices::Enumeration::DeviceWatcher source,
        _In_ winrt::Windows::Devices::Enumeration::DeviceInformation args)
    {
        try
        {
            auto midiEndpointDeviceInformation = winrt::make_self<MidiEndpointDeviceInformation>();

            midiEndpointDeviceInformation->UpdateFromDeviceInformation(args);

            // our properties are GUID-based, so they can't be referenced in aqs. So, instead, we
            // construct a MidiEndpointDeviceInformation object and then inspect properties there.

            if (MidiEndpointDeviceInformation::DeviceMatchesFilter(*midiEndpointDeviceInformation, m_endpointFilter))
            {
                if (m_deviceAddedEvent) m_deviceAddedEvent(*this, *midiEndpointDeviceInformation);
            }
        }
        catch (...)
        {
            // TODO: Log this
        }
    }

    void MidiEndpointDeviceWatcher::OnDeviceUpdated(
        _In_ winrt::Windows::Devices::Enumeration::DeviceWatcher source,
        _In_ winrt::Windows::Devices::Enumeration::DeviceInformationUpdate args)
    {
        // TODO: check to see if the device matches one in our list

        if (m_deviceUpdatedEvent) m_deviceUpdatedEvent(*this, args);
    }

    void MidiEndpointDeviceWatcher::OnDeviceRemoved(
        _In_ winrt::Windows::Devices::Enumeration::DeviceWatcher source,
        _In_ winrt::Windows::Devices::Enumeration::DeviceInformationUpdate args)
    {
        // TODO: check to see if the device matches one in our list

        if (m_deviceRemovedEvent) m_deviceRemovedEvent(*this, args);
    }

    void MidiEndpointDeviceWatcher::OnEnumerationCompleted(
        _In_ winrt::Windows::Devices::Enumeration::DeviceWatcher source,
        _In_ winrt::Windows::Foundation::IInspectable args)
    {
        if (m_enumerationCompletedEvent) m_enumerationCompletedEvent(*this, args);
    }

    void MidiEndpointDeviceWatcher::OnStopped(
        _In_ winrt::Windows::Devices::Enumeration::DeviceWatcher source,
        _In_ winrt::Windows::Foundation::IInspectable args)
    {
        if (m_stoppedEvent) m_stoppedEvent(*this, args);
    }


    _Use_decl_annotations_
    void MidiEndpointDeviceWatcher::InternalInitialize(
        midi2::MidiEndpointDeviceInformationFilter const& endpointFilter, 
        winrt::Windows::Devices::Enumeration::DeviceWatcher const& baseWatcher)
    {
        m_endpointFilter = endpointFilter;
        m_watcher = baseWatcher;

        if (m_watcher != nullptr)
        {
            m_deviceAddedEventRevokeToken = m_watcher.Added({ this, &MidiEndpointDeviceWatcher::OnDeviceAdded });
            m_deviceUpdatedEventRevokeToken = m_watcher.Updated({ this, &MidiEndpointDeviceWatcher::OnDeviceUpdated });
            m_deviceRemovedEventRevokeToken = m_watcher.Removed({ this, &MidiEndpointDeviceWatcher::OnDeviceRemoved });
            m_enumerationCompletedEventRevokeToken = m_watcher.EnumerationCompleted({ this, &MidiEndpointDeviceWatcher::OnEnumerationCompleted });
            m_stoppedEventRevokeToken = m_watcher.Stopped({ this, &MidiEndpointDeviceWatcher::OnStopped });
        }

    }


    _Use_decl_annotations_
    midi2::MidiEndpointDeviceWatcher MidiEndpointDeviceWatcher::CreateWatcher(
        midi2::MidiEndpointDeviceInformationFilter const& endpointFilter) noexcept
    {
        // the properties we would filter on are GUID properties, so we can't use
        // them in the aqs query. Instead, we have to filter when we get the events
        winrt::hstring aqsFilter = midi2::MidiEndpointConnection::GetDeviceSelector();

        auto watcher = winrt::make_self<MidiEndpointDeviceWatcher>();
        auto baseWatcher = winrt::Windows::Devices::Enumeration::DeviceInformation::CreateWatcher(
            aqsFilter,
            midi2::MidiEndpointDeviceInformation::GetAdditionalPropertiesList(),
            winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface);

        watcher->InternalInitialize(endpointFilter, baseWatcher);

        return *watcher;
    }


    winrt::Windows::Devices::Enumeration::DeviceWatcherStatus MidiEndpointDeviceWatcher::Status()
    {
        if (m_watcher)
        {
            return m_watcher.Status();
        }
        else
        {
            return winrt::Windows::Devices::Enumeration::DeviceWatcherStatus::Aborted;
        }
    }
}
