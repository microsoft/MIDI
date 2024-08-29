// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiEndpointDeviceInformation.h"
#include "MidiEndpointDeviceInformation.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
#define MIDI_DEVICE_PARENT_PROPERTY_KEY L"System.Devices.Parent"

    winrt::Windows::Devices::Enumeration::DeviceInformation MidiEndpointDeviceInformation::GetContainerDeviceInformation() const noexcept
    {
        // find the container with the same container ID and DeviceInstanceId as us.

        if (ContainerId() == winrt::guid{}) return nullptr;

        auto container = winrt::Windows::Devices::Enumeration::DeviceInformation::CreateFromIdAsync(
            internal::GuidToString(ContainerId()),
            {
                L"System.Devices.DeviceInstanceId",
                MIDI_DEVICE_PARENT_PROPERTY_KEY,
                L"System.Devices.Manufacturer",
                L"System.Devices.ModelName"
            },
            winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceContainer).get();

        return container;
    }


    // this has to jump through some hoops because the parent id isn't returned on a normal "CreateFromIdAsync" call
    // even when you specify it. So instead, you have to find the container, then find the child of the container 
    // that matches this device, then call the FindAllAsync to get the actual device object. That object then
    // has the parent id, whcih you can use to find the parent.
    winrt::Windows::Devices::Enumeration::DeviceInformation MidiEndpointDeviceInformation::GetParentDeviceInformation() const noexcept
    {
        try
        {
            const winrt::hstring rootParent = L"HTREE\\ROOT\\0";

            auto container = GetContainerDeviceInformation();

            // TODO: This approach needs to be verified with USB.


            auto devices = winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(
                L"System.Devices.ContainerId:=\"" + container.Id() + L"\" AND System.Devices.DeviceInstanceId:=\"" + DeviceInstanceId() + L"\"",
                {
                    MIDI_DEVICE_PARENT_PROPERTY_KEY
                },
                winrt::Windows::Devices::Enumeration::DeviceInformationKind::Device
            ).get();

            if (devices != nullptr && devices.Size() == 1)
            {
                auto myDevice = devices.GetAt(0);

                if (devices.GetAt(0).Properties().HasKey(MIDI_DEVICE_PARENT_PROPERTY_KEY))
                {
                    auto parentId = winrt::unbox_value<winrt::hstring>(devices.GetAt(0).Properties().Lookup(MIDI_DEVICE_PARENT_PROPERTY_KEY));

                    // if the parent is the system root, don't bother trying to resolve that 
                    if (parentId != rootParent)
                    {
                        auto parents = winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(
                            L"System.Devices.DeviceInstanceId:=\"" + parentId + L"\"",
                            {
                                L"System.Devices.Parent",
                                L"System.Devices.DeviceManufacturer",
                                L"System.Devices.Manufacturer"
                                L"System.Devices.ModelName",
                                L"System.Devices.ModelNumber",
                                L"System.Devices.ModelId",
                                L"System.Devices.HardwareIds",
                                L"System.Devices.Paired",
                                L"System.Devices.Present",
                                L"System.Devices.Connected",
                                L"System.Devices.Category",
                                L"System.Devices.ClassGuid",
                                L"System.Devices.DeviceHasProblem",
                                L"System.Devices.InterfaceClassGuid"
                            },
                            winrt::Windows::Devices::Enumeration::DeviceInformationKind::Device).get();

                        if (parents != nullptr && parents.Size() == 1)
                        {
                            return parents.GetAt(0);
                        }
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

        // Basics ============================================================================
        additionalProperties.Append(STRING_PKEY_MIDI_AbstractionLayer);
        additionalProperties.Append(STRING_PKEY_MIDI_TransportCode);

        additionalProperties.Append(STRING_PKEY_MIDI_NativeDataFormat);
        additionalProperties.Append(STRING_PKEY_MIDI_SupportsMulticlient);
        additionalProperties.Append(STRING_PKEY_MIDI_SupportedDataFormats);

        additionalProperties.Append(STRING_PKEY_MIDI_ManufacturerName);
        additionalProperties.Append(STRING_PKEY_MIDI_GenerateIncomingTimestamp);

        additionalProperties.Append(STRING_PKEY_MIDI_TransportSuppliedEndpointName);
        additionalProperties.Append(STRING_PKEY_MIDI_TransportSuppliedDescription);



        // USB / KS Properties ===============================================================
        // TODO: Group Terminal Blocks will likely be a single property
        additionalProperties.Append(STRING_PKEY_MIDI_IN_GroupTerminalBlocks);
        additionalProperties.Append(STRING_PKEY_MIDI_OUT_GroupTerminalBlocks);
        additionalProperties.Append(STRING_PKEY_MIDI_AssociatedUMP);
        additionalProperties.Append(STRING_PKEY_MIDI_SerialNumber);
        additionalProperties.Append(STRING_PKEY_MIDI_UsbVID);
        additionalProperties.Append(STRING_PKEY_MIDI_UsbPID);


        // Major Known Endpoint Types ========================================================
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointDevicePurpose);

        additionalProperties.Append(STRING_PKEY_MIDI_EndpointRequiresMetadataHandler);

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
        additionalProperties.Append(STRING_PKEY_MIDI_FunctionBlockCount);
        additionalProperties.Append(STRING_PKEY_MIDI_FunctionBlocksAreStatic);

        // User-supplied metadata ============================================================
        additionalProperties.Append(STRING_PKEY_MIDI_UserSuppliedEndpointName);
        additionalProperties.Append(STRING_PKEY_MIDI_UserSuppliedLargeImagePath);
        additionalProperties.Append(STRING_PKEY_MIDI_UserSuppliedSmallImagePath);
        additionalProperties.Append(STRING_PKEY_MIDI_UserSuppliedDescription);
       
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
        additionalProperties.Append(STRING_PKEY_MIDI_MidiOutUserSuppliedLatencyTicks);
        additionalProperties.Append(STRING_PKEY_MIDI_MidiOutLatencyTicksUserOverride);


        additionalProperties.Append(STRING_PKEY_MIDI_VirtualMidiEndpointAssociator);


        // these can be useful for debugging, but not much else. They are defined in MidiKSDef.h
        //additionalProperties.Append(STRING_DEVPKEY_KsMidiPort_KsFilterInterfaceId);
        //additionalProperties.Append(STRING_DEVPKEY_KsMidiPort_KsPinId);
        //additionalProperties.Append(STRING_DEVPKEY_KsMidiPort_InPinId);
        //additionalProperties.Append(STRING_DEVPKEY_KsMidiPort_OutPinId);
        //additionalProperties.Append(STRING_DEVPKEY_KsTransport);
        //additionalProperties.Append(STRING_DEVPKEY_KsMidiGroupPinMap);

        



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
        midi2::MidiEndpointDeviceInformation const& deviceInformation,
        midi2::MidiEndpointDeviceInformationFilters const& endpointFilters) noexcept
    {
        // check if diagnostic loopback 
        if (deviceInformation.EndpointPurpose() == MidiEndpointDevicePurpose::DiagnosticLoopback)
        {
            if ((endpointFilters & midi2::MidiEndpointDeviceInformationFilters::DiagnosticLoopback) ==
                midi2::MidiEndpointDeviceInformationFilters::DiagnosticLoopback)
            {
                return true;
            }
        }

        // check if diagnostic ping 
        else if (deviceInformation.EndpointPurpose() == MidiEndpointDevicePurpose::DiagnosticPing)
        {
            if ((endpointFilters & midi2::MidiEndpointDeviceInformationFilters::DiagnosticPing) ==
                midi2::MidiEndpointDeviceInformationFilters::DiagnosticPing)
            {
                return true;
            }
        }

        // check if virtual device responder
        else if (deviceInformation.EndpointPurpose() == MidiEndpointDevicePurpose::VirtualDeviceResponder)
        {
            if ((endpointFilters & midi2::MidiEndpointDeviceInformationFilters::VirtualDeviceResponder) ==
                midi2::MidiEndpointDeviceInformationFilters::VirtualDeviceResponder)
            {
                return true;
            }
        }

        // check if normal client MIDI 1.0 / bytestream
        else if ((deviceInformation.EndpointPurpose() == MidiEndpointDevicePurpose::NormalMessageEndpoint) &&
            (deviceInformation.GetTransportSuppliedInfo().NativeDataFormat == MidiEndpointNativeDataFormat::Midi1ByteFormat))
        {
            if ((endpointFilters & midi2::MidiEndpointDeviceInformationFilters::StandardNativeMidi1ByteFormat) ==
                midi2::MidiEndpointDeviceInformationFilters::StandardNativeMidi1ByteFormat)
            {
                return true;
            }
        }

        // check if normal client MIDI 2.0 / UMP 
        else if ((deviceInformation.EndpointPurpose() == MidiEndpointDevicePurpose::NormalMessageEndpoint) &&
            (deviceInformation.GetTransportSuppliedInfo().NativeDataFormat == MidiEndpointNativeDataFormat::UniversalMidiPacketFormat ||
                deviceInformation.GetTransportSuppliedInfo().NativeDataFormat == MidiEndpointNativeDataFormat::Unknown))
        {
            if ((endpointFilters & midi2::MidiEndpointDeviceInformationFilters::StandardNativeUniversalMidiPacketFormat) == 
                midi2::MidiEndpointDeviceInformationFilters::StandardNativeUniversalMidiPacketFormat)
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
    collections::IVectorView<midi2::MidiEndpointDeviceInformation> MidiEndpointDeviceInformation::FindAll(
        midi2::MidiEndpointDeviceInformationSortOrder const& sortOrder, 
        midi2::MidiEndpointDeviceInformationFilters const& endpointFilters) noexcept
    {
        std::vector<midi2::MidiEndpointDeviceInformation> midiDevices{};

        try
        {
            auto devices = winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(
                MidiEndpointConnection::GetDeviceSelector(),
                GetAdditionalPropertiesList(),
                winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface
            ).get();


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
        case MidiEndpointDeviceInformationSortOrder::Name:
            std::sort(begin(midiDevices), end(midiDevices),
                [](_In_ const auto& device1, _In_ const auto& device2)
                {
                    return device1.Name() < device2.Name();
                });
            break;

        case MidiEndpointDeviceInformationSortOrder::DeviceInstanceId:
            std::sort(begin(midiDevices), end(midiDevices),
                [](_In_ const auto& device1, _In_ const auto& device2)
                {
                    return device1.DeviceInstanceId() < device2.DeviceInstanceId();
                });
            break;

        case MidiEndpointDeviceInformationSortOrder::EndpointDeviceId:
            std::sort(begin(midiDevices), end(midiDevices),
                [](_In_ const auto& device1, _In_ const auto& device2)
                {
                    return device1.EndpointDeviceId() < device2.EndpointDeviceId();
                });
            break;

        case MidiEndpointDeviceInformationSortOrder::ContainerThenName:
            std::sort(begin(midiDevices), end(midiDevices),
                [](_In_ const auto& device1, _In_ const auto& device2)
                {
                    if (device1.ContainerId() == device2.ContainerId())
                        return device1.Name() < device2.Name();

                    return device1.ContainerId() < device2.ContainerId();
                });
            break;

        case MidiEndpointDeviceInformationSortOrder::ContainerThenDeviceInstanceId:
            std::sort(begin(midiDevices), end(midiDevices),
                [](_In_ const auto& device1, _In_ const auto& device2)
                {
                    if (device1.ContainerId() == device2.ContainerId())
                        return device1.DeviceInstanceId() < device2.DeviceInstanceId();

                    return device1.ContainerId() < device2.ContainerId();
                });
            break;

        case MidiEndpointDeviceInformationSortOrder::ContainerThenEndpointDeviceId:
            std::sort(begin(midiDevices), end(midiDevices),
                [](_In_ const auto& device1, _In_ const auto& device2)
                {
                    if (device1.ContainerId() == device2.ContainerId())
                        return device1.EndpointDeviceId() < device2.EndpointDeviceId();

                    return device1.ContainerId() < device2.ContainerId();
                });
            break;

        case MidiEndpointDeviceInformationSortOrder::TransportMnemonicThenName:
            std::sort(begin(midiDevices), end(midiDevices),
                [](_In_ const auto& device1, _In_ const auto& device2)
                {
                    if (device1.GetTransportSuppliedInfo().TransportCode == device2.GetTransportSuppliedInfo().TransportCode)
                        return device1.Name() < device2.Name();

                    return device1.GetTransportSuppliedInfo().TransportCode < device2.GetTransportSuppliedInfo().TransportCode;
                });
            break;

        case MidiEndpointDeviceInformationSortOrder::TransportMnemonicThenEndpointDeviceId:
            std::sort(begin(midiDevices), end(midiDevices),
                [](_In_ const auto& device1, _In_ const auto& device2)
                {
                    if (device1.GetTransportSuppliedInfo().TransportCode == device2.GetTransportSuppliedInfo().TransportCode)
                        return device1.EndpointDeviceId() < device2.EndpointDeviceId();

                    return device1.GetTransportSuppliedInfo().TransportCode < device2.GetTransportSuppliedInfo().TransportCode;
                });
            break;

        case MidiEndpointDeviceInformationSortOrder::TransportMnemonicThenDeviceInstanceId:
            std::sort(begin(midiDevices), end(midiDevices),
                [](_In_ const auto& device1, _In_ const auto& device2)
                {
                    if (device1.GetTransportSuppliedInfo().TransportCode == device2.GetTransportSuppliedInfo().TransportCode)
                        return device1.DeviceInstanceId() < device2.DeviceInstanceId();

                    return device1.GetTransportSuppliedInfo().TransportCode < device2.GetTransportSuppliedInfo().TransportCode;
                });
            break;


        case MidiEndpointDeviceInformationSortOrder::None:
            // do nothing
            break;

        }

        auto winrtCollection = winrt::single_threaded_observable_vector<midi2::MidiEndpointDeviceInformation>(std::move(midiDevices));

        return winrtCollection.GetView();
    }

    _Use_decl_annotations_
    collections::IVectorView<midi2::MidiEndpointDeviceInformation> MidiEndpointDeviceInformation::FindAll(
        midi2::MidiEndpointDeviceInformationSortOrder const& sortOrder) noexcept
    {
        return FindAll(sortOrder, 
            MidiEndpointDeviceInformationFilters::AllStandardEndpoints
        );
    }

    collections::IVectorView<midi2::MidiEndpointDeviceInformation> MidiEndpointDeviceInformation::FindAll() noexcept
    {
        return FindAll(midi2::MidiEndpointDeviceInformationSortOrder::Name);
    }


    _Use_decl_annotations_
    midi2::MidiEndpointDeviceInformation MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(
        winrt::hstring const& endpointDeviceId) noexcept
    {
        try
        {
            auto di = winrt::Windows::Devices::Enumeration::DeviceInformation::CreateFromIdAsync(
                endpointDeviceId, GetAdditionalPropertiesList()).get();

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
    winrt::hstring MidiEndpointDeviceInformation::GetStringProperty(
        winrt::hstring key,
        winrt::hstring defaultValue) const noexcept
    {
        if (!m_properties.HasKey(key)) return defaultValue;

        try
        {
            return winrt::unbox_value<winrt::hstring>(m_properties.Lookup(key));
        }
        catch (...)
        {
            return defaultValue;
        }
    }

    _Use_decl_annotations_
    foundation::DateTime MidiEndpointDeviceInformation::GetDateTimeProperty(
        winrt::hstring key,
        foundation::DateTime defaultValue) const noexcept
    {
        if (!m_properties.HasKey(key)) return defaultValue;

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



    _Use_decl_annotations_
    winrt::guid MidiEndpointDeviceInformation::GetGuidProperty(
        winrt::hstring key,
        winrt::guid defaultValue) const noexcept
    {
        if (!m_properties.HasKey(key)) return defaultValue;
        try
        {
            return winrt::unbox_value<winrt::guid>(m_properties.Lookup(key));
        }
        catch (...)
        {
            return defaultValue;
        }
    }

    _Use_decl_annotations_
    winrt::hstring MidiEndpointDeviceInformation::GetGuidPropertyAsString(
        winrt::hstring key,
        winrt::hstring defaultValue) const noexcept
    {
        if (!m_properties.HasKey(key)) return defaultValue;

        try
        {
            auto guid = winrt::unbox_value<winrt::guid>(m_properties.Lookup(key));

            auto s = internal::GuidToString(guid);

            return winrt::hstring(s);

        }
        catch (...)
        {
            return defaultValue;
        }
    }


    _Use_decl_annotations_
    uint8_t MidiEndpointDeviceInformation::GetByteProperty(
        winrt::hstring key,
        uint8_t defaultValue) const noexcept
    {
        if (!m_properties.HasKey(key)) return defaultValue;

        try
        {
            return winrt::unbox_value<uint8_t>(m_properties.Lookup(key));
        }
        catch (...)
        {
            return defaultValue;
        }
    }

    _Use_decl_annotations_
    uint64_t MidiEndpointDeviceInformation::GetUInt64Property(
        winrt::hstring key,
        uint64_t defaultValue) const noexcept
    {
        if (!m_properties.HasKey(key)) return defaultValue;

        try
        {
            return winrt::unbox_value<uint64_t>(m_properties.Lookup(key));
        }
        catch (...)
        {
            return defaultValue;
        }
    }

    _Use_decl_annotations_
    uint32_t MidiEndpointDeviceInformation::GetUInt32Property(
        winrt::hstring key,
        uint32_t defaultValue) const noexcept
    {
        if (!m_properties.HasKey(key)) return defaultValue;

        try
        {
            return winrt::unbox_value<uint32_t>(m_properties.Lookup(key));
        }
        catch (...)
        {
            return defaultValue;
        }
    }

    _Use_decl_annotations_
    uint16_t MidiEndpointDeviceInformation::GetUInt16Property(
        winrt::hstring key,
        uint16_t defaultValue) const noexcept
    {
        if (!m_properties.HasKey(key)) return defaultValue;

        try
        {
            return winrt::unbox_value<uint16_t>(m_properties.Lookup(key));
        }
        catch (...)
        {
            return defaultValue;
        }
    }



    _Use_decl_annotations_
    int64_t MidiEndpointDeviceInformation::GetInt64Property(
        winrt::hstring key,
        int64_t defaultValue) const noexcept
    {
        if (!m_properties.HasKey(key)) return defaultValue;

        try
        {
            return winrt::unbox_value<int64_t>(m_properties.Lookup(key));
        }
        catch (...)
        {
            return defaultValue;
        }
    }

    _Use_decl_annotations_
    int32_t MidiEndpointDeviceInformation::GetInt32Property(
        winrt::hstring key,
        int32_t defaultValue) const noexcept
    {
        if (!m_properties.HasKey(key)) return defaultValue;

        try
        {
            return winrt::unbox_value<int32_t>(m_properties.Lookup(key));
        }
        catch (...)
        {
            return defaultValue;
        }
    }

    _Use_decl_annotations_
    int16_t MidiEndpointDeviceInformation::GetInt16Property(
        winrt::hstring key,
        int16_t defaultValue) const noexcept
    {
        if (!m_properties.HasKey(key)) return defaultValue;

        try
        {
            return winrt::unbox_value<int16_t>(m_properties.Lookup(key));
        }
        catch (...)
        {
            return defaultValue;
        }
    }





    _Use_decl_annotations_
    bool MidiEndpointDeviceInformation::GetBoolProperty(
        winrt::hstring key,
        bool defaultValue) const noexcept
    {
        if (!m_properties.HasKey(key)) return defaultValue;

        try
        {
            return winrt::unbox_value<bool>(m_properties.Lookup(key));
        }
        catch (...)
        {
            return defaultValue;
        }
    }




    winrt::hstring MidiEndpointDeviceInformation::Name() const noexcept
    {
        // user-supplied name overrides all others
        if (GetUserSuppliedInfo().Name != L"") return GetUserSuppliedInfo().Name;

        // endpoint name discovered in-protocol is next highest
        if (GetDeclaredEndpointInfo().Name != L"") return GetDeclaredEndpointInfo().Name;

        // transport-supplied name is last
        if (GetTransportSuppliedInfo().Name != L"") return GetTransportSuppliedInfo().Name;

        return GetStringProperty(L"System.ItemNameDisplay", L"(Unknown)");
    }

    midi2::MidiEndpointDevicePurpose MidiEndpointDeviceInformation::EndpointPurpose() const noexcept
    {
        // This assumes we're keeping things in sync with the service
        // like we should be

        return (midi2::MidiEndpointDevicePurpose)GetUInt32Property(STRING_PKEY_MIDI_EndpointDevicePurpose, 0);
    }



    collections::IVectorView<midi2::MidiFunctionBlock> MidiEndpointDeviceInformation::GetDeclaredFunctionBlocks() const noexcept
    {
        auto blocks = winrt::single_threaded_vector<midi2::MidiFunctionBlock>();

        for (uint8_t fb = 0; fb < MIDI_MAX_FUNCTION_BLOCKS && fb < GetDeclaredEndpointInfo().DeclaredFunctionBlockCount; fb++)
        {
            winrt::hstring functionBlockProperty = internal::BuildFunctionBlockPropertyKey(fb);
            winrt::hstring functionBlockNameProperty = internal::BuildFunctionBlockNamePropertyKey(fb);

            if (auto refArray = GetBinaryProperty(functionBlockProperty); refArray != nullptr)
            {
                auto data = refArray.Value();
                auto arraySize = data.size();

                auto block = winrt::make_self<midi2::implementation::MidiFunctionBlock>();

                if (arraySize == sizeof(MidiFunctionBlockProperty))
                {
                    MidiFunctionBlockProperty prop;

                    memcpy(&prop, data.data(), arraySize);

                    block->UpdateFromDevPropertyStruct(prop);
                    block->InternalSetName(GetStringProperty(functionBlockNameProperty, L""));
                }

                blocks.Append(*block);
            }
        }

        return blocks.GetView();
    }


    collections::IVectorView<midi2::MidiGroupTerminalBlock> MidiEndpointDeviceInformation::GetGroupTerminalBlocks() const noexcept
    {
        return m_groupTerminalBlocks.GetView();
    }


    midi2::MidiEndpointUserSuppliedInfo MidiEndpointDeviceInformation::GetUserSuppliedInfo() const noexcept
    {
        midi2::MidiEndpointUserSuppliedInfo info{};

        info.Name = GetStringProperty(STRING_PKEY_MIDI_UserSuppliedEndpointName, L"");
        info.Description = GetStringProperty(STRING_PKEY_MIDI_UserSuppliedDescription, L"");
        
        info.LargeImagePath = GetStringProperty(STRING_PKEY_MIDI_UserSuppliedLargeImagePath, L"");
        info.SmallImagePath = GetStringProperty(STRING_PKEY_MIDI_UserSuppliedSmallImagePath, L"");

        info.RequiresNoteOffTranslation = GetBoolProperty(STRING_PKEY_MIDI_RequiresNoteOffTranslation, false);
        info.SupportsMidiPolyphonicExpression = GetBoolProperty(STRING_PKEY_MIDI_SupportsMidiPolyphonicExpression, false);
        info.RecommendedControlChangeAutomationIntervalMilliseconds = GetUInt16Property(STRING_PKEY_MIDI_RecommendedCCAutomationIntervalMS, 0);

        return info;
    }

    midi2::MidiEndpointTransportSuppliedInfo MidiEndpointDeviceInformation::GetTransportSuppliedInfo() const noexcept
    {
        midi2::MidiEndpointTransportSuppliedInfo info{};

        info.Name = GetStringProperty(STRING_PKEY_MIDI_TransportSuppliedEndpointName, L"");
        info.Description = GetStringProperty(STRING_PKEY_MIDI_TransportSuppliedDescription, L"");

//        info.LargeImagePath = GetStringProperty(STRING_PKEY_MIDI_TransportSuppliedLargeImagePath, L"");
//        info.SmallImagePath = GetStringProperty(STRING_PKEY_MIDI_TransportSuppliedSmallImagePath, L"");

        info.TransportId = GetGuidProperty(STRING_PKEY_MIDI_AbstractionLayer, winrt::guid{});
        info.TransportCode = GetStringProperty(STRING_PKEY_MIDI_TransportCode, L"");
        info.SupportsMultiClient = GetBoolProperty(STRING_PKEY_MIDI_SupportsMulticlient, true);

        info.VendorId = GetUInt16Property(STRING_PKEY_MIDI_UsbVID, 0);
        info.ProductId = GetUInt16Property(STRING_PKEY_MIDI_UsbPID, 0);
        info.SerialNumber = GetStringProperty(STRING_PKEY_MIDI_SerialNumber, L"");
        info.ManufacturerName = GetStringProperty(STRING_PKEY_MIDI_ManufacturerName, L"");

        auto formatProperty = GetByteProperty(STRING_PKEY_MIDI_NativeDataFormat, 0);

        if (formatProperty == MIDI_PROP_NATIVEDATAFORMAT_BYTESTREAM)
            info.NativeDataFormat = midi2::MidiEndpointNativeDataFormat::Midi1ByteFormat;
        else if (formatProperty == MIDI_PROP_NATIVEDATAFORMAT_UMP)
            info.NativeDataFormat = midi2::MidiEndpointNativeDataFormat::UniversalMidiPacketFormat;
        else
            info.NativeDataFormat = midi2::MidiEndpointNativeDataFormat::Unknown;
        
        return info;
    }

    midi2::MidiDeclaredStreamConfiguration MidiEndpointDeviceInformation::GetDeclaredStreamConfiguration() const noexcept
    {
        midi2::MidiDeclaredStreamConfiguration config{};

        auto protocolProperty = GetByteProperty(STRING_PKEY_MIDI_EndpointConfiguredProtocol, (uint8_t)MidiProtocol::Default);

        if (protocolProperty == MIDI_PROP_CONFIGURED_PROTOCOL_MIDI1)
            config.Protocol = midi2::MidiProtocol::Midi1;
        else if (protocolProperty == MIDI_PROP_CONFIGURED_PROTOCOL_MIDI2)
            config.Protocol = midi2::MidiProtocol::Midi2;
        else
            config.Protocol = midi2::MidiProtocol::Default;

        config.ReceiveJitterReductionTimestamps = GetBoolProperty(STRING_PKEY_MIDI_EndpointConfiguredToReceiveJRTimestamps, false);
        config.SendJitterReductionTimestamps = GetBoolProperty(STRING_PKEY_MIDI_EndpointConfiguredToSendJRTimestamps, false);

        return config;
    }



    midi2::MidiDeclaredDeviceIdentity MidiEndpointDeviceInformation::GetDeclaredDeviceIdentity() const noexcept
    {
        auto key = STRING_PKEY_MIDI_DeviceIdentity;

        midi2::MidiDeclaredDeviceIdentity di{};

        auto refArray = GetBinaryProperty(key);

        if (refArray != nullptr)
        {
            auto data = refArray.Value();
            auto arraySize = data.size();

            if (arraySize == sizeof(MidiDeviceIdentityProperty))
            {
                MidiDeviceIdentityProperty identity;

                memcpy(&identity, data.data(), arraySize);

                di.DeviceFamilyLsb = identity.DeviceFamilyLsb;
                di.DeviceFamilyMsb = identity.DeviceFamilyMsb;

                di.DeviceFamilyModelNumberLsb = identity.DeviceFamilyModelNumberLsb;
                di.DeviceFamilyModelNumberMsb = identity.DeviceFamilyModelNumberMsb;

                di.SoftwareRevisionLevelByte1 = identity.SoftwareRevisionLevelByte1;
                di.SoftwareRevisionLevelByte2 = identity.SoftwareRevisionLevelByte2;
                di.SoftwareRevisionLevelByte3 = identity.SoftwareRevisionLevelByte3;
                di.SoftwareRevisionLevelByte4 = identity.SoftwareRevisionLevelByte4;

                di.SystemExclusiveIdByte1 = identity.ManufacturerSysExIdByte1;
                di.SystemExclusiveIdByte2 = identity.ManufacturerSysExIdByte2;
                di.SystemExclusiveIdByte3 = identity.ManufacturerSysExIdByte3;

                return di;
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

            }
        }

        return di;
    }


    midi2::MidiDeclaredEndpointInfo MidiEndpointDeviceInformation::GetDeclaredEndpointInfo() const noexcept
    {
        midi2::MidiDeclaredEndpointInfo info;

        info.Name = GetStringProperty(STRING_PKEY_MIDI_EndpointProvidedName, L"");
        info.ProductInstanceId = GetStringProperty(STRING_PKEY_MIDI_EndpointProvidedProductInstanceId, L"");

        info.SupportsMidi10Protocol = GetBoolProperty(STRING_PKEY_MIDI_EndpointSupportsMidi1Protocol, false);
        info.SupportsMidi20Protocol = GetBoolProperty(STRING_PKEY_MIDI_EndpointSupportsMidi2Protocol, false);
        info.SupportsReceivingJitterReductionTimestamps = GetBoolProperty(STRING_PKEY_MIDI_EndpointSupportsReceivingJRTimestamps, false);
        info.SupportsSendingJitterReductionTimestamps = GetBoolProperty(STRING_PKEY_MIDI_EndpointSupportsSendingJRTimestamps, false);

        info.SpecificationVersionMajor = GetByteProperty(STRING_PKEY_MIDI_EndpointUmpVersionMajor, (uint8_t)0);
        info.SpecificationVersionMinor = GetByteProperty(STRING_PKEY_MIDI_EndpointUmpVersionMinor, (uint8_t)0);

        info.HasStaticFunctionBlocks = GetBoolProperty(STRING_PKEY_MIDI_FunctionBlocksAreStatic, false);
        info.DeclaredFunctionBlockCount = GetByteProperty(STRING_PKEY_MIDI_FunctionBlockCount, (uint8_t)0);

        return info;
    }



    _Use_decl_annotations_
    void MidiEndpointDeviceInformation::InternalUpdateFromDeviceInformation(
        winrt::Windows::Devices::Enumeration::DeviceInformation const& deviceInformation) noexcept
    {
        if (deviceInformation == nullptr) return;

        for (auto&& [key, value] : deviceInformation.Properties())
        {
            // insert does a replace if the key exists in the map

            m_properties.Insert(key, value);

            if (key == STRING_PKEY_MIDI_IN_GroupTerminalBlocks)
            {
                ReadGroupTerminalBlocks();
            }
        }

        m_id = internal::NormalizeEndpointInterfaceIdHStringCopy(deviceInformation.Id().c_str());
    }

    _Use_decl_annotations_
    bool MidiEndpointDeviceInformation::UpdateFromDeviceInformation(
        winrt::Windows::Devices::Enumeration::DeviceInformation const& deviceInformation) noexcept
    {
        if (deviceInformation == nullptr) return false;

        // TODO: If an Id is present, check that the ids (after lowercasing) are the same before allowing this.
        // return false if the ids don't match

        InternalUpdateFromDeviceInformation(deviceInformation);

        return true;
    }


    _Use_decl_annotations_
    bool MidiEndpointDeviceInformation::UpdateFromDeviceInformationUpdate(
        winrt::Windows::Devices::Enumeration::DeviceInformationUpdate const& deviceInformationUpdate) noexcept
    {
        if (deviceInformationUpdate == nullptr) return false;

        // TODO: Check that the ids (after lowercasing) are the same before allowing this.
        // return false if the ids don't match


        for (auto&& [key, value] : deviceInformationUpdate.Properties())
        {
            // insert does a replace if the key exists in the map

            m_properties.Insert(key, value);

            if (key == STRING_PKEY_MIDI_IN_GroupTerminalBlocks)
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
        if (m_properties.HasKey(key))
        {
            auto value = m_properties.Lookup(key).as<winrt::Windows::Foundation::IPropertyValue>();

            if (value != nullptr)
            {
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
                }
            }
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
                TraceLoggingWideString(L"Property key not present.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingWideString(key.c_str(), MIDI_SDK_TRACE_PROPERTY_KEY_FIELD)
            );
        }

        return nullptr;
    }



    void MidiEndpointDeviceInformation::ReadGroupTerminalBlocks()
    {
        try
        {
            // in groups property
            // STRING_PKEY_MIDI_IN_GroupTerminalBlocks

            for (auto key : { STRING_PKEY_MIDI_IN_GroupTerminalBlocks /*, STRING_PKEY_MIDI_OUT_GroupTerminalBlocks*/ })
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
                        block->InternalUpdateFromPropertyData(std::move(gtb));

                        m_groupTerminalBlocks.Append(*block);
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
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception processing endpoint GTB property.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_id.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );
        }
    }


}
