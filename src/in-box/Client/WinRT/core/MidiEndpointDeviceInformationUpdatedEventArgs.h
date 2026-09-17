// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "Enumeration.MidiEndpointDeviceInformationUpdatedEventArgs.g.h"

namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    // Not projected. Every property the watcher asks for belongs to at least one of these, so an
    // Updated event with no bits set is a defect in the watcher rather than something an
    // application needs to handle.
    struct MidiEndpointDeviceInformationUpdateFlags
    {
        static constexpr uint32_t None = 0x00000000;

        static constexpr uint32_t Name = 0x00000001;
        static constexpr uint32_t EndpointInformation = 0x00000002;
        static constexpr uint32_t DeviceIdentity = 0x00000004;
        static constexpr uint32_t StreamConfiguration = 0x00000008;
        static constexpr uint32_t FunctionBlocks = 0x00000010;
        static constexpr uint32_t UserMetadata = 0x00000020;
        static constexpr uint32_t AdditionalCapabilities = 0x00000040;
        static constexpr uint32_t UniqueIds = 0x00000080;
        static constexpr uint32_t GroupTerminalBlocks = 0x00000100;
        static constexpr uint32_t MutedState = 0x00000200;

        static constexpr uint32_t EndpointDiscoveryState = 0x00000400;
        static constexpr uint32_t Midi1PortMapping = 0x00000800;
        static constexpr uint32_t DevicePresence = 0x00001000;
        static constexpr uint32_t LatencyProperties = 0x00002000;
        static constexpr uint32_t TransportSuppliedProperties = 0x00004000;
        static constexpr uint32_t SystemDeviceProperties = 0x00008000;
    };


    struct MidiEndpointDeviceInformationUpdatedEventArgs : MidiEndpointDeviceInformationUpdatedEventArgsT<MidiEndpointDeviceInformationUpdatedEventArgs>
    {
        MidiEndpointDeviceInformationUpdatedEventArgs() = default;

        midi2enum::MidiEndpointDeviceInformation UpdatedDevice() const noexcept { return m_updatedDevice; }

        bool IsNameUpdated() const noexcept { return IsFlagSet(MidiEndpointDeviceInformationUpdateFlags::Name); }
        bool IsEndpointInformationUpdated() const noexcept { return IsFlagSet(MidiEndpointDeviceInformationUpdateFlags::EndpointInformation); }
        bool IsDeviceIdentityUpdated() const noexcept { return IsFlagSet(MidiEndpointDeviceInformationUpdateFlags::DeviceIdentity); }
        bool IsStreamConfigurationUpdated() const noexcept { return IsFlagSet(MidiEndpointDeviceInformationUpdateFlags::StreamConfiguration); }
        bool AreFunctionBlocksUpdated() const noexcept { return IsFlagSet(MidiEndpointDeviceInformationUpdateFlags::FunctionBlocks); }
        bool IsUserMetadataUpdated() const noexcept { return IsFlagSet(MidiEndpointDeviceInformationUpdateFlags::UserMetadata); }
        bool AreAdditionalCapabilitiesUpdated() const noexcept { return IsFlagSet(MidiEndpointDeviceInformationUpdateFlags::AdditionalCapabilities); }
        bool AreUniqueIdsUpdated() const noexcept { return IsFlagSet(MidiEndpointDeviceInformationUpdateFlags::UniqueIds); }

        bool AreGroupTerminalBlocksUpdated() const noexcept { return IsFlagSet(MidiEndpointDeviceInformationUpdateFlags::GroupTerminalBlocks); }

        bool IsMutedStateUpdated() const noexcept { return IsFlagSet(MidiEndpointDeviceInformationUpdateFlags::MutedState); }

        bool IsEndpointDiscoveryStateUpdated() const noexcept { return IsFlagSet(MidiEndpointDeviceInformationUpdateFlags::EndpointDiscoveryState); }
        bool IsMidi1PortMappingUpdated() const noexcept { return IsFlagSet(MidiEndpointDeviceInformationUpdateFlags::Midi1PortMapping); }
        bool IsDevicePresenceUpdated() const noexcept { return IsFlagSet(MidiEndpointDeviceInformationUpdateFlags::DevicePresence); }
        bool AreLatencyPropertiesUpdated() const noexcept { return IsFlagSet(MidiEndpointDeviceInformationUpdateFlags::LatencyProperties); }
        bool AreTransportSuppliedPropertiesUpdated() const noexcept { return IsFlagSet(MidiEndpointDeviceInformationUpdateFlags::TransportSuppliedProperties); }
        bool AreSystemDevicePropertiesUpdated() const noexcept { return IsFlagSet(MidiEndpointDeviceInformationUpdateFlags::SystemDeviceProperties); }

        enumeration::DeviceInformationUpdate DeviceInformationUpdate() const noexcept { return m_deviceInformationUpdate; }

        void InternalInitialize(
            _In_ midi2enum::MidiEndpointDeviceInformation const& updatedDevice,
            _In_ enumeration::DeviceInformationUpdate const& deviceInformationUpdate,
            _In_ uint32_t const updatedFlags
        ) noexcept;

    private:
        bool IsFlagSet(_In_ uint32_t const flag) const noexcept { return (m_updatedFlags & flag) != 0; }

        uint32_t m_updatedFlags{ MidiEndpointDeviceInformationUpdateFlags::None };

        midi2enum::MidiEndpointDeviceInformation m_updatedDevice{ nullptr };
        enumeration::DeviceInformationUpdate m_deviceInformationUpdate{ nullptr };
    };
}
