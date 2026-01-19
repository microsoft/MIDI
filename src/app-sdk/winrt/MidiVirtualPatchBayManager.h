// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "VirtualPatchBay.MidiVirtualPatchBayManager.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::VirtualPatchBay::implementation
{
    struct MidiVirtualPatchBayManager
    {
        //MidiVirtualPatchBayManager() = default;

        static bool IsTransportAvailable();
        static winrt::guid TransportId();


        static vpb::MidiVirtualPatchBayRouteCreationResult CreateRoute(
            _In_ vpb::MidiVirtualPatchBayRouteCreationConfig const& creationConfig);

        static vpb::MidiVirtualPatchBayRouteUpdateResult UpdateRoute(
            _In_ vpb::MidiVirtualPatchBayRouteUpdateConfig const& updateConfig);

        static bool RemoveRoute(
            _In_ vpb::MidiVirtualPatchBayRouteRemovalConfig const& removalConfig);


        static collections::IVectorView<vpb::MidiVirtualPatchBayRouteDefinition> GetRoutes();

        static vpb::MidiVirtualPatchBayRouteDefinition GetRoute(
            _In_ winrt::guid const& routeId);

        static vpb::MidiVirtualPatchBayRouteStatus GetRouteStatus(
            _In_ winrt::guid const& routeId);

    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::VirtualPatchBay::factory_implementation
{
    struct MidiVirtualPatchBayManager : MidiVirtualPatchBayManagerT<MidiVirtualPatchBayManager, implementation::MidiVirtualPatchBayManager, winrt::static_lifetime>
    {
    };
}
