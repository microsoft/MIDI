// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "ServiceConfig.MidiServiceConfigEndpointMatchCriteria.g.h"

#include "MidiEndpointMatchCriteria.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::implementation
{
    struct MidiServiceConfigEndpointMatchCriteria : MidiServiceConfigEndpointMatchCriteriaT<MidiServiceConfigEndpointMatchCriteria>
    {
        MidiServiceConfigEndpointMatchCriteria() = default;

        static winrt::hstring MatchObjectKey() { return WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria::PropertyKey; }

        static midi2::ServiceConfig::MidiServiceConfigEndpointMatchCriteria FromJson(_In_ winrt::hstring const& matchObjectJson) noexcept;

        winrt::hstring EndpointDeviceId() const noexcept { return m_match->EndpointDeviceId; }
        void EndpointDeviceId(_In_ winrt::hstring const& value) noexcept { m_match->EndpointDeviceId = internal::NormalizeEndpointInterfaceIdHStringCopy(value); }

        winrt::hstring DeviceInstanceId() const noexcept { return m_match->DeviceInstanceId; }
        void DeviceInstanceId(_In_ winrt::hstring const& value) noexcept { m_match->DeviceInstanceId = internal::NormalizeDeviceInstanceIdHStringCopy(value); }

        uint16_t UsbVendorId() const noexcept { return m_match->UsbVendorId; }
        void UsbVendorId(_In_ uint16_t const& value) noexcept { m_match->UsbVendorId = value; }

        uint16_t UsbProductId() const noexcept { return m_match->UsbProductId; }
        void UsbProductId(_In_ uint16_t const& value) noexcept { m_match->UsbProductId = value; }

        winrt::hstring UsbSerialNumber() const noexcept { return m_match->UsbSerialNumber; }
        void UsbSerialNumber(_In_ winrt::hstring const& value) noexcept { m_match->UsbSerialNumber = internal::TrimmedHStringCopy(value); }

        winrt::hstring Midi2ProductInstanceId() const noexcept { return m_match->DeviceProductInstanceId; }
        void Midi2ProductInstanceId(_In_ winrt::hstring const& value) noexcept { m_match->DeviceProductInstanceId = internal::TrimmedHStringCopy(value); }

        winrt::hstring StaticIPAddress() const noexcept { return m_match->NetworkStaticIPAddress; }
        void StaticIPAddress(_In_ winrt::hstring const& value) noexcept { m_match->NetworkStaticIPAddress = internal::TrimmedHStringCopy(value); }

        uint16_t Port() const noexcept { return m_match->NetworkPort; }
        void Port(uint16_t value) noexcept { m_match->NetworkPort = value; }

        winrt::hstring TransportSuppliedEndpointName() const noexcept { return m_match->TransportSuppliedEndpointName; }
        void TransportSuppliedEndpointName(_In_ winrt::hstring const& value) noexcept { m_match->TransportSuppliedEndpointName = internal::TrimmedHStringCopy(value); }

        winrt::hstring ParentDeviceName() const noexcept { return m_match->ParentDeviceName; }
        void ParentDeviceName(_In_ winrt::hstring const& value) noexcept { m_match->ParentDeviceName = internal::TrimmedHStringCopy(value); }

        bool Matches(_In_ midi2::ServiceConfig::MidiServiceConfigEndpointMatchCriteria const& other) const noexcept;



        winrt::hstring GetConfigJson() const noexcept;

        std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria> InternalGetMatchObject() noexcept { return m_match; }
        void InternalSetMatchObject(_In_ std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria> value) noexcept { m_match = value; }

    private:
        std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria> m_match{ std::make_shared<WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria>() };


    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::factory_implementation
{
    struct MidiServiceConfigEndpointMatchCriteria : MidiServiceConfigEndpointMatchCriteriaT<MidiServiceConfigEndpointMatchCriteria, implementation::MidiServiceConfigEndpointMatchCriteria>
    {
    };
}
