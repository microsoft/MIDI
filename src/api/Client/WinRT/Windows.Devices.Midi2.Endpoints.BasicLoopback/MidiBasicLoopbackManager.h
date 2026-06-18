// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "MidiBasicLoopbackManager.g.h"

namespace winrt::Windows::Devices::Midi2::Endpoints::BasicLoopback::implementation
{
    struct MidiBasicLoopbackManager
    {
        MidiBasicLoopbackManager() = default;

        static bool IsTransportAvailable() noexcept;

        static const winrt::guid TransportId() noexcept { return internal::StringToGuid(L"{10088473-9478-4E62-850B-3D2315E135B8}"); }

        static bloop::MidiBasicLoopbackCreationResult CreateTransientLoopback(
            _In_ bloop::MidiBasicLoopbackCreationConfig const& creationConfig) noexcept;

        static bloop::MidiBasicLoopbackRemovalResult RemoveTransientLoopback(
            _In_ bloop::MidiBasicLoopbackRemovalConfig const& removalConfig) noexcept;

        static winrt::guid GetAssociationId(_In_ midi2enum::MidiEndpointDeviceInformation const& basicLoopbackEndpoint) noexcept;

        static bool DoesLoopbackExist(_In_ winrt::hstring const& uniqueIdentifier);


        static bloop::MidiBasicLoopbackUpdateResult MuteLoopback(_In_ winrt::guid const& associationId);
        static bloop::MidiBasicLoopbackUpdateResult UnmuteLoopback(_In_ winrt::guid const& associationId);


        static collections::IVector<bloop::MidiBasicLoopbackEntry> GetActiveLoopbackEntries();
    };
}
namespace winrt::Windows::Devices::Midi2::Endpoints::BasicLoopback::factory_implementation
{
    struct MidiBasicLoopbackManager : MidiBasicLoopbackManagerT<MidiBasicLoopbackManager, implementation::MidiBasicLoopbackManager>
    {
    };
}
