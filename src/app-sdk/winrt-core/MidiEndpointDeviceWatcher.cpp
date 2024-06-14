// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiEndpointDeviceWatcher.h"
#include "MidiEndpointDeviceWatcher.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
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

    void MidiEndpointDeviceWatcher::OnDeviceAdded(
        _In_ winrt::Windows::Devices::Enumeration::DeviceWatcher source,
        _In_ winrt::Windows::Devices::Enumeration::DeviceInformation args)
    {
        try
        {
            auto midiEndpointDeviceInformation = winrt::make_self<midi2::implementation::MidiEndpointDeviceInformation>();

            midiEndpointDeviceInformation->UpdateFromDeviceInformation(args);

            // our properties are GUID-based, so they can't be referenced in aqs. So, instead, we
            // construct a MidiEndpointDeviceInformation object and then inspect properties there.

            if (MidiEndpointDeviceInformation::DeviceMatchesFilter(*midiEndpointDeviceInformation, m_endpointFilter))
            {
                // add to our map

                auto mapKey = internal::NormalizeEndpointInterfaceIdHStringCopy(midiEndpointDeviceInformation->EndpointDeviceId());

                if (!m_enumeratedEndpointDevices.HasKey(mapKey))
                {
                    m_enumeratedEndpointDevices.Insert(mapKey, *midiEndpointDeviceInformation);

                    if (m_deviceAddedEvent)
                    {
                        auto newArgs = winrt::make_self<midi2::implementation::MidiEndpointDeviceInformationAddedEventArgs>();
                        newArgs->InternalInitialize(*midiEndpointDeviceInformation);

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

    void MidiEndpointDeviceWatcher::OnDeviceUpdated(
        _In_ winrt::Windows::Devices::Enumeration::DeviceWatcher source,
        _In_ winrt::Windows::Devices::Enumeration::DeviceInformationUpdate args)
    {
        try
        {
            auto mapKey = internal::NormalizeEndpointInterfaceIdHStringCopy(args.Id());

            if (m_enumeratedEndpointDevices.HasKey(mapKey))
            {
                auto ep = winrt::get_self<implementation::MidiEndpointDeviceInformation>(m_enumeratedEndpointDevices.Lookup(mapKey));

                ep->UpdateFromDeviceInformationUpdate(args);

                if (m_deviceUpdatedEvent)
                {
                    auto newArgs = winrt::make_self<midi2::implementation::MidiEndpointDeviceInformationUpdatedEventArgs>();

                    bool updatedName{ false };
                    bool updatedInProtocolEndpointInformation{ false };
                    bool updatedDeviceIdentity{ false };
                    bool updatedStreamConfiguration{ false };
                    bool updatedFunctionBlocks{ false };
                    bool updatedUserMetadata{ false };
                    bool updatedAdditionalCapabilities{ false };


                    if (args.Properties().HasKey(STRING_PKEY_MIDI_TransportSuppliedEndpointName) ||
                        args.Properties().HasKey(L"System.ItemNameDisplay") ||
                        args.Properties().HasKey(L"System.Devices.FriendlyName") ||
                        args.Properties().HasKey(STRING_PKEY_MIDI_EndpointProvidedName) ||
                        args.Properties().HasKey(STRING_PKEY_MIDI_UserSuppliedEndpointName))
                    {
                        updatedName = true;
                    }



                    if (args.Properties().HasKey(STRING_PKEY_MIDI_EndpointSupportsMidi2Protocol) ||
                        args.Properties().HasKey(STRING_PKEY_MIDI_EndpointSupportsMidi1Protocol) ||
                        args.Properties().HasKey(STRING_PKEY_MIDI_EndpointUmpVersionMajor) ||
                        args.Properties().HasKey(STRING_PKEY_MIDI_EndpointUmpVersionMinor) ||
                        args.Properties().HasKey(STRING_PKEY_MIDI_EndpointSupportsReceivingJRTimestamps) ||
                        args.Properties().HasKey(STRING_PKEY_MIDI_EndpointSupportsSendingJRTimestamps) ||
                        args.Properties().HasKey(STRING_PKEY_MIDI_EndpointProvidedName) ||
                        args.Properties().HasKey(STRING_PKEY_MIDI_EndpointProvidedProductInstanceId) ||
                        args.Properties().HasKey(STRING_PKEY_MIDI_FunctionBlocksAreStatic) ||
                        args.Properties().HasKey(STRING_PKEY_MIDI_FunctionBlockCount)
                        )
                    {
                        updatedInProtocolEndpointInformation = true;
                    }

                    if (args.Properties().HasKey(STRING_PKEY_MIDI_DeviceIdentity)
                        )
                    {
                        updatedDeviceIdentity = true;
                    }


                    if (args.Properties().HasKey(STRING_PKEY_MIDI_EndpointConfiguredProtocol) ||
                        args.Properties().HasKey(STRING_PKEY_MIDI_EndpointConfiguredToSendJRTimestamps) ||
                        args.Properties().HasKey(STRING_PKEY_MIDI_EndpointConfiguredToReceiveJRTimestamps))
                    {
                        updatedStreamConfiguration = true;
                    }

                    if (internal::PropertyMapContainsAnyFunctionBlockProperty(args.Properties()))
                    {
                        updatedFunctionBlocks = true;
                    }

                    if (args.Properties().HasKey(STRING_PKEY_MIDI_UserSuppliedEndpointName) ||
                        args.Properties().HasKey(STRING_PKEY_MIDI_UserSuppliedLargeImagePath) ||
                        args.Properties().HasKey(STRING_PKEY_MIDI_UserSuppliedSmallImagePath) ||
                        args.Properties().HasKey(STRING_PKEY_MIDI_UserSuppliedDescription)
                        )
                    {
                        updatedUserMetadata = true;
                    }

                    if (args.Properties().HasKey(STRING_PKEY_MIDI_RequiresNoteOffTranslation) ||
                        args.Properties().HasKey(STRING_PKEY_MIDI_SupportsMidiPolyphonicExpression) ||
                        args.Properties().HasKey(STRING_PKEY_MIDI_RecommendedCCAutomationIntervalMS)
                        )
                    {
                        updatedAdditionalCapabilities = true;
                    }

                    newArgs->InternalInitialize(
                        args.Id(),
                        args,
                        updatedName,
                        updatedInProtocolEndpointInformation,
                        updatedDeviceIdentity,
                        updatedStreamConfiguration,
                        updatedFunctionBlocks,
                        updatedUserMetadata,
                        updatedAdditionalCapabilities
                        );

                    m_deviceUpdatedEvent(*this, *newArgs);
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
                TraceLoggingWideString(L"exception in Updated event, likely thrown by the application using this API", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
        }
    }

    void MidiEndpointDeviceWatcher::OnDeviceRemoved(
        _In_ winrt::Windows::Devices::Enumeration::DeviceWatcher source,
        _In_ winrt::Windows::Devices::Enumeration::DeviceInformationUpdate args)
    {
        try
        {
            auto mapKey = internal::NormalizeEndpointInterfaceIdHStringCopy(args.Id());

            auto newArgs = winrt::make_self<midi2::implementation::MidiEndpointDeviceInformationRemovedEventArgs>();

            newArgs->InternalInitialize(args.Id(), args);

            if (m_enumeratedEndpointDevices.HasKey(mapKey))
            {
                m_enumeratedEndpointDevices.Remove(mapKey);

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
    void MidiEndpointDeviceWatcher::InternalInitialize(
        midi2::MidiEndpointDeviceInformationFilters const& endpointFilters, 
        winrt::Windows::Devices::Enumeration::DeviceWatcher const& baseWatcher)
    {
        m_endpointFilter = endpointFilters;
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
    midi2::MidiEndpointDeviceWatcher MidiEndpointDeviceWatcher::Create(
        midi2::MidiEndpointDeviceInformationFilters const& endpointFilters) noexcept
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

            watcher->InternalInitialize(endpointFilters, baseWatcher);

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
