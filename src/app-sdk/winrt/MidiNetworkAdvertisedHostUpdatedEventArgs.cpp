// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkAdvertisedHostUpdatedEventArgs.h"
#include "Endpoints.Network.MidiNetworkAdvertisedHostUpdatedEventArgs.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{
    hstring MidiNetworkAdvertisedHostUpdatedEventArgs::HostId()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Enumeration::DeviceInformationUpdate MidiNetworkAdvertisedHostUpdatedEventArgs::DeviceInformationUpdate()
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostUpdatedEventArgs::InternalInitialize(
        winrt::hstring const& id, 
        enumeration::DeviceInformationUpdate const& args) noexcept
    {
        m_id = id;

        UNREFERENCED_PARAMETER(args);
    }

}
