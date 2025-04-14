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
        winrt::guid const& containerId,
        winrt::hstring const& parentDeviceInstanceId,
        winrt::hstring const& parentEndpointDeviceId,
        midi2::MidiGroup const& group,
        midi2::Midi1PortFlow const portFlow,
        winrt::hstring const& portName,
        winrt::hstring const& portDeviceId,
        uint32_t const portNumber,
        enumeration::DeviceInformation const& deviceInformation
    )
    {
        m_containerId = containerId;
        m_parentDeviceInstanceId = internal::NormalizeDeviceInstanceIdHStringCopy(parentDeviceInstanceId);
        m_parentEndpointDeviceId = internal::NormalizeEndpointInterfaceIdHStringCopy(parentEndpointDeviceId);
        m_group = group;
        m_portFlow = portFlow;
        m_portName = portName;
        m_portDeviceId = internal::NormalizeEndpointInterfaceIdHStringCopy(portDeviceId);
        m_portNumber = portNumber;
        m_deviceInformation = deviceInformation;
    }


    winrt::hstring MidiEndpointAssociatedPortDeviceInformation::ToString()
    {
        return m_portName + L": " + m_group.ToString();
    }
}
