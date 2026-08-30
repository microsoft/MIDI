// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiEndpointDeviceInformationRemovedEventArgs.h"
#include "Enumeration.MidiEndpointDeviceInformationRemovedEventArgs.g.cpp"


namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    _Use_decl_annotations_
    void MidiEndpointDeviceInformationRemovedEventArgs::InternalInitialize(
        midi2enum::MidiEndpointDeviceInformation const& removedDevice,
        enumeration::DeviceInformationUpdate const& deviceInformationUpdate
    ) noexcept
    {
        m_removedDevice = removedDevice;
        m_deviceInformationUpdate = deviceInformationUpdate;
    }
}
