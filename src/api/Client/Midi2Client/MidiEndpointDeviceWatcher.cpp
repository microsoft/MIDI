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
    void MidiEndpointDeviceWatcher::Start()
    {
        m_enumeratedEndpointDevices.Clear();

        if (m_watcher)
        {
            m_watcher.Start();
        }
    }

    void MidiEndpointDeviceWatcher::Stop()
    { 
        if (m_watcher)
        {
            m_watcher.Stop();
        }
    }


    MidiEndpointDeviceWatcher::~MidiEndpointDeviceWatcher()
    {
        try
        {
            m_enumeratedEndpointDevices.Clear();

            if (m_watcher)
            {
                // unwire events

                m_watcher.Added(m_deviceAddedEventRevokeToken);
                m_watcher.Updated(m_deviceUpdatedEventRevokeToken);
                m_watcher.Removed(m_deviceRemovedEventRevokeToken);
                m_watcher.EnumerationCompleted(m_enumerationCompletedEventRevokeToken);
                m_watcher.Stopped(m_stoppedEventRevokeToken);
            }
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"exception unwiring event handlers");
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
                // add to our map

                auto mapKey = internal::ToLowerHStringCopy(midiEndpointDeviceInformation->Id());

                if (!m_enumeratedEndpointDevices.HasKey(mapKey))
                {
                    m_enumeratedEndpointDevices.Insert(mapKey, *midiEndpointDeviceInformation);

                    if (m_deviceAddedEvent)
                    {
                        m_deviceAddedEvent(*this, *midiEndpointDeviceInformation);
                    }
                }
                else
                {
                    // duplicate key. This should never happen, but just in case ...
                    internal::LogGeneralError(__FUNCTION__, L"Duplicate endpoint device id. This is unexpected.");
                }
            }
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"exception in Added event, likely thrown by the application using this API.");
        }
    }

    void MidiEndpointDeviceWatcher::OnDeviceUpdated(
        _In_ winrt::Windows::Devices::Enumeration::DeviceWatcher source,
        _In_ winrt::Windows::Devices::Enumeration::DeviceInformationUpdate args)
    {
        // TODO: check to see if the device matches one in our list

        try
        {
            auto mapKey = internal::ToLowerHStringCopy(args.Id());

            if (m_enumeratedEndpointDevices.HasKey(mapKey))
            {
                auto ep = m_enumeratedEndpointDevices.Lookup(mapKey);

                ep.UpdateFromDeviceInformationUpdate(args);

                if (m_deviceUpdatedEvent)
                {
                    m_deviceUpdatedEvent(*this, args);
                }
            }
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"exception in Updated event, likely thrown by the application using this API.");
        }
    }

    void MidiEndpointDeviceWatcher::OnDeviceRemoved(
        _In_ winrt::Windows::Devices::Enumeration::DeviceWatcher source,
        _In_ winrt::Windows::Devices::Enumeration::DeviceInformationUpdate args)
    {
        // TODO: check to see if the device matches one in our list

        try
        {
            auto mapKey = internal::ToLowerHStringCopy(args.Id());

            if (m_enumeratedEndpointDevices.HasKey(mapKey))
            {
                m_enumeratedEndpointDevices.Remove(mapKey);

                if (m_deviceRemovedEvent)
                {
                    m_deviceRemovedEvent(*this, args);
                }
            }
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"exception in Removed event, likely thrown by the application using this API.");
        }
    }

    void MidiEndpointDeviceWatcher::OnEnumerationCompleted(
        _In_ winrt::Windows::Devices::Enumeration::DeviceWatcher source,
        _In_ winrt::Windows::Foundation::IInspectable args)
    {
        try
        {
            if (m_enumerationCompletedEvent) m_enumerationCompletedEvent(*this, args);
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"exception in Enumeration Completed event, likely thrown by the application using this API.");
        }
    }

    void MidiEndpointDeviceWatcher::OnStopped(
        _In_ winrt::Windows::Devices::Enumeration::DeviceWatcher source,
        _In_ winrt::Windows::Foundation::IInspectable args)
    {
        try
        {
            if (m_stoppedEvent) m_stoppedEvent(*this, args);
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"exception in Stopped event, likely thrown by the application using this API.");
        }
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
        try
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
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"exception creating MidiEndpointDeviceWatcher.");

            return nullptr;
        }
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
