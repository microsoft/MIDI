// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiEndpointDeviceInformationUpdatedEventArgs.h"
#include "Enumeration.MidiEndpointDeviceInformationUpdatedEventArgs.g.cpp"


namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    _Use_decl_annotations_
    void MidiEndpointDeviceInformationUpdatedEventArgs::InternalInitialize(
        midi2enum::MidiEndpointDeviceInformation const& updatedDevice,
        enumeration::DeviceInformationUpdate const& deviceInformationUpdate,
        uint32_t const updatedFlags
    ) noexcept
    {
        m_updatedDevice = updatedDevice;
        m_deviceInformationUpdate = deviceInformationUpdate;
        m_updatedFlags = updatedFlags;
    }

}
