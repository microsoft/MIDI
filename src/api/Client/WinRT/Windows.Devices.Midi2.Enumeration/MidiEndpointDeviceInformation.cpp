// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiEndpointDeviceInformation.h"
#include "MidiEndpointDeviceInformation.g.cpp"

#include "MidiEndpointCustomProperties.h"

#include "mmdeviceapi.h"    // for E_NOTFOUND in the pin map property code
#include "midi_ksa_pin_map_property.h"

namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    winrt::hstring MidiEndpointDeviceInformation::ToString()
    {
        winrt::hstring baseName{ L"MidiEndpointDeviceInformation: " };

        if (Name().empty())
        {
            // TODO: Move to resources
            baseName = baseName + L"Unnamed endpoint";
        }
        else
        {
            baseName = baseName + Name();
        }

        return baseName;
    }

#define MIDI_DEVICE_PARENT_PROPERTY_KEY L"System.Devices.Parent"

    enumeration::DeviceInformation MidiEndpointDeviceInformation::GetContainerDeviceInformation() const noexcept
    {
        // find the container with the same container ID and DeviceInstanceId as us.

        if (ContainerId() == winrt::guid{}) return nullptr;

        auto containerId = ContainerId();

        // STA-safe: run the async on the thread pool and wait.
        auto container = internal::RunOnBackgroundThreadAndWait(
            [containerId]()
            {
                return enumeration::DeviceInformation::CreateFromIdAsync(
                    internal::GuidToString(containerId),
                    {
                        L"System.Devices.DeviceInstanceId",
                        MIDI_DEVICE_PARENT_PROPERTY_KEY,
                        L"System.Devices.Manufacturer",
                        L"System.Devices.ModelName"
                    },
                    enumeration::DeviceInformationKind::DeviceContainer);
            });

        return container;
    }


    midi2enum::MidiParentDeviceInformation MidiEndpointDeviceInformation::GetParentDeviceInformation() const noexcept
    {
        try
        {
            // TODO: Need to see if this will return parent information for virtual endpoints as well

            auto parentId = winrt::unbox_value<winrt::hstring>(m_properties.Lookup(MIDI_DEVICE_PARENT_PROPERTY_KEY));

            return midi2enum::MidiParentDeviceInformation::CreateFromId(parentId);
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
                TraceLoggingWideString(L"Exception finding the parent of the MIDI Endpoint device.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );
        }

        return nullptr;
    }


    collections::IVectorView<winrt::hstring> MidiEndpointDeviceInformation::GetAdditionalPropertiesList() noexcept
    {
        auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();

        additionalProperties.Append(L"System.ItemNameDisplay");
        additionalProperties.Append(L"System.Devices.FriendlyName");
        additionalProperties.Append(MIDI_DEVICE_PARENT_PROPERTY_KEY);
        additionalProperties.Append(L"System.Devices.DeviceManufacturer");
        additionalProperties.Append(L"System.Devices.InterfaceClassGuid");
        additionalProperties.Append(L"System.Devices.Present");
        additionalProperties.Append(L"System.Devices.InterfaceEnabled");

        // Basics ============================================================================
        additionalProperties.Append(STRING_PKEY_MIDI_TransportLayer);
        additionalProperties.Append(STRING_PKEY_MIDI_TransportCode);

        additionalProperties.Append(STRING_PKEY_MIDI_NativeDataFormat);
        additionalProperties.Append(STRING_PKEY_MIDI_SupportsMulticlient);
        additionalProperties.Append(STRING_PKEY_MIDI_SupportedDataFormats);

        additionalProperties.Append(STRING_PKEY_MIDI_ManufacturerName);
        additionalProperties.Append(STRING_PKEY_MIDI_GenerateIncomingTimestamp);

        additionalProperties.Append(STRING_PKEY_MIDI_EndpointName);
        additionalProperties.Append(STRING_PKEY_MIDI_Description);



        // USB / KS Properties ===============================================================
        additionalProperties.Append(STRING_PKEY_MIDI_GroupTerminalBlocks);
        additionalProperties.Append(STRING_PKEY_MIDI_AssociatedUMP);
        additionalProperties.Append(STRING_PKEY_MIDI_SerialNumber);
        additionalProperties.Append(STRING_PKEY_MIDI_UsbVID);
        additionalProperties.Append(STRING_PKEY_MIDI_UsbPID);


        // Major Known Endpoint Types ========================================================
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointDevicePurpose);

//        additionalProperties.Append(STRING_PKEY_MIDI_EndpointRequiresMetadataHandler);

        // In-protocol Endpoint information ==================================================
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointSupportsMidi2Protocol);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointSupportsMidi1Protocol);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointSupportsReceivingJRTimestamps);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointSupportsSendingJRTimestamps);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointUmpVersionMajor);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointUmpVersionMinor);

        additionalProperties.Append(STRING_PKEY_MIDI_EndpointProvidedName);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointProvidedProductInstanceId);
        additionalProperties.Append(STRING_PKEY_MIDI_DeviceIdentity);

        // In-protocol Endpoint configuration ================================================
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointConfiguredProtocol);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointConfiguredToSendJRTimestamps);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointConfiguredToReceiveJRTimestamps);

        // Function Blocks ===================================================================
        additionalProperties.Append(STRING_PKEY_MIDI_FunctionBlockDeclaredCount);
        additionalProperties.Append(STRING_PKEY_MIDI_FunctionBlocksAreStatic);

        // User-supplied metadata ============================================================
        additionalProperties.Append(STRING_PKEY_MIDI_CustomEndpointName);
        additionalProperties.Append(STRING_PKEY_MIDI_CustomImagePath);
        additionalProperties.Append(STRING_PKEY_MIDI_CustomDescription);
       
        // Additional Capabilities ============================================================
        additionalProperties.Append(STRING_PKEY_MIDI_RequiresNoteOffTranslation);
        additionalProperties.Append(STRING_PKEY_MIDI_RecommendedCCAutomationIntervalMS);
        additionalProperties.Append(STRING_PKEY_MIDI_SupportsMidiPolyphonicExpression);

        // In-protocol Metadata Timestamps ====================================================

        additionalProperties.Append(STRING_PKEY_MIDI_EndpointProvidedProductInstanceIdLastUpdateTime);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointProvidedNameLastUpdateTime);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointConfigurationLastUpdateTime);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointInformationLastUpdateTime);
        additionalProperties.Append(STRING_PKEY_MIDI_DeviceIdentityLastUpdateTime);
        additionalProperties.Append(STRING_PKEY_MIDI_FunctionBlocksLastUpdateTime);


        additionalProperties.Append(STRING_PKEY_MIDI_MidiOutCalculatedLatencyTicks);
        additionalProperties.Append(STRING_PKEY_MIDI_MidiOutCustomLatencyTicks);
        additionalProperties.Append(STRING_PKEY_MIDI_MidiOutLatencyTicksUserOverride);


        // transport-specific

        additionalProperties.Append(STRING_PKEY_MIDI_TransportEndpointConfigId);
       
        additionalProperties.Append(STRING_PKEY_MIDI_VirtualMidiEndpointAssociator);

        additionalProperties.Append(STRING_PKEY_MIDI_NetworkMidiLastRemoteHostName);
        additionalProperties.Append(STRING_PKEY_MIDI_NetworkMidiLastRemotePort);




        // WinMM / Naming properties ==========================================================
  //      additionalProperties.Append(STRING_PKEY_MIDI_UseLegacyMidi1PortNamingScheme);
        additionalProperties.Append(STRING_PKEY_MIDI_CreateMidi1PortsForEndpoint);
        additionalProperties.Append(STRING_PKEY_MIDI_Midi1PortNamingSelection);
        additionalProperties.Append(STRING_PKEY_MIDI_Midi1PortNameTable);

        // these can be useful for debugging, but not much else. They are defined in MidiKSDef.h
        //additionalProperties.Append(STRING_DEVPKEY_KsMidiPort_KsFilterInterfaceId);
        //additionalProperties.Append(STRING_DEVPKEY_KsMidiPort_KsPinId);
        //additionalProperties.Append(STRING_DEVPKEY_KsMidiPort_InPinId);
        //additionalProperties.Append(STRING_DEVPKEY_KsMidiPort_OutPinId);
        //additionalProperties.Append(STRING_DEVPKEY_KsTransport);
        additionalProperties.Append(STRING_DEVPKEY_KsAggMidiGroupPinMap);
        additionalProperties.Append(STRING_PKEY_MIDI_DriverDeviceInterface);

        
        additionalProperties.Append(STRING_PKEY_MIDI_IsMuted);
        additionalProperties.Append(STRING_PKEY_MIDI_CreateMidi1PortsForEndpoint);



        // Calculated metrics =================================================================
        // we don't load them here because they would spam device information update events
        // and they are (potentially) used only in the service


        // Function Blocks =================================================================


        for (uint8_t fb = 0; fb < MIDI_MAX_FUNCTION_BLOCKS; fb++)
        {
            winrt::hstring functionBlockProperty = internal::BuildFunctionBlockPropertyKey(fb);
            winrt::hstring functionBlockNameProperty = internal::BuildFunctionBlockNamePropertyKey(fb);

            additionalProperties.Append(functionBlockProperty);
            additionalProperties.Append(functionBlockNameProperty);
        }

        return additionalProperties.GetView();
    }


    _Use_decl_annotations_
    bool MidiEndpointDeviceInformation::DeviceMatchesFilter(
        midi2enum::MidiEndpointDeviceInformation const& deviceInformation,
        midi2enum::MidiEndpointDeviceInformationFilters const& endpointFilters) noexcept
    {
        // check if diagnostic loopback 
        if (deviceInformation.EndpointPurpose() == midi2enum::MidiEndpointDevicePurpose::DiagnosticLoopback)
        {
            if ((endpointFilters & midi2enum::MidiEndpointDeviceInformationFilters::DiagnosticLoopback) ==
                midi2enum:: MidiEndpointDeviceInformationFilters::DiagnosticLoopback)
            {
                return true;
            }
        }

        // check if diagnostic ping 
        else if (deviceInformation.EndpointPurpose() == midi2enum::MidiEndpointDevicePurpose::DiagnosticPing)
        {
            if ((endpointFilters & midi2enum::MidiEndpointDeviceInformationFilters::DiagnosticPing) ==
                midi2enum::MidiEndpointDeviceInformationFilters::DiagnosticPing)
            {
                return true;
            }
        }

        // check if virtual device responder
        else if (deviceInformation.EndpointPurpose() == midi2enum::MidiEndpointDevicePurpose::VirtualDeviceResponder)
        {
            if ((endpointFilters & midi2enum::MidiEndpointDeviceInformationFilters::VirtualDeviceResponder) ==
                midi2enum::MidiEndpointDeviceInformationFilters::VirtualDeviceResponder)
            {
                return true;
            }
        }


        // check if normal client MIDI 1.0 / bytestream
        else if ((deviceInformation.EndpointPurpose() == midi2enum::MidiEndpointDevicePurpose::NormalMessageEndpoint) &&
            (deviceInformation.GetTransportSuppliedInfo().NativeDataFormat() == midi2enum::MidiEndpointNativeDataFormat::Midi1ByteFormat))
        {
            if ((endpointFilters & midi2enum::MidiEndpointDeviceInformationFilters::StandardNativeMidi1ByteFormat) ==
                midi2enum::MidiEndpointDeviceInformationFilters::StandardNativeMidi1ByteFormat)
            {
                return true;
            }
        }

        // check if normal client MIDI 2.0 / UMP 
        else if ((deviceInformation.EndpointPurpose() == midi2enum::MidiEndpointDevicePurpose::NormalMessageEndpoint) &&
            (deviceInformation.GetTransportSuppliedInfo().NativeDataFormat() == midi2enum::MidiEndpointNativeDataFormat::UniversalMidiPacketFormat ||
                deviceInformation.GetTransportSuppliedInfo().NativeDataFormat() == midi2enum::MidiEndpointNativeDataFormat::Unknown))
        {
            if ((endpointFilters & midi2enum::MidiEndpointDeviceInformationFilters::StandardNativeUniversalMidiPacketFormat) ==
                midi2enum::MidiEndpointDeviceInformationFilters::StandardNativeUniversalMidiPacketFormat)
            {
                return true;
            }
        }

        // no idea what kind of endpoint this is, so default to including it
        else
        {
            return true;
        }

        return false;
    }


    _Use_decl_annotations_
    collections::IVectorView<midi2enum::MidiEndpointDeviceInformation> MidiEndpointDeviceInformation::FindAll(
        midi2enum::MidiEndpointDeviceInformationSortOrder const& sortOrder,
        midi2enum::MidiEndpointDeviceInformationFilters const& endpointFilters) noexcept
    {
        std::vector<midi2enum::MidiEndpointDeviceInformation> midiDevices{};

        try
        {
            // STA-safe: the inner co_await runs on the thread pool, so the
            // outer .get() never blocks waiting for the caller's apartment.
            auto devices = internal::RunOnBackgroundThreadAndWait(
                []()
                {
                    return winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(
                        MidiEndpointConnection::GetDeviceSelector(),
                        GetAdditionalPropertiesList(),
                        winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface);
                });


            if (devices != nullptr)
            {
                for (auto const& di : devices)
                {
                    auto midiDevice = winrt::make_self<MidiEndpointDeviceInformation>();

                    midiDevice->UpdateFromDeviceInformation(di);

                    if (DeviceMatchesFilter(*midiDevice, endpointFilters))
                    {
                        midiDevices.push_back(*midiDevice);
                    }

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
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception retrieving list of matching MIDI Endpoint devices.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
        }


        switch (sortOrder)
        {
        case midi2enum::MidiEndpointDeviceInformationSortOrder::Name:
            std::sort(begin(midiDevices), end(midiDevices),
                [](_In_ const auto& device1, _In_ const auto& device2)
                {
                    return device1.Name() < device2.Name();
                });
            break;

        case midi2enum::MidiEndpointDeviceInformationSortOrder::DeviceInstanceId:
            std::sort(begin(midiDevices), end(midiDevices),
                [](_In_ const auto& device1, _In_ const auto& device2)
                {
                    return device1.DeviceInstanceId() < device2.DeviceInstanceId();
                });
            break;

        case midi2enum::MidiEndpointDeviceInformationSortOrder::EndpointDeviceId:
            std::sort(begin(midiDevices), end(midiDevices),
                [](_In_ const auto& device1, _In_ const auto& device2)
                {
                    return device1.EndpointDeviceId() < device2.EndpointDeviceId();
                });
            break;

        case midi2enum::MidiEndpointDeviceInformationSortOrder::ContainerThenName:
            std::sort(begin(midiDevices), end(midiDevices),
                [](_In_ const auto& device1, _In_ const auto& device2)
                {
                    if (device1.ContainerId() == device2.ContainerId())
                        return device1.Name() < device2.Name();

                    return device1.ContainerId() < device2.ContainerId();
                });
            break;

        case midi2enum::MidiEndpointDeviceInformationSortOrder::ContainerThenDeviceInstanceId:
            std::sort(begin(midiDevices), end(midiDevices),
                [](_In_ const auto& device1, _In_ const auto& device2)
                {
                    if (device1.ContainerId() == device2.ContainerId())
                        return device1.DeviceInstanceId() < device2.DeviceInstanceId();

                    return device1.ContainerId() < device2.ContainerId();
                });
            break;

        case midi2enum::MidiEndpointDeviceInformationSortOrder::ContainerThenEndpointDeviceId:
            std::sort(begin(midiDevices), end(midiDevices),
                [](_In_ const auto& device1, _In_ const auto& device2)
                {
                    if (device1.ContainerId() == device2.ContainerId())
                        return device1.EndpointDeviceId() < device2.EndpointDeviceId();

                    return device1.ContainerId() < device2.ContainerId();
                });
            break;

        case midi2enum::MidiEndpointDeviceInformationSortOrder::TransportCodeThenName:
            std::sort(begin(midiDevices), end(midiDevices),
                [](_In_ const auto& device1, _In_ const auto& device2)
                {
                    if (device1.GetTransportSuppliedInfo().TransportCode() == device2.GetTransportSuppliedInfo().TransportCode())
                        return device1.Name() < device2.Name();

                    return device1.GetTransportSuppliedInfo().TransportCode() < device2.GetTransportSuppliedInfo().TransportCode();
                });
            break;

        case midi2enum::MidiEndpointDeviceInformationSortOrder::TransportCodeThenEndpointDeviceId:
            std::sort(begin(midiDevices), end(midiDevices),
                [](_In_ const auto& device1, _In_ const auto& device2)
                {
                    if (device1.GetTransportSuppliedInfo().TransportCode() == device2.GetTransportSuppliedInfo().TransportCode())
                        return device1.EndpointDeviceId() < device2.EndpointDeviceId();

                    return device1.GetTransportSuppliedInfo().TransportCode() < device2.GetTransportSuppliedInfo().TransportCode();
                });
            break;

        case midi2enum::MidiEndpointDeviceInformationSortOrder::TransportCodeThenDeviceInstanceId:
            std::sort(begin(midiDevices), end(midiDevices),
                [](_In_ const auto& device1, _In_ const auto& device2)
                {
                    if (device1.GetTransportSuppliedInfo().TransportCode() == device2.GetTransportSuppliedInfo().TransportCode())
                        return device1.DeviceInstanceId() < device2.DeviceInstanceId();

                    return device1.GetTransportSuppliedInfo().TransportCode() < device2.GetTransportSuppliedInfo().TransportCode();
                });
            break;


        case midi2enum::MidiEndpointDeviceInformationSortOrder::None:
            // do nothing
            break;

        }

        auto winrtCollection = winrt::single_threaded_observable_vector<midi2enum::MidiEndpointDeviceInformation>(std::move(midiDevices));

        return winrtCollection.GetView();
    }

    _Use_decl_annotations_
    collections::IVectorView<midi2enum::MidiEndpointDeviceInformation> MidiEndpointDeviceInformation::FindAll(
        midi2enum::MidiEndpointDeviceInformationSortOrder const& sortOrder) noexcept
    {
        return FindAll(sortOrder, 
            midi2enum::MidiEndpointDeviceInformationFilters::AllStandardEndpoints
        );
    }

    collections::IVectorView<midi2enum::MidiEndpointDeviceInformation> MidiEndpointDeviceInformation::FindAll() noexcept
    {
        return FindAll(midi2enum::MidiEndpointDeviceInformationSortOrder::Name);
    }


    _Use_decl_annotations_
    collections::IVectorView<midi2enum::MidiEndpointDeviceInformation> MidiEndpointDeviceInformation::FindAllForContainer(
        winrt::guid const& containerId) noexcept
    {
        auto midiEndpoints = winrt::single_threaded_vector<midi2enum::MidiEndpointDeviceInformation>();

        try
        {

            auto selector = MidiEndpointConnection::GetDeviceSelector()
                + L" AND System.Devices.ContainerId:="
                + winrt::to_hstring(containerId);

            // STA-safe enumeration via background-thread continuation.
            auto devices = internal::RunOnBackgroundThreadAndWait(
                [selector]()
                {
                    return winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(
                        selector,
                        GetAdditionalPropertiesList(),
                        enumeration::DeviceInformationKind::DeviceInterface);
                });


            if (devices != nullptr)
            {
                for (auto const& di : devices)
                {
                    auto midiDevice = winrt::make_self<MidiEndpointDeviceInformation>();

                    midiDevice->UpdateFromDeviceInformation(di);

                    midiEndpoints.Append(*midiDevice);
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
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception creating and updating device.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingGuid(containerId, "container id")
            );
        }


        return midiEndpoints.GetView();
    }





    _Use_decl_annotations_
    midi2enum::MidiEndpointDeviceInformation MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(
        winrt::hstring const& endpointDeviceId) noexcept
    {
        try
        {
            auto cleanedEndpointDeviceId = internal::NormalizeEndpointInterfaceIdHStringCopy(endpointDeviceId);

            // this helps ensure we aren't just using random ids
            if (!internal::IsValidWindowsMidiServicesEndpointId(cleanedEndpointDeviceId))
            {
                return nullptr;
            }

            // we don't like to show stale info. Only return info if device is present
            if (!internal::IsWindowsMidiServicesEndpointPresent(cleanedEndpointDeviceId))
            {
                return nullptr;
            }


            // STA-safe: the inner co_await runs on the thread pool.
            auto di = internal::RunOnBackgroundThreadAndWait(
                [cleanedEndpointDeviceId]()
                {
                    return enumeration::DeviceInformation::CreateFromIdAsync(
                        cleanedEndpointDeviceId,
                        GetAdditionalPropertiesList(),
                        enumeration::DeviceInformationKind::DeviceInterface);
                });


            if (di == nullptr)
            {
                return nullptr;
            }

            // check to see if the interface is enabled
            if (di != nullptr)
            {
                auto endpointInfo = winrt::make_self<MidiEndpointDeviceInformation>();

                endpointInfo->InternalUpdateFromDeviceInformation(di);

                return *endpointInfo;
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
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception creating and updating device.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );
        }

        return nullptr;
    }




    _Use_decl_annotations_
    foundation::DateTime MidiEndpointDeviceInformation::GetDateTimeProperty(
        winrt::hstring key,
        foundation::DateTime defaultValue) const noexcept
    {
        if (!m_properties.HasKey(key)) return defaultValue;
        if (m_properties.Lookup(key) == nullptr) return defaultValue;

        try
        {
            auto propValue = m_properties.Lookup(key).as<winrt::Windows::Foundation::IPropertyValue>();

            if (propValue != nullptr)
            {
                auto ty = propValue.Type();

                if (ty == foundation::PropertyType::DateTime)
                {
                    auto val = propValue.GetDateTime();

                    return val;
                }
            }

        }
        catch (...)
        {
        }

        return defaultValue;

    }





    winrt::hstring MidiEndpointDeviceInformation::Name() const noexcept
    {
        // user-supplied name overrides all others
        if (GetUserSuppliedInfo().Name() != L"") return GetUserSuppliedInfo().Name();

        // endpoint name discovered in-protocol is next highest
        if (GetDeclaredEndpointInfo().Name() != L"") return GetDeclaredEndpointInfo().Name();

        // transport-supplied name is last
        if (GetTransportSuppliedInfo().Name() != L"") return GetTransportSuppliedInfo().Name();

        if (internal::GetDeviceInfoProperty<winrt::hstring>(m_properties, L"System.Devices.FriendlyName", L"") != L"") return internal::GetDeviceInfoProperty<winrt::hstring>(m_properties, L"System.Devices.FriendlyName", L"");   

        return internal::GetDeviceInfoProperty<winrt::hstring>(m_properties, L"System.ItemNameDisplay", L"(Unknown)");
    }

    midi2enum::MidiEndpointDevicePurpose MidiEndpointDeviceInformation::EndpointPurpose() const noexcept
    {
        // This assumes we're keeping things in sync with the service
        // like we should be

        return (midi2enum::MidiEndpointDevicePurpose)internal::GetDeviceInfoProperty<uint32_t>(m_properties, STRING_PKEY_MIDI_EndpointDevicePurpose, 0);
    }



    collections::IVectorView<midi2enum::MidiFunctionBlock> MidiEndpointDeviceInformation::GetDeclaredFunctionBlocks() const noexcept
    {
        auto blocks = winrt::single_threaded_vector<midi2enum::MidiFunctionBlock>();

        try
        {
            auto functionBlockCount = GetDeclaredEndpointInfo().DeclaredFunctionBlockCount();

            for (uint8_t fb = 0; fb < MIDI_MAX_FUNCTION_BLOCKS && fb < functionBlockCount; fb++)
            {
                winrt::hstring functionBlockProperty = internal::BuildFunctionBlockPropertyKey(fb);
                winrt::hstring functionBlockNameProperty = internal::BuildFunctionBlockNamePropertyKey(fb);

                if (auto refArray = GetBinaryProperty(functionBlockProperty); refArray != nullptr)
                {
                    auto data = refArray.Value();
                    auto arraySize = data.size();

                    auto block = winrt::make_self<MidiFunctionBlock>();

                    if (arraySize == sizeof(MidiFunctionBlockProperty))
                    {
                        MidiFunctionBlockProperty prop;

                        memcpy(&prop, data.data(), arraySize);

                        block->UpdateFromDevPropertyStruct(prop);
                        block->InternalSetName(internal::GetDeviceInfoProperty<winrt::hstring>(m_properties, functionBlockNameProperty, L""));
                    }

                    blocks.Append(*block);
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
                TraceLoggingWideString(L"Exception reading function blocks.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingHResult(ex.code(), "exception hresult"),
                TraceLoggingWideString(ex.message().c_str(), "exception message")
            );

            // we let the blocks get returned outside the handler
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
                TraceLoggingWideString(L"Exception reading function blocks.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            // we let the blocks get returned outside the handler
        }

        return blocks.GetView();
    }


    collections::IVectorView<midi2enum::MidiGroupTerminalBlock> MidiEndpointDeviceInformation::GetGroupTerminalBlocks() const noexcept
    {
        return m_groupTerminalBlocks.GetView();
    }


    midi2enum::MidiEndpointUserSuppliedInfo MidiEndpointDeviceInformation::GetUserSuppliedInfo() const noexcept
    {
        auto info = winrt::make_self<MidiEndpointUserSuppliedInfo>();

        if (info == nullptr)
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unable to allocate memory for return type.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            return nullptr;
        }

        try
        {
            info->Name(internal::GetDeviceInfoProperty<winrt::hstring>(m_properties, STRING_PKEY_MIDI_CustomEndpointName, L""));
            info->Description(internal::GetDeviceInfoProperty<winrt::hstring>(m_properties, STRING_PKEY_MIDI_CustomDescription, L""));
        
            info->ImageFileName(internal::GetDeviceInfoProperty<winrt::hstring>(m_properties, STRING_PKEY_MIDI_CustomImagePath, L""));

            info->RequiresNoteOffTranslation(internal::GetDeviceInfoProperty<bool>(m_properties, STRING_PKEY_MIDI_RequiresNoteOffTranslation, false));
            info->SupportsMidiPolyphonicExpression(internal::GetDeviceInfoProperty<bool>(m_properties, STRING_PKEY_MIDI_SupportsMidiPolyphonicExpression, false));
            info->RecommendedControlChangeAutomationIntervalMilliseconds(internal::GetDeviceInfoProperty<uint16_t>(m_properties, STRING_PKEY_MIDI_RecommendedCCAutomationIntervalMS, 0));

            info->CustomMidiOutgoingLatencyTicks(internal::GetDeviceInfoProperty<uint64_t>(m_properties, STRING_PKEY_MIDI_MidiOutCustomLatencyTicks, 0));
            info->UseCustomMidiOutgoingLatencyTicksForScheduling(internal::GetDeviceInfoProperty<bool>(m_properties, STRING_PKEY_MIDI_MidiOutLatencyTicksUserOverride, false));

            info->InternalSetReadOnly();
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
                TraceLoggingWideString(L"Exception reading properties.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingHResult(ex.code(), "exception hresult"),
                TraceLoggingWideString(ex.message().c_str(), "exception message")
            );

            // we let the data get returned outside the handler
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
                TraceLoggingWideString(L"Exception reading properties.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            // we let the data get returned outside the handler
        }

        return *info;
    }

    midi2enum::MidiEndpointTransportSuppliedInfo MidiEndpointDeviceInformation::GetTransportSuppliedInfo() const noexcept
    {
        auto info = winrt::make_self<MidiEndpointTransportSuppliedInfo>();

        if (info == nullptr)
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unable to allocate memory for return type.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            return nullptr;
        }

        try
        {
            info->Name(internal::GetDeviceInfoProperty<winrt::hstring>(m_properties, STRING_PKEY_MIDI_EndpointName, L""));
            info->Description(internal::GetDeviceInfoProperty<winrt::hstring>(m_properties, STRING_PKEY_MIDI_Description, L""));

            info->TransportId(internal::GetDeviceInfoProperty<winrt::guid>(m_properties, STRING_PKEY_MIDI_TransportLayer, winrt::guid{}));
            info->TransportCode(internal::GetDeviceInfoProperty<winrt::hstring>(m_properties, STRING_PKEY_MIDI_TransportCode, L""));
            info->SupportsMultiClient(internal::GetDeviceInfoProperty<bool>(m_properties, STRING_PKEY_MIDI_SupportsMulticlient, true));

            info->VendorId(internal::GetDeviceInfoProperty<uint16_t>(m_properties, STRING_PKEY_MIDI_UsbVID, 0));
            info->ProductId(internal::GetDeviceInfoProperty<uint16_t>(m_properties, STRING_PKEY_MIDI_UsbPID, 0));
            info->SerialNumber(internal::GetDeviceInfoProperty<winrt::hstring>(m_properties, STRING_PKEY_MIDI_SerialNumber, L""));
            info->ManufacturerName(internal::GetDeviceInfoProperty<winrt::hstring>(m_properties, STRING_PKEY_MIDI_ManufacturerName, L""));

            auto formatProperty = internal::GetDeviceInfoProperty<uint8_t>(m_properties, STRING_PKEY_MIDI_NativeDataFormat, 0);

            if (formatProperty == MidiDataFormats::MidiDataFormats_ByteStream)
                info->NativeDataFormat(midi2enum::MidiEndpointNativeDataFormat::Midi1ByteFormat);
            else if (formatProperty == MidiDataFormats::MidiDataFormats_UMP)
                info->NativeDataFormat(midi2enum::MidiEndpointNativeDataFormat::UniversalMidiPacketFormat);
            else
                info->NativeDataFormat(midi2enum::MidiEndpointNativeDataFormat::Unknown);
        

    //        info->DriverName(internal::GetDeviceInfoProperty<winrt::hstring>(m_properties, STRING_PKEY_MIDI_DriverName, L""));
            info->DriverDeviceInterfaceId(internal::GetDeviceInfoProperty<winrt::hstring>(m_properties, STRING_PKEY_MIDI_DriverDeviceInterface, L""));


            info->InternalSetReadOnly();
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
                TraceLoggingWideString(L"Exception reading properties.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingHResult(ex.code(), "exception hresult"),
                TraceLoggingWideString(ex.message().c_str(), "exception message")
            );

            // we let the data get returned outside the handler
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
                TraceLoggingWideString(L"Exception reading properties.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            // we let the data get returned outside the handler
        }

        return *info;
    }

    midi2enum::MidiDeclaredStreamConfiguration MidiEndpointDeviceInformation::GetDeclaredStreamConfiguration() const noexcept
    {
        auto config = winrt::make_self<MidiDeclaredStreamConfiguration>();

        if (config == nullptr)
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unable to allocate memory for return type.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            return nullptr;
        }

        try
        {
            auto protocolProperty = internal::GetDeviceInfoProperty<uint8_t>(m_properties, STRING_PKEY_MIDI_EndpointConfiguredProtocol, (uint8_t)MidiProtocol::Default);

            if (protocolProperty == MIDI_PROP_CONFIGURED_PROTOCOL_MIDI1)
                config->Protocol(midi2enum::MidiProtocol::Midi1);
            else if (protocolProperty == MIDI_PROP_CONFIGURED_PROTOCOL_MIDI2)
                config->Protocol(midi2enum::MidiProtocol::Midi2);
            else
                config->Protocol(midi2enum::MidiProtocol::Default);

            config->ReceiveJitterReductionTimestamps(internal::GetDeviceInfoProperty<bool>(m_properties, STRING_PKEY_MIDI_EndpointConfiguredToReceiveJRTimestamps, false));
            config->SendJitterReductionTimestamps(internal::GetDeviceInfoProperty<bool>(m_properties, STRING_PKEY_MIDI_EndpointConfiguredToSendJRTimestamps, false));

            config->InternalSetReadOnly();
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
                TraceLoggingWideString(L"Exception reading properties.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingHResult(ex.code(), "exception hresult"),
                TraceLoggingWideString(ex.message().c_str(), "exception message")
            );

            // we let the data get returned outside the handler
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
                TraceLoggingWideString(L"Exception reading properties.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            // we let the data get returned outside the handler
        }

        return *config;
    }



    midi2enum::MidiDeclaredDeviceIdentity MidiEndpointDeviceInformation::GetDeclaredDeviceIdentity() const noexcept
    {
        auto di = winrt::make_self<MidiDeclaredDeviceIdentity>();

        auto key = STRING_PKEY_MIDI_DeviceIdentity;

        auto refArray = GetBinaryProperty(key);

        if (refArray == nullptr || di == nullptr)
        {
            return nullptr;
        }

        auto data = refArray.Value();
        auto arraySize = data.size();

        try
        {
            if (arraySize == sizeof(MidiDeviceIdentityProperty))
            {
                MidiDeviceIdentityProperty identity;

                memcpy(&identity, data.data(), arraySize);

                di->SetDeviceFamily(identity.DeviceFamilyLsb, identity.DeviceFamilyMsb);
                di->SetDeviceFamilyModelNumber(identity.DeviceFamilyModelNumberLsb, identity.DeviceFamilyModelNumberMsb);
                di->SetSoftwareRevisionLevel(identity.SoftwareRevisionLevelByte1, identity.SoftwareRevisionLevelByte2, identity.SoftwareRevisionLevelByte3, identity.SoftwareRevisionLevelByte4);
                di->SetSystemExclusiveId(identity.ManufacturerSysExIdByte1, identity.ManufacturerSysExIdByte2, identity.ManufacturerSysExIdByte3);

                di->InternalSetReadOnly();
            }
            else
            {
                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Unable to read device identity. Size of array is incorrect.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                );

                return nullptr;
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
                TraceLoggingWideString(L"Exception reading properties.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingHResult(ex.code(), "exception hresult"),
                TraceLoggingWideString(ex.message().c_str(), "exception message")
            );

            // we let the data get returned outside the handler
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
                TraceLoggingWideString(L"Exception reading properties.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            // we let the data get returned outside the handler
        }

        return *di;
    }


    midi2enum::MidiDeclaredEndpointInfo MidiEndpointDeviceInformation::GetDeclaredEndpointInfo() const noexcept
    {
        auto info = winrt::make_self<MidiDeclaredEndpointInfo>();

        if (info == nullptr)
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unable to allocate memory for return type.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            return nullptr;
        }

        try
        {
            info->Name(internal::GetDeviceInfoProperty<winrt::hstring>(m_properties, STRING_PKEY_MIDI_EndpointProvidedName, L""));
            info->ProductInstanceId(internal::GetDeviceInfoProperty<winrt::hstring>(m_properties, STRING_PKEY_MIDI_EndpointProvidedProductInstanceId, L""));

            info->SupportsMidi10Protocol(internal::GetDeviceInfoProperty<bool>(m_properties, STRING_PKEY_MIDI_EndpointSupportsMidi1Protocol, false));
            info->SupportsMidi20Protocol(internal::GetDeviceInfoProperty<bool>(m_properties, STRING_PKEY_MIDI_EndpointSupportsMidi2Protocol, false));
            info->SupportsReceivingJitterReductionTimestamps(internal::GetDeviceInfoProperty<bool>(m_properties, STRING_PKEY_MIDI_EndpointSupportsReceivingJRTimestamps, false));
            info->SupportsSendingJitterReductionTimestamps(internal::GetDeviceInfoProperty<bool>(m_properties, STRING_PKEY_MIDI_EndpointSupportsSendingJRTimestamps, false));

            info->SpecificationVersionMajor(internal::GetDeviceInfoProperty<uint8_t>(m_properties, STRING_PKEY_MIDI_EndpointUmpVersionMajor, (uint8_t)0));
            info->SpecificationVersionMinor(internal::GetDeviceInfoProperty<uint8_t>(m_properties, STRING_PKEY_MIDI_EndpointUmpVersionMinor, (uint8_t)0));

            info->HasStaticFunctionBlocks(internal::GetDeviceInfoProperty<bool>(m_properties, STRING_PKEY_MIDI_FunctionBlocksAreStatic, false));
            info->DeclaredFunctionBlockCount(internal::GetDeviceInfoProperty<uint8_t>(m_properties, STRING_PKEY_MIDI_FunctionBlockDeclaredCount, (uint8_t)0));

            info->InternalSetReadOnly();
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
                TraceLoggingWideString(L"Exception reading properties.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingHResult(ex.code(), "exception hresult"),
                TraceLoggingWideString(ex.message().c_str(), "exception message")
            );

            // we let the data get returned outside the handler
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
                TraceLoggingWideString(L"Exception reading properties.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            // we let the data get returned outside the handler
        }
        return *info;
    }



    _Use_decl_annotations_
    void MidiEndpointDeviceInformation::InternalUpdateFromDeviceInformation(
        enumeration::DeviceInformation const& deviceInformation) noexcept
    {
        if (deviceInformation == nullptr) return;

        for (auto&& [key, value] : deviceInformation.Properties())
        {
            // insert does a replace if the key exists in the map

            m_properties.Insert(key, value);

            if (key == STRING_PKEY_MIDI_GroupTerminalBlocks)
            {
                ReadGroupTerminalBlocks();
            }
        }

        m_id = internal::NormalizeEndpointInterfaceIdHStringCopy(deviceInformation.Id().c_str());


        // get the parent device id
        winrt::hstring deviceInstanceId{};

        if (deviceInformation.Properties().HasKey(L"System.Devices.DeviceInstanceId"))
        {
            deviceInstanceId = internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(L"System.Devices.DeviceInstanceId", deviceInformation, winrt::hstring());
        }


        // Resolve the parent device instance id via MidiPnpDeviceInfo
        // (cfgmgr-backed, synchronous, STA-safe).
        m_parentDeviceInstanceId = {};

        if (!deviceInstanceId.empty())
        {
            if (auto pnpInfo = internal::MidiPnpDeviceInfo::CreateFromInstanceId(
                std::wstring_view{ deviceInstanceId }))
            {
                m_parentDeviceInstanceId = pnpInfo->ParentInstanceId();
            }
            else
            {
                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_WARNING,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"MidiPnpDeviceInfo::CreateFromInstanceId returned no value", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(deviceInstanceId.c_str(), "device instance id")
                );
            }
        }
    }

    _Use_decl_annotations_
    bool MidiEndpointDeviceInformation::UpdateFromDeviceInformation(
        enumeration::DeviceInformation const& deviceInformation) noexcept
    {
        if (deviceInformation == nullptr) return false;

        // TODO: If an Id is present, check that the ids (after lowercasing) are the same before allowing this.
        // return false if the ids don't match

        InternalUpdateFromDeviceInformation(deviceInformation);

        return true;
    }


    _Use_decl_annotations_
    bool MidiEndpointDeviceInformation::UpdateFromDeviceInformationUpdate(
        enumeration::DeviceInformationUpdate const& deviceInformationUpdate) noexcept
    {
        if (deviceInformationUpdate == nullptr) return false;

        // TODO: Check that the ids (after lowercasing) are the same before allowing this.
        // return false if the ids don't match


        for (auto&& [key, value] : deviceInformationUpdate.Properties())
        {
            // insert does a replace if the key exists in the map

            m_properties.Insert(key, value);

            if (key == STRING_PKEY_MIDI_GroupTerminalBlocks)
            {
                // this shouldn't happen because these should be static
                ReadGroupTerminalBlocks();
            }
        }

        // TODO: need to update the name

        return false;
    }

    _Use_decl_annotations_
    foundation::IReferenceArray<uint8_t> MidiEndpointDeviceInformation::GetBinaryProperty(winrt::hstring key) const noexcept
    {
        try
        {
            if (!m_properties.HasKey(key)) return nullptr;
            if (m_properties.Lookup(key) == nullptr) return nullptr;

            auto value = m_properties.Lookup(key).as<foundation::IPropertyValue>();

            auto t = value.Type();

            if (t == foundation::PropertyType::UInt8Array)
            {
                auto refArray = winrt::unbox_value<foundation::IReferenceArray<uint8_t>>(m_properties.Lookup(key));

                return refArray;
            }
            else
            {
                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Value type is something unexpected.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                    TraceLoggingWideString(key.c_str(), MIDI_SDK_TRACE_PROPERTY_KEY_FIELD)
                );

                return nullptr;
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
                TraceLoggingWideString(L"Exception reading properties.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingHResult(ex.code(), "exception hresult"),
                TraceLoggingWideString(ex.message().c_str(), "exception message")
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
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception reading properties.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            return nullptr;
        }
        
    }



    void MidiEndpointDeviceInformation::ReadGroupTerminalBlocks()
    {
        try
        {
            // in groups property
            // STRING_PKEY_MIDI_IN_GroupTerminalBlocks

            for (auto key : { STRING_PKEY_MIDI_GroupTerminalBlocks /*, STRING_PKEY_MIDI_OUT_GroupTerminalBlocks*/ })
            {
                auto refArray = GetBinaryProperty(key);

                if (refArray != nullptr)
                {
                    auto data = refArray.Value();
                    auto arraySize = data.size();

                    auto gtbs = internal::ReadGroupTerminalBlocksFromPropertyData(refArray.Value().data(), arraySize);

                    for (auto gtb : gtbs)
                    {
                        auto block = winrt::make_self<MidiGroupTerminalBlock>();
                        block->InternalUpdateFromPropertyData(gtb, Name());

                        m_groupTerminalBlocks.Append(*block);
                    }
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
                TraceLoggingWideString(L"Exception reading properties.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingHResult(ex.code(), "exception hresult"),
                TraceLoggingWideString(ex.message().c_str(), "exception message")
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
                TraceLoggingWideString(L"Exception reading properties.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

        }
    }


    midi2enum::Midi1PortNamingApproach MidiEndpointDeviceInformation::Midi1PortNamingApproach() const noexcept
    {
        auto namingPropVal = internal::GetDeviceInfoProperty<uint32_t>(m_properties, STRING_PKEY_MIDI_Midi1PortNamingSelection, (uint32_t)WindowsMidiServicesNamingLib::Midi1PortNameSelection::UseGlobalDefault);

        // these types are value-compatible. However, we don't simply cast because
        // sometimes these properties have garbage in them before they are set for
        // the first time

        switch (namingPropVal)
        {
        case WindowsMidiServicesNamingLib::Midi1PortNameSelection::UseGlobalDefault:
            return midi2enum::Midi1PortNamingApproach::Default;

        case WindowsMidiServicesNamingLib::Midi1PortNameSelection::UseLegacyWinMM:
            return midi2enum::Midi1PortNamingApproach::UseClassicCompatible;

        case WindowsMidiServicesNamingLib::Midi1PortNameSelection::UseNewStyleName:
            return midi2enum::Midi1PortNamingApproach::UseNewStyle;

        default:
            return midi2enum::Midi1PortNamingApproach::Default;
        }
    }


    // instance method
    collections::IVectorView<midi2enum::Midi1PortNameTableEntry> MidiEndpointDeviceInformation::GetNameTable() const noexcept
    {
        collections::IVector<midi2enum::Midi1PortNameTableEntry> nameTable{ winrt::single_threaded_vector<midi2enum::Midi1PortNameTableEntry>() };

        auto nameTableRefArray = GetBinaryProperty(STRING_PKEY_MIDI_Midi1PortNameTable);

        if (nameTableRefArray != nullptr)
        {
            auto refData = nameTableRefArray.Value();

            auto nameTableInternal = WindowsMidiServicesNamingLib::MidiEndpointNameTable::FromPropertyData(refData);

            for (auto const& internalTable : { nameTableInternal->GetAllSourceEntries(), nameTableInternal->GetAllDestinationEntries() })
            {
                for (auto const& nameTableInternalEntry : internalTable)
                {
                    auto entry = winrt::make_self<Midi1PortNameTableEntry>();

                    Midi1PortFlow flow;

                    switch (nameTableInternalEntry.DataFlowFromUserPerspective)
                    {
                    case MidiFlow::MidiFlowIn:
                        flow = Midi1PortFlow::MidiMessageSource;
                        break;
                    case MidiFlow::MidiFlowOut:
                        flow = Midi1PortFlow::MidiMessageDestination;
                        break;
                    default:
                        // this should not happen. Assigning destination as a fallback.
                        flow = Midi1PortFlow::MidiMessageDestination;
                        break;
                    }

                    entry->InternalInitialize(
                        midi2::MidiGroup(nameTableInternalEntry.GroupIndex),
                        flow,
                        nameTableInternalEntry.CustomName,
                        nameTableInternalEntry.LegacyWinMMName,
                        nameTableInternalEntry.NewStyleName
                    );


                    nameTable.Append(*entry);
                }
            }
        }

        return nameTable.GetView();
    }

}
