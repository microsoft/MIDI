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
        MidiEndpointAssociatedPortDeviceInformation() = default;

        winrt::guid ContainerId() { return m_containerId; }
        winrt::hstring ParentDeviceInstanceId() { return m_parentDeviceInstanceId; }
        winrt::hstring ParentEndpointDeviceId() { return m_parentEndpointDeviceId; }
        midi2::MidiGroup Group() { return m_group; }
        midi2::MidiPortFlow PortFlow() { return m_portFlow; }
        winrt::hstring PortName() { return m_portName; }
        winrt::hstring PortDeviceId() { return m_portDeviceId; }
        uint32_t PortIndex() { return m_portIndex; }
        enumeration::DeviceInformation DeviceInformation() { return m_deviceInformation; }

        winrt::hstring ToString();

        void InternalInitialize(
            winrt::guid containerId,
            winrt::hstring parentDeviceInstanceId,
            winrt::hstring parentEndpointDeviceId,
            midi2::MidiGroup group,
            midi2::MidiPortFlow portFlow,
            winrt::hstring portName,
            winrt::hstring portDeviceId,
            uint32_t portIndex,
            enumeration::DeviceInformation const& deviceInformation
            );

    private:
        winrt::guid m_containerId{};
        winrt::hstring m_parentDeviceInstanceId{};
        winrt::hstring m_parentEndpointDeviceId{};
        midi2::MidiGroup m_group;
        midi2::MidiPortFlow m_portFlow;
        winrt::hstring m_portName;
        winrt::hstring m_portDeviceId;
        uint32_t m_portIndex;
        
        enumeration::DeviceInformation m_deviceInformation{ nullptr };

    };
}
