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
    struct MidiLoopbackEndpointManager
    {
        //MidiLoopbackEndpointManager() = default;

        static bool IsTransportAvailable() noexcept;

        static const winrt::guid TransportId() noexcept { return internal::StringToGuid(L"{942BF02D-93C0-4EA8-B03E-D51156CA75E1}"); }

        static loop::MidiLoopbackEndpointCreationResult CreateTransientLoopbackEndpoints(
            _In_ loop::MidiLoopbackEndpointCreationConfig creationConfig);

        static bool RemoveTransientLoopbackEndpoints(
            _In_ loop::MidiLoopbackEndpointRemovalConfig deletionConfig);

        static midi2::MidiEndpointDeviceInformation GetAssociatedLoopbackEndpointForId(
            _In_ winrt::hstring loopbackEndpointId);

        static midi2::MidiEndpointDeviceInformation GetAssociatedLoopbackEndpoint(
            _In_ midi2::MidiEndpointDeviceInformation const& loopbackEndpoint,
            _In_ collections::IIterable<midi2::MidiEndpointDeviceInformation> endpointsToSearch);

        static midi2::MidiEndpointDeviceInformation GetAssociatedLoopbackEndpoint(
            _In_ midi2::MidiEndpointDeviceInformation const& loopbackEndpoint);

        static bool DoesLoopbackAExist(_In_ winrt::hstring const& uniqueIdentifier);
        static bool DoesLoopbackBExist(_In_ winrt::hstring const& uniqueIdentifier);

        static winrt::guid GetAssociationId(_In_ midi2::MidiEndpointDeviceInformation const& loopbackEndpoint);
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Loopback::factory_implementation
{
    struct MidiLoopbackEndpointManager : MidiLoopbackEndpointManagerT<MidiLoopbackEndpointManager, implementation::MidiLoopbackEndpointManager, winrt::static_lifetime>
    {
    };
}
