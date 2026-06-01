// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiLegacyPortDeviceWatcher.h"
#include "Legacy.MidiLegacyPortDeviceWatcher.g.cpp"

namespace winrt::Windows::Devices::Midi2::Enumeration::Legacy::implementation
{
    legacy::MidiLegacyPortDeviceWatcher MidiLegacyPortDeviceWatcher::Create() noexcept
    {
        // TODO


        return nullptr;
    }

    _Use_decl_annotations_
    legacy::MidiLegacyPortDeviceWatcher MidiLegacyPortDeviceWatcher::Create(midi2enum::Midi1PortFlow const& flow) noexcept
    {
        // TODO

        UNREFERENCED_PARAMETER(flow);

        return nullptr;
    }

    _Use_decl_annotations_
    legacy::MidiLegacyPortDeviceWatcher MidiLegacyPortDeviceWatcher::CreateForEndpoint(winrt::hstring const& endpointDeviceId) noexcept
    {
        // TODO

        UNREFERENCED_PARAMETER(endpointDeviceId);

        return nullptr;
    }




    _Use_decl_annotations_
    winrt::event_token MidiLegacyPortDeviceWatcher::Added(
            foundation::TypedEventHandler<legacy::MidiLegacyPortDeviceWatcher, legacy::MidiLegacyPortDeviceInformationAddedEventArgs> const& handler)
    {
        return m_deviceAddedEvent.add(handler);
    }
    _Use_decl_annotations_
    void MidiLegacyPortDeviceWatcher::Added(winrt::event_token const& token) noexcept
    {
        if (m_deviceAddedEvent) m_deviceAddedEvent.remove(token);
    }

    _Use_decl_annotations_
    winrt::event_token MidiLegacyPortDeviceWatcher::Removed(
            foundation::TypedEventHandler<legacy::MidiLegacyPortDeviceWatcher, legacy::MidiLegacyPortDeviceInformationRemovedEventArgs> const& handler)
    {
        return m_deviceRemovedEvent.add(handler);
    }

    _Use_decl_annotations_
    void MidiLegacyPortDeviceWatcher::Removed(winrt::event_token const& token) noexcept
    {
        if (m_deviceRemovedEvent) m_deviceRemovedEvent.remove(token);
    }

    _Use_decl_annotations_
    winrt::event_token MidiLegacyPortDeviceWatcher::Updated(
        foundation::TypedEventHandler<legacy::MidiLegacyPortDeviceWatcher, legacy::MidiLegacyPortDeviceInformationUpdatedEventArgs> const& handler)
    {
        return m_deviceUpdatedEvent.add(handler);
    }

    _Use_decl_annotations_
    void MidiLegacyPortDeviceWatcher::Updated(winrt::event_token const& token) noexcept
    {
        if (m_deviceUpdatedEvent) m_deviceUpdatedEvent.remove(token);
    }

    _Use_decl_annotations_
    winrt::event_token MidiLegacyPortDeviceWatcher::EnumerationCompleted(
        foundation::TypedEventHandler<legacy::MidiLegacyPortDeviceWatcher, foundation::IInspectable> const& handler)
    {
        return m_enumerationCompletedEvent.add(handler);
    }

    _Use_decl_annotations_
    void MidiLegacyPortDeviceWatcher::EnumerationCompleted(winrt::event_token const& token) noexcept
    {
        if (m_enumerationCompletedEvent) m_enumerationCompletedEvent.remove(token);
    }

    _Use_decl_annotations_
    winrt::event_token MidiLegacyPortDeviceWatcher::Stopped(
        foundation::TypedEventHandler<legacy::MidiLegacyPortDeviceWatcher, foundation::IInspectable> const& handler)
    {
        return m_stoppedEvent.add(handler);
    }

    _Use_decl_annotations_
    void MidiLegacyPortDeviceWatcher::Stopped(_In_ winrt::event_token const& token) noexcept
    {
        if (m_stoppedEvent) m_stoppedEvent.remove(token);
    }



    void MidiLegacyPortDeviceWatcher::Start() noexcept
    {
        m_enumeratedPorts.Clear();

        if (m_watcher)
        {
            m_watcher.Start();
        }
    }

    void MidiLegacyPortDeviceWatcher::Stop() noexcept
    {
        if (m_watcher)
        {
            m_watcher.Stop();
        }
    }

    MidiLegacyPortDeviceWatcher::~MidiLegacyPortDeviceWatcher()
    {
        try
        {
            m_enumeratedPorts.Clear();

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
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(ex.code());   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"HRESULT exception unwiring event handlers", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

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


}
