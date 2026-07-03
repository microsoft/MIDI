// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Legacy.MidiLegacyPortDeviceInformation.g.h"

#include <mmdeviceapi.h>

namespace winrt::Windows::Devices::Midi2::Enumeration::Legacy::implementation
{
    struct MidiLegacyPortDeviceInformation : MidiLegacyPortDeviceInformationT<MidiLegacyPortDeviceInformation>
    {
        MidiLegacyPortDeviceInformation() = default;

        static winrt::guid Midi1SourcePortInterfaceClass() noexcept { return DEVINTERFACE_MIDI_INPUT;}
        static winrt::guid Midi1DestinationPortInterfaceClass() noexcept { return DEVINTERFACE_MIDI_OUTPUT; }


        static legacy::MidiLegacyPortDeviceInformation CreateFromPortDeviceId(
            _In_ winrt::hstring const& portDeviceId) noexcept;
        static collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> FindAll() noexcept;
        static collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> FindAll(
            _In_ midi2enum::Midi1PortFlow const& flow) noexcept;
        //static collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> FindAllForParentDevice(
        //    _In_ winrt::hstring const& parentDeviceInstanceId) noexcept;
        //static collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> FindAllForParentDevice(
        //    _In_ winrt::hstring const& parentDeviceInstanceId,
        //    _In_ midi2enum::Midi1PortFlow const& flow) noexcept;

        static collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> FindAllForName(
            _In_ winrt::hstring const& portName) noexcept;
        static collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> FindAllForContainer(
            _In_ winrt::guid const& containerId) noexcept;

        static collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> FindAllForEndpoint(
            _In_ winrt::hstring const& endpointDeviceId) noexcept;
        static collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> FindAllForEndpoint(
            _In_ winrt::hstring const& endpointDeviceId,
            _In_ midi2enum::Midi1PortFlow const& flow) noexcept;


        static collections::IVectorView<winrt::hstring> GetAdditionalPropertiesList() noexcept;

        winrt::hstring PortDeviceId() const noexcept { return m_id; }
        winrt::hstring PortDeviceInstanceId() const noexcept { return internal::NormalizeDeviceInstanceIdHStringCopy(internal::GetDeviceInfoProperty<winrt::hstring>(m_properties, L"System.Devices.DeviceInstanceId", L"")); }
        winrt::hstring Name() const noexcept { return m_name; }
        winrt::guid ContainerId() const noexcept { return internal::GetDeviceInfoProperty<winrt::guid>(m_properties, L"System.Devices.ContainerId", winrt::guid()); }
        winrt::hstring EndpointDeviceId()  const noexcept { return internal::NormalizeEndpointInterfaceIdHStringCopy(internal::GetDeviceInfoProperty<winrt::hstring>(m_properties, STRING_PKEY_MIDI_AssociatedUMP, L"")); }
        winrt::hstring ParentDeviceInstanceId() const noexcept { return m_parentDeviceInstanceId; }
        midi2enum::MidiParentDeviceInformation GetParentDeviceInformation() const noexcept;
        winrt::hstring DriverDeviceInterfaceId() const noexcept { return internal::NormalizeEndpointInterfaceIdHStringCopy(internal::GetDeviceInfoProperty<winrt::hstring>(m_properties, STRING_PKEY_MIDI_DriverDeviceInterface, L"")); }
        midi2enum::Midi1PortFlow Flow() const noexcept { return m_portFlow; }
        uint32_t Number() const noexcept { return m_portNumber; }

        midi2::MidiGroup Group() const noexcept { return MidiGroup(internal::GetDeviceInfoProperty<uint8_t>(m_properties, STRING_PKEY_MIDI_PortAssignedGroupIndex, 0)); }

        midi2enum::MidiEndpointNativeDataFormat NativeDataFormat() const noexcept
        {
            auto dataFormat = internal::GetDeviceInfoProperty<uint32_t>(m_properties, STRING_PKEY_MIDI_NativeDataFormat, 0);

            switch (dataFormat)
            {
            case MidiDataFormats::MidiDataFormats_ByteStream:
                return midi2enum::MidiEndpointNativeDataFormat::Midi1ByteFormat;
            case MidiDataFormats::MidiDataFormats_UMP:
                return midi2enum::MidiEndpointNativeDataFormat::UniversalMidiPacketFormat;
            default:
                return midi2enum::MidiEndpointNativeDataFormat::Unknown;
            }
        }

        winrt::hstring ToString() const noexcept;

        collections::IMapView<winrt::hstring, foundation::IInspectable> Properties() { return m_properties.GetView(); }


        //bool InternalInitialize(_In_ enumeration::DeviceInformation const& deviceInformation) noexcept;

        bool InternalInitialize(
            _In_ winrt::hstring name,
            _In_ winrt::hstring id,
            _In_ collections::IMapView<winrt::hstring, IInspectable> const& properties) noexcept;


        bool InternalUpdateFromDeviceInformationUpdate(_In_ enumeration::DeviceInformationUpdate const& deviceInformationUpdate) noexcept;

        static winrt::hstring InternalGetSelectorForSourceAndDestinationPorts() noexcept;
        static winrt::hstring InternalGetSelectorForDestinationPorts() noexcept;
        static winrt::hstring InternalGetSelectorForSourcePorts() noexcept;


        //static winrt::hstring InternalGetSelectorForSourceAndDestinationPortsForParentDeviceInstanceId(_In_ winrt::hstring const& parentDeviceInstanceId) noexcept;
        static winrt::hstring InternalGetSelectorForSourcePortsForParentDeviceInstanceId(_In_ winrt::hstring const& parentDeviceInstanceId) noexcept;
        static winrt::hstring InternalGetSelectorForDestinationPortsForParentDeviceInstanceId(_In_ winrt::hstring const& parentDeviceInstanceId) noexcept;

        //static winrt::hstring InternalGetSelectorForSourceAndDestinationPortsForContainer(_In_ winrt::guid const& containerId) noexcept;
        static winrt::hstring InternalGetSelectorForSourcePortsForContainer(_In_ winrt::guid const& containerId) noexcept;
        static winrt::hstring InternalGetSelectorForDestinationPortsForContainer(_In_ winrt::guid const& containerId) noexcept;

    private:
        winrt::hstring m_parentDeviceInstanceId{};
        winrt::hstring m_mediaDriverParentDeviceInstanceId{};

        winrt::hstring m_name{};
        winrt::hstring m_id{};
        uint32_t m_portNumber{ 0 };
        midi2enum::Midi1PortFlow m_portFlow{ };

        collections::IMap<winrt::hstring, foundation::IInspectable> m_properties
            { winrt::multi_threaded_map<winrt::hstring, foundation::IInspectable>() };

        //winrt::hstring GetStringProperty(_In_ DEVPROPKEY const& key) const noexcept
        //{
        //    return internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(internal::DevPropKeyToWinRTPropertyHString(key), m_deviceInformation, L"");
        //}


    };
}
namespace winrt::Windows::Devices::Midi2::Enumeration::Legacy::factory_implementation
{
    struct MidiLegacyPortDeviceInformation : MidiLegacyPortDeviceInformationT<MidiLegacyPortDeviceInformation, implementation::MidiLegacyPortDeviceInformation>
    {
    };
}