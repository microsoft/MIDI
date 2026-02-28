// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "Endpoints.BasicLoopback.MidiBasicLoopbackEndpointManager.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::BasicLoopback::implementation
{
    struct MidiBasicLoopbackEndpointManager
    {
        MidiBasicLoopbackEndpointManager() = default;

        static bool IsTransportAvailable() noexcept;

        static const winrt::guid TransportId() noexcept { return internal::StringToGuid(L"{10088473-9478-4E62-850B-3D2315E135B8}"); }

        static bloop::MidiBasicLoopbackEndpointCreationResult CreateTransientLoopbackEndpoint(
            _In_ bloop::MidiBasicLoopbackEndpointCreationConfig const& creationConfig) noexcept;

        static bool RemoveTransientLoopbackEndpoint(
            _In_ bloop::MidiBasicLoopbackEndpointRemovalConfig const& removalConfig) noexcept;

        static winrt::guid GetAssociationId(_In_ midi2::MidiEndpointDeviceInformation const& basicLoopbackEndpoint) noexcept;

        static bool DoesLoopbackExist(_In_ winrt::hstring const& uniqueIdentifier);


        static bool MuteLoopback(_In_ winrt::guid const& associationId);
        static bool UnmuteLoopback(_In_ winrt::guid const& associationId);
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::BasicLoopback::factory_implementation
{
    struct MidiBasicLoopbackEndpointManager : MidiBasicLoopbackEndpointManagerT<MidiBasicLoopbackEndpointManager, implementation::MidiBasicLoopbackEndpointManager>
    {
    };
}
