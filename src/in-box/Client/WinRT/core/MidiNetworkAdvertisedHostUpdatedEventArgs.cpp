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

    network::MidiNetworkAdvertisedHostChangedProperties MidiNetworkAdvertisedHostUpdatedEventArgs::ChangedProperties() noexcept
    {
        return m_changedProperties;
    }

    network::MidiNetworkAdvertisedHost MidiNetworkAdvertisedHostUpdatedEventArgs::UpdatedHost() noexcept
    {
        return m_host;
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostUpdatedEventArgs::InternalInitialize(
        winrt::hstring const& id, 
        network::MidiNetworkAdvertisedHostChangedProperties const changedProperties,
        network::MidiNetworkAdvertisedHost const& host) noexcept
    {
        m_id = id;
        m_changedProperties = changedProperties;
        m_host = host;
    }

}
