// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "ServiceConfig.MidiServiceEndpointCustomizationProvenance.g.h"

#include "MidiEndpointCustomizationProvenance.h"

namespace winrt::Windows::Devices::Midi2::ServiceConfig::implementation
{
    struct MidiServiceEndpointCustomizationProvenance : MidiServiceEndpointCustomizationProvenanceT<MidiServiceEndpointCustomizationProvenance>
    {
        MidiServiceEndpointCustomizationProvenance() = default;

        static winrt::hstring ProvenanceObjectKey() { return WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomizationProvenance::PropertyKey; }

        static midi2::ServiceConfig::MidiServiceEndpointCustomizationProvenance CreateForEndpoint(
            _In_ midi2enum::MidiEndpointDeviceInformation const& endpointDeviceInformation) noexcept;

        winrt::hstring CreatedFor() const noexcept { return m_provenance->CreatedFor; }
        void CreatedFor(_In_ winrt::hstring const& value) noexcept { m_provenance->CreatedFor = internal::TrimmedHStringCopy(value); }

        winrt::hstring Created() const noexcept { return m_provenance->Created; }
        void Created(_In_ winrt::hstring const& value) noexcept { m_provenance->Created = internal::TrimmedHStringCopy(value); }

        uint16_t UsbVendorId() const noexcept { return m_provenance->UsbVendorId; }
        void UsbVendorId(_In_ uint16_t const value) noexcept { m_provenance->UsbVendorId = value; }

        uint16_t UsbProductId() const noexcept { return m_provenance->UsbProductId; }
        void UsbProductId(_In_ uint16_t const value) noexcept { m_provenance->UsbProductId = value; }

        winrt::hstring UsbSerialNumber() const noexcept { return m_provenance->UsbSerialNumber; }
        void UsbSerialNumber(_In_ winrt::hstring const& value) noexcept { m_provenance->UsbSerialNumber = internal::TrimmedHStringCopy(value); }

        winrt::hstring ManufacturerName() const noexcept { return m_provenance->ManufacturerName; }
        void ManufacturerName(_In_ winrt::hstring const& value) noexcept { m_provenance->ManufacturerName = internal::TrimmedHStringCopy(value); }

        winrt::hstring TransportSuppliedName() const noexcept { return m_provenance->TransportSuppliedName; }
        void TransportSuppliedName(_In_ winrt::hstring const& value) noexcept { m_provenance->TransportSuppliedName = internal::TrimmedHStringCopy(value); }

        winrt::hstring ParentDeviceInstanceId() const noexcept { return m_provenance->ParentDeviceInstanceId; }
        void ParentDeviceInstanceId(_In_ winrt::hstring const& value) noexcept { m_provenance->ParentDeviceInstanceId = internal::TrimmedHStringCopy(value); }

        midi2::ServiceConfig::MidiCustomizationLatencySource LatencySource() const noexcept
        {
            return static_cast<midi2::ServiceConfig::MidiCustomizationLatencySource>(m_provenance->LatencySource);
        }

        void LatencySource(_In_ midi2::ServiceConfig::MidiCustomizationLatencySource const value) noexcept
        {
            m_provenance->LatencySource = static_cast<WindowsMidiServicesPluginConfigurationLib::MidiCustomizationLatencySource>(value);
        }

        winrt::hstring LatencyMeasured() const noexcept { return m_provenance->LatencyMeasured; }
        void LatencyMeasured(_In_ winrt::hstring const& value) noexcept { m_provenance->LatencyMeasured = internal::TrimmedHStringCopy(value); }

        winrt::hstring GetConfigJson() const noexcept;

        std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomizationProvenance> InternalGetProvenanceObject() noexcept { return m_provenance; }

        void InternalSetProvenanceObject(
            _In_ std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomizationProvenance> const& value) noexcept
        {
            if (value != nullptr)
            {
                m_provenance = value;
            }
        }

    private:
        std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomizationProvenance> m_provenance{
            std::make_shared<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomizationProvenance>() };
    };
}

namespace winrt::Windows::Devices::Midi2::ServiceConfig::factory_implementation
{
    struct MidiServiceEndpointCustomizationProvenance : MidiServiceEndpointCustomizationProvenanceT<MidiServiceEndpointCustomizationProvenance, implementation::MidiServiceEndpointCustomizationProvenance>
    {
    };
}
