// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiEndpointAssociatedPortDeviceInformation.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    struct MidiEndpointAssociatedPortDeviceInformation : MidiEndpointAssociatedPortDeviceInformationT<MidiEndpointAssociatedPortDeviceInformation>
    {
        //MidiEndpointAssociatedPortDeviceInformation() = default;

        winrt::guid ContainerId() const noexcept { return m_containerId; }
        winrt::hstring ParentDeviceInstanceId() const noexcept { return m_parentDeviceInstanceId; }
        winrt::hstring ParentEndpointDeviceId() const noexcept { return m_parentEndpointDeviceId; }
        midi2::MidiGroup Group() const noexcept { return m_group; }
        midi2::Midi1PortFlow PortFlow() const noexcept { return m_portFlow; }
        winrt::hstring PortName() const noexcept { return m_portName; }
        winrt::hstring PortDeviceId() const noexcept { return m_portDeviceId; }
        uint32_t PortNumber() const noexcept { return m_portNumber; }
        enumeration::DeviceInformation DeviceInformation() const noexcept { return m_deviceInformation; }

        winrt::hstring ToString();

        void InternalInitialize(
            winrt::guid const& containerId,
            winrt::hstring const& parentDeviceInstanceId,
            winrt::hstring const& parentEndpointDeviceId,
            midi2::MidiGroup const& group,
            midi2::Midi1PortFlow const portFlow,
            winrt::hstring const& portName,
            winrt::hstring const& portDeviceId,
            uint32_t const portNumber,
            enumeration::DeviceInformation const& deviceInformation
            );

    private:
        winrt::guid m_containerId{};
        winrt::hstring m_parentDeviceInstanceId{};
        winrt::hstring m_parentEndpointDeviceId{};
        midi2::MidiGroup m_group{ nullptr };
        midi2::Midi1PortFlow m_portFlow;
        winrt::hstring m_portName{};
        winrt::hstring m_portDeviceId{};
        uint32_t m_portNumber{};
        
        enumeration::DeviceInformation m_deviceInformation{ nullptr };

    };
}
