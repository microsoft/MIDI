// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiLoopbackEntry.g.h"

namespace winrt::Windows::Devices::Midi2::Endpoints::Loopback::implementation
{
    struct MidiLoopbackEntry : MidiLoopbackEntryT<MidiLoopbackEntry>
    {
        MidiLoopbackEntry() = default;

        winrt::guid AssociationId();
        winrt::Windows::Devices::Midi2::Endpoints::Loopback::MidiLoopbackEndpointEntry EndpointA();
        winrt::Windows::Devices::Midi2::Endpoints::Loopback::MidiLoopbackEndpointEntry EndpointB();
        bool IsMuted();
    };
}
