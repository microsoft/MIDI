// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiVirtualPatchBayManager.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::VirtualPatchBay::implementation
{
    struct MidiVirtualPatchBayManager : MidiVirtualPatchBayManagerT<MidiVirtualPatchBayManager>
    {
        MidiVirtualPatchBayManager() = default;

        static bool IsTransportAvailable();
        static winrt::guid AbstractionId();


        static local::MidiVirtualPatchBayRouteCreationResult CreateRoute(
            _In_ local::MidiVirtualPatchBayRouteCreationConfig const& creationConfig);

        static local::MidiVirtualPatchBayRouteUpdateResult UpdateRoute(
            _In_ local::MidiVirtualPatchBayRouteUpdateConfig const& updateConfig);

        static bool RemoveRoute(
            _In_ local::MidiVirtualPatchBayRouteRemovalConfig const& removalConfig);

        static collections::IVector<local::MidiVirtualPatchBayRouteDefinition> GetRoutes();

        static local::MidiVirtualPatchBayRouteDefinition GetRoute(
            _In_ winrt::guid routeId);
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::VirtualPatchBay::factory_implementation
{
    struct MidiVirtualPatchBayManager : MidiVirtualPatchBayManagerT<MidiVirtualPatchBayManager, implementation::MidiVirtualPatchBayManager, winrt::static_lifetime>
    {
    };
}
