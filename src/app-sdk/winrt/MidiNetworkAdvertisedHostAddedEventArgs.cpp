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
    network::MidiNetworkAdvertisedHost MidiNetworkAdvertisedHostAddedEventArgs::AddedHost()
    {
        throw hresult_not_implemented();
    }


    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostAddedEventArgs::InternalInitialize(
        winrt::hstring const& id, 
        enumeration::DeviceInformation const& args) noexcept
    {

    }

}
