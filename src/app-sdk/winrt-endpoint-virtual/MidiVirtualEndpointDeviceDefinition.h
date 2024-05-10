// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiVirtualEndpointDeviceDefinition.g.h"


namespace MIDI_CPP_NAMESPACE::implementation
{
    struct MidiVirtualEndpointDeviceDefinition : MidiVirtualEndpointDeviceDefinitionT<MidiVirtualEndpointDeviceDefinition>
    {
        MidiVirtualEndpointDeviceDefinition() = default;

        winrt::hstring EndpointName() { return m_endpointName; }
        void EndpointName(_In_ winrt::hstring const& value) { m_endpointName = internal::TrimmedHStringCopy(value); }

        winrt::hstring TransportSuppliedDescription() { return m_description; }
        void TransportSuppliedDescription(_In_ winrt::hstring const& value) { m_description = internal::TrimmedHStringCopy(value); }

        winrt::hstring EndpointProductInstanceId() { return m_endpointProductInstanceId; }
        void EndpointProductInstanceId(_In_ winrt::hstring const& value) { m_endpointProductInstanceId = internal::TrimmedHStringCopy(value); }

        bool SupportsMidi1ProtocolMessages() { return m_supportsMidi1ProtocolMessages; }
        void SupportsMidi1ProtocolMessages(_In_ bool const value) { m_supportsMidi1ProtocolMessages = value; }

        bool SupportsMidi2ProtocolMessages() { return m_supportsMidi2ProtocolMessages; }
        void SupportsMidi2ProtocolMessages(_In_ bool const value) { m_supportsMidi2ProtocolMessages = value; }

        bool SupportsReceivingJRTimestamps() { return m_supportsReceivingJRTimestamps; }
        void SupportsReceivingJRTimestamps(_In_ bool const value) { m_supportsReceivingJRTimestamps = value; }

        bool SupportsSendingJRTimestamps() { return m_supportsSendingJRTimestamps; }
        void SupportsSendingJRTimestamps(_In_ bool const value) { m_supportsSendingJRTimestamps = value; }


        collections::IVector<uint8_t> DeviceManufacturerSystemExclusiveId() { return m_deviceManufacturerSystemExclusiveId; }

        collections::IVector<uint8_t> SoftwareRevisionLevel() { return m_softwareRevisionLevel; }


        uint8_t DeviceFamilyLsb() { return m_deviceFamilyLsb; }
        void DeviceFamilyLsb(_In_ uint8_t const value) { m_deviceFamilyLsb = value; }

        uint8_t DeviceFamilyMsb() { return m_deviceFamilyMsb; }
        void DeviceFamilyMsb(_In_ uint8_t const value) { m_deviceFamilyMsb = value; }

        uint8_t DeviceFamilyModelLsb() { return m_deviceFamilyModelLsb; }
        void DeviceFamilyModelLsb(_In_ uint8_t const value) { m_deviceFamilyModelLsb = value; }

        uint8_t DeviceFamilyModelMsb() { return m_deviceFamilyModelMsb; }
        void DeviceFamilyModelMsb(_In_ uint8_t const value) { m_deviceFamilyModelMsb = value; }

        bool AreFunctionBlocksStatic() { return m_areFunctionBlocksStatic; }
        void AreFunctionBlocksStatic(_In_ bool const value) { m_areFunctionBlocksStatic = value; }

        collections::IVector<midi2::MidiFunctionBlock> FunctionBlocks() { return m_functionBlocks; }

    private:
        winrt::hstring m_endpointName{};
        winrt::hstring m_description{};
        winrt::hstring m_endpointProductInstanceId{};

        bool m_supportsMidi1ProtocolMessages{ false };
        bool m_supportsMidi2ProtocolMessages{ false };
        bool m_supportsReceivingJRTimestamps{ false };
        bool m_supportsSendingJRTimestamps{ false };

        uint8_t m_deviceFamilyLsb{ 0 };
        uint8_t m_deviceFamilyMsb{ 0 };
        uint8_t m_deviceFamilyModelLsb{ 0 };
        uint8_t m_deviceFamilyModelMsb{ 0 };

        bool m_areFunctionBlocksStatic{ false };

        //winrt::array_view<uint8_t> m_deviceManufacturerSystemExclusiveId{ 0,0,0 };
        //winrt::array_view<uint8_t> m_softwareRevisionLevel{ 0,0,0,0 };

        //uint8_t m_deviceManufacturerSystemExclusiveId[MIDI_ENDPOINT_IDENTITY_MANUFACTURER_SYSEX_ID_BYTE_COUNT]{};
        //uint8_t m_softwareRevisionLevel[MIDI_ENDPOINT_IDENTITY_SOFTWARE_REVISION_LEVEL_BYTE_COUNT]{};

        collections::IVector<std::uint8_t>
            m_deviceManufacturerSystemExclusiveId{ winrt::multi_threaded_vector<std::uint8_t>() };

        collections::IVector<std::uint8_t>
            m_softwareRevisionLevel{ winrt::multi_threaded_vector<std::uint8_t>() };


        collections::IVector<midi2::MidiFunctionBlock>
            m_functionBlocks{ winrt::multi_threaded_vector<midi2::MidiFunctionBlock>() };

    };
}
namespace MIDI_CPP_NAMESPACE::factory_implementation
{
    struct MidiVirtualEndpointDeviceDefinition : MidiVirtualEndpointDeviceDefinitionT<MidiVirtualEndpointDeviceDefinition, implementation::MidiVirtualEndpointDeviceDefinition>
    {
    };
}
