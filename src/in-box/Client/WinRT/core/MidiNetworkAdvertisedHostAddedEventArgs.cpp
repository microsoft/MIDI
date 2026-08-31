// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkAdvertisedHostAddedEventArgs.h"
#include "Transports.Network.MidiNetworkAdvertisedHostAddedEventArgs.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostAddedEventArgs::InternalInitialize(
        network::MidiNetworkAdvertisedHost const& host) noexcept
    {
        m_host = host;
    }

}
