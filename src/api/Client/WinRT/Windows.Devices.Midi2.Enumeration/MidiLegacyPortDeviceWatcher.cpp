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
        try
        {
            auto watcher = winrt::make_self<MidiLegacyPortDeviceWatcher>();

            auto baseWatcher = enumeration::DeviceInformation::CreateWatcher(
                MidiLegacyPortDeviceInformation::InternalGetSelectorForSourceAndDestinationPorts(),
                MidiLegacyPortDeviceInformation::GetAdditionalPropertiesList(),
                enumeration::DeviceInformationKind::DeviceInterface);

            watcher->InternalInitialize(baseWatcher);

            return *watcher;
        }
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(ex.code());   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception creating MidiLegacyPortDeviceWatcher.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            return nullptr;
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
                TraceLoggingWideString(L"Exception creating MidiLegacyPortDeviceWatcher.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return nullptr;
        }
    }

    _Use_decl_annotations_
    legacy::MidiLegacyPortDeviceWatcher MidiLegacyPortDeviceWatcher::Create(midi2enum::Midi1PortFlow const& flow) noexcept
    {
        try
        {
            winrt::hstring selector;

            if (flow == midi2enum::Midi1PortFlow::MidiMessageSource)
            {
                selector = winrt::Windows::Devices::Midi::MidiInPort::GetDeviceSelector();
            }
            else
            {
                selector = winrt::Windows::Devices::Midi::MidiOutPort::GetDeviceSelector();
            }


            auto watcher = winrt::make_self<MidiLegacyPortDeviceWatcher>();

            auto baseWatcher = enumeration::DeviceInformation::CreateWatcher(
                selector,
                MidiLegacyPortDeviceInformation::GetAdditionalPropertiesList(),
                enumeration::DeviceInformationKind::DeviceInterface);

            watcher->InternalInitialize(baseWatcher);

            return *watcher;
        }
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(ex.code());   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception creating directional MidiLegacyPortDeviceWatcher.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            return nullptr;
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
                TraceLoggingWideString(L"Exception creating directional MidiLegacyPortDeviceWatcher.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return nullptr;
        }

    }

    _Use_decl_annotations_
    legacy::MidiLegacyPortDeviceWatcher MidiLegacyPortDeviceWatcher::CreateForEndpoint(winrt::hstring const& endpointDeviceId) noexcept
    {
        // TODO

        //auto sourceClass = internal::GuidToString(DEVINTERFACE_MIDI_INPUT);
        //auto destinationClass = internal::GuidToString(DEVINTERFACE_MIDI_OUTPUT);

        //// we only want MIDI 1 ports which have the specified endpoing as the parent device
        //winrt::hstring selector = L"System.Devices.InterfaceClassGuid:=\"" + deviceClass + L"\" AND " +
        //    L"System.Devices.Parent:=\"" + endpointDeviceId + L"\" AND " +
        //    L"System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True";






        UNREFERENCED_PARAMETER(endpointDeviceId);

        return nullptr;
    }



    _Use_decl_annotations_
    void MidiLegacyPortDeviceWatcher::OnDeviceAdded(
        enumeration::DeviceWatcher const& source,
        enumeration::DeviceInformation const& args) noexcept
    {
        UNREFERENCED_PARAMETER(source);

        try
        {
            auto legacyPortDeviceInformation = winrt::make_self<MidiLegacyPortDeviceInformation>();

            legacyPortDeviceInformation->InternalInitialize(args.Name(), args.Id(), args.Properties());

            auto mapKey = internal::NormalizeEndpointInterfaceIdHStringCopy(legacyPortDeviceInformation->PortDeviceId());

            if (!m_enumeratedPorts.HasKey(mapKey))
            {
                m_enumeratedPorts.Insert(mapKey, *legacyPortDeviceInformation);

                if (m_deviceAddedEvent)
                {
                    auto newArgs = winrt::make_self<MidiLegacyPortDeviceInformationAddedEventArgs>();
                    newArgs->InternalInitialize(*legacyPortDeviceInformation);

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
                    TraceLoggingWideString(L"Duplicate port id. This is unexpected", MIDI_SDK_TRACE_MESSAGE_FIELD)
                );

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
                TraceLoggingWideString(L"Exception in Added event, likely thrown by the application using this API", MIDI_SDK_TRACE_MESSAGE_FIELD),
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
                TraceLoggingWideString(L"Exception in Added event, likely thrown by the application using this API", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
        }

    }

    _Use_decl_annotations_
    void MidiLegacyPortDeviceWatcher::OnDeviceUpdated(
        enumeration::DeviceWatcher const& source,
        enumeration::DeviceInformationUpdate const& args) noexcept
    {
        UNREFERENCED_PARAMETER(source);

        try
        {
            auto mapKey = internal::NormalizeEndpointInterfaceIdHStringCopy(args.Id());

            if (m_enumeratedPorts.HasKey(mapKey))
            {
                auto port = winrt::get_self<MidiLegacyPortDeviceInformation>(m_enumeratedPorts.Lookup(mapKey));

                if (!port)
                {
                    // this is unexpected

                    LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                    TraceLoggingWrite(
                        Midi2SdkTelemetryProvider::Provider(),
                        MIDI_SDK_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                        TraceLoggingWideString(L"Received an update for a MIDI 1 port that is no longer in our list", MIDI_SDK_TRACE_MESSAGE_FIELD)
                    );

                    return;
                }

                // Update the port properties based on what changed
                port->InternalUpdateFromDeviceInformationUpdate(args);

                // raise an event if the app has subscribed to it
                if (m_deviceUpdatedEvent)
                {
                    auto newArgs = winrt::make_self<MidiLegacyPortDeviceInformationUpdatedEventArgs>();

                    bool updatedName{ false };
                    bool updatedPortNumber{ false };

                    if (args.Properties().HasKey(L"System.Devices.FriendlyName"))
                    {
                        updatedName = true;
                    }

                    if (args.Properties().HasKey(STRING_PKEY_MIDI_ServiceAssignedPortNumber))
                    {
                        updatedPortNumber = true;
                    }

                    newArgs->InternalInitialize(
                        args.Id(),
                        args,
                        updatedName,
                        updatedPortNumber
                    );

                    m_deviceUpdatedEvent(*this, *newArgs);
                }
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
                TraceLoggingWideString(L"exception in Updated event, likely thrown by the application using this API", MIDI_SDK_TRACE_MESSAGE_FIELD),
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
                TraceLoggingWideString(L"exception in Updated event, likely thrown by the application using this API", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
        }
    }

    _Use_decl_annotations_
    void MidiLegacyPortDeviceWatcher::OnDeviceRemoved(
        enumeration::DeviceWatcher const& source,
        enumeration::DeviceInformationUpdate const& args) noexcept
    {
        UNREFERENCED_PARAMETER(source);

        try
        {
            auto mapKey = internal::NormalizeEndpointInterfaceIdHStringCopy(args.Id());

            auto newArgs = winrt::make_self<MidiLegacyPortDeviceInformationRemovedEventArgs>();

            newArgs->InternalInitialize(args, args.Id());

            if (m_enumeratedPorts.HasKey(mapKey))
            {
                m_enumeratedPorts.Remove(mapKey);

                if (m_deviceRemovedEvent)
                {
                    m_deviceRemovedEvent(*this, *newArgs);
                }
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
    void MidiLegacyPortDeviceWatcher::OnEnumerationCompleted(
        enumeration::DeviceWatcher const& source,
        foundation::IInspectable const& args) noexcept
    {

        UNREFERENCED_PARAMETER(source);

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
    void MidiLegacyPortDeviceWatcher::OnStopped(
        enumeration::DeviceWatcher const& source,
        foundation::IInspectable const& args) noexcept
    {
        UNREFERENCED_PARAMETER(source);

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


    _Use_decl_annotations_
    void MidiLegacyPortDeviceWatcher::InternalInitialize(
        enumeration::DeviceWatcher const& baseWatcher) noexcept
    {
        m_watcher = baseWatcher;

        if (m_watcher != nullptr)
        {
            m_deviceAddedEventRevokeToken = m_watcher.Added({ this, &MidiLegacyPortDeviceWatcher::OnDeviceAdded });
            m_deviceUpdatedEventRevokeToken = m_watcher.Updated({ this, &MidiLegacyPortDeviceWatcher::OnDeviceUpdated });
            m_deviceRemovedEventRevokeToken = m_watcher.Removed({ this, &MidiLegacyPortDeviceWatcher::OnDeviceRemoved });
            m_enumerationCompletedEventRevokeToken = m_watcher.EnumerationCompleted({ this, &MidiLegacyPortDeviceWatcher::OnEnumerationCompleted });
            m_stoppedEventRevokeToken = m_watcher.Stopped({ this, &MidiLegacyPortDeviceWatcher::OnStopped });
        }

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
