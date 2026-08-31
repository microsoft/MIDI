// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Enumeration.MidiEndpointTransportSuppliedInfo.g.h"


namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    struct MidiEndpointTransportSuppliedInfo : MidiEndpointTransportSuppliedInfoT<MidiEndpointTransportSuppliedInfo>
    {
        MidiEndpointTransportSuppliedInfo() = default;
        MidiEndpointTransportSuppliedInfo(
            _In_ winrt::hstring const& name,
            _In_ winrt::hstring const& description,
            _In_ winrt::hstring const& serialNumber,
            _In_ uint16_t const vendorId,
            _In_ uint16_t const productId,
            _In_ winrt::hstring const& manufacturerName,
            _In_ bool const supportsMultiClient,
            _In_ midi2enum::MidiEndpointNativeDataFormat const nativeDataFormat,
            _In_ winrt::guid const& transportId,
            _In_ winrt::hstring const& transportCode,
            _In_ winrt::hstring const& driverDeviceInterfaceId
        )
        {
            m_name = name;
            m_description = description;
            m_serialNumber = serialNumber;
            m_vendorId = vendorId;
            m_productId = productId;
            m_manufacturerName = manufacturerName;
            m_supportsMultiClient = supportsMultiClient;
            m_nativeDataFormat = nativeDataFormat;
            m_transportId = transportId;
            m_transportCode = transportCode;
            m_driverDeviceInterfaceId = driverDeviceInterfaceId;

        }

        bool IsReadOnly() const noexcept { return m_isReadOnly; }

        winrt::hstring Name() const noexcept { return m_name; }
        void Name(_In_ winrt::hstring const& value) noexcept
        {
            if (IsReadOnly()) return;
            m_name = value;
        }

        winrt::hstring Description() const noexcept { return m_description; }
        void Description(_In_ winrt::hstring const& value) noexcept
        {
            if (IsReadOnly()) return;
            m_description = value;
        }

        winrt::hstring SerialNumber() const noexcept { return m_serialNumber; }
        void SerialNumber(_In_ winrt::hstring const& value) noexcept
        {
            if (IsReadOnly()) return;
            m_serialNumber = value;
        }

        uint16_t VendorId() const noexcept { return m_vendorId; }
        void VendorId(_In_ uint16_t const value) noexcept
        {
            if (IsReadOnly()) return;
            m_vendorId = value;
        }

        uint16_t ProductId() const noexcept { return m_productId; }
        void ProductId(_In_ uint16_t const value) noexcept
        {
            if (IsReadOnly()) return;
            m_productId = value;
        }

        winrt::hstring ManufacturerName() const noexcept { return m_manufacturerName; }
        void ManufacturerName(_In_ winrt::hstring const& value) noexcept
        {
            if (IsReadOnly()) return;
            m_manufacturerName = value; 
        }

        bool SupportsMultiClient() const noexcept { return m_supportsMultiClient; }
        void SupportsMultiClient(_In_ bool const value) noexcept
        {
            if (IsReadOnly()) return;
            m_supportsMultiClient = value;
        }

        midi2enum::MidiEndpointNativeDataFormat NativeDataFormat() const noexcept { return m_nativeDataFormat; }
        void NativeDataFormat(_In_ midi2enum::MidiEndpointNativeDataFormat const& value) noexcept
        {
            if (IsReadOnly()) return;
            m_nativeDataFormat = value;
        }

        winrt::guid TransportId() const noexcept { return m_transportId; }
        void TransportId(_In_ winrt::guid const& value) noexcept
        {
            if (IsReadOnly()) return;
            m_transportId = value;     
        }

        winrt::hstring TransportCode() const noexcept { return m_transportCode; }
        void TransportCode(_In_ winrt::hstring const& value) noexcept
        {
            if (IsReadOnly()) return;
            m_transportCode = value;
        }

        winrt::hstring DriverDeviceInterfaceId() const noexcept { return m_driverDeviceInterfaceId; }
        void DriverDeviceInterfaceId(_In_ winrt::hstring const& value) noexcept
        {
            if (IsReadOnly()) return;
            m_driverDeviceInterfaceId = value;
        }

        void InternalSetReadOnly() noexcept { m_isReadOnly = true; }

    private:
        bool m_isReadOnly{ false };
        winrt::hstring m_name{};
        winrt::hstring m_description{};
        winrt::hstring m_serialNumber{};
        uint16_t m_vendorId{ 0 };
        uint16_t m_productId{ 0 };
        winrt::hstring m_manufacturerName{};
        bool m_supportsMultiClient{ false };
        midi2enum::MidiEndpointNativeDataFormat m_nativeDataFormat{};
        winrt::guid m_transportId{};
        winrt::hstring m_transportCode{};
        winrt::hstring m_driverDeviceInterfaceId{};


    };
}
namespace winrt::Windows::Devices::Midi2::Enumeration::factory_implementation
{
    struct MidiEndpointTransportSuppliedInfo : MidiEndpointTransportSuppliedInfoT<MidiEndpointTransportSuppliedInfo, implementation::MidiEndpointTransportSuppliedInfo>
    {};
}
