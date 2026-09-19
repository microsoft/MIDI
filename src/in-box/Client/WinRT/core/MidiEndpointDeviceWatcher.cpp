// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiEndpointDeviceWatcher.h"
#include "Enumeration.MidiEndpointDeviceWatcher.g.cpp"

#include "MidiEndpointDeviceInformation.h"
#include "MidiEndpointDeviceInformationUpdatedEventArgs.h"
#include "MidiEndpointDeviceInformationRemovedEventArgs.h"
#include "MidiEndpointDeviceInformationAddedEventArgs.h"

#include "mmdeviceapi.h"    // for E_NOTFOUND in the pin map property code
#include "midi_ksa_pin_map_property.h"


namespace
{
    // Every one of these is also in MidiEndpointDeviceInformation::GetAdditionalPropertiesList().
    // A property which is requested there but appears in none of the groups below produces an
    // Updated event with no flags set, which is what applications have to guess about.

    constexpr PCWSTR PropertyKeySystemDeviceParent = L"System.Devices.Parent";
    constexpr PCWSTR PropertyKeySystemDeviceManufacturer = L"System.Devices.DeviceManufacturer";
    constexpr PCWSTR PropertyKeySystemInterfaceClassGuid = L"System.Devices.InterfaceClassGuid";
    constexpr PCWSTR PropertyKeySystemDevicePresent = L"System.Devices.Present";
    constexpr PCWSTR PropertyKeySystemInterfaceEnabled = L"System.Devices.InterfaceEnabled";
    constexpr PCWSTR PropertyKeySystemItemNameDisplay = L"System.ItemNameDisplay";
    constexpr PCWSTR PropertyKeySystemFriendlyName = L"System.Devices.FriendlyName";

    bool ContainsAnyProperty(
        _In_ collections::IMapView<winrt::hstring, foundation::IInspectable> const& properties,
        _In_ std::initializer_list<PCWSTR> const keys)
    {
        for (auto const& key : keys)
        {
            if (properties.HasKey(key))
            {
                return true;
            }
        }

        return false;
    }

    uint32_t ComputeUpdatedFlags(
        _In_ collections::IMapView<winrt::hstring, foundation::IInspectable> const& properties)
    {
        using flags = winrt::Windows::Devices::Midi2::Enumeration::implementation::MidiEndpointDeviceInformationUpdateFlags;

        uint32_t updatedFlags{ flags::None };

        if (ContainsAnyProperty(properties, {
            STRING_PKEY_MIDI_EndpointName,
            PropertyKeySystemItemNameDisplay,
            PropertyKeySystemFriendlyName,
            STRING_PKEY_MIDI_EndpointProvidedName,
            STRING_PKEY_MIDI_CustomEndpointName,
            STRING_PKEY_MIDI_EndpointProvidedNameLastUpdateTime }))
        {
            updatedFlags |= flags::Name;
        }

        if (ContainsAnyProperty(properties, {
            STRING_PKEY_MIDI_EndpointProvidedProductInstanceId,
            STRING_PKEY_MIDI_EndpointProvidedProductInstanceIdLastUpdateTime,
            STRING_PKEY_MIDI_SerialNumber }))
        {
            updatedFlags |= flags::UniqueIds;
        }

        if (ContainsAnyProperty(properties, {
            STRING_PKEY_MIDI_EndpointSupportsMidi2Protocol,
            STRING_PKEY_MIDI_EndpointSupportsMidi1Protocol,
            STRING_PKEY_MIDI_EndpointSupportsReceivingJRTimestamps,
            STRING_PKEY_MIDI_EndpointSupportsSendingJRTimestamps,
            STRING_PKEY_MIDI_EndpointUmpVersionMajor,
            STRING_PKEY_MIDI_EndpointUmpVersionMinor,
            STRING_PKEY_MIDI_EndpointProvidedName,
            STRING_PKEY_MIDI_EndpointProvidedProductInstanceId,
            STRING_PKEY_MIDI_FunctionBlocksAreStatic,
            STRING_PKEY_MIDI_FunctionBlockDeclaredCount,
            STRING_PKEY_MIDI_EndpointInformationLastUpdateTime }))
        {
            updatedFlags |= flags::EndpointInformation;
        }

        if (ContainsAnyProperty(properties, {
            STRING_PKEY_MIDI_DeviceIdentity,
            STRING_PKEY_MIDI_DeviceIdentityLastUpdateTime }))
        {
            updatedFlags |= flags::DeviceIdentity;
        }

        if (ContainsAnyProperty(properties, {
            STRING_PKEY_MIDI_EndpointConfiguredProtocol,
            STRING_PKEY_MIDI_EndpointConfiguredToSendJRTimestamps,
            STRING_PKEY_MIDI_EndpointConfiguredToReceiveJRTimestamps,
            STRING_PKEY_MIDI_EndpointConfigurationLastUpdateTime }))
        {
            updatedFlags |= flags::StreamConfiguration;
        }

        if (internal::PropertyMapContainsAnyFunctionBlockProperty(properties) ||
            properties.HasKey(STRING_PKEY_MIDI_FunctionBlocksLastUpdateTime))
        {
            updatedFlags |= flags::FunctionBlocks;
        }

        if (ContainsAnyProperty(properties, {
            STRING_PKEY_MIDI_CustomEndpointName,
            STRING_PKEY_MIDI_CustomImagePath,
            STRING_PKEY_MIDI_CustomDescription }))
        {
            updatedFlags |= flags::UserMetadata;
        }

        if (ContainsAnyProperty(properties, {
            STRING_PKEY_MIDI_RequiresNoteOffTranslation,
            STRING_PKEY_MIDI_SupportsMidiPolyphonicExpression,
            STRING_PKEY_MIDI_RecommendedCCAutomationIntervalMS }))
        {
            updatedFlags |= flags::AdditionalCapabilities;
        }

        if (properties.HasKey(STRING_PKEY_MIDI_GroupTerminalBlocks))
        {
            updatedFlags |= flags::GroupTerminalBlocks;
        }

        if (properties.HasKey(STRING_PKEY_MIDI_IsMuted))
        {
            updatedFlags |= flags::MutedState;
        }

        if (properties.HasKey(STRING_PKEY_MIDI_EndpointDiscoveryProcessComplete))
        {
            updatedFlags |= flags::EndpointDiscoveryState;
        }

        if (ContainsAnyProperty(properties, {
            STRING_PKEY_MIDI_Midi1PortNameTable,
            STRING_PKEY_MIDI_Midi1PortNamingSelection }))
        {
            updatedFlags |= flags::Midi1PortMapping;
        }

        if (ContainsAnyProperty(properties, {
            PropertyKeySystemDevicePresent,
            PropertyKeySystemInterfaceEnabled }))
        {
            updatedFlags |= flags::DevicePresence;
        }

        if (ContainsAnyProperty(properties, {
            STRING_PKEY_MIDI_MidiOutCalculatedLatencyTicks,
            STRING_PKEY_MIDI_MidiOutCustomLatencyTicks,
            STRING_PKEY_MIDI_MidiOutLatencyTicksUserOverride }))
        {
            updatedFlags |= flags::LatencyProperties;
        }

        if (ContainsAnyProperty(properties, {
            STRING_PKEY_MIDI_TransportLayer,
            STRING_PKEY_MIDI_TransportCode,
            STRING_PKEY_MIDI_NativeDataFormat,
            STRING_PKEY_MIDI_SupportedDataFormats,
            STRING_PKEY_MIDI_SupportsMulticlient,
            STRING_PKEY_MIDI_ManufacturerName,
            STRING_PKEY_MIDI_GenerateIncomingTimestamp,
            STRING_PKEY_MIDI_Description,
            STRING_PKEY_MIDI_EndpointName,
            STRING_PKEY_MIDI_SerialNumber,
            STRING_PKEY_MIDI_UsbVID,
            STRING_PKEY_MIDI_UsbPID,
            STRING_PKEY_MIDI_AssociatedUMP,
            STRING_PKEY_MIDI_EndpointDevicePurpose,
            STRING_PKEY_MIDI_DriverDeviceInterface,
            STRING_DEVPKEY_KsAggMidiGroupPinMap,
            STRING_PKEY_MIDI_TransportEndpointConfigId,
            STRING_PKEY_MIDI_VirtualMidiEndpointAssociator,
            STRING_PKEY_MIDI_NetworkMidiLastRemoteHostName,
            STRING_PKEY_MIDI_NetworkMidiLastRemotePort,
            STRING_PKEY_MIDI_NetworkMidiConnectionRole }))
        {
            updatedFlags |= flags::TransportSuppliedProperties;
        }

        if (ContainsAnyProperty(properties, {
            PropertyKeySystemDeviceParent,
            PropertyKeySystemDeviceManufacturer,
            PropertyKeySystemInterfaceClassGuid,
            PropertyKeySystemDevicePresent,
            PropertyKeySystemInterfaceEnabled,
            PropertyKeySystemItemNameDisplay,
            PropertyKeySystemFriendlyName }))
        {
            updatedFlags |= flags::SystemDeviceProperties;
        }

        return updatedFlags;
    }
}


namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    _Use_decl_annotations_
    winrt::event_token MidiEndpointDeviceWatcher::Added(
        foundation::TypedEventHandler<midi2enum::MidiEndpointDeviceWatcher, midi2enum::MidiEndpointDeviceInformationAddedEventArgs> const& handler)
    {
        return m_deviceAddedEvent.add(handler);
    }
    _Use_decl_annotations_
    void MidiEndpointDeviceWatcher::Added(winrt::event_token const& token) noexcept
    {
        if (m_deviceAddedEvent) m_deviceAddedEvent.remove(token);
    }

    _Use_decl_annotations_
    winrt::event_token MidiEndpointDeviceWatcher::Removed(
        foundation::TypedEventHandler<midi2enum::MidiEndpointDeviceWatcher, midi2enum::MidiEndpointDeviceInformationRemovedEventArgs> const& handler)
    {
        return m_deviceRemovedEvent.add(handler);
    }
    _Use_decl_annotations_
    void MidiEndpointDeviceWatcher::Removed(winrt::event_token const& token) noexcept
    {
        if (m_deviceRemovedEvent) m_deviceRemovedEvent.remove(token);
    }

    _Use_decl_annotations_
    winrt::event_token MidiEndpointDeviceWatcher::Updated(
        foundation::TypedEventHandler<midi2enum::MidiEndpointDeviceWatcher, midi2enum::MidiEndpointDeviceInformationUpdatedEventArgs> const& handler)
    {
        return m_deviceUpdatedEvent.add(handler);
    }
    _Use_decl_annotations_
    void MidiEndpointDeviceWatcher::Updated(winrt::event_token const& token) noexcept
    {
        if (m_deviceUpdatedEvent) m_deviceUpdatedEvent.remove(token);
    }

    _Use_decl_annotations_
    winrt::event_token MidiEndpointDeviceWatcher::EnumerationCompleted(
        foundation::TypedEventHandler<midi2enum::MidiEndpointDeviceWatcher, foundation::IInspectable> const& handler)
    {
        return m_enumerationCompletedEvent.add(handler);
    }
    _Use_decl_annotations_
    void MidiEndpointDeviceWatcher::EnumerationCompleted(winrt::event_token const& token) noexcept
    {
        if (m_enumerationCompletedEvent) m_enumerationCompletedEvent.remove(token);
    }
    
    _Use_decl_annotations_  
    winrt::event_token MidiEndpointDeviceWatcher::Stopped(
        foundation::TypedEventHandler<midi2enum::MidiEndpointDeviceWatcher, foundation::IInspectable> const& handler)
    {
        return m_stoppedEvent.add(handler);
    }
    _Use_decl_annotations_  
    void MidiEndpointDeviceWatcher::Stopped(_In_ winrt::event_token const& token) noexcept
    {
        if (m_stoppedEvent) m_stoppedEvent.remove(token);
    }



    void MidiEndpointDeviceWatcher::Start()
    {
        try
        {
            {
                std::lock_guard<std::mutex> guard(m_enumeratedDevicesLock);
                m_enumeratedEndpointDevices.Clear();
            }

            if (m_watcher)
            {
                m_watcher.Start();
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(this, ex, L"hresult error starting endpoint device watcher.");
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(this, L"General exception starting endpoint device watcher.");
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

    _Use_decl_annotations_
    void MidiEndpointDeviceWatcher::OnDeviceAdded(
        enumeration::DeviceWatcher source,
        enumeration::DeviceInformation args)
    {
        UNREFERENCED_PARAMETER(source);

        try
        {

            auto midiEndpointDeviceInformation = winrt::make_self<MidiEndpointDeviceInformation>();

            midiEndpointDeviceInformation->UpdateFromDeviceInformation(args);

            // our properties are GUID-based, so they can't be referenced in aqs. So, instead, we
            // construct a MidiEndpointDeviceInformation object and then inspect properties there.

            if (midi2enum::MidiEndpointDeviceInformation::DeviceMatchesFilter(*midiEndpointDeviceInformation, m_endpointFilter))
            {
                winrt::com_ptr<MidiEndpointDeviceInformationAddedEventArgs> newArgs{ nullptr };

                {
                    std::lock_guard<std::mutex> guard(m_enumeratedDevicesLock);

                    // add to our map

                    auto mapKey = internal::NormalizeEndpointInterfaceIdHStringCopy(midiEndpointDeviceInformation->EndpointDeviceId());

                    if (!m_enumeratedEndpointDevices.HasKey(mapKey))
                    {
                        m_enumeratedEndpointDevices.Insert(mapKey, *midiEndpointDeviceInformation);

                        if (m_deviceAddedEvent)
                        {
                            newArgs = winrt::make_self<MidiEndpointDeviceInformationAddedEventArgs>();
                            newArgs->InternalInitialize(*midiEndpointDeviceInformation);
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

                // Raised with no lock held: Start() and the other watcher callbacks take
                // m_enumeratedDevicesLock, and std::mutex is not recursive, so an application
                // handler that calls back into this watcher would otherwise deadlock its own thread.
                if (newArgs != nullptr && m_deviceAddedEvent)
                {
                    m_deviceAddedEvent(*this, *newArgs);
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

    _Use_decl_annotations_
    void MidiEndpointDeviceWatcher::OnDeviceUpdated(
        enumeration::DeviceWatcher source,
        enumeration::DeviceInformationUpdate args)
    {
        UNREFERENCED_PARAMETER(source);

        try
        {
            auto mapKey = internal::NormalizeEndpointInterfaceIdHStringCopy(args.Id());

            winrt::com_ptr<MidiEndpointDeviceInformationUpdatedEventArgs> newArgs{ nullptr };

            {
                std::lock_guard<std::mutex> guard(m_enumeratedDevicesLock);

                if (m_enumeratedEndpointDevices.HasKey(mapKey))
                {
                    auto ep = winrt::get_self<MidiEndpointDeviceInformation>(m_enumeratedEndpointDevices.Lookup(mapKey));

                    ep->UpdateFromDeviceInformationUpdate(args);

                    if (m_deviceUpdatedEvent)
                    {
                        auto updatedFlags = ComputeUpdatedFlags(args.Properties());

                        if (updatedFlags == MidiEndpointDeviceInformationUpdateFlags::None)
                        {
                            // A property was requested during enumeration but belongs to no group,
                            // so applications see an update they cannot act on. Fix the grouping.
                            LOG_IF_FAILED(E_UNEXPECTED);   // this also generates a fallback error with file and line number info

                            TraceLoggingWrite(
                                Midi2SdkTelemetryProvider::Provider(),
                                MIDI_SDK_TRACE_EVENT_ERROR,
                                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                                TraceLoggingWideString(L"Updated properties match no update flag group", MIDI_SDK_TRACE_MESSAGE_FIELD),
                                TraceLoggingWideString(mapKey.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                            );
                        }

                        newArgs = winrt::make_self<MidiEndpointDeviceInformationUpdatedEventArgs>();
                        newArgs->InternalInitialize(*ep, args, updatedFlags);
                    }
                }
            }

            // raised with no lock held. See the note in OnDeviceAdded
            if (newArgs != nullptr && m_deviceUpdatedEvent)
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
    void MidiEndpointDeviceWatcher::OnDeviceRemoved(
        enumeration::DeviceWatcher source,
        enumeration::DeviceInformationUpdate args)
    {
        UNREFERENCED_PARAMETER(source);

        try
        {
            auto mapKey = internal::NormalizeEndpointInterfaceIdHStringCopy(args.Id());

            winrt::com_ptr<MidiEndpointDeviceInformationRemovedEventArgs> newArgs{ nullptr };

            {
                std::lock_guard<std::mutex> guard(m_enumeratedDevicesLock);

                if (m_enumeratedEndpointDevices.HasKey(mapKey))
                {
                    newArgs = winrt::make_self<MidiEndpointDeviceInformationRemovedEventArgs>();
                    newArgs->InternalInitialize(m_enumeratedEndpointDevices.Lookup(mapKey), args);

                    m_enumeratedEndpointDevices.Remove(mapKey);
                }
            }

            // raised with no lock held. See the note in OnDeviceAdded
            if (newArgs != nullptr && m_deviceRemovedEvent)
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

    void MidiEndpointDeviceWatcher::OnEnumerationCompleted(
        _In_ enumeration::DeviceWatcher source,
        _In_ foundation::IInspectable args)
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
    void MidiEndpointDeviceWatcher::OnStopped(
        enumeration::DeviceWatcher source,
        foundation::IInspectable args)
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
    void MidiEndpointDeviceWatcher::InternalInitialize(
        midi2enum::MidiEndpointDeviceInformationFilters const& endpointFilters,
        enumeration::DeviceWatcher const& baseWatcher)
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
    midi2enum::MidiEndpointDeviceWatcher MidiEndpointDeviceWatcher::Create(
        midi2enum::MidiEndpointDeviceInformationFilters const& endpointFilters) noexcept
    {
        try
        {
            // the properties we would filter on are GUID properties, so we can't use
            // them in the aqs query. Instead, we have to filter when we get the events
            winrt::hstring aqsFilter = midi2::MidiEndpointConnection::GetDeviceSelector();

            auto watcher = winrt::make_self<MidiEndpointDeviceWatcher>();
            auto baseWatcher = winrt::Windows::Devices::Enumeration::DeviceInformation::CreateWatcher(
                aqsFilter,
                midi2enum::MidiEndpointDeviceInformation::GetAdditionalPropertiesList(),
                enumeration::DeviceInformationKind::DeviceInterface);

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

    midi2enum::MidiEndpointDeviceWatcher MidiEndpointDeviceWatcher::Create() noexcept
    {
        return Create(MidiEndpointDeviceInformationFilters::AllStandardEndpoints);
    }



    enumeration::DeviceWatcherStatus MidiEndpointDeviceWatcher::Status()
    {
        try
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
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(this, ex, L"hresult error getting endpoint device watcher status.");
            return enumeration::DeviceWatcherStatus::Aborted;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(this, L"General exception getting endpoint device watcher status.");
            return enumeration::DeviceWatcherStatus::Aborted;
        }
    }
}
