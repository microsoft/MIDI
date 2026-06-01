// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Legacy.MidiLegacyPortDeviceInformation.g.h"

namespace winrt::Windows::Devices::Midi2::Enumeration::Legacy::implementation
{
    struct MidiLegacyPortDeviceInformation : MidiLegacyPortDeviceInformationT<MidiLegacyPortDeviceInformation>
    {
        MidiLegacyPortDeviceInformation() = default;

        static legacy::MidiLegacyPortDeviceInformation CreateFromPortDeviceId(
            _In_ winrt::hstring const& portDeviceId, midi2enum::Midi1PortFlow const& flow) noexcept;
        static collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> FindAll() noexcept;
        static collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> FindAll(
            _In_ midi2enum::Midi1PortFlow const& flow) noexcept;
        static collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> FindAllForEndpoint(
            _In_ winrt::hstring const& endpointDeviceId) noexcept;
        static collections::IVectorView<winrt::hstring> GetAdditionalPropertiesList() noexcept;

        enumeration::DeviceInformation DeviceInformation() const noexcept { return m_deviceInformation; }

        winrt::hstring PortDeviceId() const noexcept { return m_deviceInformation != nullptr ? internal::NormalizeEndpointInterfaceIdHStringCopy(m_deviceInformation.Id()) : L""; }
        winrt::hstring Name() const noexcept { return m_deviceInformation != nullptr ? m_deviceInformation.Name() : L""; }
        winrt::guid ContainerId() const noexcept { return internal::SafeGetSwdPropertyFromDeviceInformation<winrt::guid>(L"System.Devices.ContainerId", m_deviceInformation, winrt::guid()); }
        winrt::hstring EndpointDeviceId()  const noexcept { return internal::NormalizeEndpointInterfaceIdHStringCopy(internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(L"System.Devices.Parent", m_deviceInformation, L"")); }
        winrt::hstring DriverDeviceInterfaceId() const noexcept { return internal::NormalizeEndpointInterfaceIdHStringCopy(internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(STRING_PKEY_MIDI_DriverDeviceInterface, m_deviceInformation, L"")); }
        midi2enum::Midi1PortFlow PortFlow() const noexcept { return m_portFlow; }
        uint32_t WinMMPortNumber() const noexcept { return internal::SafeGetSwdPropertyFromDeviceInformation<uint32_t>(STRING_PKEY_MIDI_ServiceAssignedPortNumber, m_deviceInformation, 0); }

        midi2::MidiGroup Group() const noexcept { return MidiGroup(internal::SafeGetSwdPropertyFromDeviceInformation<uint8_t>(STRING_PKEY_MIDI_PortAssignedGroupIndex, m_deviceInformation, 0)); }

        midi2enum::MidiEndpointNativeDataFormat NativeDataFormat() const noexcept
        {
            auto dataFormat = internal::SafeGetSwdPropertyFromDeviceInformation<uint32_t>(STRING_PKEY_MIDI_NativeDataFormat, m_deviceInformation, 0);

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

        void InternalInitialize(_In_ enumeration::DeviceInformation const& deviceInformation, _In_ midi2enum::Midi1PortFlow const flow)
        {
            m_deviceInformation = deviceInformation;
            m_portFlow = flow;
        }

    private:
        enumeration::DeviceInformation m_deviceInformation{ nullptr };
        midi2enum::Midi1PortFlow m_portFlow{ };

        winrt::hstring GetStringProperty(_In_ DEVPROPKEY const& key) const noexcept
        {
            return internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(internal::DevPropKeyToWinRTPropertyHString(key), m_deviceInformation, L"");
        }


    };
}
namespace winrt::Windows::Devices::Midi2::Enumeration::Legacy::factory_implementation
{
    struct MidiLegacyPortDeviceInformation : MidiLegacyPortDeviceInformationT<MidiLegacyPortDeviceInformation, implementation::MidiLegacyPortDeviceInformation>
    {
    };
}
