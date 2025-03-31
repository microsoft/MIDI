// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiEndpointAssociatedPortDeviceInformation.h"
#include "MidiEndpointAssociatedPortDeviceInformation.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    void MidiEndpointAssociatedPortDeviceInformation::InternalInitialize(
        winrt::guid containerId,
        winrt::hstring parentDeviceInstanceId,
        winrt::hstring parentEndpointDeviceId,
        midi2::MidiGroup group,
        midi2::MidiPortFlow portFlow,
        winrt::hstring portName,
        winrt::hstring portDeviceId,
        uint32_t portIndex,
        enumeration::DeviceInformation const& deviceInformation
    )
    {
        m_containerId = containerId;
        m_parentDeviceInstanceId = parentDeviceInstanceId;
        m_parentEndpointDeviceId = parentEndpointDeviceId;
        m_group = group;
        m_portFlow = portFlow;
        m_portName = portName;
        m_portDeviceId = portDeviceId;
        m_portIndex = portIndex;
        m_deviceInformation = deviceInformation;
    }


    winrt::hstring MidiEndpointAssociatedPortDeviceInformation::ToString()
    {
        return m_portName + L": " + m_group.ToString();
    }
}
