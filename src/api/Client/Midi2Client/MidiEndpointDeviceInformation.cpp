// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointDeviceInformation.h"
#include "MidiEndpointDeviceInformation.g.cpp"

namespace winrt::Windows::Devices::Midi2::implementation
{

    winrt::Windows::Devices::Enumeration::DeviceInformation MidiEndpointDeviceInformation::GetContainerInformation() const noexcept
    {
        // find the container with the same container ID and DeviceInstanceId as us.

        if (ContainerId() == winrt::guid{}) return nullptr;

        auto container = winrt::Windows::Devices::Enumeration::DeviceInformation::CreateFromIdAsync(
            internal::GuidToString(ContainerId()),
            {
                L"System.Devices.DeviceInstanceId",
                L"System.Devices.Parent",
                L"System.Devices.Manufacturer",
                L"System.Devices.ModelName"
            },
            winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceContainer).get();

        return container;
    }

#define MIDI_DEVICE_PARENT_PROPERTY_KEY L"System.Devices.Parent"

    // this has to jump through some hoops because the parent id isn't returned on a normal "CreateFromIdAsync" call
    // even when you specify it. So instead, you have to find the container, then find the child of the container 
    // that matches this device, then call the FindAllAsync to get the actual device object. That object then
    // has the parent id, whcih you can use to find the parent.
    winrt::Windows::Devices::Enumeration::DeviceInformation MidiEndpointDeviceInformation::GetParentDeviceInformation() const noexcept
    {
        try
        {
            const winrt::hstring rootParent = L"HTREE\\ROOT\\0";

            auto container = GetContainerInformation();

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
                                L"System.Devices.ModelName",
                                L"System.Devices.HardwareIds",
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
            internal::LogGeneralError(__FUNCTION__, L"finding the parent of the MIDI Endpoint device.");
        }

        return nullptr;
    }


    collections::IVectorView<winrt::hstring> MidiEndpointDeviceInformation::GetAdditionalPropertiesList() noexcept
    {
        auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();

        additionalProperties.Append(L"System.ItemNameDisplay");
        additionalProperties.Append(L"System.Devices.FriendlyName");
        additionalProperties.Append(L"System.Devices.Parent");
        additionalProperties.Append(L"System.Devices.DeviceManufacturer");
        additionalProperties.Append(L"System.Devices.InterfaceClassGuid");
        additionalProperties.Append(L"System.Devices.Present");

        // Basics ============================================================================
        additionalProperties.Append(STRING_PKEY_MIDI_AbstractionLayer);
        additionalProperties.Append(STRING_PKEY_MIDI_TransportMnemonic);
        additionalProperties.Append(STRING_PKEY_MIDI_NativeDataFormat);
        additionalProperties.Append(STRING_PKEY_MIDI_SupportedDataFormats);
        additionalProperties.Append(STRING_PKEY_MIDI_SupportsMulticlient);
        additionalProperties.Append(STRING_PKEY_MIDI_TransportSuppliedEndpointName);
        additionalProperties.Append(STRING_PKEY_MIDI_GenerateIncomingTimestamp);

        // USB / KS Properties ===============================================================
        // TODO: Group Terminal Blocks will likely be a single property
        additionalProperties.Append(STRING_PKEY_MIDI_IN_GroupTerminalBlocks);
        additionalProperties.Append(STRING_PKEY_MIDI_OUT_GroupTerminalBlocks);
        additionalProperties.Append(STRING_PKEY_MIDI_AssociatedUMP);
        additionalProperties.Append(STRING_PKEY_MIDI_ManufacturerName);
        additionalProperties.Append(STRING_PKEY_MIDI_SerialNumber);

        // Major Known Endpoint Types ========================================================
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointDevicePurpose);

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
       
        // Additional Capabiltiies ============================================================
        additionalProperties.Append(STRING_PKEY_MIDI_RequiresNoteOffTranslation);
        additionalProperties.Append(STRING_PKEY_MIDI_RecommendedCCAutomationIntervalMS);
        additionalProperties.Append(STRING_PKEY_MIDI_SupportsMidiPolyphonicExpression);

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
        midi2::MidiEndpointDeviceInformationFilter const& endpointFilter) noexcept
    {
        // check if diagnostic loopback 
        if (deviceInformation.EndpointPurpose() == MidiEndpointDevicePurpose::DiagnosticLoopback)
        {
            if ((endpointFilter & midi2::MidiEndpointDeviceInformationFilter::IncludeDiagnosticLoopback) ==
                midi2::MidiEndpointDeviceInformationFilter::IncludeDiagnosticLoopback)
            {
                return true;
            }
        }

        // check if diagnostic ping 
        else if (deviceInformation.EndpointPurpose() == MidiEndpointDevicePurpose::DiagnosticPing)
        {
            if ((endpointFilter & midi2::MidiEndpointDeviceInformationFilter::IncludeDiagnosticPing) ==
                midi2::MidiEndpointDeviceInformationFilter::IncludeDiagnosticPing)
            {
                return true;
            }
        }

        // check if virtual device responder
        else if (deviceInformation.EndpointPurpose() == MidiEndpointDevicePurpose::VirtualDeviceResponder)
        {
            if ((endpointFilter & midi2::MidiEndpointDeviceInformationFilter::IncludeVirtualDeviceResponder) ==
                midi2::MidiEndpointDeviceInformationFilter::IncludeVirtualDeviceResponder)
            {
                return true;
            }
        }

        // check if normal client MIDI 1.0 / bytestream
        else if ((deviceInformation.EndpointPurpose() == MidiEndpointDevicePurpose::NormalMessageEndpoint) &&
            (deviceInformation.NativeDataFormat() == MidiEndpointNativeDataFormat::ByteStream))
        {
            if ((endpointFilter & midi2::MidiEndpointDeviceInformationFilter::IncludeClientByteStreamNative) ==
                midi2::MidiEndpointDeviceInformationFilter::IncludeClientByteStreamNative)
            {
                return true;
            }
        }

        // check if normal client MIDI 2.0 / UMP 
        else if ((deviceInformation.EndpointPurpose() == MidiEndpointDevicePurpose::NormalMessageEndpoint) &&
            (deviceInformation.NativeDataFormat() == MidiEndpointNativeDataFormat::UniversalMidiPacket ||
                deviceInformation.NativeDataFormat() == MidiEndpointNativeDataFormat::Unknown))
        {
            if ((endpointFilter & midi2::MidiEndpointDeviceInformationFilter::IncludeClientUmpNative) == midi2::MidiEndpointDeviceInformationFilter::IncludeClientUmpNative)
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
        midi2::MidiEndpointDeviceInformationFilter const& endpointFilter) noexcept
    {
        std::vector<midi2::MidiEndpointDeviceInformation> midiDevices{};

        try
        {
            auto devices = Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(
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

                    if (DeviceMatchesFilter(*midiDevice, endpointFilter))
                    {
                        midiDevices.push_back(*midiDevice);
                    }

                }
            }
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"exception retrieving list of matching MIDI Endpoint devices");
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
            midi2::MidiEndpointDeviceInformationFilter::IncludeClientUmpNative | 
            midi2::MidiEndpointDeviceInformationFilter::IncludeClientByteStreamNative
        );
    }

    collections::IVectorView<midi2::MidiEndpointDeviceInformation> MidiEndpointDeviceInformation::FindAll() noexcept
    {
        return FindAll(midi2::MidiEndpointDeviceInformationSortOrder::Name);
    }


    _Use_decl_annotations_
    midi2::MidiEndpointDeviceInformation MidiEndpointDeviceInformation::CreateFromId(
        winrt::hstring const& id) noexcept
    {
        try
        {
            auto di = Windows::Devices::Enumeration::DeviceInformation::CreateFromIdAsync(
                id, GetAdditionalPropertiesList()).get();

            if (di != nullptr)
            {
                auto endpointInfo = winrt::make_self<MidiEndpointDeviceInformation>();

                endpointInfo->InternalUpdateFromDeviceInformation(di);

                return *endpointInfo;
            }
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"exception creating and updating device");
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


    winrt::hstring MidiEndpointDeviceInformation::Id() const noexcept
    { 
        return m_id;
    }

    winrt::hstring MidiEndpointDeviceInformation::Name() const noexcept
    {
        // user-supplied name overrides all others
        if (UserSuppliedName() != L"") return UserSuppliedName();

        // endpoint name discovered in-protocol is next highest
        if (EndpointSuppliedName() != L"") return EndpointSuppliedName();

        // transport-supplied name is last
        if (TransportSuppliedName() != L"") return TransportSuppliedName();


        return L"Unknown";
    }

    midi2::MidiEndpointNativeDataFormat MidiEndpointDeviceInformation::NativeDataFormat() const noexcept
    {
        auto formatProperty = GetByteProperty(STRING_PKEY_MIDI_NativeDataFormat, 0);

        if (formatProperty == MIDI_PROP_NATIVEDATAFORMAT_BYTESTREAM)
            return MidiEndpointNativeDataFormat::ByteStream;
        else if (formatProperty == MIDI_PROP_NATIVEDATAFORMAT_UMP)
            return  MidiEndpointNativeDataFormat::UniversalMidiPacket;
        else 
            return midi2::MidiEndpointNativeDataFormat::Unknown;
    }

    midi2::MidiEndpointDevicePurpose MidiEndpointDeviceInformation::EndpointPurpose() const noexcept
    {
        // This assumes we're keeping things in sync with the service
        // like we should be

        return (midi2::MidiEndpointDevicePurpose)GetUInt32Property(STRING_PKEY_MIDI_EndpointDevicePurpose, 0);
    }

    midi2::MidiProtocol MidiEndpointDeviceInformation::ConfiguredProtocol() const noexcept
    {
        auto protocolProperty = GetByteProperty(STRING_PKEY_MIDI_EndpointConfiguredProtocol, (uint8_t)MidiProtocol::Default);

        if (protocolProperty == MIDI_PROP_CONFIGURED_PROTOCOL_MIDI1)
            return MidiProtocol::Midi1;
        else if (protocolProperty == MIDI_PROP_CONFIGURED_PROTOCOL_MIDI2)
            return  MidiProtocol::Midi2;
        else
            return midi2::MidiProtocol::Default;
    }


    collections::IMapView<uint8_t, midi2::MidiFunctionBlock> MidiEndpointDeviceInformation::FunctionBlocks() const noexcept
    {
        return m_functionBlocks.GetView();
    }


    collections::IVectorView<midi2::MidiGroupTerminalBlock> MidiEndpointDeviceInformation::GroupTerminalBlocks() const noexcept
    {
        return m_groupTerminalBlocks.GetView();
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
            else if (key == STRING_PKEY_MIDI_DeviceIdentity)
            {
                ReadDeviceIdentity();
            }
            //else if (key == STRING_PKEY_MIDI_FunctionBlocks)
            //{
            //    ReadFunctionBlocks();
            //}
        }

        m_id = internal::NormalizeEndpointInterfaceIdHStringCopy(deviceInformation.Id().c_str());
        m_transportSuppliedEndpointName = deviceInformation.Name();


        // this is not efficient, but it works. Optimize later.
        for (uint8_t fb = 0; fb < MIDI_MAX_FUNCTION_BLOCKS && fb < FunctionBlockCount(); fb++)
        {
            winrt::hstring functionBlockProperty = winrt::hstring(MIDI_STRING_PKEY_GUID) + winrt::hstring(MIDI_STRING_PKEY_PID_SEPARATOR) + winrt::to_hstring(fb + MIDI_FUNCTION_BLOCK_PROPERTY_INDEX_START);
            winrt::hstring functionBlockNameProperty = winrt::hstring(MIDI_STRING_PKEY_GUID) + winrt::hstring(MIDI_STRING_PKEY_PID_SEPARATOR) + winrt::to_hstring(fb + MIDI_FUNCTION_BLOCK_NAME_PROPERTY_INDEX_START);

            if (deviceInformation.Properties().HasKey(functionBlockProperty))
            {
                // update the function block
                auto refArray = GetBinaryProperty(functionBlockProperty);

                if (refArray != nullptr)
                {
                    AddOrUpdateFunctionBlock(refArray);
                }
            }


            if (deviceInformation.Properties().HasKey(functionBlockNameProperty))
            {
                // update the function block property name

                if (m_functionBlocks.HasKey(fb))
                {
                    auto blockProjection = m_functionBlocks.Lookup(fb);

                    // convert to internal and then set the name
                    auto impl = winrt::get_self<implementation::MidiFunctionBlock>(blockProjection);

                    // we can do this because the properties have already been added/updated to bag
                    impl->InternalSetName(GetStringProperty(functionBlockNameProperty, L""));

                    // TODO: revisit if this is necessary
                    m_functionBlocks.Insert(fb, *impl);
                }
            }
        }

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
            else if (key == STRING_PKEY_MIDI_DeviceIdentity)
            {
                ReadDeviceIdentity();
            }
        }

        // this is not efficient, but it works. Optimize later.
        for (uint8_t fb = 0; fb < MIDI_MAX_FUNCTION_BLOCKS && fb < FunctionBlockCount(); fb++)
        {
            winrt::hstring functionBlockProperty = internal::BuildFunctionBlockPropertyKey(fb);
            winrt::hstring functionBlockNameProperty = internal::BuildFunctionBlockNamePropertyKey(fb);

            if (deviceInformationUpdate.Properties().HasKey(functionBlockProperty))
            {
                // update the function block
                auto refArray = GetBinaryProperty(functionBlockProperty);
                
                if (refArray != nullptr)
                {
                    AddOrUpdateFunctionBlock(refArray);
                }
            }

            if (deviceInformationUpdate.Properties().HasKey(functionBlockNameProperty))
            {
                // update the function block property name

                if (m_functionBlocks.HasKey(fb))
                {
                    auto blockProjection = m_functionBlocks.Lookup(fb);

                    // convert to internal and then set the name
                    auto impl = winrt::get_self<implementation::MidiFunctionBlock>(blockProjection);

                    // we can do this because the properties have already been added/updated to bag
                    impl->InternalSetName(GetStringProperty(functionBlockNameProperty, L""));

                    // revisit if this is necessary
                    m_functionBlocks.Insert(fb, *impl);
                }

            }

        }

        // TODO: need to update the name

        return false;
    }

    _Use_decl_annotations_
    void MidiEndpointDeviceInformation::AddOrUpdateFunctionBlock(foundation::IReferenceArray<uint8_t> refArray)
    {
        if (refArray != nullptr)
        {
            auto data = refArray.Value();
            auto arraySize = data.size();

            if (arraySize == sizeof(MidiFunctionBlockProperty))
            {
                MidiFunctionBlockProperty prop;

                memcpy(&prop, data.data(), arraySize);

                auto block = winrt::make_self<midi2::implementation::MidiFunctionBlock>();

                block->UpdateFromDevPropertyStruct(prop);

                // adds or replaces
                m_functionBlocks.Insert(block->Number(), *block);

            }
        }
    }






    com_array<uint8_t> MidiEndpointDeviceInformation::DeviceIdentitySystemExclusiveId() const noexcept
    {
        return { m_deviceIdentity.ManufacturerSysExIdByte1, m_deviceIdentity.ManufacturerSysExIdByte2, m_deviceIdentity.ManufacturerSysExIdByte3 };
    }

    com_array<uint8_t> MidiEndpointDeviceInformation::DeviceIdentitySoftwareRevisionLevel() const noexcept
    {
        return { m_deviceIdentity.SoftwareRevisionLevelByte1, m_deviceIdentity.SoftwareRevisionLevelByte2, m_deviceIdentity.SoftwareRevisionLevelByte3, m_deviceIdentity.SoftwareRevisionLevelByte4 };
    }

    void MidiEndpointDeviceInformation::ReadDeviceIdentity()
    {
        auto key = STRING_PKEY_MIDI_DeviceIdentity;

        auto refArray = GetBinaryProperty(key);

        if (refArray != nullptr)
        {
            auto data = refArray.Value();
            auto arraySize = data.size();

            if (arraySize == sizeof(m_deviceIdentity))
            {
                memcpy(&m_deviceIdentity, data.data(), arraySize);
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Unable to read device identity. Size of array is incorrect");
            }
        }
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
                    internal::LogGeneralError(__FUNCTION__, L"Value type is something unexpected");
                }
            }
        }
        else
        {
            internal::LogGeneralError(__FUNCTION__, L"Property key not present");
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

                    uint32_t offset = 0;

                    // get the KS_MULTIPLE_ITEMS info. First is a ULONG of the total size, next is a ULONG of the count of items
                    // I should use the struct directly, but I don't want to pull in all that KS stuff here.
                            
                    // we don't actually need anything from the KS_MULTIPLE_ITEMS header
                    offset += sizeof(ULONG) * 2;


                    // read all entries
                    while (offset < arraySize)
                    {
                        auto pheader = (UMP_GROUP_TERMINAL_BLOCK_HEADER*)(data.data() + offset);
                        auto block = winrt::make_self<MidiGroupTerminalBlock>();


                        int charOffset = sizeof(UMP_GROUP_TERMINAL_BLOCK_HEADER);

                        std::wstring name{};

                        // TODO this could be much more efficient just pointing to a string instead of char by char
                        while (charOffset + 1 < pheader->Size)
                        {
                            wchar_t ch = (wchar_t)(*(data.data() + offset + charOffset));

                            name += ch;

                            charOffset += sizeof(wchar_t);
                        }

                        block->InternalUpdateFromPropertyData(pheader, name);

                        // add to our list
                        m_groupTerminalBlocks.Append(*block);

                        // move to the next struct, if there is one
                        offset += pheader->Size;
                    }
                }
            }
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception processing endpoint GTB property");
        }
    }


}
