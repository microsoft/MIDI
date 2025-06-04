// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkAdvertisedHostAddedEventArgs.h"
#include "Endpoints.Network.MidiNetworkAdvertisedHostAddedEventArgs.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostAddedEventArgs::InternalInitialize(
        network::MidiNetworkAdvertisedHost const& host) noexcept
    {
        m_host = host;
    }

}
