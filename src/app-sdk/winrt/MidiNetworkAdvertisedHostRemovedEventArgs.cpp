// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkAdvertisedHostRemovedEventArgs.h"
#include "Endpoints.Network.MidiNetworkAdvertisedHostRemovedEventArgs.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{
    winrt::hstring MidiNetworkAdvertisedHostRemovedEventArgs::HostId()
    {
        throw hresult_not_implemented();
    }
    enumeration::DeviceInformationUpdate MidiNetworkAdvertisedHostRemovedEventArgs::DeviceInformationUpdate()
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostRemovedEventArgs::InternalInitialize(
        winrt::hstring const& id, 
        enumeration::DeviceInformationUpdate const& args) noexcept
    {

    }

}
