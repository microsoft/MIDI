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
        static winrt::hstring UniversalMidiPacketBidirectionalInterfaceClassId() noexcept { return L"" /* STRING_DEVINTERFACE_UNIVERSALMIDIPACKET_BIDI */; }
        static winrt::hstring UniversalMidiPacketInputInterfaceClassId() noexcept { return L"" /* STRING_DEVINTERFACE_UNIVERSALMIDIPACKET_INPUT */ ; }
        static winrt::hstring UniversalMidiPacketOutInterfaceClassId() noexcept { return L"" /* STRING_DEVINTERFACE_UNIVERSALMIDIPACKET_OUTPUT */ ; }

        winrt::hstring Id() noexcept { return m_id; }
        winrt::hstring ParentDeviceId() noexcept { return m_parentDeviceId; }
        winrt::hstring ContainerId() noexcept { return m_containerId; }
        winrt::hstring Name() noexcept { return m_name; }
        winrt::hstring TransportSuppliedName() noexcept { return m_transportSuppliedName; }
        winrt::hstring TransportId() noexcept { return m_transportId; }
        winrt::hstring Description() noexcept { return m_description; }
        winrt::hstring ImagePath() noexcept { return m_imagePath; }

        bool HasUniqueIdentifier() noexcept { return m_hasUniqueIdentifier; }
        winrt::hstring UniqueIdentifier() noexcept { return m_uniqueIdentifier; }

        bool SupportsMultiClient() noexcept { return m_supportsMultiClient; }
        midi2::MidiEndpointType EndpointType() noexcept { return m_endpointType; }
        midi2::MidiEndpointNativeDataFormat NativeDataFormat() noexcept { return m_endpointNativeDataFormat; }
        midi2::MidiEndpointDevicePurpose EndpointPurpose() noexcept { return m_endpointPurpose; }

        collections::IVectorView<midi2::MidiGroupTerminalBlock> GroupTerminalBlocks() noexcept { return m_groupTerminalBlocks.GetView(); }


        void InternalUpdateFromDeviceInformation(_In_ Windows::Devices::Enumeration::DeviceInformation const& info) noexcept;
        void InternalUpdate(_In_ winrt::hstring const& deviceId) noexcept;

        // TODO: will also need a func to update from JSON

    private:
        winrt::hstring m_id{};
        winrt::hstring m_parentDeviceId{};
        winrt::hstring m_containerId{};
        winrt::hstring m_name{};
        winrt::hstring m_transportSuppliedName{};
        winrt::hstring m_transportId{};
        winrt::hstring m_description{};
        winrt::hstring m_imagePath{};

        bool m_hasUniqueIdentifier{ false };
        winrt::hstring m_uniqueIdentifier{};

        bool m_supportsMultiClient{ true };

        midi2::MidiEndpointType m_endpointType{};
        midi2::MidiEndpointNativeDataFormat m_endpointNativeDataFormat{};
        midi2::MidiEndpointDevicePurpose m_endpointPurpose{};

        collections::IVector<midi2::MidiGroupTerminalBlock> m_groupTerminalBlocks{ winrt::single_threaded_vector<midi2::MidiGroupTerminalBlock>() };

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiEndpointDeviceInformation : MidiEndpointDeviceInformationT<MidiEndpointDeviceInformation, implementation::MidiEndpointDeviceInformation>
    {
    };
}
