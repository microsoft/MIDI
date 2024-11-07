// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



#pragma once
#include "Endpoints.Virtual.MidiVirtualDeviceManager.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Virtual::implementation
{
    struct MidiVirtualDeviceManager : MidiVirtualDeviceManagerT<MidiVirtualDeviceManager>
    {
        MidiVirtualDeviceManager() = default;

        static bool IsTransportAvailable() noexcept;
        static const winrt::guid TransportId() noexcept { return internal::StringToGuid(L"{8FEAAD91-70E1-4A19-997A-377720A719C1}"); }

        static virt::MidiVirtualDevice CreateVirtualDevice(_In_ virt::MidiVirtualDeviceCreationConfig creationConfig) noexcept;
        //static bool RemoveVirtualDevice(_In_ virt::MidiVirtualDeviceRemovalConfig removalConfig) noexcept;
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Virtual::factory_implementation
{
    struct MidiVirtualDeviceManager : MidiVirtualDeviceManagerT<MidiVirtualDeviceManager, implementation::MidiVirtualDeviceManager, winrt::static_lifetime>
    {
    };
}
