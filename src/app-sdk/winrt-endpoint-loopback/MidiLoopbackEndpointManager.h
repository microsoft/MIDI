// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiLoopbackEndpointManager.g.h"

namespace winrt::Microsoft::Devices::Midi2::Endpoints::Loopback::implementation
{
    struct MidiLoopbackEndpointManager : MidiLoopbackEndpointManagerT<MidiLoopbackEndpointManager>
    {
        MidiLoopbackEndpointManager() = default;

        static bool IsTransportAvailable() noexcept;

        static const winrt::guid AbstractionId() noexcept { return internal::StringToGuid(L"{942BF02D-93C0-4EA8-B03E-D51156CA75E1}"); }

        static loop::MidiLoopbackEndpointCreationResult CreateTransientLoopbackEndpoints(
            _In_ loop::MidiLoopbackEndpointCreationConfiguration creationConfiguration);

        static bool RemoveTransientLoopbackEndpoints(
            _In_ loop::MidiLoopbackEndpointDeletionConfiguration deletionConfiguration);
    };
}
namespace winrt::Microsoft::Devices::Midi2::Endpoints::Loopback::factory_implementation
{
    struct MidiLoopbackEndpointManager : MidiLoopbackEndpointManagerT<MidiLoopbackEndpointManager, implementation::MidiLoopbackEndpointManager, winrt::static_lifetime>
    {
    };
}
