// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkAdvertisedHostWatcher.h"
#include "Endpoints.Network.MidiNetworkAdvertisedHostWatcher.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{
    MidiNetworkAdvertisedHostWatcher::~MidiNetworkAdvertisedHostWatcher()
    {
        try
        {
            m_enumeratedHosts.Clear();

            if (m_watcher != nullptr)
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
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"exception unwiring event handlers", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

        }
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostWatcher::InternalInitialize(
        enumeration::DeviceWatcher const& baseWatcher)
    {
        m_watcher = baseWatcher;

        if (m_watcher != nullptr)
        {
            m_deviceAddedEventRevokeToken = m_watcher.Added({ this, &MidiNetworkAdvertisedHostWatcher::OnDeviceAdded });
            m_deviceUpdatedEventRevokeToken = m_watcher.Updated({ this, &MidiNetworkAdvertisedHostWatcher::OnDeviceUpdated });
            m_deviceRemovedEventRevokeToken = m_watcher.Removed({ this, &MidiNetworkAdvertisedHostWatcher::OnDeviceRemoved });
            m_enumerationCompletedEventRevokeToken = m_watcher.EnumerationCompleted({ this, &MidiNetworkAdvertisedHostWatcher::OnEnumerationCompleted });
            m_stoppedEventRevokeToken = m_watcher.Stopped({ this, &MidiNetworkAdvertisedHostWatcher::OnStopped });
        }
    }


    network::MidiNetworkAdvertisedHostWatcher MidiNetworkAdvertisedHostWatcher::Create() noexcept
    {
        try
        {

            auto watcher = winrt::make_self<MidiNetworkAdvertisedHostWatcher>();
            auto baseWatcher = enumeration::DeviceInformation::CreateWatcher(
                network::MidiNetworkEndpointManager::MidiNetworkUdpDnsSdQueryString(),
                network::MidiNetworkEndpointManager::MidiNetworkUdpDnsSdQueryAdditionalProperties(),
                network::MidiNetworkEndpointManager::MidiNetworkUdpDnsSdDeviceInformationKind());

            watcher->InternalInitialize(baseWatcher);

            return *watcher;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception creating MidiEndpointDeviceWatcher.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return nullptr;
        }
    }


    void MidiNetworkAdvertisedHostWatcher::Start()
    {
        m_enumeratedHosts.Clear();

        if (m_watcher)
        {
            m_watcher.Start();
        }
    }
    void MidiNetworkAdvertisedHostWatcher::Stop()
    {
        if (m_watcher)
        {
            m_watcher.Stop();
        }
    }


    enumeration::DeviceWatcherStatus MidiNetworkAdvertisedHostWatcher::Status()
    {
        if (m_watcher)
        {
            return m_watcher.Status();
        }
        else
        {
            return enumeration::DeviceWatcherStatus::Aborted;
        }
    }

    _Use_decl_annotations_
    winrt::event_token MidiNetworkAdvertisedHostWatcher::Added(
        foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, network::MidiNetworkAdvertisedHostAddedEventArgs> const& handler)
    {
        return m_deviceAddedEvent.add(handler);
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostWatcher::Added(winrt::event_token const& token) noexcept
    {
        m_deviceAddedEvent.remove(token);
    }


    _Use_decl_annotations_
    winrt::event_token MidiNetworkAdvertisedHostWatcher::Removed(
        foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, network::MidiNetworkAdvertisedHostRemovedEventArgs> const& handler)
    {
        return m_deviceRemovedEvent.add(handler);
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostWatcher::Removed(
        winrt::event_token const& token) noexcept
    {
        m_deviceRemovedEvent.remove(token);
    }


    _Use_decl_annotations_
    winrt::event_token MidiNetworkAdvertisedHostWatcher::Updated(
        foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, network::MidiNetworkAdvertisedHostUpdatedEventArgs> const& handler)
    {
        return m_deviceUpdatedEvent.add(handler);
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostWatcher::Updated(
        winrt::event_token const& token) noexcept
    {
        m_deviceUpdatedEvent.remove(token);
    }


    _Use_decl_annotations_
    winrt::event_token MidiNetworkAdvertisedHostWatcher::EnumerationCompleted(
        foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, foundation::IInspectable> const& handler)
    {
        return m_enumerationCompletedEvent.add(handler);
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostWatcher::EnumerationCompleted(
        winrt::event_token const& token) noexcept
    {
        m_enumerationCompletedEvent.remove(token);
    }


    _Use_decl_annotations_
    winrt::event_token MidiNetworkAdvertisedHostWatcher::Stopped(
        foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, foundation::IInspectable> const& handler)
    {
        return m_stoppedEvent.add(handler);
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostWatcher::Stopped(
        winrt::event_token const& token) noexcept
    {
        m_stoppedEvent.remove(token);
    }




    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostWatcher::OnDeviceAdded(
        enumeration::DeviceWatcher /*source*/,
        enumeration::DeviceInformation args)
    {
        try
        {
            auto host = winrt::make_self<network::implementation::MidiNetworkAdvertisedHost>();

            host->InternalUpdateFromDeviceInformation(args);

            // add to our map

            auto mapKey = host->DeviceId();

            if (!m_enumeratedHosts.HasKey(mapKey))
            {
                m_enumeratedHosts.Insert(mapKey, *host);

                if (m_deviceAddedEvent)
                {
                    auto newArgs = winrt::make_self<network::implementation::MidiNetworkAdvertisedHostAddedEventArgs>();
                    newArgs->InternalInitialize(*host);

                    m_deviceAddedEvent(*this, *newArgs);
                }
            }
            else
            {
                // duplicate key. This should never happen, but just in case ...

                LOG_IF_FAILED(E_UNEXPECTED);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Duplicate endpoint device id. This is unexpected", MIDI_SDK_TRACE_MESSAGE_FIELD)
                );

            }
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception in Added event, likely thrown by the application using this API", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
        }

    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostWatcher::OnDeviceUpdated(
        enumeration::DeviceWatcher /*source*/,
        enumeration::DeviceInformationUpdate args)
    {
        try
        {
            auto newArgs = winrt::make_self<network::implementation::MidiNetworkAdvertisedHostUpdatedEventArgs>();

            newArgs->InternalInitialize(args.Id(), args);

            // TODO: Update entry in collection with the updated properties in args

            if (m_deviceUpdatedEvent)
            {
                m_deviceUpdatedEvent(*this, *newArgs);
            }
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"exception in Updated event, likely thrown by the application using this API", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
        }
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostWatcher::OnDeviceRemoved(
        enumeration::DeviceWatcher /*source*/,
        enumeration::DeviceInformationUpdate args)
    {
        try
        {
            auto newArgs = winrt::make_self<network::implementation::MidiNetworkAdvertisedHostRemovedEventArgs>();

            newArgs->InternalInitialize(args.Id(), args);

            if (m_enumeratedHosts.HasKey(args.Id()))
            {
                m_enumeratedHosts.Remove(args.Id());
            }

            if (m_deviceRemovedEvent)
            {
                m_deviceRemovedEvent(*this, *newArgs);
            }
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"exception in Removed event, likely thrown by the application using this API", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
        }
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostWatcher::OnEnumerationCompleted(
        enumeration::DeviceWatcher /*source*/,
        foundation::IInspectable args)
    {
        try
        {
            if (m_enumerationCompletedEvent) m_enumerationCompletedEvent(*this, args);
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception in Enumeration Completed event, likely thrown by the application using this API.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
        }
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostWatcher::OnStopped(
        enumeration::DeviceWatcher /*source*/,
        foundation::IInspectable args)
    {
        try
        {
            if (m_stoppedEvent) m_stoppedEvent(*this, args);
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception in Enumeration Stopped event, likely thrown by the application using this API.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
        }
    }
}
