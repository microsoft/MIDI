// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiEndpointDeviceInformation.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiEndpointDeviceInformation : MidiEndpointDeviceInformationT<MidiEndpointDeviceInformation>
    {
        MidiEndpointDeviceInformation() = default;

        static midi2::MidiEndpointDeviceInformation CreateFromId(_In_ winrt::hstring const& id) noexcept;

        //static winrt::hstring UniversalMidiPacketBidirectionalInterfaceClassId() noexcept { return L"" /* STRING_DEVINTERFACE_UNIVERSALMIDIPACKET_BIDI */; }

        static winrt::hstring DiagnosticsLoopbackAEndpointId() noexcept { return LOOPBACK_BIDI_ID_A; }
        static winrt::hstring DiagnosticsLoopbackBEndpointId() noexcept { return LOOPBACK_BIDI_ID_B; }

        static midi2::MidiEndpointDeviceWatcher CreateWatcher(_In_ bool includeDiagnosticsEndpoints);
        static collections::IVectorView<midi2::MidiEndpointDeviceInformation> FindAll(_In_ bool includeDiagnosticsEndpoints);


        static collections::IVectorView<winrt::hstring> GetAdditionalPropertiesList() noexcept;

        winrt::hstring Id() noexcept { return m_id; }
        winrt::hstring ParentDeviceId() noexcept { return m_parentDeviceId; }

        winrt::Windows::Devices::Enumeration::DeviceInformation GetParentDeviceInformation();
        winrt::Windows::Devices::Enumeration::DeviceInformation DeviceInformation() noexcept { return m_deviceInformation; }

        winrt::hstring Name() noexcept { return m_name; }
        winrt::hstring TransportSuppliedName() noexcept { return m_transportSuppliedName; }
        winrt::hstring EndpointSuppliedName() noexcept { return m_endpointSuppliedName; }
        winrt::hstring UserSuppliedName() noexcept { return m_userSuppliedName; }

        winrt::hstring TransportId() noexcept { return m_transportId; }
        winrt::hstring Description() noexcept { return m_description; }
        winrt::hstring ImagePath() noexcept { return m_imagePath; }


        winrt::hstring UniqueIdentifier() noexcept { return m_uniqueIdentifier; }

        bool SupportsMultiClient() noexcept { return m_supportsMultiClient; }

        midi2::MidiEndpointNativeDataFormat NativeDataFormat() noexcept { return m_endpointNativeDataFormat; }
        midi2::MidiEndpointDevicePurpose EndpointPurpose() noexcept { return m_endpointPurpose; }

  //      midi2::MidiEndpointInformation EndpointInformation() noexcept { return m_endpointInformation; }

        collections::IVectorView<midi2::MidiFunctionBlock> FunctionBlocks() noexcept { return m_functionBlocks.GetView(); }

        collections::IVectorView<midi2::MidiGroupTerminalBlock> GroupTerminalBlocks() noexcept { return m_groupTerminalBlocks.GetView(); }




        winrt::hstring ProductInstanceId() const noexcept { return m_productInstanceId; }
        uint8_t SpecificationVersionMajor() const noexcept { return m_umpVersionMajor; }
        uint8_t SpecificationVersionMinor() const noexcept { return m_umpVersionMinor; }
        bool HasStaticFunctionBlocks() const noexcept { return m_hasStaticFunctionBlocks; }
        uint8_t FunctionBlockCount() const noexcept { return m_functionBlockCount; }
        bool SupportsMidi10Protocol() const noexcept { return m_supportsMidi10Protocol; }
        bool SupportsMidi20Protocol() const noexcept { return m_supportsMidi20Protocol; }
        bool SupportsReceivingJRTimestamps() const noexcept { return m_supportsReceivingJRTimestamps; }
        bool SupportsSendingJRTimestamps() const noexcept { return m_supportsSendingJRTimestamps; }

        midi2::MidiProtocol ConfiguredProtocol() const noexcept { return m_configuredProtocol; }










        void InternalUpdateFromDeviceInformation(_In_ winrt::Windows::Devices::Enumeration::DeviceInformation const& info) noexcept;
        void InternalUpdate(_In_ winrt::hstring const& deviceId) noexcept;


        

    private:
        winrt::hstring m_id{};
        winrt::hstring m_parentDeviceId{};
        winrt::hstring m_name{};
        winrt::hstring m_transportSuppliedName{};
        winrt::hstring m_userSuppliedName{};
        winrt::hstring m_endpointSuppliedName{};

        winrt::hstring m_transportId{};
        winrt::hstring m_description{};
        winrt::hstring m_imagePath{};

        midi2::MidiProtocol m_configuredProtocol{ midi2::MidiProtocol::Midi1 };

        winrt::hstring m_productInstanceId;
        uint8_t m_umpVersionMajor{ 0 };
        uint8_t m_umpVersionMinor{ 0 };
        bool m_hasStaticFunctionBlocks{ false };
        uint8_t m_functionBlockCount{ 0 };
        bool m_supportsMidi10Protocol{ false };
        bool m_supportsMidi20Protocol{ false };
        bool m_supportsReceivingJRTimestamps{ false };
        bool m_supportsSendingJRTimestamps{ false };


        winrt::hstring m_uniqueIdentifier{};

        bool m_supportsMultiClient{ true };

        midi2::MidiEndpointType m_endpointType{};
        midi2::MidiEndpointNativeDataFormat m_endpointNativeDataFormat{};
        midi2::MidiEndpointDevicePurpose m_endpointPurpose{};

 //       midi2::MidiEndpointInformation m_endpointInformation{ nullptr };

        collections::IVector<midi2::MidiGroupTerminalBlock> m_groupTerminalBlocks{ winrt::single_threaded_vector<midi2::MidiGroupTerminalBlock>() };
        collections::IVector<midi2::MidiFunctionBlock> m_functionBlocks{ winrt::single_threaded_vector<midi2::MidiFunctionBlock>() };

        winrt::Windows::Devices::Enumeration::DeviceInformation m_deviceInformation{ nullptr };

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiEndpointDeviceInformation : MidiEndpointDeviceInformationT<MidiEndpointDeviceInformation, implementation::MidiEndpointDeviceInformation>
    {
    };
}
