// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiApi.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiApi
    {
        MidiApi() = default;

        static bool EnsureServiceAvailable() noexcept;


        static MidiApiMode GetCurrentApiMode() noexcept;
    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiApi : MidiApiT<MidiApi, implementation::MidiApi, winrt::static_lifetime>
    {
    };
}
