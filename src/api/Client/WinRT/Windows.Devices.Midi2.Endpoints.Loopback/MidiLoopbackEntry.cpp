// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiLoopbackEntry.h"
#include "MidiLoopbackEntry.g.cpp"

namespace winrt::Windows::Devices::Midi2::Endpoints::Loopback::implementation
{
    winrt::guid MidiLoopbackEntry::AssociationId()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi2::Endpoints::Loopback::MidiLoopbackEndpointEntry MidiLoopbackEntry::EndpointA()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi2::Endpoints::Loopback::MidiLoopbackEndpointEntry MidiLoopbackEntry::EndpointB()
    {
        throw hresult_not_implemented();
    }
    bool MidiLoopbackEntry::IsMuted()
    {
        throw hresult_not_implemented();
    }
}
