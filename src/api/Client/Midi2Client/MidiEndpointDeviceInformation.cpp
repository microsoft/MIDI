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
    winrt::Windows::Devices::Enumeration::DeviceInformation MidiEndpointDeviceInformation::GetContainerInformation()
    {
        // find the container with the same container ID and DeviceInstanceId as us.

        if (ContainerId() == winrt::guid{}) return nullptr;

        auto container = winrt::Windows::Devices::Enumeration::DeviceInformation::CreateFromIdAsync(
            GuidToString(ContainerId()),
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
    winrt::Windows::Devices::Enumeration::DeviceInformation MidiEndpointDeviceInformation::GetParentDeviceInformation()
    {
        const winrt::hstring rootParent = L"HTREE\\ROOT\\0";



        auto container = GetContainerInformation();

        // TODO: This approach needs to be verified with USB.


        OutputDebugString(__FUNCTION__ L"Looking for candidate device entries");

        auto devices = winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(
            L"System.Devices.ContainerId:=\"" + container.Id() + L"\" AND System.Devices.DeviceInstanceId:=\"" + DeviceInstanceId() + L"\"",
            {
                MIDI_DEVICE_PARENT_PROPERTY_KEY
            },
            winrt::Windows::Devices::Enumeration::DeviceInformationKind::Device
        ).get();

        if (devices != nullptr && devices.Size() == 1)
        {
            OutputDebugString(__FUNCTION__ L"Found single possible matching device");

            auto myDevice = devices.GetAt(0);

            if (devices.GetAt(0).Properties().HasKey(MIDI_DEVICE_PARENT_PROPERTY_KEY))
            {
                OutputDebugString(__FUNCTION__ L"Found parent id");

                auto parentId = winrt::unbox_value<winrt::hstring>(devices.GetAt(0).Properties().Lookup(MIDI_DEVICE_PARENT_PROPERTY_KEY));

                // if the parent is the system root, don't bother trying to resolve that 
                if (parentId != rootParent)
                {
                    OutputDebugString(parentId.c_str());

                    OutputDebugString(__FUNCTION__ L"Looking for candidate parents");

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

        return nullptr;

    }


    collections::IVectorView<winrt::hstring> MidiEndpointDeviceInformation::GetAdditionalPropertiesList() noexcept
    {
        auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();

        additionalProperties.Append(L"System.Devices.Parent");
        additionalProperties.Append(L"System.Devices.DeviceManufacturer");
        additionalProperties.Append(L"System.Devices.InterfaceClassGuid");
        additionalProperties.Append(L"System.Devices.Present");

        // Basics ============================================================================
        additionalProperties.Append(STRING_PKEY_MIDI_AbstractionLayer);
        additionalProperties.Append(STRING_PKEY_MIDI_TransportMnemonic);
        additionalProperties.Append(STRING_PKEY_MIDI_NativeDataFormat);
        additionalProperties.Append(STRING_PKEY_MIDI_UniqueIdentifier);
        additionalProperties.Append(STRING_PKEY_MIDI_SupportsMulticlient);
        additionalProperties.Append(STRING_PKEY_MIDI_TransportSuppliedEndpointName);

        // USB / KS Properties ===============================================================
        // TODO: Group Terminal Blocks will likely be a single property
        additionalProperties.Append(STRING_PKEY_MIDI_IN_GroupTerminalBlocks);
        additionalProperties.Append(STRING_PKEY_MIDI_OUT_GroupTerminalBlocks);
        additionalProperties.Append(STRING_PKEY_MIDI_AssociatedUMP);

        // Major Known Endpoint Types ========================================================
        additionalProperties.Append(STRING_PKEY_MIDI_IsVirtualDeviceResponder);
        additionalProperties.Append(STRING_PKEY_MIDI_UmpPing);
        additionalProperties.Append(STRING_PKEY_MIDI_UmpLoopback);

        // In-protocol Endpoint information ==================================================
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointSupportsMidi2Protocol);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointSupportsMidi1Protocol);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointSupportsReceivingJRTimestamps);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointSupportsSendingJRTimestamps);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointUmpVersionMajor);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointUmpVersionMinor);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointProvidedName);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointProvidedProductInstanceId);
        additionalProperties.Append(STRING_PKEY_MIDI_FunctionBlocks);
        additionalProperties.Append(STRING_PKEY_MIDI_FunctionBlocksAreStatic);
        additionalProperties.Append(STRING_PKEY_MIDI_DeviceIdentification);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointConfiguredProtocol);

        // User-supplied metadata ============================================================
        additionalProperties.Append(STRING_PKEY_MIDI_UserSuppliedEndpointName);
        additionalProperties.Append(STRING_PKEY_MIDI_UserSuppliedLargeImagePath);
        additionalProperties.Append(STRING_PKEY_MIDI_UserSuppliedSmallImagePath);
        additionalProperties.Append(STRING_PKEY_MIDI_UserSuppliedDescription);
       

        return additionalProperties.GetView();
    }


    _Use_decl_annotations_
    midi2::MidiEndpointDeviceWatcher MidiEndpointDeviceInformation::CreateWatcher(bool /*includeDiagnosticsEndpoints*/)
    {
        // TODO
        return nullptr;
    }

    _Use_decl_annotations_
    collections::IVectorView<midi2::MidiEndpointDeviceInformation> MidiEndpointDeviceInformation::FindAll(
        midi2::MidiEndpointDeviceInformationSortOrder const& sortOrder, 
        midi2::MidiEndpointDeviceInformationFilter const& endpointFilter) noexcept
    {
        //auto midiDevices = winrt::single_threaded_vector<midi2::MidiEndpointDeviceInformation>();

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
                    auto endpointInfo = winrt::make_self<MidiEndpointDeviceInformation>();

                    endpointInfo->InternalUpdateFromDeviceInformation(di);

                    // check if diagnostic loopback 
                    if (endpointInfo->EndpointPurpose() == MidiEndpointDevicePurpose::DiagnosticLoopback)
                    {
                        if ((endpointFilter & midi2::MidiEndpointDeviceInformationFilter::IncludeDiagnosticLoopback) ==
                                              midi2::MidiEndpointDeviceInformationFilter::IncludeDiagnosticLoopback)
                        {
                            midiDevices.push_back(*endpointInfo);
                        }
                    }
                        
                    // check if diagnostic ping 
                    else if (endpointInfo->EndpointPurpose() == MidiEndpointDevicePurpose::DiagnosticPing)
                    {
                        if ((endpointFilter & midi2::MidiEndpointDeviceInformationFilter::IncludeDiagnosticPing) == 
                                              midi2::MidiEndpointDeviceInformationFilter::IncludeDiagnosticPing)
                        {
                            midiDevices.push_back(*endpointInfo);
                        }
                    }

                    // check if virtual device responder
                    else if (endpointInfo->EndpointPurpose() == MidiEndpointDevicePurpose::VirtualDeviceResponder)
                    {
                        if ((endpointFilter & midi2::MidiEndpointDeviceInformationFilter::IncludeVirtualDeviceResponder) == 
                                              midi2::MidiEndpointDeviceInformationFilter::IncludeVirtualDeviceResponder)
                        {
                            midiDevices.push_back(*endpointInfo);
                        }
                    }

                    // check if normal client MIDI 1.0 / bytestream
                    else if ((endpointInfo->EndpointPurpose() == MidiEndpointDevicePurpose::NormalMessageEndpoint) &&
                        (endpointInfo->NativeDataFormat() == MidiEndpointNativeDataFormat::ByteStream))
                    {
                        if ((endpointFilter & midi2::MidiEndpointDeviceInformationFilter::IncludeClientByteStreamNative) ==
                                              midi2::MidiEndpointDeviceInformationFilter::IncludeClientByteStreamNative)
                        {
                            midiDevices.push_back(*endpointInfo);
                        }
                    }

                    // check if normal client MIDI 2.0 / UMP 
                    else if ((endpointInfo->EndpointPurpose() == MidiEndpointDevicePurpose::NormalMessageEndpoint) &&
                             (endpointInfo->NativeDataFormat() == MidiEndpointNativeDataFormat::UniversalMidiPacket ||
                              endpointInfo->NativeDataFormat() == MidiEndpointNativeDataFormat::Unknown))
                    {
                        if ((endpointFilter & midi2::MidiEndpointDeviceInformationFilter::IncludeClientUmpNative) == midi2::MidiEndpointDeviceInformationFilter::IncludeClientUmpNative)
                        {
                            midiDevices.push_back(*endpointInfo);
                        }
                    }

                    // no idea what kind of endpoint this is, so default to including it
                    else
                    {
                        midiDevices.push_back(*endpointInfo);
                    }
                }
            }
        }
        catch (...)
        {
            // TODO: Log this
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
        auto di = Windows::Devices::Enumeration::DeviceInformation::CreateFromIdAsync(
            id, GetAdditionalPropertiesList()).get();

        if (di != nullptr)
        {
            auto endpointInfo = winrt::make_self<MidiEndpointDeviceInformation>();

            endpointInfo->InternalUpdateFromDeviceInformation(di);

            return *endpointInfo;

        }
        else
        {
            return nullptr;
        }
    }


    _Use_decl_annotations_
    winrt::hstring MidiEndpointDeviceInformation::GetStringProperty(
        winrt::hstring key,
        winrt::hstring defaultValue) const noexcept
    {
        if (m_deviceInformation == nullptr) return defaultValue;

        try
        {
            return winrt::unbox_value<winrt::hstring>(m_deviceInformation.Properties().Lookup(key));
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
        if (m_deviceInformation == nullptr) return defaultValue;

        try
        {
            return winrt::unbox_value<winrt::guid>(m_deviceInformation.Properties().Lookup(key));
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
        if (m_deviceInformation == nullptr) return defaultValue;

        try
        {
            return winrt::unbox_value<uint8_t>(m_deviceInformation.Properties().Lookup(key));
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
        if (m_deviceInformation == nullptr) return defaultValue;

        try
        {
            return winrt::unbox_value<bool>(m_deviceInformation.Properties().Lookup(key));
        }
        catch (...)
        {
            return defaultValue;
        }
    }


    winrt::hstring MidiEndpointDeviceInformation::Id() const noexcept
    { 
        if (m_deviceInformation != nullptr)
        {
            return m_deviceInformation.Id();
        }
        else
        {
            return L"";
        }
    }

    winrt::hstring MidiEndpointDeviceInformation::Name() const noexcept
    {
        // user-supplied name overrides all others
        if (UserSuppliedName() != L"") return UserSuppliedName();

        // endpoint name discovered in-protocol is next highest
        if (EndpointSuppliedName() != L"") return EndpointSuppliedName();

        // transport-supplied name is last
        if (TransportSuppliedName() != L"") return TransportSuppliedName();


        // this is typically the same as the transport-supplied name
        if (m_deviceInformation != nullptr)
            return m_deviceInformation.Name();

        return L"Unknown";
    }

    midi2::MidiEndpointNativeDataFormat MidiEndpointDeviceInformation::NativeDataFormat() const noexcept
    {
        auto formatProperty = GetByteProperty(STRING_PKEY_MIDI_UmpLoopback, 0);

        if (formatProperty == MIDI_PROP_NATIVEDATAFORMAT_BYTESTREAM)
            return MidiEndpointNativeDataFormat::ByteStream;
        else if (formatProperty == MIDI_PROP_NATIVEDATAFORMAT_UMP)
            return  MidiEndpointNativeDataFormat::UniversalMidiPacket;
        else 
            return midi2::MidiEndpointNativeDataFormat::Unknown;
    }

    midi2::MidiEndpointDevicePurpose MidiEndpointDeviceInformation::EndpointPurpose() const noexcept
    {
        if (GetBoolProperty(STRING_PKEY_MIDI_UmpLoopback, false) == true)
            return MidiEndpointDevicePurpose::DiagnosticLoopback;

        if (GetBoolProperty(STRING_PKEY_MIDI_UmpPing, false) == true)
            return MidiEndpointDevicePurpose::DiagnosticPing;

        if (GetBoolProperty(STRING_PKEY_MIDI_IsVirtualDeviceResponder, false) == true)
            return MidiEndpointDevicePurpose::VirtualDeviceResponder;

        return midi2::MidiEndpointDevicePurpose::NormalMessageEndpoint;
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


    collections::IVectorView<midi2::MidiFunctionBlock> MidiEndpointDeviceInformation::FunctionBlocks() const noexcept
    {
        collections::IVector<midi2::MidiFunctionBlock> blocks{ winrt::single_threaded_vector<midi2::MidiFunctionBlock>() };

        // TODO: Populate from property

        return blocks.GetView();
    }


    collections::IVectorView<midi2::MidiGroupTerminalBlock> MidiEndpointDeviceInformation::GroupTerminalBlocks() const noexcept
    {
        collections::IVector<midi2::MidiGroupTerminalBlock> blocks{ winrt::single_threaded_vector<midi2::MidiGroupTerminalBlock>() };

        // TODO: Populate from property

        return blocks.GetView();
    }






    _Use_decl_annotations_
    void MidiEndpointDeviceInformation::InternalUpdateFromDeviceInformation(
        winrt::Windows::Devices::Enumeration::DeviceInformation const& info) noexcept
    {
        if (info != nullptr)
        {
            m_deviceInformation = info;
        }
    }


}
