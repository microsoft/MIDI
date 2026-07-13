// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "Transports.Loopback.MidiLoopbackManager.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Loopback::implementation
{
    struct MidiLoopbackManager
    {
        //MidiLoopbackEndpointManager() = default;

        static bool IsTransportAvailable() noexcept;

        static const winrt::guid TransportId() noexcept { return internal::StringToGuid(L"{942BF02D-93C0-4EA8-B03E-D51156CA75E1}"); }

        static loop::MidiLoopbackCreationResult CreateTransientLoopback(
            _In_ loop::MidiLoopbackCreationConfig const& creationConfig) noexcept;

        static loop::MidiLoopbackRemovalResult RemoveTransientLoopback(
            _In_ loop::MidiLoopbackRemovalConfig const& deletionConfig) noexcept;

        static midi2enum::MidiEndpointDeviceInformation GetAssociatedLoopbackEndpointForId(
            _In_ winrt::hstring const& loopbackEndpointId) noexcept;

        static midi2enum::MidiEndpointDeviceInformation GetAssociatedLoopbackEndpoint(
            _In_ midi2enum::MidiEndpointDeviceInformation const& loopbackEndpoint,
            _In_ collections::IIterable<midi2enum::MidiEndpointDeviceInformation> const& endpointsToSearch) noexcept;

        static midi2enum::MidiEndpointDeviceInformation GetAssociatedLoopbackEndpoint(
            _In_ midi2enum::MidiEndpointDeviceInformation const& loopbackEndpoint) noexcept;

        static bool DoesLoopbackAExist(_In_ winrt::hstring const& uniqueIdentifier) noexcept;
        static bool DoesLoopbackBExist(_In_ winrt::hstring const& uniqueIdentifier) noexcept;

        static winrt::guid GetAssociationId(_In_ midi2enum::MidiEndpointDeviceInformation const& loopbackEndpoint) noexcept;

        static collections::IVectorView<loop::MidiLoopbackEntry> GetActiveLoopbackEntries() noexcept;



        static loop::MidiLoopbackUpdateResult MuteLoopback(_In_ winrt::guid const& associationId) noexcept;
        static loop::MidiLoopbackUpdateResult UnmuteLoopback(_In_ winrt::guid const& associationId) noexcept;

    };
}
namespace winrt::Windows::Devices::Midi2::Transports::Loopback::factory_implementation
{
    struct MidiLoopbackManager : MidiLoopbackManagerT<MidiLoopbackManager, implementation::MidiLoopbackManager, winrt::static_lifetime>
    {
    };
}
