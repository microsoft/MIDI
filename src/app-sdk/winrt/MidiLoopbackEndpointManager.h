// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "Endpoints.Loopback.MidiLoopbackEndpointManager.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Loopback::implementation
{
    struct MidiLoopbackEndpointManager : MidiLoopbackEndpointManagerT<MidiLoopbackEndpointManager>
    {
        MidiLoopbackEndpointManager() = default;

        static bool IsTransportAvailable() noexcept;

        static const winrt::guid TransportId() noexcept { return internal::StringToGuid(L"{942BF02D-93C0-4EA8-B03E-D51156CA75E1}"); }

        static loop::MidiLoopbackEndpointCreationResult CreateTransientLoopbackEndpoints(
            _In_ loop::MidiLoopbackEndpointCreationConfig creationConfig);

        static bool RemoveTransientLoopbackEndpoints(
            _In_ loop::MidiLoopbackEndpointRemovalConfig deletionConfig);
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Loopback::factory_implementation
{
    struct MidiLoopbackEndpointManager : MidiLoopbackEndpointManagerT<MidiLoopbackEndpointManager, implementation::MidiLoopbackEndpointManager, winrt::static_lifetime>
    {
    };
}
