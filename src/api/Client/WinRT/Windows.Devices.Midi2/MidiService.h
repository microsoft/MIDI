// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiService.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiService
    {
        MidiService() = default;

        static bool EnsureAvailable();


        static MidiApiMode GetCurrentApiMode() noexcept;
    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiService : MidiServiceT<MidiService, implementation::MidiService, winrt::static_lifetime>
    {
    };
}
