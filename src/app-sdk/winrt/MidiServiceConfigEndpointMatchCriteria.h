// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "ServiceConfig.MidiServiceConfigEndpointMatchCriteria.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::implementation
{
    struct MidiServiceConfigEndpointMatchCriteria : MidiServiceConfigEndpointMatchCriteriaT<MidiServiceConfigEndpointMatchCriteria>
    {
        MidiServiceConfigEndpointMatchCriteria() = default;

        static hstring MatchObjectKey() { return MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_OBJECT_KEY; }

        winrt::hstring EndpointDeviceId() const noexcept { return m_endpointDeviceId; }
        void EndpointDeviceId(_In_ winrt::hstring const& value) noexcept { m_endpointDeviceId = internal::TrimmedHStringCopy(value); }

        uint16_t UsbVendorId() const noexcept { return m_usbVendorId; }
        void UsbVendorId(_In_ uint16_t const& value) noexcept { m_usbVendorId = value; }

        uint16_t UsbProductId() const noexcept { return m_usbProductId; }
        void UsbProductId(_In_ uint16_t const& value) noexcept { m_usbProductId = value; }

        winrt::hstring UsbSerialNumber() const noexcept { return m_usbSerialNumber; }
        void UsbSerialNumber(_In_ winrt::hstring const& value) noexcept { m_usbSerialNumber = internal::TrimmedHStringCopy(value); }

        winrt::hstring Midi2ProductInstanceId() const noexcept { return m_midi2ProductInstanceId; }
        void Midi2ProductInstanceId(_In_ winrt::hstring const& value) noexcept { m_midi2ProductInstanceId = internal::TrimmedHStringCopy(value); }

        winrt::hstring StaticIPAddress() const noexcept { return m_staticIPAddress; }
        void StaticIPAddress(_In_ winrt::hstring const& value) noexcept { m_staticIPAddress = internal::TrimmedHStringCopy(value); }

        uint16_t Port() const noexcept { return m_port; }
        void Port(uint16_t value) noexcept { m_port = value; }

        winrt::hstring TransportSuppliedEndpointName() const noexcept { return m_transportSuppliedEndpointName; }
        void TransportSuppliedEndpointName(_In_ winrt::hstring const& value) noexcept { m_transportSuppliedEndpointName = internal::TrimmedHStringCopy(value); }

        winrt::hstring ParentDeviceName() const noexcept { return m_parentDeviceName; }
        void ParentDeviceName(_In_ winrt::hstring const& value) noexcept { m_parentDeviceName = internal::TrimmedHStringCopy(value); }

        winrt::hstring GetConfigJson() const noexcept;

    private:
        winrt::hstring m_endpointDeviceId{};

        uint16_t m_usbVendorId{};
        uint16_t m_usbProductId{};
        winrt::hstring m_usbSerialNumber{};

        winrt::hstring m_midi2ProductInstanceId{};

        winrt::hstring m_staticIPAddress{};
        uint16_t m_port{};

        winrt::hstring m_transportSuppliedEndpointName{};
        winrt::hstring m_parentDeviceName{};

    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::factory_implementation
{
    struct MidiServiceConfigEndpointMatchCriteria : MidiServiceConfigEndpointMatchCriteriaT<MidiServiceConfigEndpointMatchCriteria, implementation::MidiServiceConfigEndpointMatchCriteria>
    {
    };
}
