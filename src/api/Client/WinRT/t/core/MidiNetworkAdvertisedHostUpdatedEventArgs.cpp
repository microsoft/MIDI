// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkAdvertisedHostUpdatedEventArgs.h"
#include "Transports.Network.MidiNetworkAdvertisedHostUpdatedEventArgs.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    winrt::hstring MidiNetworkAdvertisedHostUpdatedEventArgs::HostDeviceId() noexcept
    {
        return m_id;
    }
    winrt::Windows::Devices::Enumeration::DeviceInformationUpdate MidiNetworkAdvertisedHostUpdatedEventArgs::DeviceInformationUpdate() noexcept
    {
        return m_deviceInformationUpdate;
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostUpdatedEventArgs::InternalInitialize(
        winrt::hstring const& id, 
        enumeration::DeviceInformationUpdate const& args) noexcept
    {
        m_id = id;
        m_deviceInformationUpdate = args;
    }

}
