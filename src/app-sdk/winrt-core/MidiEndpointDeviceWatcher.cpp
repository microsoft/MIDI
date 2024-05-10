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

namespace MIDI_CPP_NAMESPACE::implementation
{
    void MidiEndpointDeviceWatcher::Start()
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        m_enumeratedEndpointDevices.Clear();

        if (m_watcher)
        {
            m_watcher.Start();
        }
    }

    void MidiEndpointDeviceWatcher::Stop()
    { 
        internal::LogInfo(__FUNCTION__, L"Enter");

        if (m_watcher)
        {
            m_watcher.Stop();
        }
    }

    MidiEndpointDeviceWatcher::~MidiEndpointDeviceWatcher()
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

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
        internal::LogInfo(__FUNCTION__, L"Enter");

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
        internal::LogInfo(__FUNCTION__, L"Enter");

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
            internal::LogGeneralError(__FUNCTION__, L"exception in Updated event, likely thrown by the application using this API.");
        }
    }

    void MidiEndpointDeviceWatcher::OnDeviceRemoved(
        _In_ winrt::Windows::Devices::Enumeration::DeviceWatcher source,
        _In_ winrt::Windows::Devices::Enumeration::DeviceInformationUpdate args)
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

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
            internal::LogGeneralError(__FUNCTION__, L"exception in Removed event, likely thrown by the application using this API.");
        }
    }

    void MidiEndpointDeviceWatcher::OnEnumerationCompleted(
        _In_ winrt::Windows::Devices::Enumeration::DeviceWatcher source,
        _In_ winrt::Windows::Foundation::IInspectable args)
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

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
        internal::LogInfo(__FUNCTION__, L"Enter");

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
        midi2::MidiEndpointDeviceInformationFilters const& endpointFilters, 
        winrt::Windows::Devices::Enumeration::DeviceWatcher const& baseWatcher)
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

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
    midi2::MidiEndpointDeviceWatcher MidiEndpointDeviceWatcher::CreateWatcher(
        midi2::MidiEndpointDeviceInformationFilters const& endpointFilters) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

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
            internal::LogGeneralError(__FUNCTION__, L"exception creating MidiEndpointDeviceWatcher.");

            return nullptr;
        }
    }


    winrt::Windows::Devices::Enumeration::DeviceWatcherStatus MidiEndpointDeviceWatcher::Status()
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

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
